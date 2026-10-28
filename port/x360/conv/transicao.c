/* Transições de cenário, ids 0x1BF/0x1C0. Leitores: battle_load.c e eft_d_b.c.
   Especificação em docs/x360/formatos/transicao.md. */
#include "conv.h"
#include "conv_pecas.h"

static void modelo_transicao(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 ng, grupos, nt, texturas, i;
    if (Conv_Ler32(c, a) != 0x6C646D63u) {
        Conv_Erro(c, "modelo da transição: assinatura cmdl inválida");
        return;
    }
    Conv_Bytes(c, a, 4); /* assinatura cmdl */
    ng = Conv_U32(c, a + 4);
    grupos = Conv_U32(c, a + 8);
    nt = Conv_U32(c, a + 12);
    texturas = Conv_U32(c, a + 16);
    if (Conv_U32(c, a + 20) != 0 || ng > 256 || nt > 256 ||
        grupos < 24 || (grupos & 3) != 0 || grupos > b - a || ng * 24 > b - a - grupos ||
        texturas < 24 || (texturas & 3) != 0 || texturas > b - a || nt * 64 > b - a - texturas) {
        Conv_Erro(c, "modelo da transição: contagens, deslocamentos ou relocação inválidos");
        return;
    }
    for (i = 0; i < nt && !c->erro; i++) {
        conv_ofs t = a + texturas + i * 64;
        conv_u32 pix = Conv_U32(c, t), pal = Conv_U32(c, t + 4);
        conv_u32 np = Conv_U32(c, t + 8), nc = Conv_U32(c, t + 12);
        Conv_U32s(c, t + 16, 6);
        Conv_Bytes(c, t + 40, 8);
        Conv_U64(c, t + 48);
        Conv_U32s(c, t + 56, 2);
        /* Ao contrário do TexFile, estes deslocamentos são bytes, não palavras. */
        if (pix > b - a || np > b - a - pix || pal > b - a || nc > b - a - pal) {
            Conv_Erro(c, "textura do modelo da transição fora da entrada");
            return;
        }
        if (pix != 0) Conv_PacoteImagem(c, a + pix, np);
        if (pal != 0) Conv_PacoteImagem(c, a + pal, nc);
    }
    for (i = 0; i < ng && !c->erro; i++) {
        conv_ofs g = a + grupos + i * 24;
        conv_u32 tex, verts, tris, nv, ntri, j;
        Conv_U32(c, g);
        tex = Conv_U32(c, g + 4);
        verts = Conv_U32(c, g + 8);
        tris = Conv_U32(c, g + 12);
        Conv_U32(c, g + 16); /* escala f32 */
        ntri = Conv_U16(c, g + 20);
        nv = Conv_U16(c, g + 22);
        if (tex >= nt || ntri > 32767 || nv > 32768 || (verts & 1) != 0 || (tris & 1) != 0 || verts > b - a || nv * 6 > b - a - verts ||
            tris > b - a || ntri * 30 > b - a - tris) {
            Conv_Erro(c, "grupo do modelo da transição fora da entrada");
            return;
        }
        for (j = 0; j < ntri && !c->erro; j++) {
            conv_ofs t = a + tris + j * 30;
            conv_u32 k;
            for (k = 0; k < 3; k++) {
                if (Conv_Ler16(c, t + k * 2) >= nv) {
                    Conv_Erro(c, "índice de vértice da transição fora do grupo");
                    return;
                }
            }
        }
        Conv_U16s(c, a + verts, nv * 3);
        /* O desenho lê inclusive RG e BA como u16 e extrai os bytes por deslocamento. */
        Conv_U16s(c, a + tris, ntri * 15);
    }
    Conv_Zeros(c, a, b, "modelo da transição");
}

void Conv_Transicao(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_u32 ent[5], n;
    n = Conv_Tabela(c, base, tam, ent, 3);
    if (c->erro || n != 3) {
        Conv_Erro(c, "transição: esperadas três entradas");
        return;
    }
    Conv_Camera(c, base + ent[1], base + ent[2]);
    modelo_transicao(c, base + ent[2], base + ent[3]);
    Conv_TexFile(c, base + ent[3], base + ent[4], 0);
    Conv_Zeros(c, base, base + ent[1], "cabeçalho da transição");
}
