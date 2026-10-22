/*
 * (F6, etapa 6, docs/x360/plano/F6/06.md, seção 5) O teste da leitura do filme em blocos (port/x360/src/f2/blocos.c)
 * contra a leitura direta, pelo decodificador do port (port/src/plat_mpeg2.c) sobre os .PSS de verdade.
 *
 *   teste_blocos <arquivo.PSS> <saída> <modo> [bloco_kb] [opções]
 *     modo arquivo   o decodificador lê pelo stdio, como no PC (a leitura de antes no 360 era a mesma sequência de
 *                    leituras na ISO, uma por fread)
 *     modo blocos    o decodificador lê pelo Blocos_Ler, com a thread de leitura adiantando o bloco seguinte
 *     modo sincrono  pelo Blocos_Ler sem thread (cada bloco lido na hora em que falta)
 *   opções: --planos   as imagens pelo Mpeg2_NextPlanes (o caminho do 360) em vez do Mpeg2_Next (RGBA, o do PC), com
 *                      a conversão do ps_filme (gs_d3d.hlsl) refeita em float de 32 bits, comparada pixel a pixel com
 *                      a conta inteira do Mpeg2_Next;
 *           --atraso   a leitura de cada pedaço dorme de 0 a 3 ms (sacode a ordem entre as threads);
 *           --estraga  um byte do arquivo, na posição 1.000.003, chega trocado ao decodificador (controle: o
 *                      verificador tem de acusar).
 * A saída tem uma linha por chamada de leitura do decodificador ("L op posição pedido entregue hash") e uma por imagem
 * ("I número hash"); duas saídas iguais são os mesmos bytes na mesma ordem e as mesmas imagens. Com --planos, a linha
 * "C imagens pixels_diferentes" fecha o arquivo.
 */
#define _POSIX_C_SOURCE 200112L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "blocos.h"

typedef struct TArq {
    FILE *fp;    /* modo arquivo */
    Blocos *b;   /* modos blocos e sincrono */
    long pos, tam;
} TArq;

static int sModo; /* 0 arquivo, 1 blocos, 2 sincrono */
static long sBloco = 256 * 1024;
static int sAtraso, sEstraga;
static FILE *sOut;
static const char *sNome;

static uint64_t fnv(const void *p, size_t n) {
    const unsigned char *c = p;
    uint64_t h = 1469598103934665603ULL;
    size_t i;
    for (i = 0; i < n; i++) {
        h = (h ^ c[i]) * 1099511628211ULL;
    }
    return h;
}

/* a leitura de um pedaço pelos blocos: pread no arquivo, com o atraso e o estrago pedidos */
static int ler_pedaco(void *ctx, long pos, void *buf, long n) {
    FILE *fp = ctx;
    if (sAtraso) {
        struct timespec t = {0, (rand() % 4) * 1000000L};
        nanosleep(&t, NULL);
    }
    if (fseek(fp, pos, SEEK_SET) != 0 || fread(buf, 1, (size_t)n, fp) != (size_t)n) {
        return 0;
    }
    if (sEstraga && pos <= 1000003L && pos + n > 1000003L) {
        ((unsigned char *)buf)[1000003L - pos] ^= 0x10;
    }
    return 1;
}

static FILE *t_fopen(const char *path, const char *modo) {
    TArq *a = calloc(1, sizeof(TArq));
    FILE *fp = fopen(path, modo);
    (void)modo;
    if (a == NULL || fp == NULL) {
        free(a);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    a->tam = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sModo == 0) {
        a->fp = fp;
    } else {
        a->b = Blocos_Abrir(ler_pedaco, fp, a->tam, sBloco, sModo == 1 ? 1 : -1);
        a->fp = fp; /* (o ctx do ler_pedaco; fechado no t_fclose) */
    }
    return (FILE *)a;
}

static size_t ler(TArq *a, void *buf, size_t n) {
    size_t r;
    if (a->b == NULL) {
        r = fread(buf, 1, n, a->fp);
        if (sEstraga && a->pos <= 1000003L && a->pos + (long)r > 1000003L) {
            ((unsigned char *)buf)[1000003L - a->pos] ^= 0x10;
        }
    } else {
        long g = Blocos_Ler(a->b, a->pos, buf, (long)n);
        r = g < 0 ? 0 : (size_t)g;
    }
    a->pos += (long)r;
    return r;
}

static size_t t_fread(void *buf, size_t tam, size_t n, FILE *f) {
    TArq *a = (TArq *)f;
    long antes = a->pos;
    size_t r = tam == 0 || n == 0 ? 0 : ler(a, buf, tam * n);
    fprintf(sOut, "L r %ld %zu %zu %016llx\n", antes, tam * n, r, (unsigned long long)fnv(buf, r));
    return tam == 0 ? 0 : r / tam;
}

static int t_fgetc(FILE *f) {
    TArq *a = (TArq *)f;
    unsigned char c;
    long antes = a->pos;
    size_t r = ler(a, &c, 1);
    fprintf(sOut, "L c %ld 1 %zu %d\n", antes, r, r ? c : -1);
    return r ? c : EOF;
}

static int t_fseek(FILE *f, long d, int origem) {
    TArq *a = (TArq *)f;
    long p = origem == SEEK_SET ? d : origem == SEEK_CUR ? a->pos + d : a->tam + d;
    fprintf(sOut, "L s %ld %ld %d\n", a->pos, d, origem);
    if (p < 0) {
        return -1;
    }
    a->pos = p;
    if (a->b == NULL) {
        return fseek(a->fp, p, SEEK_SET);
    }
    return 0;
}

static BlocosConta sConta;
static int t_fclose(FILE *f) {
    TArq *a = (TArq *)f;
    if (a->b != NULL) {
        Blocos_Conta(a->b, &sConta);
        Blocos_Fechar(a->b);
    }
    fclose(a->fp);
    free(a);
    return 0;
}

/* o decodificador, com o stdio dele trocado pelo de cima */
#define fopen t_fopen
#define fread t_fread
#define fgetc t_fgetc
#define fseek t_fseek
#define fclose t_fclose
#include "plat_mpeg2.c"
#undef fopen
#undef fread
#undef fgetc
#undef fseek
#undef fclose

/* a conta do ps_filme (gs_d3d.hlsl) em float de 32 bits: as partes inteiras abaixo de 2^24, a divisão por 65536 */
static uint8_t canal(float v) { return (uint8_t)(v < 0.0f ? 0.0f : v > 255.0f ? 255.0f : v); }
static void shader(int Y, int Cb, int Cr, uint8_t o[3]) {
    volatile float ty = (float)Y / 255.0f, tb = (float)Cb / 255.0f, tr = (float)Cr / 255.0f; /* o que a textura L8 dá */
    float y = floorf(ty * 255.0f + 0.5f) - 16.0f, d = floorf(tb * 255.0f + 0.5f) - 128.0f, e = floorf(tr * 255.0f + 0.5f) - 128.0f;
    float r = y + e + floorf((10773.0f * y + 39061.0f * e + 32768.0f) / 65536.0f);
    float g = y + floorf((10773.0f * y - 25675.0f * d - 53279.0f * e + 32768.0f) / 65536.0f);
    float b = y + 2.0f * d + floorf((10773.0f * y + 1129.0f * d + 32768.0f) / 65536.0f);
    o[0] = canal(r);
    o[1] = canal(g);
    o[2] = canal(b);
}

int main(int argc, char **argv) {
    Mpeg2 *m;
    uint8_t *rgba;
    int planos = 0, n = 0, i, w, hh;
    unsigned long long difs = 0;

    if (argc < 4) {
        fprintf(stderr, "uso: teste_blocos arquivo.PSS saida arquivo|blocos|sincrono [bloco_kb] [--planos] [--atraso] [--estraga]\n");
        return 2;
    }
    sNome = argv[1];
    sModo = strcmp(argv[3], "blocos") == 0 ? 1 : strcmp(argv[3], "sincrono") == 0 ? 2 : 0;
    for (i = 4; i < argc; i++) {
        if (strcmp(argv[i], "--planos") == 0) planos = 1;
        else if (strcmp(argv[i], "--atraso") == 0) sAtraso = 1;
        else if (strcmp(argv[i], "--estraga") == 0) sEstraga = 1;
        else sBloco = atol(argv[i]) * 1024;
    }
    sOut = fopen(argv[2], "w");
    m = sOut != NULL ? Mpeg2_Open(sNome) : NULL;
    if (m == NULL) {
        fprintf(stderr, "não abriu %s\n", sNome);
        return 2;
    }
    rgba = malloc(512 * 448 * 4);
    for (;;) {
        uint64_t h;
        if (planos) {
            const uint8_t *py, *pb, *pr;
            int s, x, y;
            if (!Mpeg2_NextPlanes(m, &py, &pb, &pr, &s)) {
                break;
            }
            h = fnv(py, (size_t)s * (size_t)m->height);
            h ^= fnv(pb, (size_t)(s / 2) * (size_t)(m->height / 2)) * 31;
            h ^= fnv(pr, (size_t)(s / 2) * (size_t)(m->height / 2)) * 131;
            /* a conta inteira do Mpeg2_Next e a do shader, pixel a pixel */
            for (y = 0; y < m->height; y++) {
                for (x = 0; x < m->width; x++) {
                    int Y = py[y * s + x], Cb = pb[(y >> 1) * (s >> 1) + (x >> 1)], Cr = pr[(y >> 1) * (s >> 1) + (x >> 1)];
                    int c = 76309 * (Y - 16) + 32768, d = Cb - 128, e = Cr - 128;
                    int r = (c + 104597 * e) >> 16, g = (c - 25675 * d - 53279 * e) >> 16, b = (c + 132201 * d) >> 16;
                    uint8_t ref[3], sh[3];
                    ref[0] = (uint8_t)(r < 0 ? 0 : r > 255 ? 255 : r);
                    ref[1] = (uint8_t)(g < 0 ? 0 : g > 255 ? 255 : g);
                    ref[2] = (uint8_t)(b < 0 ? 0 : b > 255 ? 255 : b);
                    shader(Y, Cb, Cr, sh);
                    difs += memcmp(ref, sh, 3) != 0;
                }
            }
        } else {
            if (!Mpeg2_Next(m, rgba)) {
                break;
            }
            h = fnv(rgba, (size_t)m->width * (size_t)m->height * 4);
        }
        fprintf(sOut, "I %d %016llx\n", n, (unsigned long long)h);
        n++;
    }
    w = m->width;
    hh = m->height;
    Mpeg2_Close(m);
    if (planos) {
        fprintf(sOut, "C %d %llu\n", n, difs);
    }
    fclose(sOut);
    printf("%s %s%s: %d imagens de %d x %d; blocos %u (faltas %u), esperas %u (%.1f ms, pior %.2f), leitura %.1f ms (pior bloco %.2f)%s\n",
           argv[3], strrchr(sNome, '/') ? strrchr(sNome, '/') + 1 : sNome, planos ? " (planos)" : "", n, w, hh, sConta.blocos, sConta.faltas, sConta.esperas,
           sConta.msEspera, sConta.msEsperaPior, sConta.msLeitura, sConta.msLeituraPior, "");
    if (planos) {
        printf("   conversão do ps_filme em float: %llu pixels diferentes da conta inteira em %d imagens\n", difs, n);
    }
    return 0;
}
