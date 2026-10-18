#!/usr/bin/env bash
# Teste do diário do 360 (port/x360/src/diario.c: anel em memória e thread escritora) no WSL, sem o console:
#   port/x360/tools/teste_diario.sh             roda os testes sobre o diario.c de verdade
#   port/x360/tools/teste_diario.sh --mutantes  estraga o diario.c de propósito, quatro vezes, e exige que o teste
#                                               acuse cada uma (o verificador visto falhando)
# O diario.c é compilado para o Linux com uma camada mínima da API do Xbox (pthreads no lugar das threads e das seções
# críticas; um "disco" que demora 40 ms por FlushFileBuffers, como o FATX do HD, e que guarda até onde os dados foram
# commitados, para simular uma queda). O que se confere:
#   1. ordem e integridade: quatro threads, mais de 1 MB de linhas (o anel dá a volta), cada linha inteira e cada
#      thread na sua ordem, nenhuma perdida com a escritora andando;
#   2. quem escreve não espera o disco: o pior Diario_Escrever fica abaixo de 10 ms com o flush de 40 ms; no jeito
#      síncrono (BT3_LOG_SINCRONO) o pior passa de 30 ms (o controle: o teste enxerga o custo antigo);
#   3. a queda: depois de Diario_Descarregar, tudo o que foi escrito já está commitado; sem ele, logo depois da escrita,
#      ainda não (o controle: o teste enxerga a diferença);
#   4. anel cheio com a escritora parada: quem escreve não trava, a conta fecha (linhas no arquivo + perdidas = escritas)
#      e o aviso "linhas perdidas" aparece;
#   5. Diario_Fechar grava tudo e a thread acaba.
# O que não confere: o custo real do FlushFileBuffers no FATX, a thread de hardware 1 e a prioridade (só o console diz).
set -euo pipefail
RAIZ=$(cd "$(dirname "$0")/../../.." && pwd)
cd "$RAIZ"
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/inc"

cat > "$T/inc/xtl.h" <<'EOF'
/* o pedaço da API do Xbox que o diario.c usa, sobre pthreads e o sistema de arquivos do Linux */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
typedef int LONG;
typedef unsigned int DWORD;
typedef void *LPVOID;
typedef struct Alca { int tipo; int fd; pthread_t th; DWORD (*fn)(LPVOID); LPVOID arg; int ligada; } *HANDLE;
typedef struct { long long QuadPart; } LARGE_INTEGER;
typedef pthread_mutex_t CRITICAL_SECTION;
#define WINAPI
#define INVALID_HANDLE_VALUE ((HANDLE)0)
#define GENERIC_WRITE 0
#define FILE_SHARE_READ 0
#define CREATE_ALWAYS 0
#define FILE_ATTRIBUTE_NORMAL 0
#define CREATE_SUSPENDED 4
#define THREAD_PRIORITY_BELOW_NORMAL (-1)
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#define GetLastError() 0u
static long long agora_ns(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000000000LL + t.tv_nsec; }
static void QueryPerformanceCounter(LARGE_INTEGER *x) { x->QuadPart = agora_ns(); }
static void QueryPerformanceFrequency(LARGE_INTEGER *x) { x->QuadPart = 1000000000LL; }
static DWORD GetTickCount(void) { return (DWORD)(agora_ns() / 1000000); }
static void Sleep(DWORD ms) { usleep(ms * 1000); }
static void InitializeCriticalSection(CRITICAL_SECTION *c) {
    pthread_mutexattr_t a; pthread_mutexattr_init(&a); pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(c, &a); }
#define EnterCriticalSection(c) pthread_mutex_lock(c)
#define LeaveCriticalSection(c) pthread_mutex_unlock(c)
#define InterlockedCompareExchange(d, novo, velho) __sync_val_compare_and_swap((d), (velho), (novo))
static LONG InterlockedExchange(volatile LONG *d, LONG v) { return __sync_lock_test_and_set(d, v); }

/* o "disco": cada WriteFile vai ao arquivo; o FlushFileBuffers demora gGravaMs e marca até onde está commitado */
long long gEscrito, gDuravel;
int gFlushMs = 40, gParado;       /* gParado: o WriteFile espera (a escritora travada, o teste do anel cheio) */
pthread_mutex_t gDisco = PTHREAD_MUTEX_INITIALIZER;
static HANDLE CreateFile(const char *n, int a, int b, void *c, int d, int e, void *f) {
    HANDLE h = calloc(1, sizeof(*h)); (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    h->tipo = 1; h->fd = open(n, O_WRONLY | O_CREAT | O_TRUNC, 0644); return h; }
static int WriteFile(HANDLE h, const void *p, DWORD n, DWORD *esc, void *x) {
    int espera = 0; (void)x;
    while (gParado && espera++ < 3000) usleep(1000); /* (no máximo 3 s: um diario.c que grava na thread de quem escreve prenderia o teste) */
    pthread_mutex_lock(&gDisco);
    if (write(h->fd, p, n) != (ssize_t)n) { abort(); }
    gEscrito += n; pthread_mutex_unlock(&gDisco); *esc = n; return 1; }
static int FlushFileBuffers(HANDLE h) {
    long long ate; (void)h;
    pthread_mutex_lock(&gDisco); ate = gEscrito; pthread_mutex_unlock(&gDisco);
    usleep(gFlushMs * 1000);
    pthread_mutex_lock(&gDisco); if (ate > gDuravel) gDuravel = ate; pthread_mutex_unlock(&gDisco);
    return 1; }
static void *parte(void *p) { HANDLE h = p; h->fn(h->arg); return 0; }
static HANDLE CreateThread(void *a, int pilha, DWORD (*fn)(LPVOID), LPVOID arg, int flags, DWORD *id) {
    HANDLE h = calloc(1, sizeof(*h)); (void)a; (void)pilha; (void)id;
    h->tipo = 2; h->fn = fn; h->arg = arg;
    if (!(flags & CREATE_SUSPENDED)) { h->ligada = 1; pthread_create(&h->th, 0, parte, h); }
    return h; }
static DWORD ResumeThread(HANDLE h) { if (!h->ligada) { h->ligada = 1; pthread_create(&h->th, 0, parte, h); } return 1; }
static int XSetThreadProcessor(HANDLE h, int n) { (void)h; (void)n; return 0; }
static int SetThreadPriority(HANDLE h, int n) { (void)h; (void)n; return 1; }
static DWORD WaitForSingleObject(HANDLE h, DWORD ms) { (void)ms; if (h->tipo == 2 && h->ligada) pthread_join(h->th, 0); return 0; }
static int CloseHandle(HANDLE h) { if (h->tipo == 1) close(h->fd); free(h); return 1; }
EOF

cat > "$T/inc/diario.h" <<'EOF'
void Diario_Abrir(const char *caminho);
void Diario_Escrever(const char *formato, ...);
void Diario_Fechar(void);
void Diario_Descarregar(void);
void Diario_Configurar(int sincrono, int flushMs);
void Diario_Gancho(void (*funcao)(void));
int Diario_Assincrono(void);
EOF

cat > "$T/teste.c" <<'EOF'
#include "diario.c"

static int falhas;
static void confere(int ok, const char *nome, const char *detalhe) {
    printf("%s %s%s%s\n", ok ? "ok    " : "FALHOU", nome, detalhe[0] ? ": " : "", detalhe);
    falhas += !ok;
}

#define NT 4
#define NL 3000
static long long pior_us[NT], gInicio;
static void *escreve(void *p) {
    long t = (long)p;
    int i;
    for (i = 0; i < NL; i++) {
        long long a = agora_ns(), d;
        if (a - gInicio > 8000000000LL) break; /* (um diario.c que paga o disco por linha, nos mutantes, levaria minutos) */
        /* linhas de 40 a 400 bytes: o anel (256 KB) dá a volta várias vezes e há emendas no meio de linhas */
        Diario_Escrever("t%ld l%05d %.*s", t, i, 20 + (i * 37) % 360,
                        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
                        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
                        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
                        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz");
        d = (agora_ns() - a) / 1000;
        if (d > pior_us[t]) pior_us[t] = d;
        if (i % 2 == 0) usleep(1000); /* a thread do jogo não escreve sem parar: o anel tem tempo de ser esvaziado */
    }
    return 0;
}

/* confere o arquivo: devolve quantas linhas dos escritores há, e preenche a ordem e a integridade */
static int le_arquivo(const char *caminho, int *ordem_ok, int *inteiras_ok, int *avisos, long *perdidas_aviso) {
    FILE *f = fopen(caminho, "r");
    char linha[2048];
    int prox[NT] = {0}, n = 0;
    *ordem_ok = *inteiras_ok = 1; *avisos = 0; *perdidas_aviso = 0;
    while (fgets(linha, sizeof(linha), f) != NULL) {
        long t; int l, esperado;
        size_t len = strlen(linha);
        if (len == 0 || linha[len - 1] != '\n') { *inteiras_ok = 0; continue; }
        if (strncmp(linha, "[... ", 5) == 0) { long p = 0; sscanf(linha, "[... %ld", &p); *perdidas_aviso += p; (*avisos)++; continue; }
        if (linha[0] != 't' || sscanf(linha, "t%ld l%d", &t, &l) != 2) continue; /* (as linhas "diario:") */
        if (t < 0 || t >= NT) { *inteiras_ok = 0; continue; }
        /* o tamanho tem de ser o que o escritor gerou: "tN lNNNNN " + 20 + (l*37)%360 letras */
        esperado = 10 + 20 + (l * 37) % 360 + 1;
        if ((int)len != esperado) *inteiras_ok = 0;
        if (l < prox[t]) *ordem_ok = 0; /* só para a frente (com perdas, saltos) */
        prox[t] = l + 1;
        n++;
    }
    fclose(f);
    return n;
}

int main(int argc, char **argv) {
    const char *arq = argv[1];
    pthread_t th[NT];
    long t;
    int ordem, inteiras, avisos, n, i;
    long perdidas;
    long long pior;
    int esc4 = 0;
    char det[200];

    /* 1 e 2: o jeito do anel, com o disco de 40 ms por flush */
    Diario_Abrir(arq);
    confere(Diario_Assincrono(), "a thread escritora ligou", "");
    Diario_Configurar(0, 0);
    gInicio = agora_ns();
    for (t = 0; t < NT; t++) pthread_create(&th[t], 0, escreve, (void *)t);
    for (t = 0; t < NT; t++) pthread_join(th[t], 0);
    Diario_Fechar();
    n = le_arquivo(arq, &ordem, &inteiras, &avisos, &perdidas);
    snprintf(det, sizeof(det), "%d de %d linhas, %d avisos de perda", n, NT * NL, avisos);
    confere(n == NT * NL && perdidas == 0, "nenhuma linha perdida num ritmo de jogo, o anel dando a volta várias vezes", det);
    confere(ordem, "cada thread na sua ordem", "");
    confere(inteiras, "toda linha inteira, do tamanho certo", "");
    pior = 0; for (t = 0; t < NT; t++) if (pior_us[t] > pior) pior = pior_us[t];
    snprintf(det, sizeof(det), "pior Diario_Escrever %lld us, com o flush de %d ms", pior, gFlushMs);
    confere(pior < 10000, "quem escreve não espera o disco", det);
    confere(sThread == NULL && sArquivo == INVALID_HANDLE_VALUE, "Diario_Fechar fechou e parou a thread", "");

    /* 2 (controle): o jeito síncrono paga o flush na própria chamada */
    memset(pior_us, 0, sizeof(pior_us));
    gEscrito = gDuravel = 0;
    Diario_Abrir(arq);
    Diario_Configurar(1, 0);
    { long long a, d, pior_s = 0;
      for (i = 0; i < 5; i++) { a = agora_ns(); Diario_Escrever("t0 l%05d x", i); d = (agora_ns() - a) / 1000; if (d > pior_s) pior_s = d; }
      snprintf(det, sizeof(det), "pior Diario_Escrever %lld us", pior_s);
      confere(pior_s > 30000, "controle: o jeito síncrono paga o flush (BT3_LOG_SINCRONO)", det); }
    Diario_Fechar();

    /* 3: a queda */
    gEscrito = gDuravel = 0;
    Diario_Configurar(0, 60000); /* (antes do Abrir: o jeito síncrono do teste anterior não commita as linhas de abertura) */
    Diario_Abrir(arq);
    for (i = 0; i < 50; i++) Diario_Escrever("t0 l%05d %s", i, "linha antes da queda");
    snprintf(det, sizeof(det), "escrito %lld, commitado %lld", gEscrito, gDuravel);
    confere(gDuravel == 0, "controle: sem Diario_Descarregar, logo depois da escrita, nada está commitado", det);
    Diario_Descarregar();
    snprintf(det, sizeof(det), "escrito %lld, commitado %lld", gEscrito, gDuravel);
    confere(gEscrito > 0 && gDuravel == gEscrito, "depois de Diario_Descarregar tudo está commitado (a queda não perde nada)", det);
    Diario_Fechar();

    /* 4: anel cheio, escritora parada (o disco não responde): quem escreve não trava, a conta fecha */
    gEscrito = gDuravel = 0;
    Diario_Abrir(arq);
    Diario_Configurar(0, 0);
    gParado = 1;
    Sleep(150); /* a escritora entra no WriteFile parado com o que houver; o resto fica no anel */
    { long long a = agora_ns(), d;
      for (i = 0; i < 3000 && agora_ns() - a < 8000000000LL; i++) Diario_Escrever("t0 l%05d %.*s", i, 20 + (i * 37) % 360,
        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz");
      d = (agora_ns() - a) / 1000000;
      esc4 = i;
      snprintf(det, sizeof(det), "%d linhas (uns 600 KB) em %lld ms com o disco parado", i, d);
      confere(i == 3000 && d < 1000, "anel cheio: quem escreve não trava", det); }
    gParado = 0;
    Diario_Fechar();
    n = le_arquivo(arq, &ordem, &inteiras, &avisos, &perdidas);
    snprintf(det, sizeof(det), "%d linhas no arquivo + %ld perdidas = %d (escritas %d)", n, perdidas, n + (int)perdidas, esc4);
    confere(n + perdidas == esc4 && perdidas > 0, "anel cheio: a conta fecha e houve perda", det);
    confere(avisos >= 1, "anel cheio: o aviso de linhas perdidas está no arquivo", "");
    confere(ordem && inteiras, "anel cheio: o que ficou está inteiro e em ordem", "");

    printf("%s\n", falhas ? "FALHAS" : "tudo certo");
    (void)argc;
    return falhas != 0;
}
EOF

# o diario.c tem de ser C89 (o cl.exe -TC): declarações no começo do bloco, sem //, sem inline
gcc -std=c89 -pedantic -Wall -Wextra -Wdeclaration-after-statement -Wno-unused-function -Wno-long-long \
    -I"$T/inc" -fsyntax-only port/x360/src/diario.c
echo "diario.c: C89 limpo"

roda() { # roda $1 (o diario.c a testar)
    cp "$1" "$T/diario.c"
    gcc -std=gnu99 -O1 -Wall -Wextra -Wno-unused-function -I"$T/inc" -I"$T" -o "$T/teste" "$T/teste.c" -lpthread
    "$T/teste" "$T/saida.log"
}

if [ "${1:-}" = "--mutantes" ]; then
    mut() { # mut NOME 'sed' : o diario.c estragado tem de fazer o teste falhar
        sed "$2" port/x360/src/diario.c > "$T/mutante.c"
        if cmp -s "$T/mutante.c" port/x360/src/diario.c; then echo "mutante $1: o sed não mudou nada"; exit 2; fi
        if roda "$T/mutante.c" > "$T/mutante.txt" 2>&1; then
            echo "mutante $1: o teste NÃO acusou (verificador cego)"; cat "$T/mutante.txt"; exit 1
        fi
        echo "mutante $1: acusado ($(grep -c '^FALHOU' "$T/mutante.txt") falhas): $(grep '^FALHOU' "$T/mutante.txt" | head -2 | cut -c1-110 | tr '\n' '|')"
    }
    # a queda: o Descarregar não faz nada
    mut sem-descarga '/^void Diario_Descarregar/,/^}/{s/    Drenar();/    \/\* sem \*\//;s/    Comitar();/    \/\* sem \*\//}'
    # a emenda do anel: a segunda parte da gravação some
    mut sem-emenda 's/        if (n > parte) {/        if (0 \&\& n > parte) {/'
    # quem escreve paga o disco: o Escrever sempre grava e dá flush
    mut escreve-no-disco 's/    if (sThread == NULL || sSincrono) {/    if (1) {/'
    # o anel cheio bloqueia em vez de descartar: sem a conta das perdidas
    mut sem-conta-de-perdas 's/        sPerdidas++;/        \/\* sem \*\//'
    echo "os quatro mutantes foram acusados"
    exit 0
fi

roda port/x360/src/diario.c
