/* Conversão sequencial: autentica a passagem única antes de publicar. */
#include "iso_conv.h"
#include "icones360_arte.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BLOCO (4U * 1024U * 1024U)
#define AMOSTRA (64U * 1024U)
#define MAX_ENTRADAS 4096
#define MAX_ARQUIVO (8U * 1024U * 1024U)
typedef struct Entrada {
    conv_u32 pos, tam;
    char nome[32];
} Entrada;
static conv_u32 le32(const conv_u8 *p) {
    return (conv_u32)p[0] | (conv_u32)p[1] << 8 | (conv_u32)p[2] << 16 | (conv_u32)p[3] << 24;
}
static int cabe(conv_u32 p, conv_u32 n) { return p <= IC_TAMANHO && n <= IC_TAMANHO - p; }
static void progresso(IsoConvIO *io, int fase, conv_u32 p, conv_u32 n) {
    if (io->progresso)
        io->progresso(io->ctx, fase, p, n);
}
/* Busca só um componente, sem recursão; diretórios conhecidos menores que 1
 * MiB. */
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
    raiz = le32(b + 158);
    if (raiz > IC_TAMANHO / 2048)
        return 0;
    raiz *= 2048;
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
/* CRC32 IEEE: conferência rápida da persistência; a identidade usa SHA256. */
static conv_u32 crc(const conv_u8 *b, unsigned n) {
    conv_u32 tab[256], c = 0xffffffffU, x;
    unsigned i, j;
    for (i = 0; i < 256; i++) {
        x = i;
        for (j = 0; j < 8; j++)
            x = (x >> 1) ^ (0xedb88320U & (0U - (x & 1)));
        tab[i] = x;
    }
    for (i = 0; i < n; i++)
        c = tab[(c ^ b[i]) & 255] ^ (c >> 8);
    return c ^ 0xffffffffU;
}
static int converter(const Entrada *e, conv_u8 *buf, conv_u8 *feito) {
    conv_u32 i;
    char msg[240];
    memset(feito, 0, e->tam + 1);
    if (Conv_Arquivo(e->nome, buf, e->tam, feito, msg, sizeof(msg)) != 1)
        return 0;
    for (i = 0; i < e->tam; i++)
        if (feito[i] == CONV_LIVRE)
            return 0;
    return strcmp(e->nome, "pzs3us1/00004.bin") ||
           Conv_Icones360(buf, e->tam, icones360, sizeof(icones360) / sizeof(icones360[0]), msg) >= 0;
}
/* Peças em ordem: entradas inteiras e lacunas de até 4 MiB. O primeiro MiB
   preservado cobre descritores ISO/UDF. As outras lacunas têm início e fim de
   64 KiB relidos. */
typedef struct Peca {
    conv_u32 pos, tam, crc[2];
    int entrada, inteira;
} Peca;
#define MAX_PECAS (MAX_ENTRADAS * 3)
static unsigned planejar(Entrada *e, unsigned n, Peca *v) {
    conv_u32 p = 0;
    unsigned i, k = 0;
    e[n].pos = IC_MARCA_POS;
    e[n].tam = IC_MARCA_TAM;
    e[n].nome[0] = 0;
    qsort(e, n + 1, sizeof(*e), ordem);
    for (i = 0; p < IC_TAMANHO;) {
        Peca *a;
        conv_u32 fim = i <= n ? e[i].pos : IC_TAMANHO;
        if (k == MAX_PECAS)
            return 0;
        a = v + k++;
        a->pos = p;
        a->entrada = -1;
        if (p == fim && i <= n) {
            a->entrada = (int)i;
            a->tam = e[i++].tam;
            a->inteira = 1;
        } else {
            a->tam = fim - p > BLOCO ? BLOCO : fim - p;
            if (!p && a->tam > 1024U * 1024U)
                a->tam = 1024U * 1024U;
            a->inteira = !p || a->tam <= 2 * AMOSTRA;
        }
        if (!a->tam || !cabe(p, a->tam))
            return 0;
        p += a->tam;
    }
    return k;
}
/* Agrupa peças consecutivas para evitar uma chamada FATX por arquivo pequeno.
   Nunca corta uma entrada nem passa de 8 MiB; normalmente envia de 4 a 8 MiB. */
typedef struct Lote {
    conv_u32 pos, tam;
    unsigned primeira, fim;
} Lote;
static unsigned agrupar(Peca *v, unsigned n, Lote *l) {
    unsigned i, k = 0;
    Lote *atual = NULL;
    for (i = 0; i < n; i++) {
        if (!atual || v[i].tam > MAX_ARQUIVO - atual->tam) {
            atual = l + k++;
            atual->pos = v[i].pos;
            atual->tam = 0;
            atual->primeira = i;
        }
        atual->tam += v[i].tam;
        atual->fim = i + 1;
        if (atual->tam >= BLOCO)
            atual = NULL;
    }
    return k;
}
int IsoConv_Executar(IsoConvIO *io, const conv_u8 selo[IC_MARCA_TAM], char *erro, unsigned nmsg) {
    static const conv_u8 esperado[32] = {0x93, 0xf1, 0xf9, 0x11, 0xe9, 0xa2, 0xbb, 0xdf, 0x92, 0xe5, 0x79,
                                         0x4f, 0xc3, 0x07, 0x5d, 0xdf, 0x3f, 0x87, 0xdb, 0x3d, 0xf4, 0xb1,
                                         0xcb, 0x8c, 0xfb, 0xb0, 0x44, 0x87, 0x21, 0xc9, 0x11, 0x72};
    conv_u8 *buf[3] = {NULL, NULL, NULL}, *feito = NULL, hash[32];
    Entrada *e = NULL;
    Peca *v = NULL;
    Lote *l = NULL;
    IsoSha origem;
    unsigned n = 0, i, j, k, quantos, nlotes;
    conv_u32 tam, inicio;
    int r = IC_IO, criado = 0, leitura = 0, escrita = 0;
    const char *msg = "falha de leitura, escrita ou sincronização";
    for (i = 0; i < 3; i++)
        buf[i] = (conv_u8 *)io->alocar(MAX_ARQUIVO + 1);
    feito = (conv_u8 *)malloc(MAX_ARQUIVO + 1);
    e = (Entrada *)malloc(MAX_ENTRADAS * sizeof(*e));
    v = (Peca *)malloc(MAX_PECAS * sizeof(*v));
    l = (Lote *)malloc(MAX_PECAS * sizeof(*l));
    if (!buf[0] || !buf[1] || !buf[2] || !feito || !e || !v || !l)
        goto fim;
    r = IC_ORIGEM;
    msg = "origem ou índices incompatíveis com a ISO USA";
    /* Ainda não autenticada: todos os limites do inventário são conferidos. Os
     metadados ficam em cache no adaptador e entram no SHA da passagem única. */
    if (!io->tamanho(io->ctx, 0, &tam) || tam != IC_TAMANHO || !inventario(io, e, &n))
        goto fim;
    r = IC_FORMATO;
    msg = "selo incompatível";
    if (memcmp(selo, "BT3X360-ISO\0\0\0\0\0", 16) || le32(selo + 16) != 3 || le32(selo + 20) != 512 ||
        le32(selo + 24) != IC_TAMANHO || le32(selo + 28) != 0 || !(quantos = planejar(e, n, v)))
        goto fim;
    nlotes = agrupar(v, quantos, l);
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
    IsoSha_Iniciar(&origem);
    leitura = 1;
    if (!io->antecipar(io->ctx, l[0].pos, buf[0], l[0].tam))
        goto fim;
    for (k = 0; k < nlotes; k++) {
        Lote *atual = l + k;
        conv_u8 *b = buf[k % 3];
        if (!io->esperar_leitura(io->ctx))
            goto fim;
        leitura = 0;
        if (k + 1 < nlotes) {
            leitura = 1;
            if (!io->antecipar(io->ctx, l[k + 1].pos, buf[(k + 1) % 3], l[k + 1].tam))
                goto fim;
        }
        /* Thread 2: identidade da origem antes de qualquer alteração e conversão.
           Thread 1: SHA256 de saída e gravação. Thread 3: próxima leitura. */
        inicio = io->relogio();
        IsoSha_Atualizar(&origem, b, atual->tam);
        io->hash_origem_ms += io->relogio() - inicio;
        for (i = atual->primeira; i < atual->fim; i++) {
            Peca *a = v + i;
            conv_u8 *trecho = b + a->pos - atual->pos;
            inicio = io->relogio();
            if (a->entrada >= 0) {
                Entrada *entrada = e + a->entrada;
                if (entrada->nome[0]) {
                    if (!converter(entrada, trecho, feito)) {
                        r = IC_FORMATO;
                        msg = "conversor recusou a entrada";
                        goto fim;
                    }
                } else {
                    for (j = 0; j < IC_MARCA_TAM; j++)
                        if (trecho[j]) {
                            r = IC_ORIGEM;
                            msg = "região da marca original não está livre";
                            goto fim;
                        }
                    memcpy(trecho, selo, IC_MARCA_TAM);
                }
            }
            io->conversao_ms += io->relogio() - inicio;
            a->crc[0] = crc(trecho, a->inteira ? a->tam : AMOSTRA);
            if (!a->inteira)
                a->crc[1] = crc(trecho + a->tam - AMOSTRA, AMOSTRA);
        }
        if (escrita && !io->esperar_gravacao(io->ctx))
            goto fim;
        escrita = 1;
        if (!io->enviar(io->ctx, atual->pos, b, atual->tam))
            goto fim;
        /* Cópia e conversão são uma passagem, com uma só barra. */
        progresso(io, IC_CONVERTER, atual->pos + atual->tam, IC_TAMANHO);
    }
    if (escrita && !io->esperar_gravacao(io->ctx))
        goto fim;
    escrita = 0;
    IsoSha_Final(&origem, hash);
    r = IC_ORIGEM;
    msg = "identidade SHA256 da original não confere";
    if (memcmp(hash, esperado, 32))
        goto fim;
    io->hash_saida(io->ctx, io->sha_saida);
    r = IC_IO;
    msg = "falha ao sincronizar, fechar ou reabrir";
    if (!io->reabrir(io->ctx))
        goto fim;
    r = IC_CONFERE;
    msg = "saída reaberta difere dos trechos gravados";
    if (!io->tamanho(io->ctx, 1, &tam) || tam != IC_TAMANHO)
        goto fim;
    io->relidos = 0;
    if (!io->conferir_indices(io->ctx, &io->relidos))
        goto fim;
    for (k = 0; k < quantos; k++) {
        Peca *a = v + k;
        unsigned z = a->inteira ? a->tam : AMOSTRA;
        if (!io->ler(io->ctx, 1, a->pos, buf[0], z) || crc(buf[0], z) != a->crc[0])
            goto fim;
        io->relidos += z;
        if (!a->inteira) {
            if (!io->ler(io->ctx, 1, a->pos + a->tam - AMOSTRA, buf[0], AMOSTRA) || crc(buf[0], AMOSTRA) != a->crc[1])
                goto fim;
            io->relidos += AMOSTRA;
        }
        progresso(io, IC_VERIFICAR, k + 1, quantos);
    }
    /* O selo também tem comparação literal, além do CRC da peça inteira. */
    if (!io->ler(io->ctx, 1, IC_MARCA_POS, buf[0], IC_MARCA_TAM) || memcmp(buf[0], selo, IC_MARCA_TAM))
        goto fim;
    io->relidos += IC_MARCA_TAM;
    progresso(io, IC_PUBLICAR, 0, 1);
    r = IC_DESTINO;
    msg = "publicação recusada; destino preservado";
    if (!io->publicar(io->ctx))
        goto fim;
    criado = 0;
    r = IC_OK;
    msg = "tudo certo";
fim:
    /* Nenhum buffer pode ser solto nem arquivo fechado com E/S ainda em curso. */
    if (leitura)
        io->esperar_leitura(io->ctx);
    if (escrita)
        io->esperar_gravacao(io->ctx);
    if (criado)
        io->descartar(io->ctx);
    for (i = 0; i < 3; i++)
        if (buf[i])
            io->liberar(buf[i]);
    free(feito);
    free(e);
    free(v);
    free(l);
    if (nmsg) {
        strncpy(erro, msg, nmsg - 1);
        erro[nmsg - 1] = 0;
    }
    return r;
}
