/*
 * F6, etapa 2 (redes 3 e 4): o gs/snd_se.c do port inteiro, sem mudar uma linha, mais o que os testes precisam ver
 * dele. Na build headless.py --gravador este objeto entra no lugar do snd_se.c; no teste_som (rede 3), também.
 *   Som_FluxoEfeitos, Som_VozesAtivas: para o gravador (gravador.c) saber qual fluxo é a mistura dos efeitos e quantas
 *     vozes estão ativas;
 *   Som_SabotaBanco: o controle negativo "banco com um byte trocado" (som.c, BT3_SABOTA=banco);
 *   Som_DecodificaVag: o decode_block do port sobre uma amostra (o ADPCM do chip de som), para a referência
 *     independente em Python (port/x360/tools/som_ref.py) comparar.
 */
#include "../../src/gs/snd_se.c"

SDL_AudioStream *Som_FluxoEfeitos(void) { return sStream; }

int Som_VozesAtivas(void) {
    int k, n = 0;
    for (k = 0; k < VOICES; k++) {
        n += sVoices[k].on != 0;
    }
    return n;
}

/* uma vez por rodada: troca um byte de dados (o segundo bloco) da amostra do efeito `id` do banco `mask` */
void Som_SabotaBanco(int mask, int id) {
    static int feito;
    int slot = slot_of(mask);
    uint32_t start, rate;
    if (feito || slot < 0 || !sBanks[slot].set || !lookup(&sBanks[slot], id, &start, &rate) || start + 32 > SPU_SIZE) {
        return;
    }
    feito = 1;
    sSpu[start + 16 + 4] ^= 0x5A;
    fprintf(stderr, "som: estrago: banco %d efeito %d, byte 0x%x da memória do chip\n", slot, id, start + 16 + 4);
}

/* Decodifica pelo decode_block do port a amostra que começa em `ini` dos `tam` bytes de `smp` (a parte das amostras
   de um banco, posta no começo da memória do chip), até o bloco com a bandeira 1 (fim ou laço), inclusive, ou o fim
   da parte. Grava as amostras (s16 little-endian) em `saida`; devolve quantas, ou -1. `estraga` >= 0: troca o byte
   nesse deslocamento da memória antes (controle negativo). */
long Som_DecodificaVag(const uint8_t *smp, uint32_t tam, uint32_t ini, FILE *saida, long estraga) {
    Voice v;
    long n = 0;
    int i;
    if (tam > SPU_SIZE || ini >= tam) {
        return -1;
    }
    memcpy(sSpu, smp, tam);
    memset(sSpu + tam, 0, SPU_TOTAL - tam);
    if (estraga >= 0 && (uint32_t)estraga < tam) {
        sSpu[estraga] ^= 0x5A;
    }
    memset(&v, 0, sizeof(v));
    v.pos = ini;
    for (;;) {
        uint32_t em = v.pos;
        int flags;
        if (em + 16 > tam) {
            break;
        }
        flags = sSpu[em + 1];
        decode_block(&v);
        if (v.ended) {
            break;
        }
        for (i = 0; i < 28; i++) {
            uint8_t b[2];
            b[0] = (uint8_t)v.buf[i];
            b[1] = (uint8_t)((uint16_t)v.buf[i] >> 8);
            fwrite(b, 1, 2, saida);
        }
        n += 28;
        if (flags & 1) {
            break;
        }
    }
    return n;
}
