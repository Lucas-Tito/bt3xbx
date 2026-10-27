/*
 * (F6, etapa 6, docs/x360/plano/F6/06.md, seção 5) O filme desenhado no console, pelo atalho da decisão 6 (b): a imagem
 * decodificada vai direto para uma textura do backend, uma cópia por pixel, sem passar pela memória do GS imitada.
 *
 * Com BT3_FILME=1 no ambiente.txt:
 *   - a leitura: o .PSS do decodificador em blocos de BT3_FILME_BLOCO_KB (256; 0 volta à leitura a cada fread), o
 *     seguinte adiantado numa thread (iso360.c, f2/blocos.c); a leitura do jogo pelo ADXF_ReadNw, que o port ignora,
 *     não vai mais à ISO (plat_file.c): o jogo vê os mesmos setores contados e a mesma posição;
 *   - a imagem: o sceMpegGetPicture (plat_movie.c, thread do jogo) chama Filme360_Imagem, que decodifica os planos
 *     (Mpeg2_NextPlanes, sem a conversão para RGBA da CPU), copia Y, Cb e Cr para uma das quatro vagas do backend (uma
 *     textura L8 de 512 x 672, GsD3d_FilmeVaga em gs_d3d.cpp) e publica o número da imagem em gFilme360Imagem. Os blocos
 *     de 16 x 16 do jogo não são escritos;
 *   - o envio: o MovieTag_Send do jogo segue igual (a cadeia, as esperas do jogo); na gravação, o transfer_data
 *     (gs_core.c) reconhece a imagem do filme (a página 0, 512 de largura, 32 bits), marca o lugar dela na lista
 *     (GsGpu_FbUpload, gs_draw.c, com o número da imagem em `first`) e não escreve a memória do GS;
 *   - o desenho: a thread dona do dispositivo, no lugar do desenho (o mesmo do PC: depois da limpeza, antes do fade),
 *     põe a vaga no alvo pelo ps_filme (gs_d3d.hlsl), que converte para RGB com a mesma conta inteira do Mpeg2_Next.
 * O fim, o START e o fade são do jogo (src/sys/movie.c) e não mudam. Sem BT3_FILME, nada disto roda: o filme é o de
 * antes (decodificado, contado e não desenhado).
 *
 * A medição: por imagem, a espera pela leitura (a thread do jogo parada no Blocos_Ler), a decodificação (o resto do
 * Mpeg2_NextPlanes), a espera pela vaga, a cópia para a textura, o total do sceMpegGetPicture e o período entre duas
 * imagens (o ritmo de verdade, 33,37 ms no filme); no fim do filme, o p50, o p99 e o máximo de cada um no diário, como
 * a medição da primeira parte (mpeg_mede.c), e a conta contra os 33,37 ms. A conversão é da GPU: o tempo dela é a
 * classe "filme" do tempo por passada do gs_d3d.cpp (BT3_TEMPOS).
 */
#include <xtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "diario.h"

#pragma warning(disable : 4996) /* (o getenv é o de posix360.c, que lê o ambiente.txt) */

typedef struct Mpeg2 Mpeg2;
int Mpeg2_NextPlanes(Mpeg2 *m, const unsigned char **y, const unsigned char **cb, const unsigned char **cr, int *stride);
int Mpeg2_Width(const Mpeg2 *m);
int Mpeg2_Height(const Mpeg2 *m);
int GsD3d_FilmeVaga(unsigned imagem, unsigned local, unsigned tipo, unsigned char **dados, unsigned *pitch, double *msEspera); /* gs_d3d.cpp */
void GsD3d_FilmeFim(void);
void GsD3d_FilmeEsperaCaptura(void);
int Iso_FilmeEspera(double *ms, unsigned *esperas); /* iso360.c */
extern unsigned __int64 gPicosIsoTiques;              /* iso360.c: o tempo dentro do ReadFile da ISO, de todas as threads */

#define LARGURA 512 /* a do plat_movie.c (MOVIE_W, MOVIE_H) */
#define ALTURA 448
#define MAX_N 5000  /* o final tem 4.200 imagens */
#define PERIODO_MS 33.367

/* o número da imagem do filme na vaga mais nova (0: nenhuma; o transfer_data e o GsGpu_FbUpload da gravação leem) */
volatile unsigned gFilme360Imagem;

enum { M_LEITURA, M_DEC, M_VAGA, M_COPIA, M_TOTAL, M_PERIODO, M_N };
static const char *const kNome[M_N] = {"leitura (a espera pelo bloco)", "decodificação", "espera da vaga", "cópia para a textura",
                                       "sceMpegGetPicture", "período entre imagens"};
static unsigned long (*sMed)[M_N]; /* em tiques do QueryPerformanceCounter */
static unsigned long *sOrdem;
static unsigned sImagens, sSemVaga;
static unsigned sNumero; /* o número da última imagem entregue: só cresce, de um filme ao outro (o gs_core.c compara) */
static double sTiqueMs;
static LARGE_INTEGER sAnterior;

int Filme360_Ligado(void) {
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("BT3_FILME"); /* ligada por padrão desde a rodada do console de 22/10; BT3_FILME=0 desliga */
        v = e == NULL || atoi(e) == 1;
    }
    return v;
}

/* os KB por bloco da leitura do filme (0: sem blocos, a leitura de antes) */
int Filme360_BlocoKb(void) {
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("BT3_FILME_BLOCO_KB");
        v = !Filme360_Ligado() ? 0 : e == NULL ? 256 : atoi(e);
        v = v < 0 ? 0 : v > 4096 ? 4096 : (v + 1) / 2 * 2; /* (em setores inteiros) */
    }
    return v;
}

static double ms_de(LARGE_INTEGER a, LARGE_INTEGER b) { return (double)(b.QuadPart - a.QuadPart) * sTiqueMs; }

/* A próxima imagem do filme numa vaga do backend. 1: entregue (ou sem vaga: o filme segue sem ela); 0: o fim. */
int Filme360_Imagem(Mpeg2 *m) {
    const unsigned char *py, *pb, *pr;
    unsigned char *dados;
    unsigned pitch, imagem, n, esperas0 = 0, esperas1 = 0;
    int s, y, ok, temEspera;
    double msEspera0 = 0, msEspera1 = 0, msVaga = 0;
    unsigned __int64 iso0;
    LARGE_INTEGER t0, t1, t2, t3;

    if (sMed == NULL) {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        sTiqueMs = 1000.0 / (double)f.QuadPart;
        sMed = (unsigned long (*)[M_N])malloc(sizeof(*sMed) * MAX_N);
        sOrdem = (unsigned long *)malloc(sizeof(unsigned long) * MAX_N);
        sImagens = sSemVaga = 0;
        sAnterior.QuadPart = 0;
        Diario_Escrever("filme: BT3_FILME=1: a imagem direto para a textura (vagas do gs_d3d.cpp, ps_filme); leitura em blocos "
                        "de %d KB (0: a cada fread)", Filme360_BlocoKb());
    }
    QueryPerformanceCounter(&t0);
    temEspera = Iso_FilmeEspera(&msEspera0, &esperas0);
    iso0 = gPicosIsoTiques;
    ok = Mpeg2_NextPlanes(m, &py, &pb, &pr, &s);
    QueryPerformanceCounter(&t1);
    if (temEspera) {
        Iso_FilmeEspera(&msEspera1, &esperas1);
    } else {
        /* (sem os blocos, BT3_FILME_BLOCO_KB=0: as leituras são desta thread, dentro do Mpeg2_NextPlanes, como na
           medição da primeira parte; vale enquanto só ela lê a ISO) */
        msEspera1 = (double)(gPicosIsoTiques - iso0) * sTiqueMs;
    }
    if (!ok || Mpeg2_Width(m) != LARGURA || Mpeg2_Height(m) != ALTURA) {
        return 0;
    }
    imagem = ++sNumero;
    {
        extern char gPortMoviePath[512];
        unsigned tipo = strstr(gPortMoviePath, "zs3used") || strstr(gPortMoviePath, "ZS3USED") ? 2 : 1;
        ok = GsD3d_FilmeVaga(imagem, sImagens, tipo, &dados, &pitch, &msVaga);
    }
    if (!ok) {
        sSemVaga++;
        gFilme360Imagem = 0; /* (a imagem não aparece; o filme segue no ritmo) */
        QueryPerformanceCounter(&t2);
        t3 = t2;
    } else {
        QueryPerformanceCounter(&t2);
        /* Y nas linhas 0 a 447; Cb e Cr lado a lado nas 448 a 671 (o layout que o ps_filme lê) */
        for (y = 0; y < ALTURA; y++) {
            memcpy(dados + y * pitch, py + y * s, LARGURA);
        }
        for (y = 0; y < ALTURA / 2; y++) {
            memcpy(dados + (ALTURA + y) * pitch, pb + y * (s / 2), LARGURA / 2);
            memcpy(dados + (ALTURA + y) * pitch + LARGURA / 2, pr + y * (s / 2), LARGURA / 2);
        }
        __sync(); /* (a escrita combinada sai antes de a GPU poder ler, e antes do número publicado) */
        gFilme360Imagem = imagem;
        QueryPerformanceCounter(&t3);
    }
    n = sImagens++;
    if (n < MAX_N && sMed != NULL) {
        double msLer = msEspera1 - msEspera0;
        unsigned long tLer = (unsigned long)(msLer / sTiqueMs + 0.5), tDec = (unsigned long)(t1.QuadPart - t0.QuadPart);
        sMed[n][M_LEITURA] = tLer;
        sMed[n][M_DEC] = tDec > tLer ? tDec - tLer : 0;
        sMed[n][M_VAGA] = (unsigned long)(msVaga / sTiqueMs + 0.5);
        sMed[n][M_COPIA] = (unsigned long)(t3.QuadPart - t2.QuadPart);
        sMed[n][M_TOTAL] = (unsigned long)(t3.QuadPart - t0.QuadPart);
        sMed[n][M_PERIODO] = sAnterior.QuadPart != 0 ? (unsigned long)(t0.QuadPart - sAnterior.QuadPart) : 0;
    }
    sAnterior = t0;
    (void)esperas1;
    return 1;
}

static int crescente(const void *a, const void *b) {
    unsigned long x = *(const unsigned long *)a, y = *(const unsigned long *)b;
    return x < y ? -1 : x > y;
}

/* p50, p99 (posição mais próxima) e máximo da coluna c, das imagens de `de` a n - 1, em ms */
static void estatistica(int c, unsigned de, unsigned n, double *p50, double *p99) {
    double soma = 0;
    unsigned i, k = 0, imax = de, i50, i99;
    for (i = de; i < n; i++) {
        sOrdem[k++] = sMed[i][c];
        soma += (double)sMed[i][c];
        if (sMed[i][c] > sMed[imax][c]) {
            imax = i;
        }
    }
    if (k == 0) {
        return;
    }
    i50 = (k * 50 + 99) / 100 - 1;
    i99 = (k * 99 + 99) / 100 - 1;
    qsort(sOrdem, k, sizeof(sOrdem[0]), crescente);
    Diario_Escrever("filme:   %s: p50 %.2f ms, p99 %.2f ms, máximo %.2f ms (imagem %u), média %.2f ms", kNome[c],
                    sOrdem[i50] * sTiqueMs, sOrdem[i99] * sTiqueMs, sOrdem[k - 1] * sTiqueMs, imax, soma / k * sTiqueMs);
    if (p50 != NULL) {
        *p50 = sOrdem[i50] * sTiqueMs;
    }
    if (p99 != NULL) {
        *p99 = sOrdem[i99] * sTiqueMs;
    }
}

/* O filme fechou (movie_close do plat_movie.c, também chamado sem filme): as medidas no diário e as vagas soltas. */
void Filme360_Fechou(void) {
    unsigned n = sImagens < MAX_N ? sImagens : MAX_N;
    double p99Total = 0, p50Per = 0, p99Per = 0;
    GsD3d_FilmeEsperaCaptura(); /* só as capturas: a última não se perde com o número zerado no fechamento */
    gFilme360Imagem = 0;
    if (sMed == NULL) {
        return;
    }
    if (n > 0) {
        int c;
        Diario_Escrever("filme: %u imagens de %d x %d pelo caminho direto (%u sem vaga), thread de hardware %u", sImagens,
                        LARGURA, ALTURA, sSemVaga, (unsigned)GetCurrentProcessorNumber());
        for (c = 0; c < M_PERIODO; c++) {
            estatistica(c, 0, n, NULL, c == M_TOTAL ? &p99Total : NULL);
        }
        estatistica(M_PERIODO, 1, n, &p50Per, &p99Per); /* (a primeira imagem não tem período) */
        Diario_Escrever("filme:   conta: o sceMpegGetPicture no p99 %.2f ms de %.2f por imagem (sobra %.2f para o resto da "
                        "thread do jogo); período p50 %.2f, p99 %.2f ms (%s)", p99Total, PERIODO_MS, PERIODO_MS - p99Total,
                        p50Per, p99Per, p99Per <= PERIODO_MS * 1.1 ? "no ritmo" : "FORA DO RITMO no p99");
    }
    free(sMed);
    free(sOrdem);
    sMed = NULL;
    sOrdem = NULL;
    sImagens = 0;
    GsD3d_FilmeFim();
}
