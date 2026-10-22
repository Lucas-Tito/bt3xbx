/*
 * (F5, etapa 8, gravação: docs/x360/plano/F5/08-gravacao.md, seção 18) O teste do envio de 32 bits por linhas de cache
 * (BT3_ENVIO_RAPIDO, o envio_faixa32 do port/src/gs/gs_core.c) contra o vram_rw, pixel a pixel.
 *
 *   teste_envio [N]        N envios sorteados (1500 por padrão); sai com 0 só se todos deram a mesma VRAM
 *   teste_envio --mede     o tempo do transfer_data por byte, chave 0, 1 e 2, nos tamanhos que o jogo envia
 *
 * Cada envio sorteado (o formato, dbp, dbw, o retângulo, alinhado às linhas de cache ou não, e os pedaços IMAGE em que os
 * dados chegam, múltiplos de 16 bytes) parte da mesma VRAM aleatória e roda quatro vezes: a referência, um vram_rw de
 * escrita por pixel na ordem do envio, e o transfer_data com a chave em 0, 1 e 2. As VRAMs têm de sair iguais byte a byte.
 * Compilado com -DBT3_TESTE_ENVIO, o dcbz128 do 360 vira um memset da linha de 128 bytes: uma linha zerada e não escrita
 * inteira aparece como diferença.
 * (Os de 24 bits vão num pedaço só: o transfer_data conta os pixels a partir do começo de cada pedaço, e um pixel de 3
 * bytes partido entre dois pedaços de 16 sai errado, com ou sem a chave; o jogo não envia 24 bits, envio_conta.py.)
 * Roda na x86 (-DBT3_TESTE_VU1) e na PPC de verdade (powerpc-linux-gnu-gcc -static -DPORT_BIG_ENDIAN, no qemu-ppc):
 * na x86 com PORT_BIG_ENDIAN o GS_LE32 troca os bytes mas a memória segue little-endian, e só a PPC prova a ordem
 * dos bytes (guia de port, "As texturas novas"). Monta-se incluindo o gs_core.c, pelo port/x360/tools/teste_envio.sh.
 */
#include "gs_core.c"

static uint32_t sBase[1 << 20], sRef[1 << 20];
static uint8_t sDados[1 << 21];
static uint32_t sSemente = 1;

static uint32_t sorteia(void) {
    sSemente = sSemente * 1103515245u + 12345u;
    return sSemente >> 8;
}

/* o envio pelo caminho do jogo: BITBLTBUF, TRXPOS, TRXREG, TRXDIR e os dados em pedaços (cada pedaço é um IMAGE) */
static void envia(uint32_t psm, uint32_t dbp, uint32_t dbw, uint32_t tx, uint32_t ty, uint32_t tw, uint32_t th,
                  const uint8_t *dados, uint32_t bytes, int pedacos) {
    uint32_t feito = 0;
    gs.bitbltbuf = (uint64_t)dbp << 32 | (uint64_t)dbw << 48 | (uint64_t)psm << 56;
    gs.trxpos = (uint64_t)tx << 32 | (uint64_t)ty << 48;
    gs.trxreg = (uint64_t)tw | (uint64_t)th << 32;
    gs.trxdir = 0;
    transfer_begin();
    while (feito < bytes) {
        uint32_t n = bytes - feito;
        if (pedacos) {
            uint32_t q = 1 + sorteia() % 64;
            if (sorteia() % 4 == 0) {
                q = 1 + sorteia() % 2048;
            }
            if (q * 16 < n) {
                n = q * 16;
            }
        }
        transfer_data(dados + feito, n);
        feito += n;
    }
}

static uint32_t pixel_de(const uint8_t *d, uint32_t bits, uint32_t k) {
    switch (bits) {
    case 32: return d[k * 4] | d[k * 4 + 1] << 8 | d[k * 4 + 2] << 16 | (uint32_t)d[k * 4 + 3] << 24;
    case 24: return d[k * 3] | d[k * 3 + 1] << 8 | d[k * 3 + 2] << 16;
    case 16: return d[k * 2] | d[k * 2 + 1] << 8;
    case 8: return d[k];
    default: return (d[k >> 1] >> ((k & 1) * 4)) & 15;
    }
}

static int mede(void) {
    /* os envios que pesam na luta (envio_conta.py, seção 18): PSMCT32 em 0,0, os tamanhos mais comuns */
    static const struct { uint32_t psm, tw, th; } c[] = {{0x00, 128, 64}, {0x00, 128, 128}, {0x00, 256, 256}, {0x00, 512, 128}, {0x00, 64, 32}};
    uint32_t k, r, v;
    for (k = 0; k < sizeof(c) / sizeof(c[0]); k++) {
        uint32_t bytes = c[k].tw * c[k].th * (uint32_t)Gs_PsmBits(c[k].psm) / 8, vezes = (64u << 20) / bytes;
        double ns[3];
        for (v = 0; v < 3; v++) {
            struct timespec t0, t1;
            sEnvioRapido = (int)v;
            clock_gettime(CLOCK_MONOTONIC, &t0);
            for (r = 0; r < vezes; r++) {
                envia(c[k].psm, (r * 512) & 0x3FFF, c[k].tw / 64, 0, 0, c[k].tw, c[k].th, sDados, bytes, 0);
            }
            clock_gettime(CLOCK_MONOTONIC, &t1);
            ns[v] = ((double)(t1.tv_sec - t0.tv_sec) * 1e9 + (double)(t1.tv_nsec - t0.tv_nsec)) / ((double)vezes * bytes);
        }
        printf("mede psm %02x %ux%u (%u bytes): ns por byte com a chave 0 %.3f, 1 %.3f, 2 %.3f\n", (unsigned)c[k].psm,
               (unsigned)c[k].tw, (unsigned)c[k].th, (unsigned)bytes, ns[0], ns[1], ns[2]);
    }
    return 0;
}

int main(int argc, char **argv) {
    static const uint32_t psms[] = {0x00, 0x00, 0x00, 0x00, 0x13, 0x14, 0x02, 0x0A, 0x01, 0x1B, 0x24, 0x2C};
    uint32_t i, n = 1500, ruins[3] = {0, 0, 0}, porpsm[64], faixas = 0;
    memset(porpsm, 0, sizeof(porpsm));
    tables_init();
    for (i = 0; i < (1u << 20); i++) {
        sBase[i] = sorteia() * 2654435761u ^ sorteia();
    }
    for (i = 0; i < sizeof(sDados); i++) {
        sDados[i] = (uint8_t)sorteia();
    }
    if (argc > 1 && strcmp(argv[1], "--mede") == 0) {
        return mede();
    }
    if (argc > 1) {
        n = (uint32_t)atoi(argv[1]);
    }
    for (i = 0; i < n; i++) {
        uint32_t psm = psms[sorteia() % (sizeof(psms) / sizeof(psms[0]))], bits = (uint32_t)Gs_PsmBits(psm);
        uint32_t larg = 8, dbp = 1 + sorteia() % 0x3FFF, dbw = sorteia() % 17, tx, ty, tw, th, bytes, k, v;
        int alinhado = sorteia() % 4 != 0;
        if (sorteia() % 5 == 0) {
            dbw = sorteia() % 2; /* bw >> 1 == 0: as linhas de páginas se sobrepõem */
        }
        tw = 1 + sorteia() % 300;
        th = 1 + sorteia() % 200;
        tx = sorteia() % 1024;
        ty = sorteia() % 1024;
        if (alinhado) {
            tw = larg * (1 + sorteia() % (512 / larg));
            tx = larg * (sorteia() % (1024 / larg));
            if (sorteia() % 2) {
                ty &= ~3u;
                th = 4 * (1 + sorteia() % 64);
            }
        }
        bytes = (tw * th * bits + 7) / 8;
        bytes = (bytes + 15) & ~15u; /* em quadwords, como o IMAGE */
        if (bytes > sizeof(sDados)) {
            th = (uint32_t)(sizeof(sDados) * 8 / (tw * bits));
            bytes = ((tw * th * bits + 7) / 8 + 15) & ~15u;
        }
        /* a referência: o vram_rw por pixel, na ordem do envio */
        memcpy(sVram, sBase, sizeof(sVram));
        for (k = 0; k < tw * th && (k + 1) * bits <= bytes * 8; k++) {
            vram_rw(dbp, dbw, psm, tx + k % tw, ty + k / tw, 1, pixel_de(sDados, bits, k));
        }
        memcpy(sRef, sVram, sizeof(sVram));
        for (v = 0; v < 3; v++) {
            uint32_t semente = sSemente;
            memcpy(sVram, sBase, sizeof(sVram));
            sEnvioRapido = (int)v;
            envia(psm, dbp, dbw, tx, ty, tw, th, sDados, bytes, (int)(i & 1) && bits != 24);
            if (v < 2) {
                sSemente = semente; /* os mesmos pedaços nas três */
            }
            if (memcmp(sVram, sRef, sizeof(sVram)) != 0) {
                uint32_t w = 0;
                while (sVram[w] == sRef[w]) {
                    w++;
                }
                if (ruins[0] + ruins[1] + ruins[2] < 8) {
                    printf("DIFERE chave %u: psm %02x dbp %x dbw %u em %u,%u %ux%u, %u bytes; a primeira palavra %x: %08x contra %08x\n",
                           (unsigned)v, (unsigned)psm, (unsigned)dbp, (unsigned)dbw, (unsigned)tx, (unsigned)ty, (unsigned)tw,
                           (unsigned)th, (unsigned)bytes, (unsigned)w, (unsigned)sVram[w], (unsigned)sRef[w]);
                }
                ruins[v]++;
            }
        }
        porpsm[psm]++;
        if (alinhado && psm == 0x00 && th >= 4) {
            faixas++;
        }
    }
    printf("por formato:");
    for (i = 0; i < 64; i++) {
        if (porpsm[i]) {
            printf(" %02x:%u", (unsigned)i, (unsigned)porpsm[i]);
        }
    }
    printf("; %u de 32 bits alinhados às linhas de cache\n", (unsigned)faixas);
    printf("%u envios, diferentes com a chave 0: %u, 1: %u, 2: %u\n", (unsigned)n, (unsigned)ruins[0], (unsigned)ruins[1], (unsigned)ruins[2]);
    return ruins[0] + ruins[1] + ruins[2] != 0;
}
