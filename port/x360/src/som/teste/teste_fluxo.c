/*
 * F6, etapa 5: o teste da leitura em blocos (port/x360/src/som/fluxo.c), sem o jogo nem dados dele: um arquivo de
 * 1.000.000 bytes com um padrão conhecido, blocos de 4096 (BT3_FLUXO_BLOCO). Roda com port/x360/tools/teste_som360.sh,
 * que também estraga cópias do fluxo.c (mutantes) e confere que este teste acusa cada uma.
 *   1. leitura em sequência por quadros de 36 bytes, com laço que cruza blocos, três voltas: os bytes certos e
 *      nenhuma falta depois do primeiro bloco (o seguinte e o do começo do laço chegam adiantados);
 *   2. muitos fluxos abertos e fechados ao acaso, fechados também enquanto o bloco está sendo lido: os bytes certos,
 *      a tabela volta a zero e a memória fica no teto;
 *   3. arquivo que não existe: FLUXO_ERRO; só o cabeçalho: o primeiro bloco pequeno; depois de Fluxo_Parar, NULL;
 *   4. (F6, a thread do jogo sem esperar) o caminho do console: o fluxo.c com FLUXO_PELO_PORT abre pelo
 *      Port_X360Fopen daqui (o de port/x360/src/f2/posix360.c sem os mods/) e lê pelo port/x360/src/iso360.c de
 *      verdade, compilado sobre teste/xtl/xtl.h, numa imagem ISO feita aqui (sem nada do jogo). A leitura é lenta
 *      (50 ms dormidos dentro do ReadFile, com a trava de quem lê pega, como o disco do console) e a thread do jogo
 *      tem de abrir abaixo de 1 ms: (a) com a thread de leitura dos fluxos no meio de uma leitura, a abertura de um
 *      ADX (o Port_FilePath, que abre e fecha a entrada da ISO, e o Fluxo_Abrir) e uma carga do jogo pelo handle
 *      comum; (b) com outra thread lendo pelo handle comum (como o filme), a abertura de um ADX, a primeira daquele
 *      AFS. O teste_som360.sh estraga cópias do iso360.c (a leitura dos fluxos pela trava comum, o afs() e a tabela
 *      dos abertos com ela, a tabela do AFS que não se lê no boot) e confere que o teste acusa cada uma.
 */
#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "fluxo.h"

#define TAM 1000000L
#define BLOCO 4096L
static char sArq[256];
static int sFalhas;

static unsigned char byte_em(long i) { return (unsigned char)(((unsigned long)i * 2654435761u) >> 13); }

static void dorme_us(long us) {
    struct timespec t;
    t.tv_sec = us / 1000000;
    t.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&t, NULL);
}

#define CONFERE(c, ...) do { if (!(c)) { printf("FALHA: " __VA_ARGS__); printf("\n"); sFalhas++; } } while (0)

/* ------------------------------------------------- o que o console põe em volta do fluxo.c (FLUXO_PELO_PORT) */

void Iso_Iniciar(void);
int Iso_Recusada(void);
void *Iso_Abrir(const char *rel);
int Iso_Eh(const void *f);
size_t Iso_Ler(void *buf, size_t tam, size_t n, void *f);
int Iso_Posicionar(void *f, long desl, int origem);
long Iso_Posicao(void *f);
int Iso_Fechar(void *f);

static char sRaiz[300]; /* a raiz dos dados (BT3_DATA): o que está sob ela vem da ISO */

/* o Port_X360Fopen do posix360.c, sem os mods/ e os conversores */
FILE *Port_X360Fopen(const char *caminho, const char *modo) {
    size_t n = strlen(sRaiz);
    if (n > 0 && strchr(modo, 'w') == NULL && strncmp(caminho, sRaiz, n) == 0 && caminho[n] == '/') {
        void *v = Iso_Abrir(caminho + n + 1);
        if (v != (void *)-1) {
            return (FILE *)v;
        }
    }
    return fopen(caminho, modo);
}
size_t Port_X360Fread(void *buf, size_t tam, size_t n, FILE *f) {
    return Iso_Eh(f) ? Iso_Ler(buf, tam, n, f) : fread(buf, tam, n, f);
}
int Port_X360Fseek(FILE *f, long desl, int origem) {
    return Iso_Eh(f) ? Iso_Posicionar(f, desl, origem) : fseek(f, desl, origem);
}
long Port_X360Ftell(FILE *f) {
    return Iso_Eh(f) ? Iso_Posicao(f) : ftell(f);
}
int Port_X360Fclose(FILE *f) {
    return Iso_Eh(f) ? Iso_Fechar(f) : fclose(f);
}

/* o resto que o iso360.c chama */
void Diario_Escrever(const char *formato, ...);
#include <stdarg.h>
void Diario_Escrever(const char *formato, ...) {
    va_list ap;
    va_start(ap, formato);
    printf("diário: ");
    vprintf(formato, ap);
    printf("\n");
    va_end(ap);
}
const char *Conv_FormatoDe(const char *nome) { (void)nome; return NULL; }
int Conv_Arquivo(const char *nome, unsigned char *buf, unsigned long tam, unsigned char *feito, char *msg, int nmsg) {
    (void)nome; (void)buf; (void)tam; (void)feito; (void)msg; (void)nmsg;
    return -1;
}
int Filme360_BlocoKb(void) { return 0; }
void Port_Abortar(void) { abort(); }
void *Blocos_Abrir(void *ler, void *ctx, long tam, long bloco, int hw) { (void)ler; (void)ctx; (void)tam; (void)bloco; (void)hw; return NULL; }
long Blocos_Ler(void *b, long pos, void *dst, long n) { (void)b; (void)pos; (void)dst; (void)n; return -1; }
void Blocos_Conta(void *b, void *c) { (void)b; (void)c; }
void Blocos_Fechar(void *b) { (void)b; }

/* a leitora lenta do xtl.h do teste */
long long gTesteLentaIni, gTesteLentaFim;
long gTesteLentaUs;
volatile int gTesteDentro;
#define DENTRO() (__atomic_load_n(&gTesteDentro, __ATOMIC_SEQ_CST) > 0)

/* -------------------------------------------------------------------------------------------------------------- */

static int espera_pronto(Fluxo *f, long *tam) {
    int e, k;
    for (k = 0; k < 200000 && (e = Fluxo_Estado(f, tam)) == FLUXO_PENDENTE; k++) {
        dorme_us(20);
    }
    return e;
}

/* os n bytes em pos, esperando (quantas vezes não estavam: *faltas) */
static int confere_em(Fluxo *f, long pos, int n, int adianta, unsigned *faltas) {
    const unsigned char *p;
    int k, i;
    for (k = 0; (p = Fluxo_Pegar(f, pos, n, adianta)) == NULL; k++) {
        if (k > 200000) {
            return 0;
        }
        (*faltas)++;
        dorme_us(20);
    }
    for (i = 0; i < n; i++) {
        if (p[i] != byte_em(pos + i)) {
            return 0;
        }
    }
    return 1;
}

static void teste_sequencia(void) {
    const long data = 0x120, quadro = 36, lacoIni = data + 1000 * quadro, lacoFim = data + 20000 * quadro;
    long tam, pos, ultimoBloco = -1;
    unsigned faltas = 0, voltas = 0;
    int ok = 1;
    Fluxo *f = Fluxo_Abrir(sArq, 0);
    CONFERE(f != NULL, "1: Fluxo_Abrir deu NULL");
    if (f == NULL) {
        return;
    }
    CONFERE(espera_pronto(f, &tam) == FLUXO_PRONTO && tam == TAM, "1: o primeiro bloco não chegou (tamanho %ld)", tam);
    Fluxo_Laco(f, lacoIni, lacoFim);
    ok = confere_em(f, 0, 0x40, 0, &faltas);
    pos = data;
    while (ok && voltas < 3) {
        if (pos / BLOCO != ultimoBloco) { /* (um bloco novo: tempo para o adiantado, como o tocar de verdade dá) */
            ultimoBloco = pos / BLOCO;
            dorme_us(5000);
        }
        ok = confere_em(f, pos, (int)quadro, 1, &faltas);
        pos += quadro;
        if (pos >= lacoFim) {
            pos = lacoIni;
            voltas++;
            ultimoBloco = -1;
        }
    }
    CONFERE(ok, "1: bytes errados ou que não chegaram em %ld", pos);
    CONFERE(faltas == 0, "1: %u faltas na leitura em sequência com laço (o adiantado não chegou antes)", faltas);
    Fluxo_Fechar(f);
}

static void teste_muitos(void) {
    Fluxo *abertos[20];
    FluxoContadores c;
    unsigned faltas = 0, semLugar = 0;
    int i, k, ok = 1;
    srand(25);
    memset(abertos, 0, sizeof(abertos));
    for (i = 0; i < 3000 && ok; i++) {
        k = rand() % 20;
        if (abertos[k] != NULL && rand() % 2) {
            Fluxo_Fechar(abertos[k]); /* (às vezes logo depois de abrir: com o primeiro bloco ainda sendo lido) */
            abertos[k] = NULL;
        } else if (abertos[k] == NULL) {
            abertos[k] = Fluxo_Abrir(sArq, 0);
            if (abertos[k] == NULL) {
                semLugar++;
            }
        } else {
            long pos = ((long)rand() * 7919L) % (TAM - 100);
            ok = confere_em(abertos[k], pos, 36, rand() % 2, &faltas);
        }
    }
    CONFERE(ok, "2: bytes errados ou que não chegaram com muitos fluxos");
    CONFERE(semLugar == 0, "2: Fluxo_Abrir sem lugar %u vezes com no máximo 20 abertos (a tabela não se esvazia)", semLugar);
    for (k = 0; k < 20; k++) {
        Fluxo_Fechar(abertos[k]);
    }
    for (k = 0; k < 2000; k++) {
        Fluxo_Contadores(&c);
        if (c.abertos == 0) {
            break;
        }
        dorme_us(500);
    }
    CONFERE(c.abertos == 0, "2: %u fluxos ainda abertos depois de fechar todos", c.abertos);
    CONFERE(c.memoria <= 32u * 2u * (unsigned)(BLOCO + FLUXO_SOBRA), "2: memória de %u bytes passou do teto", c.memoria);
    CONFERE(c.erros == 0, "2: %u erros de leitura", c.erros);
    printf("2: %u aberturas, %u blocos lidos, %u faltas pedidas, %u adiantados, no máximo %u abertos, %u bytes reservados\n",
           c.aberturas, c.leituras, c.faltas, c.adiantados, c.abertosMax, c.memoria);
}

static void teste_bordas(void) {
    long tam = 0;
    unsigned faltas = 0;
    char falso[300];
    Fluxo *f;
    snprintf(falso, sizeof(falso), "%s.nao_existe", sArq);
    f = Fluxo_Abrir(falso, 0);
    CONFERE(f != NULL && espera_pronto(f, &tam) == FLUXO_ERRO, "3: o arquivo que não existe não deu FLUXO_ERRO");
    if (f != NULL) {
        CONFERE(Fluxo_Pegar(f, 0, 16, 1) == NULL, "3: dados de um arquivo que não existe");
        Fluxo_Fechar(f);
    }
    f = Fluxo_Abrir(sArq, 0x800);
    CONFERE(f != NULL && espera_pronto(f, &tam) == FLUXO_PRONTO, "3: só o cabeçalho não chegou");
    if (f != NULL) {
        CONFERE(confere_em(f, 0, 0x40, 0, &faltas) && faltas == 0, "3: o cabeçalho errado");
        CONFERE(Fluxo_Pegar(f, 0x800 + FLUXO_SOBRA, 4, 0) == NULL, "3: o primeiro bloco do cabeçalho veio grande");
        Fluxo_Fechar(f);
    }
    Fluxo_Parar();
    CONFERE(Fluxo_Abrir(sArq, 0) == NULL, "3: Fluxo_Abrir depois de Fluxo_Parar");
}

/* ------------------------------------------------------------------------------- 4. a thread do jogo sem esperar */

#define SETOR 2048L
#define LENTA_US 50000L
static char sIso[300];
static long long sIni1, sTam1;      /* PZS3US1.AFS, entrada 0: a que a outra thread lê pelo handle comum (4b) */
static long long sIni2, sTam2;      /* PZS3US2.AFS, entrada 0: a do fluxo lento (4a) */

static void p32(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}
static void grava(FILE *o, long long pos, const void *d, size_t n) {
    fseek(o, (long)pos, SEEK_SET);
    fwrite(d, 1, n, o);
}
/* um registro de diretório da ISO 9660 (o que o achar() do iso360.c lê) */
static int registro(unsigned char *s, int k, const char *nome, unsigned lba, unsigned tam, int dir) {
    int nl = (int)strlen(nome), len = 33 + nl + ((33 + nl) & 1);
    s[k] = (unsigned char)len;
    p32(s + k + 2, lba);
    p32(s + k + 10, tam);
    s[k + 25] = dir ? 2 : 0;
    s[k + 32] = (unsigned char)nl;
    memcpy(s + k + 33, nome, (size_t)nl);
    return k + len;
}
/* um AFS no setor `lba`: as entradas em setores seguidos depois do cabeçalho, com o padrão de byte_em; devolve o
   tamanho e a posição da entrada 0 na imagem */
static unsigned afs_em(FILE *o, unsigned lba, int n, const long *tams, long long *ini0) {
    unsigned char cab[SETOR], *d;
    long desl = SETOR;
    int i;
    long j;
    memset(cab, 0, sizeof(cab));
    memcpy(cab, "AFS", 4);
    p32(cab + 4, (unsigned)n);
    for (i = 0; i < n; i++) {
        p32(cab + 8 + i * 8, (unsigned)desl);
        p32(cab + 12 + i * 8, (unsigned)tams[i]);
        d = (unsigned char *)malloc((size_t)tams[i]);
        for (j = 0; j < tams[i]; j++) {
            d[j] = byte_em(j);
        }
        grava(o, (long long)lba * SETOR + desl, d, (size_t)tams[i]);
        free(d);
        if (i == 0) {
            *ini0 = (long long)lba * SETOR + desl;
        }
        desl += (tams[i] + SETOR - 1) / SETOR * SETOR;
    }
    grava(o, (long long)lba * SETOR, cab, sizeof(cab));
    return (unsigned)desl;
}
/* a imagem: o PVD, a raiz com DATA, DATA com os três AFS, e a marca da convertida em 0x3DF400 */
static int faz_iso(void) {
    static const long t0[1] = {4096}, t1[1] = {131072}, t2[2] = {262144, 16384};
    unsigned char s[SETOR];
    unsigned tam0, tam1, tam2, lba0 = 1984, lba1 = 1990, lba2 = 2100;
    long long nada;
    int k;
    FILE *o = fopen(sIso, "wb");
    if (o == NULL) {
        return 0;
    }
    tam0 = afs_em(o, lba0, 1, t0, &nada);
    tam1 = afs_em(o, lba1, 1, t1, &sIni1);
    tam2 = afs_em(o, lba2, 2, t2, &sIni2);
    sTam1 = t1[0];
    sTam2 = t2[0];
    memset(s, 0, sizeof(s));
    s[0] = 1;
    memcpy(s + 1, "CD001", 5);
    registro(s, 156, "\0", 18, SETOR, 1);
    grava(o, 16 * SETOR, s, SETOR);
    memset(s, 0, sizeof(s));
    registro(s, 0, "DATA", 19, SETOR, 1);
    grava(o, 18 * SETOR, s, SETOR);
    memset(s, 0, sizeof(s));
    k = registro(s, 0, "PZS3US0.AFS;1", lba0, tam0, 0);
    k = registro(s, k, "PZS3US1.AFS;1", lba1, tam1, 0);
    registro(s, k, "PZS3US2.AFS;1", lba2, tam2, 0);
    grava(o, 19 * SETOR, s, SETOR);
    memset(s, 0, sizeof(s));
    memcpy(s, "BT3X360-ISO", 11);
    grava(o, 0x3DF400, s, 512);
    fclose(o);
    return 1;
}

static double agora_us(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e6 + (double)t.tv_nsec / 1e3;
}
static void caminho_de(char *c, size_t n, int part, int ent) {
    snprintf(c, n, "%s/pzs3us%d/%05d.bin", sRaiz, part, ent);
}
/* o que a thread do jogo faz num ADXT_StartAfs: o Port_FilePath (abre e fecha a entrada da ISO) e o Fluxo_Abrir */
static double abertura(int part, int ent, Fluxo **f) {
    char c[400];
    double t0 = agora_us();
    FILE *a;
    caminho_de(c, sizeof(c), part, ent);
    a = Port_X360Fopen(c, "rb");
    if (a != NULL) {
        Port_X360Fclose(a);
    }
    *f = a != NULL ? Fluxo_Abrir(c, 0) : NULL;
    return agora_us() - t0;
}
/* uma carga do jogo pelo handle comum: os 4 KiB do começo da entrada 1 do PZS3US2.AFS; 0 se os bytes não batem */
static double carga(int *ok) {
    unsigned char b[4096];
    char c[400];
    double t0 = agora_us();
    FILE *a;
    int i;
    caminho_de(c, sizeof(c), 2, 1);
    a = Port_X360Fopen(c, "rb");
    *ok = a != NULL && Port_X360Fread(b, 1, sizeof(b), a) == sizeof(b);
    if (a != NULL) {
        Port_X360Fclose(a);
    }
    for (i = 0; *ok && i < (int)sizeof(b); i++) {
        *ok = b[i] == byte_em(i);
    }
    return agora_us() - t0;
}
static int espera_dentro(int quer) { /* até a leitora lenta estar (1) ou não estar (0) dormindo, até 2 s */
    int k;
    for (k = 0; k < 2000 && DENTRO() != quer; k++) {
        dorme_us(1000);
    }
    return DENTRO() == quer;
}

static volatile int sPararOutra;
static void *outra_leitora(void *arg) { /* como o filme: 64 KiB por vez pelo handle comum, com uma folga entre elas */
    static unsigned char b[65536];
    char c[400];
    FILE *a;
    int n;
    (void)arg;
    caminho_de(c, sizeof(c), 1, 0);
    a = Port_X360Fopen(c, "rb");
    for (n = 0; a != NULL && !__atomic_load_n(&sPararOutra, __ATOMIC_SEQ_CST) && n < 200; n++) {
        Port_X360Fseek(a, (long)(n % 2) * 65536, SEEK_SET);
        Port_X360Fread(b, 1, sizeof(b), a);
        dorme_us(2000);
    }
    if (a != NULL) {
        Port_X360Fclose(a);
    }
    return NULL;
}

static void teste_iso(void) {
    pthread_t outra;
    Fluxo *f, *lento;
    double d, piorAb = 0, piorCarga = 0, piorB = 0;
    int i, ok, juntas = 0, juntasB = 0, cargaOk = 1;
    long tam;

    /* (b) outra thread lendo pelo handle comum (a trava comum pega 50 ms de cada vez); a primeira entrada do
       PZS3US2.AFS aberta aqui: a tabela tem de ter vindo do Iso_Iniciar (o boot), não da thread do jogo */
    gTesteLentaIni = sIni1;
    gTesteLentaFim = sIni1 + sTam1;
    gTesteLentaUs = LENTA_US;
    __atomic_store_n(&sPararOutra, 0, __ATOMIC_SEQ_CST);
    pthread_create(&outra, NULL, outra_leitora, NULL);
    for (i = 0; i < 10; i++) {
        if (!espera_dentro(1)) {
            break;
        }
        d = abertura(2, 1, &f);
        juntasB += DENTRO();
        piorB = d > piorB ? d : piorB;
        CONFERE(f != NULL, "4b: a abertura do ADX falhou");
        if (f != NULL) {
            CONFERE(espera_pronto(f, &tam) == FLUXO_PRONTO && tam == 16384, "4b: o fluxo não chegou (tamanho %ld)", tam);
            Fluxo_Fechar(f);
        }
    }
    __atomic_store_n(&sPararOutra, 1, __ATOMIC_SEQ_CST);
    pthread_join(outra, NULL);
    CONFERE(juntasB >= 8, "4b: só %d de 10 aberturas com a outra thread no meio de uma leitura", juntasB);
    CONFERE(piorB < 1000, "4b: a abertura de um ADX esperou %.0f us com outra thread lendo pelo handle comum", piorB);

    /* (a) a thread de leitura dos fluxos no meio de uma leitura lenta */
    gTesteLentaIni = sIni2;
    gTesteLentaFim = sIni2 + sTam2;
    for (i = 0; i < 10; i++) {
        char c[400];
        espera_dentro(0);
        caminho_de(c, sizeof(c), 2, 0);
        lento = Fluxo_Abrir(c, 0);
        if (lento == NULL || !espera_dentro(1)) {
            CONFERE(0, "4a: a leitura do fluxo lento não começou");
            Fluxo_Fechar(lento);
            break;
        }
        d = abertura(2, 1, &f);
        piorAb = d > piorAb ? d : piorAb;
        d = carga(&ok);
        piorCarga = d > piorCarga ? d : piorCarga;
        cargaOk = cargaOk && ok;
        juntas += DENTRO();
        CONFERE(f != NULL, "4a: a abertura do ADX falhou");
        Fluxo_Fechar(f);
        CONFERE(espera_pronto(lento, &tam) == FLUXO_PRONTO && tam == sTam2, "4a: o fluxo lento não chegou (tamanho %ld)", tam);
        Fluxo_Fechar(lento);
    }
    gTesteLentaUs = 0;
    espera_dentro(0);
    CONFERE(cargaOk, "4a: a carga do jogo leu bytes errados");
    CONFERE(juntas >= 8, "4a: só %d de 10 medidas com a leitura dos fluxos no meio de uma leitura", juntas);
    CONFERE(piorAb < 1000, "4a: a abertura de um ADX esperou %.0f us com a leitura dos fluxos lendo", piorAb);
    CONFERE(piorCarga < 1000, "4a: a carga do jogo esperou %.0f us com a leitura dos fluxos lendo (a trava comum)", piorCarga);
    printf("4: a thread do jogo com a leitura dormindo %ld ms: (b) outra thread no handle comum, pior abertura de ADX %.0f us "
           "(%d de 10 juntas); (a) a leitura dos fluxos, pior abertura %.0f us e pior carga do jogo %.0f us (%d de 10 juntas)\n",
           LENTA_US / 1000, piorB, juntasB, piorAb, piorCarga, juntas);
}

int main(int argc, char **argv) {
    FILE *o;
    long i;
    const char *dir = argc > 1 ? argv[1] : ".";
    snprintf(sArq, sizeof(sArq), "%s/fluxo_teste.bin", dir);
    snprintf(sIso, sizeof(sIso), "%s/fluxo_teste.iso", dir);
    snprintf(sRaiz, sizeof(sRaiz), "%s/dados", dir);
    if (!faz_iso()) {
        printf("FALHA: não cria %s\n", sIso);
        return 1;
    }
    setenv("BT3_ISO", sIso, 1);
    Iso_Iniciar(); /* (como a main do 360: antes de qualquer thread) */
    if (Iso_Recusada()) {
        printf("FALHA: a ISO do teste foi recusada\n");
        return 1;
    }
    o = fopen(sArq, "wb");
    if (o == NULL) {
        printf("FALHA: não cria %s\n", sArq);
        return 1;
    }
    for (i = 0; i < TAM; i++) {
        fputc(byte_em(i), o);
    }
    fclose(o);
    setenv("BT3_FLUXO_BLOCO", "4096", 1);
    teste_sequencia();
    teste_muitos();
    teste_iso();
    teste_bordas();
    remove(sArq);
    remove(sIso);
    printf("teste_fluxo: %s\n", sFalhas ? "FALHOU" : "tudo certo");
    return sFalhas ? 1 : 0;
}
