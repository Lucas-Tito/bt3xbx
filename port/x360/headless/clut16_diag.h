/* F5, CLUT16: diagnóstico temporário, incluído pelo clut16.py antes do texture_get.
   Compara as 16 palavras, sem um segundo hash; a chave é a do cache de texturas.
   Uma ocorrência é uma transição de paleta na mesma chave, não um desenho repetido. */
static struct Clut16Entrada {
    uint64_t t0, texa;
    uint32_t gen, usada, cor[16];
} sClut16[65536];
static unsigned long long sClut16Leituras, sClut16Colisoes, sClut16Altas, sClut16Cheia, sClut16Csm2;

static void clut16_resumo(void) {
    fprintf(stderr, "clut16: leituras %llu colisoes %llu altas %llu cheia %llu csm2 %llu\n",
            sClut16Leituras, sClut16Colisoes, sClut16Altas, sClut16Cheia, sClut16Csm2);
}

static void clut16_registra(uint64_t t0, uint64_t texa, uint32_t gen, const uint32_t *cor) {
    uint32_t sl = (uint32_t)((t0 ^ (texa * 0x9E3779B97F4A7C15ull) ^ ((uint64_t)gen << 17)) *
                            0xD6E8FEB86659FD93ull >> 48), j;
    sClut16Leituras++;
    sClut16Csm2 += (unsigned)(t0 >> 55) & 1;
    for (j = 0; j < 65536; j++, sl = (sl + 1) & 65535) {
        struct Clut16Entrada *e = &sClut16[sl];
        if (!e->usada) {
            e->usada = 1; e->t0 = t0; e->texa = texa; e->gen = gen;
            memcpy(e->cor, cor, sizeof(e->cor));
            return;
        }
        if (e->t0 == t0 && e->texa == texa && e->gen == gen) {
            if (memcmp(e->cor, cor, sizeof(e->cor)) != 0) {
                int alta = memcmp(e->cor, cor, 8 * sizeof(uint32_t)) == 0;
                sClut16Colisoes++;
                sClut16Altas += alta;
                fprintf(stderr, "clut16: colisao quadro %u tex0 %llx texa %llx gen %08x alta %d\n",
                        (unsigned)gGsFrame, (unsigned long long)t0, (unsigned long long)texa, (unsigned)gen, alta);
                memcpy(e->cor, cor, sizeof(e->cor));
            }
            return;
        }
    }
    sClut16Cheia++;
}

static void clut16_observa(uint64_t t0, uint64_t texa, uint32_t gen, uint32_t cbp, uint32_t cpsm) {
    static int ligado = -1;
    static struct { uint64_t t0, texa; uint32_t gen, epoca, usada; } memo[16];
    uint32_t cor[16], i, m = (uint32_t)(t0 ^ (t0 >> 32)) & 15;
    if (ligado < 0) {
        ligado = getenv("BT3_CLUT16_DIAG") != NULL;
        if (ligado) atexit(clut16_resumo);
    }
    if (!ligado) return;
    /* A mesma VRAM e a mesma chave não precisam de outra leitura. A época muda em toda escrita. */
    if (memo[m].usada && memo[m].t0 == t0 && memo[m].texa == texa && memo[m].gen == gen &&
        memo[m].epoca == gGsPageEpoch) return;
    memo[m].usada = 1; memo[m].t0 = t0; memo[m].texa = texa; memo[m].gen = gen; memo[m].epoca = gGsPageEpoch;
    for (i = 0; i < 16; i++) cor[i] = Gs_VramRead(cbp, 1, cpsm, i & 7, i >> 3);
    clut16_registra(t0, texa, gen, cor);
}
