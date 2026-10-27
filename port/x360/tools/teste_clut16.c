/* F5: o clut_hash extraído do fonte, com o leitor real da VRAM. Nenhum dado do jogo.
   Muda cada cor isoladamente, confere as 16/256 leituras e a memória por geração.
   O diagnóstico também tem de ver a colisão quando a chave antiga é restaurada. */
#include "gs_core.c"
#include "clut16_hash.h"
#include "clut16_diag.h"

static void invalida(void) {
    uint32_t i;
    for (i = 0; i < 512; i++) gGsPageGen[i]++;
    gGsPageEpoch++;
}

static uint32_t referencia(uint32_t bp, uint32_t psm, uint32_t n) {
    uint32_t i, h = 2166136261u;
    for (i = 0; i < n; i++) {
        uint32_t c = Gs_VramRead(bp, 1, psm, n == 16 ? i & 7 : i & 15, n == 16 ? i >> 3 : i >> 4);
        COR_MISTURA(h, c);
    }
    return h;
}

int main(void) {
    static const uint32_t formatos[] = {0, 2, 10}, bases[] = {0, 31, 32, 63, 16382, 16383};
    uint32_t f, b, c, i, h, h2, antigo, cor[16], falhas = 0, casos = 0;
    tables_init();
    for (i = 0; i < 1048576; i++) sVram[i] = GS_LE32(i * 2654435761u);
    for (f = 0; f < 3; f++) for (b = 0; b < 6; b++) {
        uint32_t bp = bases[b], psm = formatos[f];
        for (c = 0; c < 16; c++) {
            uint64_t t0 = 1 + casos;
            h = clut_hash(bp, psm, 16);
            falhas += h != referencia(bp, psm, 16);
            for (i = 0; i < 16; i++) cor[i] = Gs_VramRead(bp, 1, psm, i & 7, i >> 3);
            clut16_registra(t0, 0, h, cor);
            antigo = cor[c];
            vram_rw(bp, 1, psm, c & 7, c >> 3, 1, antigo ^ 0x21u);
            invalida();
            h2 = clut_hash(bp, psm, 16);
            falhas += h2 == h || h2 != referencia(bp, psm, 16);
            falhas += clut_hash(bp, psm, 16) != h2;
            cor[c] ^= 0x21u;
            clut16_registra(t0, 0, h2, cor);
            vram_rw(bp, 1, psm, c & 7, c >> 3, 1, antigo);
            invalida();
            falhas += clut_hash(bp, psm, 16) != h;
            casos++;
        }
        for (c = 0; c < 256; c++) {
            h = clut_hash(bp, psm, 256);
            falhas += h != referencia(bp, psm, 256);
            antigo = Gs_VramRead(bp, 1, psm, c & 15, c >> 4);
            vram_rw(bp, 1, psm, c & 15, c >> 4, 1, antigo ^ 0x21u);
            invalida();
            falhas += clut_hash(bp, psm, 256) == h;
            falhas += clut_hash(bp, psm, 256) != referencia(bp, psm, 256);
            vram_rw(bp, 1, psm, c & 15, c >> 4, 1, antigo);
            invalida();
        }
        /* A metade que o hash antigo lia por engano fica fora das 16 cores. */
        for (c = 8; c < 16; c++) {
            h = clut_hash(bp, psm, 16);
            antigo = Gs_VramRead(bp, 1, psm, c, 0);
            vram_rw(bp, 1, psm, c, 0, 1, antigo ^ 0x21u);
            invalida();
            falhas += clut_hash(bp, psm, 16) != h;
            vram_rw(bp, 1, psm, c, 0, 1, antigo);
            invalida();
        }
    }
    clut16_resumo();
    printf("CLUT16: %u mutacoes de 16 cores, 4608 de 256 cores; falhas %u\n", casos, falhas);
    return falhas || sClut16Colisoes || sClut16Cheia ? 1 : 0;
}
