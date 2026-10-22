/*
 * (F6, etapa 6, docs/x360/plano/F6/06.md) A medição do decodificador dos filmes no Xenon, antes de escolher onde ele
 * roda (decisão 25, item 5) e como a imagem chega à tela (item 6).
 *
 * Com BT3_MPEG_MEDE=1 no ambiente.txt, a thread do jogo (hardware 2), antes de entrar no jogo, abre os dois filmes da
 * ISO (DATA/ZS3USOP.PSS, a abertura, e DATA/ZS3USED.PSS, o final, pelo mesmo caminho que o plat_file.c monta) e
 * decodifica as primeiras imagens de cada um pelo decodificador do port (port/src/plat_mpeg2.c, o mesmo do
 * sceMpegGetPicture), sem desenhar e sem som. Por imagem, três tempos pelo QueryPerformanceCounter:
 *   - total: o Mpeg2_Next inteiro (ler o .PSS, separar o vídeo, decodificar, converter para RGBA);
 *   - leitura: a parte dele dentro do ReadFile da ISO, pelos contadores do iso360.c (gPicosIso*, só esta thread lê
 *     enquanto a medição roda), e quantas leituras e quantos bytes;
 *   - decodificação: total menos leitura;
 * e, à parte, uma cópia por pixel: o mesmo laço do sceMpegGetPicture (plat_movie.c), do RGBA para os blocos de
 * 16 x 16 em colunas, a primeira das quatro cópias da cadeia do filme (escopo da F6, seção 4).
 * Por filme, o p50, o p99 (posição mais próxima) e o máximo de cada um, a média, a resolução e a thread de hardware
 * (GetCurrentProcessorNumber, no começo e no fim). Depois, a mesma medição numa thread criada na thread de hardware
 * do BT3_MPEG_MEDE_HW (3, a irmã da do jogo no mesmo núcleo, o padrão; ou 1; 0 sem segunda passada), com a do jogo
 * esperando. No fim o jogo sai com código 0 (tela verde); nada do jogo roda.
 *
 * Chaves: BT3_MPEG_MEDE=1; BT3_MPEG_MEDE_N, imagens por filme (600; até o fim do filme, se for menor; 1 a 5000);
 * BT3_MPEG_MEDE_HW (3). Sem BT3_MPEG_MEDE, nada muda (MpegMede_Rodar volta 0 sem tocar em nada).
 */
#include <xtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "diario.h"

#pragma warning(disable : 4996) /* (o getenv é o de posix360.c, que lê o ambiente.txt) */

/* port/src/plat_mpeg2.c */
typedef struct Mpeg2 Mpeg2;
Mpeg2 *Mpeg2_Open(const char *path);
int Mpeg2_Next(Mpeg2 *m, unsigned char *rgba);
int Mpeg2_Width(const Mpeg2 *m);
int Mpeg2_Height(const Mpeg2 *m);
void Mpeg2_Close(Mpeg2 *m);

/* iso360.c: as leituras da ISO desde sempre (só lidas aqui, por diferença) */
extern unsigned gPicosIsoN;
extern unsigned __int64 gPicosIsoBytes, gPicosIsoTiques;

#define LARGURA 512 /* a do plat_movie.c (MOVIE_W, MOVIE_H): o sceMpegGetPicture recusa outra */
#define ALTURA 448
#define MAX_N 5000
#define PERIODO_MS 33.367 /* 1001 / 30: uma imagem a cada dois campos de 59,94 Hz */

static const char *const kFilmes[2] = {"ZS3USOP.PSS", "ZS3USED.PSS"};

typedef struct Medida {
    unsigned long total[MAX_N], leitura[MAX_N], dec[MAX_N], copia[MAX_N], ordem[MAX_N];
} Medida;

static Medida *sMed;
static unsigned char *sRgba, *sBlocos;
static int sN;
static double sTiqueMs;

static int numero(const char *nome, int padrao, int min, int max) {
    const char *v = getenv(nome);
    int n;
    if (v == NULL || *v == '\0') {
        return padrao;
    }
    n = atoi(v);
    if (n < min || n > max) {
        Diario_Escrever("mpeg: %s=%s recusado (de %d a %d): fica %d", nome, v, min, max, padrao);
        return padrao;
    }
    return n;
}

/* O laço do sceMpegGetPicture (plat_movie.c): 16 x 16 blocos de RGBA de 0x400 bytes, colunas primeiro, alfa 0x80. */
static void para_blocos(const unsigned char *rgba, unsigned char *out) {
    int cols = LARGURA / 16, rows = ALTURA / 16, i, j, y, x;
    for (i = 0; i < cols; i++) {
        for (j = 0; j < rows; j++) {
            unsigned char *b = out + (size_t)(i * rows + j) * 0x400;
            for (y = 0; y < 16; y++) {
                const unsigned char *s = &rgba[((j * 16 + y) * LARGURA + i * 16) * 4];
                for (x = 0; x < 16; x++) {
                    b[(y * 16 + x) * 4 + 0] = s[x * 4 + 0];
                    b[(y * 16 + x) * 4 + 1] = s[x * 4 + 1];
                    b[(y * 16 + x) * 4 + 2] = s[x * 4 + 2];
                    b[(y * 16 + x) * 4 + 3] = 0x80;
                }
            }
        }
    }
}

static int crescente(const void *a, const void *b) {
    unsigned long x = *(const unsigned long *)a, y = *(const unsigned long *)b;
    return x < y ? -1 : x > y;
}

/* p50, p99 (posição mais próxima: o menor valor com pelo menos q% das imagens até ele), máximo e média, em ms */
static void estatistica(const char *rotulo, const unsigned long *v, int n, double *p99Ms) {
    unsigned long *s = sMed->ordem;
    double soma = 0;
    int i, imax = 0, i50 = (n * 50 + 99) / 100 - 1, i99 = (n * 99 + 99) / 100 - 1;
    for (i = 0; i < n; i++) {
        s[i] = v[i];
        soma += (double)v[i];
        if (v[i] > v[imax]) {
            imax = i;
        }
    }
    qsort(s, (size_t)n, sizeof(s[0]), crescente);
    Diario_Escrever("mpeg:   %s: p50 %.2f ms, p99 %.2f ms, máximo %.2f ms (imagem %d), média %.2f ms", rotulo,
                    s[i50] * sTiqueMs, s[i99] * sTiqueMs, s[n - 1] * sTiqueMs, imax, soma / n * sTiqueMs);
    if (p99Ms != NULL) {
        *p99Ms = s[i99] * sTiqueMs;
    }
}

static void medir_filme(int filme, int passada) {
    char caminho[512];
    const char *raiz = getenv("BT3_DATA");
    LARGE_INTEGER t0, t1, t2;
    unsigned n0, nLeituras;
    unsigned __int64 b0, bytes;
    Mpeg2 *m;
    int n = 0, hw0, hw1;
    double totalP99, decP99, copiaP99, segundos;

    _snprintf(caminho, sizeof(caminho) - 1, "%s/disc/DATA/%s", raiz != NULL ? raiz : "game:\\gamedata", kFilmes[filme]);
    caminho[sizeof(caminho) - 1] = '\0';
    hw0 = (int)GetCurrentProcessorNumber();
    QueryPerformanceCounter(&t0);
    m = Mpeg2_Open(caminho);
    QueryPerformanceCounter(&t1);
    if (m == NULL) {
        Diario_Escrever("mpeg: %s não abriu (%s)", kFilmes[filme], caminho);
        return;
    }
    n0 = gPicosIsoN;
    b0 = gPicosIsoBytes;
    QueryPerformanceCounter(&t2);
    while (n < sN) {
        LARGE_INTEGER a, b, c;
        unsigned __int64 q = gPicosIsoTiques;
        int ok;
        QueryPerformanceCounter(&a);
        ok = Mpeg2_Next(m, sRgba);
        QueryPerformanceCounter(&b);
        if (!ok) {
            break;
        }
        if (Mpeg2_Width(m) != LARGURA || Mpeg2_Height(m) != ALTURA) {
            Diario_Escrever("mpeg: %s: imagem %d de %d x %d, não %d x %d: parou", kFilmes[filme], n, Mpeg2_Width(m),
                            Mpeg2_Height(m), LARGURA, ALTURA);
            break;
        }
        sMed->total[n] = (unsigned long)(b.QuadPart - a.QuadPart);
        sMed->leitura[n] = (unsigned long)(gPicosIsoTiques - q);
        sMed->dec[n] = sMed->total[n] - sMed->leitura[n];
        para_blocos(sRgba, sBlocos);
        QueryPerformanceCounter(&c);
        sMed->copia[n] = (unsigned long)(c.QuadPart - b.QuadPart);
        n++;
    }
    {
        LARGE_INTEGER fim;
        QueryPerformanceCounter(&fim);
        segundos = (double)(fim.QuadPart - t2.QuadPart) * sTiqueMs / 1000.0;
    }
    nLeituras = gPicosIsoN - n0;
    bytes = gPicosIsoBytes - b0;
    hw1 = (int)GetCurrentProcessorNumber();
    Mpeg2_Close(m);
    if (n == 0) {
        Diario_Escrever("mpeg: %s: nenhuma imagem", kFilmes[filme]);
        return;
    }
    Diario_Escrever("mpeg: passada %d, %s: %d imagens de %d x %d na thread de hardware %d (%d no fim), abertura %.2f ms, "
                    "%.1f s ao todo", passada, kFilmes[filme], n, LARGURA, ALTURA, hw0, hw1,
                    (double)(t1.QuadPart - t0.QuadPart) * sTiqueMs, segundos);
    estatistica("total (Mpeg2_Next)", sMed->total, n, &totalP99);
    estatistica("decodificação (sem a leitura)", sMed->dec, n, &decP99);
    estatistica("leitura da ISO", sMed->leitura, n, NULL);
    Diario_Escrever("mpeg:   leitura da ISO: %u leituras (%.1f por imagem), %.0f bytes (%.1f KB por imagem)", nLeituras,
                    (double)nLeituras / n, (double)(__int64)bytes, (double)(__int64)bytes / n / 1024.0);
    estatistica("cópia para os blocos (uma cópia por pixel)", sMed->copia, n, &copiaP99);
    Diario_Escrever("mpeg:   conta: período de %.2f ms (29,97 imagens/s); sobra no p99 do total %.2f ms, e com uma cópia "
                    "%.2f ms (as outras três cópias, a textura e o desenho cabem aí); o decodificador sozinho dá %.1f "
                    "imagens/s no p99", PERIODO_MS, PERIODO_MS - totalP99, PERIODO_MS - totalP99 - copiaP99,
                    decP99 > 0 ? 1000.0 / decP99 : 0.0);
}

static void medir(int passada) {
    int f;
    for (f = 0; f < 2; f++) {
        medir_filme(f, passada);
        Diario_Descarregar(); /* cada filme no disco já: se a passada seguinte cair, este fica */
    }
}

static DWORD WINAPI Passada2(LPVOID arg) {
    medir((int)(INT_PTR)arg);
    return 0;
}

/* Na thread do jogo, antes do jogo. 0 sem BT3_MPEG_MEDE (nada mudou); 1 com a medição feita (a main360.cpp sai). */
int MpegMede_Rodar(void) {
    const char *liga = getenv("BT3_MPEG_MEDE");
    LARGE_INTEGER f;
    int hw;
    if (liga == NULL || strcmp(liga, "1") != 0) {
        return 0;
    }
    sN = numero("BT3_MPEG_MEDE_N", 600, 1, MAX_N);
    hw = numero("BT3_MPEG_MEDE_HW", 3, 0, 3);
    if (hw == 2) {
        Diario_Escrever("mpeg: BT3_MPEG_MEDE_HW=2 é a própria thread do jogo: sem segunda passada");
        hw = 0;
    }
    QueryPerformanceFrequency(&f);
    sTiqueMs = 1000.0 / (double)f.QuadPart;
    sMed = (Medida *)malloc(sizeof(Medida));
    sRgba = (unsigned char *)malloc(LARGURA * ALTURA * 4);
    sBlocos = (unsigned char *)malloc(LARGURA * ALTURA * 4);
    if (sMed == NULL || sRgba == NULL || sBlocos == NULL) {
        Diario_Escrever("mpeg: sem memória para a medição");
        return 1;
    }
    Diario_Escrever("mpeg: medição do decodificador (BT3_MPEG_MEDE): %d imagens de cada filme, relógio de %.0f Hz; "
                    "passada 1 na thread do jogo, passada 2 na thread de hardware %d%s", sN, (double)f.QuadPart, hw,
                    hw == 0 ? " (sem)" : "");
    medir(1);
    if (hw != 0) {
        HANDLE t = CreateThread(NULL, 256 * 1024, Passada2, (LPVOID)(INT_PTR)2, CREATE_SUSPENDED, NULL);
        if (t == NULL) {
            Diario_Escrever("mpeg: a thread da passada 2 não nasceu");
        } else {
            XSetThreadProcessor(t, (DWORD)hw);
            ResumeThread(t);
            WaitForSingleObject(t, INFINITE);
            CloseHandle(t);
        }
    }
    Diario_Escrever("mpeg: medição feita; o jogo sai com código 0");
    free(sBlocos);
    free(sRgba);
    free(sMed);
    return 1;
}
