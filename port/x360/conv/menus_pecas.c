/*
 * Peças comuns dos pacotes de tela dos menus: as janelas de mensagem e de ícone, as legendas com o movimento da boca
 * (e, na etapa 5 da F4, as das telas de seleção). Especificação: docs/x360/formatos/menus-pecas.md (as seções citadas
 * abaixo). Cada peça é uma função que converte [a, b), para os conversores de pacote (menus.c) a reaproveitarem.
 */
#include "conv.h"
#include "conv_pecas.h"
#include "conv_menus.h"

/* entrada n de uma tabela já lida, a partir de a */
#define SEC(n) (a + ent[n]), (a + ent[(n) + 1])

/* §2: janela de mensagem (MsgWin_Init, src/battle/view_a_b.c:30): seção 1 TexFile (o MsgWin usa os registros 0 e
   1), seção 2 o filme */
void Conv_JanelaMsg(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 ent[4];
    if (!Conv_Secoes(c, a, b - a, ent, 2, "janela de mensagem")) {
        return;
    }
    Conv_TexFile(c, SEC(1), 0);
    Conv_Flash(c, SEC(2));
    Conv_Onde(c, "janela de mensagem", 0);
}

/* §3: janela de ícone (IconWin_Init, src/battle/view_a_c.c:24): seção 1 TexFile (registros 0..2), seção 2 o filme.
   Os ícones (registros 0 e 1 + n) vêm de um TexFile do chamador, convertido por ele */
void Conv_JanelaIcone(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 ent[4];
    if (!Conv_Secoes(c, a, b - a, ent, 2, "janela de ícone")) {
        return;
    }
    Conv_TexFile(c, SEC(1), 0);
    Conv_Flash(c, SEC(2));
    Conv_Onde(c, "janela de ícone", 0);
}

/* §4.1: um bloco de chaves da boca (LipSync_Start, src/battle/view_b_c.c:284): assinatura de 8 bytes que ninguém lê,
   s32 contagem em +8 (Mem_ReadS32), +0xC..+0xF sem leitor (zero), contagem x (u16 quadro, u16 boca) a partir de +0x10
   (Mem_ReadU16, LipSync_Update :303); depois, zeros até o bloco seguinte */
static void bloco_boca(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 n;
    Conv_Bytes(c, a, 8);
    n = Conv_U32(c, a + 8);
    if (!c->erro && (n == 0 || a + 0x10 + (conv_ofs)n * 4 > b)) {
        Conv_Erro(c, "boca em 0x%lX: %lu chaves em 0x%lX bytes", (unsigned long)a, (unsigned long)n,
                  (unsigned long)(b - a));
        return;
    }
    Conv_U16s(c, a + 0x10, (conv_ofs)n * 2);
    Conv_Zeros(c, a, b, "boca");
}

/* §4: legendas e boca (LipPack): tabela de seções com duas entradas por fala (Voice_PlayWithSubtitle,
   src/battle/view_b_c.c:36: pack[fala * 2 + 1] na primeira língua, pack[fala * 2 + 2] na segunda); cada entrada não
   vazia, um bloco de chaves. Uma entrada vazia (o mesmo deslocamento da seguinte) não tem bloco próprio */
void Conv_LipPack(ConvCtx *c, conv_ofs a, conv_ofs b) {
    static conv_u32 ent[0x800 + 2];
    conv_u32 n, i;
    n = Conv_Tabela(c, a, b - a, ent, 0x800);
    if (!c->erro && n % 2 != 0) {
        Conv_Erro(c, "legendas com %lu entradas (duas por fala)", (unsigned long)n);
    }
    for (i = 1; i <= n && !c->erro; i++) {
        if (ent[i] != ent[i + 1]) {
            bloco_boca(c, SEC(i));
            Conv_Onde(c, "legenda %lu", (unsigned long)i);
        }
    }
}
