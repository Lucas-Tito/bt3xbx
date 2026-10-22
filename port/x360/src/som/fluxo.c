/*
 * F6, etapa 5: a leitura dos fluxos ADX em blocos, fora da thread do jogo (fluxo.h, plano/F6/05.md).
 *
 * Os fluxos ficam numa tabela fixa (FLUXOS). Cada um tem dois buffers; cada buffer guarda [ini, ini + n) do arquivo e
 * está vazio, pedido, sendo lido (só a thread de leitura mexe nele então) ou pronto. A thread de leitura escolhe o
 * pedido mais urgente (o primeiro bloco de um fluxo e as faltas antes dos adiantados; entre iguais, o mais velho), lê
 * sem a trava e devolve. Quem toca só copia dos buffers prontos, com a trava por alguns acessos à memória.
 */
#ifndef _XBOX
#define _POSIX_C_SOURCE 200112L
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fluxo.h"

#ifdef _XBOX
#include <xtl.h>
#pragma warning(disable : 4996) /* (o getenv é o de posix360.c, que lê o ambiente.txt) */
/* o fopen do jogo: a ISO (iso360.c) pelo shim do posix360.c */
FILE *Port_X360Fopen(const char *caminho, const char *modo);
size_t Port_X360Fread(void *buf, size_t tam, size_t n, FILE *f);
int Port_X360Fseek(FILE *f, long desl, int origem);
long Port_X360Ftell(FILE *f);
int Port_X360Fclose(FILE *f);
#define abre Port_X360Fopen
#define le Port_X360Fread
#define posiciona Port_X360Fseek
#define posicao Port_X360Ftell
#define fecha Port_X360Fclose
#else
#include <pthread.h>
#include <time.h>
#define abre fopen
#define le fread
#define posiciona fseek
#define posicao ftell
#define fecha fclose
#endif

#define FLUXOS 32
#define CAMINHO 512

enum { VAZIO, PEDIDO, LENDO, PRONTO, FALHOU };

typedef struct Buf {
    unsigned char *dados;
    long cap;          /* bytes reservados */
    long ini, n;       /* a parte do arquivo que tem (n: a pedida enquanto não está pronta) */
    int estado, urgente;
    unsigned ordem;    /* a ordem do pedido */
} Buf;

struct Fluxo {
    int usado, fechar;
    char caminho[CAMINHO];
    FILE *arq;
    long tam;          /* -1 enquanto o arquivo não abriu */
    int estado;        /* FLUXO_PENDENTE, FLUXO_PRONTO ou FLUXO_ERRO */
    long lacoIni, lacoFim;
    int atual;         /* o buffer do último Fluxo_Pegar que achou os dados */
    Buf b[2];
};

static Fluxo sTab[FLUXOS];
static long sBloco;
static unsigned sOrdem;
static int sLigado, sParado, sSair;
static FluxoContadores sCont;

#ifdef _XBOX
static CRITICAL_SECTION sTrava;
static HANDLE sAcorda, sThread;
static unsigned agora_ms(void) { return GetTickCount(); }
static void trava(void) { EnterCriticalSection(&sTrava); }
static void destrava(void) { LeaveCriticalSection(&sTrava); }
static void acorda(void) { SetEvent(sAcorda); }
static void dorme(void) { /* (evento automático: um SetEvent antes da espera não se perde) */
    LeaveCriticalSection(&sTrava);
    WaitForSingleObject(sAcorda, 1000);
    EnterCriticalSection(&sTrava);
}
#else
static pthread_mutex_t sTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sAcorda = PTHREAD_COND_INITIALIZER;
static pthread_t sThread;
static unsigned agora_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned)t.tv_sec * 1000u + (unsigned)(t.tv_nsec / 1000000);
}
static void trava(void) { pthread_mutex_lock(&sTrava); }
static void destrava(void) { pthread_mutex_unlock(&sTrava); }
static void acorda(void) { pthread_cond_signal(&sAcorda); }
static void dorme(void) { pthread_cond_wait(&sAcorda, &sTrava); }
#endif

/* o próximo bloco depois do buffer b (com a trava): -1 se não há (fim do arquivo, ou o laço inteiro já está nele) */
static long proximo(const Fluxo *f, const Buf *b) {
    if (f->lacoFim > f->lacoIni && b->ini < f->lacoFim && f->lacoFim <= b->ini + b->n) {
        return f->lacoIni >= b->ini ? -1 : f->lacoIni; /* o fim do laço está aqui: depois, o começo dele */
    }
    if (f->tam < 0 || b->ini + sBloco >= f->tam) {
        return -1;
    }
    return b->ini + sBloco;
}

static void pede(Buf *b, long ini, int urgente) {
    b->ini = ini;
    b->n = sBloco + FLUXO_SOBRA;
    b->estado = PEDIDO;
    b->urgente = urgente;
    b->ordem = ++sOrdem;
}

/* -------------------------------------------------------------------------------------- a thread de leitura */

/* lê o pedido b de f (sem a trava); 1 se leu */
static int ler(Fluxo *f, Buf *b, long *lidos) {
    long n = b->n;
    if (f->tam >= 0 && b->ini + n > f->tam) {
        n = f->tam - b->ini;
    }
    if (n <= 0) {
        return 0;
    }
    if (b->cap < n) {
        unsigned char *novo = (unsigned char *)malloc((size_t)(sBloco + FLUXO_SOBRA));
        if (novo == NULL) {
            return 0;
        }
        trava();
        sCont.memoria += (unsigned)(sBloco + FLUXO_SOBRA) - (unsigned)b->cap;
        destrava();
        free(b->dados);
        b->dados = novo;
        b->cap = sBloco + FLUXO_SOBRA;
    }
    if (posiciona(f->arq, b->ini, SEEK_SET) != 0 || le(b->dados, 1, (size_t)n, f->arq) != (size_t)n) {
        return 0;
    }
#ifndef _XBOX
    { /* (teste do WSL) BT3_FLUXO_SABOTA=1: um byte trocado em todo bloco que não é o primeiro (controle negativo) */
        const char *s = getenv("BT3_FLUXO_SABOTA");
        if (s != NULL && s[0] == '1' && b->ini > 0 && n > 100) {
            b->dados[100] ^= 0x5A;
        }
    }
#endif
    *lidos = n;
    return 1;
}

static void conta_leitura(unsigned ms, long n) {
    static const unsigned limites[7] = {2, 5, 10, 20, 50, 100, 250};
    int k = 0;
    while (k < 7 && ms >= limites[k]) {
        k++;
    }
    sCont.hist[k]++;
    sCont.leituras++;
    sCont.msSoma += ms;
    if (ms > sCont.msPior) {
        sCont.msPior = ms;
    }
    sCont.bytes += (unsigned)n;
    while (sCont.bytes >= 1024) {
        sCont.bytes -= 1024;
        sCont.kib++;
    }
}

#ifdef _XBOX
static DWORD WINAPI leitor(LPVOID arg)
#else
static void *leitor(void *arg)
#endif
{
    (void)arg;
    trava();
    while (!sSair) {
        Fluxo *f = NULL, *solta = NULL;
        Buf *b = NULL;
        int i, k, ok, abrir;
        long lidos = 0;
        unsigned t0;
        for (i = 0; i < FLUXOS; i++) {
            Fluxo *g = &sTab[i];
            if (!g->usado) {
                continue;
            }
            if (g->fechar) {
                solta = g;
                break;
            }
            for (k = 0; k < 2; k++) {
                Buf *c = &g->b[k];
                if (c->estado == PEDIDO && (b == NULL || c->urgente > b->urgente ||
                                            (c->urgente == b->urgente && (int)(c->ordem - b->ordem) < 0))) {
                    f = g;
                    b = c;
                }
            }
        }
        if (solta != NULL) { /* um fluxo que quem toca soltou: o arquivo se fecha aqui, sem a trava */
            FILE *a = solta->arq;
            solta->arq = NULL;
            destrava();
            if (a != NULL) {
                fecha(a);
            }
            trava();
            solta->fechar = 0;
            solta->usado = 0;
            sCont.abertos--;
            continue;
        }
        if (b == NULL) {
            dorme();
            continue;
        }
        b->estado = LENDO;
        abrir = f->arq == NULL && f->estado != FLUXO_ERRO;
        destrava();
        t0 = agora_ms();
        ok = 1;
        if (abrir) {
            FILE *a = abre(f->caminho, "rb");
            long tam = -1;
            if (a != NULL && posiciona(a, 0, SEEK_END) == 0) {
                tam = posicao(a);
            }
            if (a != NULL && tam < 0) {
                fecha(a);
                a = NULL;
            }
            trava();
            f->arq = a;
            f->tam = tam;
            destrava();
            ok = a != NULL;
        }
        ok = ok && f->arq != NULL && ler(f, b, &lidos);
        t0 = agora_ms() - t0;
        trava();
        if (ok) {
            b->n = lidos; /* (numa falha fica o pedido: quem pedir a mesma parte não pede de novo) */
        }
        b->estado = ok ? PRONTO : FALHOU;
        if (b == &f->b[0] && f->estado == FLUXO_PENDENTE) {
            f->estado = ok ? FLUXO_PRONTO : FLUXO_ERRO;
        }
        if (ok) {
            conta_leitura(t0, lidos);
        } else {
            sCont.erros++;
        }
    }
    sParado = 1;
    destrava();
    return 0;
}

static int liga(void) {
    const char *s;
    if (sLigado) {
        return !sParado;
    }
    sLigado = 1;
    s = getenv("BT3_FLUXO_BLOCO");
    sBloco = s != NULL ? atol(s) : 0;
    if (sBloco < 1024 || sBloco > 4 * 1024 * 1024) {
        sBloco = 65536;
    }
#ifdef _XBOX
    InitializeCriticalSection(&sTrava);
    sAcorda = CreateEvent(NULL, FALSE, FALSE, NULL);
    sThread = CreateThread(NULL, 64 * 1024, leitor, NULL, CREATE_SUSPENDED, NULL);
    if (sAcorda == NULL || sThread == NULL) {
        sParado = 1;
        return 0;
    }
    XSetThreadProcessor(sThread, 1); /* (a 1: longe do jogo, na 2, e do XAudio2, na 4 e na 5) */
    SetThreadPriority(sThread, THREAD_PRIORITY_ABOVE_NORMAL);
    ResumeThread(sThread);
#else
    if (pthread_create(&sThread, NULL, leitor, NULL) != 0) {
        sParado = 1;
        return 0;
    }
#endif
    return 1;
}

/* ------------------------------------------------------------------------------------------ quem toca chama */

Fluxo *Fluxo_Abrir(const char *caminho, long primeiro) {
    Fluxo *f = NULL;
    int i;
    if (caminho == NULL || strlen(caminho) >= CAMINHO || !liga()) {
        return NULL;
    }
    trava();
    for (i = 0; i < FLUXOS && !sSair; i++) {
        if (!sTab[i].usado) {
            f = &sTab[i];
            break;
        }
    }
    if (f != NULL) {
        f->usado = 1;
        f->fechar = 0;
        strcpy(f->caminho, caminho);
        f->arq = NULL;
        f->tam = -1;
        f->estado = FLUXO_PENDENTE;
        f->lacoIni = f->lacoFim = 0;
        f->atual = 0;
        f->b[0].estado = f->b[1].estado = VAZIO;
        pede(&f->b[0], 0, 2);
        if (primeiro > 0 && primeiro < sBloco) {
            f->b[0].n = primeiro + FLUXO_SOBRA;
        }
        sCont.aberturas++;
        if (++sCont.abertos > sCont.abertosMax) {
            sCont.abertosMax = sCont.abertos;
        }
        acorda();
    }
    destrava();
    return f;
}

int Fluxo_Estado(Fluxo *f, long *tamanho) {
    int e;
    trava();
    e = f->estado;
    if (tamanho != NULL) {
        *tamanho = f->tam;
    }
    destrava();
    return e;
}

const unsigned char *Fluxo_Pegar(Fluxo *f, long pos, int n, int adianta) {
    const unsigned char *r = NULL;
    int k, j;
    trava();
    for (k = 0; k < 2; k++) {
        Buf *b = &f->b[k];
        if (b->estado == PRONTO && pos >= b->ini && pos + n <= b->ini + b->n) {
            r = b->dados + (pos - b->ini);
            break;
        }
    }
    if (r != NULL) {
        f->atual = k;
        j = 1 - k;
        if (adianta && f->b[j].estado != LENDO) {
            long p = proximo(f, &f->b[k]);
            if (p >= 0 && !(f->b[j].ini == p && f->b[j].estado != VAZIO)) { /* (o que falhou não se pede de novo) */
                pede(&f->b[j], p, 0);
                sCont.adiantados++;
                acorda();
            }
        }
        destrava();
        return r;
    }
    sCont.faltas++;
    for (k = 0; k < 2; k++) { /* já pedido: espera (ou falhou: silêncio, sem ler de novo) */
        Buf *b = &f->b[k];
        if ((b->estado == PEDIDO || b->estado == LENDO || b->estado == FALHOU) && pos >= b->ini &&
            pos + n <= b->ini + b->n) {
            if (b->estado == PEDIDO) {
                b->urgente = 1;
            }
            destrava();
            return NULL;
        }
    }
    /* uma falta: pede o bloco que começa em pos, num buffer que não esteja sendo lido (de preferência não o atual) */
    j = 1 - f->atual;
    if (f->b[j].estado == LENDO) {
        j = f->atual;
    }
    if (f->b[j].estado != LENDO) {
        pede(&f->b[j], pos, 1);
        acorda();
    }
    destrava();
    return NULL;
}

void Fluxo_Laco(Fluxo *f, long ini, long fim) {
    trava();
    f->lacoIni = ini;
    f->lacoFim = fim;
    destrava();
}

void Fluxo_Fechar(Fluxo *f) {
    if (f == NULL) {
        return;
    }
    trava();
    if (f->b[0].estado == PEDIDO) {
        f->b[0].estado = VAZIO;
    }
    if (f->b[1].estado == PEDIDO) {
        f->b[1].estado = VAZIO;
    }
    f->fechar = 1;
    acorda();
    destrava();
}

void Fluxo_Parar(void) {
    if (!sLigado || sParado) {
        return;
    }
    trava();
    sSair = 1;
    acorda();
    destrava();
#ifdef _XBOX
    WaitForSingleObject(sThread, 2000);
#else
    pthread_join(sThread, NULL);
#endif
}

void Fluxo_Contadores(FluxoContadores *c) {
    if (!sLigado || sParado) {
        *c = sCont;
        return;
    }
    trava();
    *c = sCont;
    destrava();
}
