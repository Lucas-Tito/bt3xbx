/*
 * F1, build PowerPC: inverte a ordem dos bytes dos dados do disco conforme o mapa do rastreio
 * (port/x360/tools/rastreio.py mapa), na hora em que o jogo lê o arquivo e depois de cada descompressão.
 *
 * BT3_MAPA=<arquivo>: linhas "arquivo  deslocamento  largura  repetições". Arquivo virtual de
 * descompressão: "<arquivo>@0x<deslocamento>#bpe", o mesmo nome que o rastreio dá.
 *
 * Para a F1 é o jeito mais curto de saber se o mapa basta; no 360 a mesma informação vira conversão de
 * dados (no instalador ou na carga), com formatos de verdade em vez de posições soltas.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Trecho { uint32_t ofs, n; uint8_t w; } Trecho;
typedef struct Mapa { char *nome; Trecho *t; uint32_t n, cap; } Mapa;

static Mapa *sMapas;
static uint32_t sN, sCap;
static int sLido;

static Mapa *achar(const char *nome, int criar) {
    uint32_t i;
    for (i = 0; i < sN; i++) {
        if (strcmp(sMapas[i].nome, nome) == 0) {
            return &sMapas[i];
        }
    }
    if (!criar) {
        return NULL;
    }
    if (sN == sCap) {
        sCap = sCap ? sCap * 2 : 256;
        sMapas = realloc(sMapas, sCap * sizeof(Mapa));
    }
    memset(&sMapas[sN], 0, sizeof(Mapa));
    sMapas[sN].nome = strdup(nome);
    return &sMapas[sN++];
}

static void carregar(void) {
    const char *caminho = getenv("BT3_MAPA");
    char nome[512];
    unsigned long ofs, w, n;
    FILE *fp;
    uint32_t trechos = 0;

    sLido = 1;
    if (caminho == NULL || (fp = fopen(caminho, "r")) == NULL) {
        fprintf(stderr, "inverter: sem mapa (BT3_MAPA)\n");
        return;
    }
    while (fscanf(fp, "%511[^\t]\t%lu\t%lu\t%lu\n", nome, &ofs, &w, &n) == 4) {
        Mapa *m = achar(nome, 1);
        if (m->n == m->cap) {
            m->cap = m->cap ? m->cap * 2 : 64;
            m->t = realloc(m->t, m->cap * sizeof(Trecho));
        }
        m->t[m->n].ofs = (uint32_t)ofs;
        m->t[m->n].n = (uint32_t)n;
        m->t[m->n].w = (uint8_t)w;
        m->n++;
        trechos++;
    }
    fclose(fp);
    fprintf(stderr, "inverter: %u trechos de %u arquivos\n", trechos, sN);
}

static void trocar(uint8_t *p, int w) {
    int i;
    for (i = 0; i < w / 2; i++) {
        uint8_t t = p[i];
        p[i] = p[w - 1 - i];
        p[w - 1 - i] = t;
    }
}

/* Inverte, dentro de buf (que contém os bytes [ofs, ofs+size) do arquivo), cada número inteiro ali. */
static void aplicar(const char *nome, uint32_t ofs, uint8_t *buf, uint32_t size) {
    Mapa *m;
    uint32_t i, k;
    if (!sLido) {
        carregar();
    }
    m = achar(nome, 0);
    if (m == NULL) {
        return;
    }
    for (i = 0; i < m->n; i++) {
        const Trecho *t = &m->t[i];
        for (k = 0; k < t->n; k++) {
            uint32_t a = t->ofs + k * t->w;
            if (a >= ofs && a + t->w <= ofs + size) {
                trocar(buf + (a - ofs), t->w);
            }
        }
    }
}

/* De onde veio cada leitura recente (para achar a origem do que se descomprime). */
#define LEITURAS 64
static struct { uint8_t *buf; uint32_t size, ofs; char nome[128]; } sLeit[LEITURAS];
static int sLeitPos;

extern size_t __real_Port_FileReadAt(const char *rel, long offset, void *buf, size_t bytes);
size_t __wrap_Port_FileReadAt(const char *rel, long offset, void *buf, size_t bytes) {
    size_t got = __real_Port_FileReadAt(rel, offset, buf, bytes);
    if (!sLido) {
        carregar();
    }
    if (getenv("BT3_INVERTER_LOG") != NULL) {
        fprintf(stderr, "inverter: leu %s +0x%lX -> %p (%u bytes)%s\n", rel, offset, buf, (unsigned)got,
                achar(rel, 0) == NULL ? " SEM MAPA" : "");
    }
    if (got > 0) {
        aplicar(rel, (uint32_t)offset, buf, (uint32_t)got);
        sLeit[sLeitPos].buf = buf;
        sLeit[sLeitPos].size = (uint32_t)got;
        sLeit[sLeitPos].ofs = (uint32_t)offset;
        snprintf(sLeit[sLeitPos].nome, sizeof(sLeit[0].nome), "%s", rel);
        sLeitPos = (sLeitPos + 1) % LEITURAS;
    }
    return got;
}

extern void *__real_Bpe_Decode(void *src, void *dst, int32_t *rawSize);
void *__wrap_Bpe_Decode(void *src, void *dst, int32_t *rawSize) {
    void *out;
    int32_t tam;
    int k;
    if (rawSize == NULL) {
        rawSize = &tam; /* quem chama nem sempre pede o tamanho; aqui ele é preciso */
    }
    if (getenv("BT3_INVERTER_LOG") != NULL) {
        fprintf(stderr, "inverter: bpe de %p: %08X %08X\n", src, ((uint32_t *)src)[0], ((uint32_t *)src)[1]);
    }
    out = __real_Bpe_Decode(src, dst, rawSize);
    if (out == NULL || *rawSize <= 0) {
        return out;
    }
    for (k = 1; k <= LEITURAS; k++) {
        int i = (sLeitPos - k + LEITURAS) % LEITURAS;
        uint8_t *s = src;
        if (sLeit[i].buf != NULL && s >= sLeit[i].buf && s < sLeit[i].buf + sLeit[i].size) {
            char nome[256];
            snprintf(nome, sizeof(nome), "%s@0x%X#bpe", sLeit[i].nome, (unsigned)(sLeit[i].ofs + (uint32_t)(s - sLeit[i].buf)));
            aplicar(nome, 0, out, (uint32_t)*rawSize);
            if (getenv("BT3_INVERTER_LOG") != NULL) {
                fprintf(stderr, "inverter: bpe %s -> %p (%d bytes)\n", nome, out, (int)*rawSize);
            }
            return out;
        }
    }
    fprintf(stderr, "inverter: descompressão de origem desconhecida (%p)\n", src);
    return out;
}
