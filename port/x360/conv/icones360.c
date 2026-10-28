/* Troca opcional da instalação, após a conversão numérica do boot para big-endian.
   A arte é entrada: nenhum pixel do jogo nem dependência do sistema operacional. */
#include <string.h>
#include <stdio.h>
#include "conv.h"

static conv_u32 be32(const conv_u8 *p) {
    return ((conv_u32)p[0] << 24) | ((conv_u32)p[1] << 16) | ((conv_u32)p[2] << 8) | p[3];
}

int Conv_Icones360(conv_u8 *buf, conv_ofs tam, const ConvIcone360 *arte, unsigned n, char *msg) {
    conv_ofs folha, fim, tabela, pix[18], clut[18], tamanho[18], a, b;
    unsigned i, j;
    const conv_u8 *e;
    if (tam < 52 || be32(buf) != 11 || n != 18 || arte == NULL) goto invalido;
    folha = be32(buf + 8 * 4);
    fim = be32(buf + 9 * 4);
    if (folha > tam || fim > tam || fim < folha || fim - folha != 0x8640) goto invalido;
    if (be32(buf + folha) != 28 || be32(buf + folha + 4) != 8) goto invalido;
    tabela = folha + 32;
    for (i = 0; i < n; i++) {
        if (arte[i].indice != 10 + i || arte[i].pixels == NULL || arte[i].paleta == NULL) goto invalido;
        e = buf + tabela + arte[i].indice * 64;
        tamanho[i] = (arte[i].indice >= 20 && arte[i].indice <= 23) || arte[i].indice >= 26 ? 2048 : 512;
        if (arte[i].tamanho != tamanho[i] || be32(e + 8) != tamanho[i] + 128 || be32(e + 12) != 192) goto invalido;
        if (be32(e) > (fim - folha) / 4 || be32(e + 4) > (fim - folha) / 4) goto invalido;
        a = folha + be32(e) * 4;
        b = folha + be32(e + 4) * 4;
        if (a < tabela + 28 * 64 || a > fim - tamanho[i] - 128 || b < a + tamanho[i] + 128 || b > fim - 192) goto invalido;
        /* TEX0 u64 em big-endian, palavra alta antes da baixa: PSMT4, largura e altura 32 ou 64, CLUT CT32. */
        if (((be32(e + 0x34) >> 20) & 63) != 20 ||
            ((be32(e + 0x34) >> 26) & 15) != (tamanho[i] == 512 ? 5U : 6U) ||
            (((be32(e + 0x34) >> 30) | (be32(e + 0x30) << 2)) & 15) != (tamanho[i] == 512 ? 5U : 6U) ||
            ((be32(e + 0x30) >> 19) & 15) != 0) goto invalido;
        for (j = 0; j < 16; j++) if (arte[i].paleta[j * 4 + 3] > 128) goto invalido;
        pix[i] = a + 96;
        clut[i] = b + 96;
        if (i && a < clut[i - 1] + 96) goto invalido;
    }
    /* Valida o pacote inteiro antes de gravar o primeiro byte. Não toca nos cabeçalhos GIF/VIF. */
    for (i = 0; i < n; i++) {
        memcpy(buf + pix[i], arte[i].pixels, tamanho[i]);
        memcpy(buf + clut[i], arte[i].paleta, 64);
    }
    return 1;
invalido:
    strcpy(msg, "ícones 360: folha ou pacote de arte incompatível");
    return -1;
}
