/* ISO USA autenticada antes de interpretar índices ou criar a saída.
   A autenticação permite usar somente o subconjunto ISO/AFS conhecido desta imagem.
   Reproduz convert_iso.py: cópia integral, entradas com conversor, Kenney e selo. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "iso_conv.h"
#include "icones360_arte.h"
#define BLOCO (1024U * 1024U)
#define MAX_ENTRADAS 4096
#define MAX_ARQUIVO (32U * 1024U * 1024U)
typedef struct Entrada {
    conv_u32 pos, tam;
    char nome[32];
} Entrada;
static conv_u32 le32(const conv_u8 *p) {
    return (conv_u32)p[0] | (conv_u32)p[1] << 8 | (conv_u32)p[2] << 16 | (conv_u32)p[3] << 24;
}
static int cabe(conv_u32 p, conv_u32 n) {
    return p <= IC_TAMANHO && n <= IC_TAMANHO - p;
}
static void progresso(IsoConvIO *io, int fase, conv_u32 p, conv_u32 n) {
    if (io->progresso)
        io->progresso(io->ctx, fase, p, n);
}
static int autenticar(IsoConvIO *io, conv_u8 *buf, int fase) {
    static const conv_u8 esperado[32] = {0x93, 0xf1, 0xf9, 0x11, 0xe9, 0xa2, 0xbb, 0xdf, 0x92, 0xe5, 0x79,
                                         0x4f, 0xc3, 0x07, 0x5d, 0xdf, 0x3f, 0x87, 0xdb, 0x3d, 0xf4, 0xb1,
                                         0xcb, 0x8c, 0xfb, 0xb0, 0x44, 0x87, 0x21, 0xc9, 0x11, 0x72};
    IsoSha s;
    conv_u8 hash[32];
    conv_u32 p = 0, tam;
    if (!io->tamanho(io->ctx, 0, &tam) || tam != IC_TAMANHO)
        return 0;
    IsoSha_Iniciar(&s);
    while (p < IC_TAMANHO) {
        unsigned n = IC_TAMANHO - p > BLOCO ? BLOCO : IC_TAMANHO - p;
        if (!io->ler(io->ctx, 0, p, buf, n))
            return 0;
        IsoSha_Atualizar(&s, buf, n);
        p += n;
        progresso(io, fase, p, IC_TAMANHO);
    }
    IsoSha_Final(&s, hash);
    return memcmp(hash, esperado, 32) == 0;
}
/* Busca só um componente, sem recursão; diretórios conhecidos menores que 1 MiB. */
static int achar(IsoConvIO *io, conv_u32 pos, conv_u32 tam, const char *nome, conv_u32 *a, conv_u32 *t) {
    conv_u8 setor[2048];
    conv_u32 p = 0;
    unsigned k, z;
    size_t nn = strlen(nome);
    if (tam > BLOCO || !cabe(pos, tam))
        return 0;
    while (p < tam) {
        unsigned n = tam - p > 2048 ? 2048 : tam - p;
        if (!io->ler(io->ctx, 0, pos + p, setor, n))
            return 0;
        for (k = 0; k < n && setor[k]; k += z) {
            const conv_u8 *r = setor + k;
            z = r[0];
            if (z < 34 || z > n - k || r[32] > z - 33)
                return 0;
            if (r[32] == nn && memcmp(r + 33, nome, nn) == 0) {
                conv_u32 lba = le32(r + 2);
                *t = le32(r + 10);
                if (lba > IC_TAMANHO / 2048)
                    return 0;
                *a = lba * 2048;
                return cabe(*a, *t);
            }
        }
        p += n;
    }
    return 0;
}
static int ordem(const void *a, const void *b) {
    conv_u32 x = ((const Entrada *)a)->pos, y = ((const Entrada *)b)->pos;
    return x < y ? -1 : x > y;
}
static int inventario(IsoConvIO *io, Entrada *e, unsigned *cont) {
    conv_u8 b[2048], *tab;
    conv_u32 raiz, nt, dir, nd, ini, tam, n, i, a, t;
    int p;
    static const conv_u32 quantos[3] = {1, 3399, 65201};
    *cont = 0;
    if (!io->ler(io->ctx, 0, 16 * 2048, b, 2048) || memcmp(b, "\1CD001\1", 7))
        return 0;
    raiz = le32(b + 158) * 2048;
    nt = le32(b + 166);
    if (!achar(io, raiz, nt, "DATA", &dir, &nd))
        return 0;
    for (p = 0; p < 3; p++) {
        char nome[24];
        sprintf(nome, "PZS3US%d.AFS;1", p);
        if (!achar(io, dir, nd, nome, &ini, &tam) || !io->ler(io->ctx, 0, ini, b, 8) || memcmp(b, "AFS\0", 4))
            return 0;
        n = le32(b + 4);
        if (n != quantos[p] || 8 + n * 8 > tam)
            return 0;
        tab = (conv_u8 *)malloc(n * 8);
        if (!tab)
            return 0;
        if (!io->ler(io->ctx, 0, ini + 8, tab, n * 8)) {
            free(tab);
            return 0;
        }
        for (i = 0; i < n; i++) {
            sprintf(nome, "pzs3us%d/%05u.bin", p, (unsigned)i);
            if (Conv_FormatoDe(nome) == NULL)
                continue;
            a = le32(tab + i * 8);
            t = le32(tab + i * 8 + 4);
            if (a > tam || t > tam - a || t > MAX_ARQUIVO || !cabe(ini + a, t) || *cont == MAX_ENTRADAS) {
                free(tab);
                return 0;
            }
            e[*cont].pos = ini + a;
            e[*cont].tam = t;
            strcpy(e[*cont].nome, nome);
            (*cont)++;
        }
        free(tab);
    }
    qsort(e, *cont, sizeof(*e), ordem);
    for (i = 0; i < *cont; i++)
        if ((i && e[i].pos < e[i - 1].pos + e[i - 1].tam) ||
            (e[i].pos < IC_MARCA_POS + IC_MARCA_TAM && e[i].pos + e[i].tam > IC_MARCA_POS))
            return 0;
    return *cont == 3327;
}
static int converter(IsoConvIO *io, const Entrada *e, int verificar) {
    conv_u8 *buf, *feito, *out = NULL;
    conv_u32 i;
    char msg[240];
    int ok = 0;
    buf = (conv_u8 *)malloc(e->tam + 1);
    feito = (conv_u8 *)calloc(e->tam + 1, 1);
    if (verificar)
        out = (conv_u8 *)malloc(e->tam + 1);
    if (!buf || !feito || (verificar && !out))
        goto fim;
    if (!io->ler(io->ctx, 0, e->pos, buf, e->tam) ||
        Conv_Arquivo(e->nome, buf, e->tam, feito, msg, sizeof(msg)) != 1)
        goto fim;
    for (i = 0; i < e->tam; i++)
        if (feito[i] == CONV_LIVRE)
            goto fim;
    if (!strcmp(e->nome, "pzs3us1/00004.bin") &&
        Conv_Icones360(buf, e->tam, icones360, sizeof(icones360) / sizeof(icones360[0]), msg) < 0)
        goto fim;
    if (verificar)
        ok = io->ler(io->ctx, 1, e->pos, out, e->tam) && memcmp(buf, out, e->tam) == 0;
    else
        ok = io->gravar(io->ctx, e->pos, buf, e->tam);
fim:
    free(buf);
    free(feito);
    free(out);
    return ok;
}
static int comparar(IsoConvIO *io, conv_u8 *buf, conv_u8 *out, conv_u32 p, conv_u32 fim) {
    while (p < fim) {
        unsigned n = fim - p > BLOCO ? BLOCO : fim - p;
        if (!io->ler(io->ctx, 0, p, buf, n) || !io->ler(io->ctx, 1, p, out, n) || memcmp(buf, out, n))
            return 0;
        p += n;
        progresso(io, IC_VERIFICAR, p, IC_TAMANHO);
    }
    return 1;
}
int IsoConv_Executar(IsoConvIO *io, const conv_u8 selo[IC_MARCA_TAM], char *erro, unsigned nmsg) {
    conv_u8 *buf = NULL, *out = NULL;
    Entrada *e = NULL;
    unsigned n = 0, i;
    conv_u32 p = 0, tam;
    int r = IC_IO, criado = 0;
    const char *msg = "falha de leitura, escrita ou sincronização";
    buf = (conv_u8 *)malloc(BLOCO);
    out = (conv_u8 *)malloc(BLOCO);
    e = (Entrada *)malloc(MAX_ENTRADAS * sizeof(*e));
    if (!buf || !out || !e)
        goto fim;
    r = IC_ORIGEM;
    msg = "origem não é a ISO USA SLUS-21678 intacta";
    if (!autenticar(io, buf, IC_IDENTIFICAR))
        goto fim;
    r = IC_FORMATO;
    msg = "índices ou selo incompatíveis";
    if (memcmp(selo, "BT3X360-ISO\0\0\0\0\0", 16) || le32(selo + 16) != 3 || le32(selo + 20) != 512 ||
        le32(selo + 24) != IC_TAMANHO || le32(selo + 28) != 0 || !inventario(io, e, &n))
        goto fim;
    r = IC_ESPACO;
    msg = "menos de 3 GiB livres";
    if (!io->espaco(io->ctx))
        goto fim;
    r = IC_DESTINO;
    msg = "saída existente ou temporário indisponível";
    if (!io->iniciar(io->ctx))
        goto fim;
    criado = 1;
    r = IC_IO;
    msg = "falha durante a gravação";
    while (p < IC_TAMANHO) {
        unsigned z = IC_TAMANHO - p > BLOCO ? BLOCO : IC_TAMANHO - p;
        if (!io->ler(io->ctx, 0, p, buf, z) || !io->gravar(io->ctx, p, buf, z))
            goto fim;
        p += z;
        progresso(io, IC_COPIAR, p, IC_TAMANHO);
    }
    for (i = 0; i < n; i++) {
        if (!converter(io, e + i, 0))
            goto fim;
        progresso(io, IC_CONVERTER, i + 1, n);
    }
    if (!io->gravar(io->ctx, IC_MARCA_POS, selo, IC_MARCA_TAM) || !io->reabrir(io->ctx))
        goto fim;
    r = IC_CONFERE;
    msg = "saída reaberta difere da conversão esperada";
    if (!io->tamanho(io->ctx, 1, &tam) || tam != IC_TAMANHO ||
        !io->ler(io->ctx, 1, IC_MARCA_POS, out, IC_MARCA_TAM) || memcmp(out, selo, IC_MARCA_TAM))
        goto fim;
    /* Acrescenta o selo à lista de intervalos alterados e compara todo o resto, inclusive UDF. */
    e[n].pos = IC_MARCA_POS;
    e[n].tam = IC_MARCA_TAM;
    e[n].nome[0] = 0;
    n++;
    qsort(e, n, sizeof(*e), ordem);
    p = 0;
    for (i = 0; i < n; i++) {
        if (!comparar(io, buf, out, p, e[i].pos))
            goto fim;
        if (e[i].nome[0] && !converter(io, e + i, 1))
            goto fim;
        p = e[i].pos + e[i].tam;
    }
    if (!comparar(io, buf, out, p, IC_TAMANHO))
        goto fim;
    r = IC_ORIGEM;
    msg = "a original mudou durante a conversão";
    if (!autenticar(io, buf, IC_VERIFICAR))
        goto fim;
    progresso(io, IC_PUBLICAR, 0, 1);
    r = IC_DESTINO;
    msg = "publicação recusada; destino preservado";
    if (!io->publicar(io->ctx))
        goto fim;
    criado = 0;
    r = IC_OK;
    msg = "tudo certo";
fim:
    if (criado)
        io->descartar(io->ctx);
    free(buf);
    free(out);
    free(e);
    if (nmsg) {
        strncpy(erro, msg, nmsg - 1);
        erro[nmsg - 1] = 0;
    }
    return r;
}
