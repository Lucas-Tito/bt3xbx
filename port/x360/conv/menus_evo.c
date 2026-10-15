/* Evolution Z e loja: docs/x360/formatos/menus-evo.md. Tabelas em conv.c, ganchos por chamador. */
#include "conv.h"
#include "conv_menus.h"
#include "conv_pecas.h"

#define SEC(n) (base + ent[n]), (base + ent[(n) + 1])

/* As telas alcançam linhas 0..minimo-1 (TextBox_AttachLine e MsgWin_Draw). */
void Conv_TextosMin(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 minimo) {
    if (c->erro) {
        return;
    }
    if (b - a < 4 || Conv_Ler32(c, a) < minimo) {
        Conv_Erro(c, "textos em 0x%lX: o leitor usa %lu linhas", (unsigned long)a, (unsigned long)minimo);
        return;
    }
    Conv_Textos(c, a, b);
}

/* Duas entradas por fala, Voice_PlayWithSubtitle. */
void Conv_LegendasMin(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 falas) {
    if (c->erro) {
        return;
    }
    if (b - a < 4 || Conv_Ler32(c, a) < 2 * falas) {
        Conv_Erro(c, "legendas em 0x%lX: a tela toca %lu falas", (unsigned long)a, (unsigned long)falas);
        return;
    }
    Conv_LipPack(c, a, b);
}

/* PassWin_Init (menu_za_b.c): filme e duas listas de texturas, sem texto da senha no pacote. */
void Conv_JanelaSenha(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_u32 ent[5];
    if (!Conv_Secoes(c, base, tam, ent, 3, "janela de senha")) {
        return;
    }
    Conv_Flash(c, SEC(1));
    Conv_TexFileMin(c, SEC(2), 3);
    Conv_TexFileMin(c, SEC(3), 1);
    Conv_Onde(c, "janela de senha", 0);
}

/* EvoTop_Init: a seção 11 vai ao Dialog_Init do DcSave; o McFlow tem o próprio gancho. */
void Conv_MenuEvoTopo(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[6] = {0, 8, 4, 2, 8, 2};
    conv_u32 ent[13], i;
    if (!Conv_Secoes(c, base, tam, ent, 11, "topo do Evolution Z")) {
        return;
    }
    for (i = 1; i <= 11 && !c->erro; i++) {
        switch (i) {
        case 6: Conv_Flash(c, SEC(i)); break;
        case 7: Conv_TextosMin(c, SEC(i), 18); break;
        case 8: Conv_JanelaMsg(c, SEC(i)); break;
        case 9: Conv_JanelaIcone(c, SEC(i)); break;
        case 10: Conv_LegendasMin(c, SEC(i), 18); break;
        case 11: Conv_Dialogo(c, SEC(i)); break;
        default: Conv_TexFileMin(c, SEC(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu do topo do Evolution Z", (unsigned long)i);
    }
}

/* Shop_Init e Shop_SwapGuide: dois guias de quinze falas; seção 23 sem leitor. */
void Conv_MenuLoja(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[34] = {
        0, 8, 3, 2, 3, 3, 0, 4, 4, 0, 1, 3, 3, 5, 4, 2, 1,
        0, 2, 0, 2, 0, 0, 0, 2, 0, 0, 4, 2, 0, 0, 4, 0, 0
    };
    conv_u32 ent[35], i;
    if (!Conv_Secoes(c, base, tam, ent, 33, "loja")) {
        return;
    }
    for (i = 1; i <= 33 && !c->erro; i++) {
        switch (i) {
        case 6: case 9: case 17: case 19: case 29: case 33: Conv_Flash(c, SEC(i)); break;
        case 21: Conv_JanelaMsg(c, SEC(i)); break;
        case 22: Conv_JanelaIcone(c, SEC(i)); break;
        case 23: Conv_RestoBytes(c, SEC(i)); break; /* sem leitor e sem forma conhecida */
        case 25: Conv_ItemHelp(c, SEC(i)); break;
        case 26: Conv_LegendasMin(c, SEC(i), 30); break;
        case 30: Conv_TextosMin(c, SEC(i), 351); break;
        case 32: Conv_TextosMin(c, SEC(i), 30); break;
        default: Conv_TexFileMin(c, SEC(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu da loja", (unsigned long)i);
    }
}

/* EvoZ_Load: três filmes, ItemHelp, diálogo, grade, fichas e janela de senha. */
void Conv_MenuEvoZ(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[32] = {
        0, 0, 0, 5, 4, 4, 2, 6, 3, 3, 5, 3, 1, 0, 3, 1,
        1, 3, 2, 5, 1, 1, 2, 0, 2, 5, 2, 1, 1, 1, 1, 0
    };
    conv_u32 ent[41], i;
    if (!Conv_Secoes(c, base, tam, ent, 39, "Evolution Z")) {
        return;
    }
    for (i = 1; i <= 39 && !c->erro; i++) {
        switch (i) {
        case 1: Conv_RestoBytes(c, SEC(i)); break; /* igual à seção 23 da loja, sem leitor */
        case 2: Conv_ItemHelp(c, SEC(i)); break;
        case 13: case 23: case 31: Conv_Flash(c, SEC(i)); break;
        case 32: Conv_Dialogo(c, SEC(i)); break;
        case 33: Conv_Celulas(c, SEC(i)); break;
        case 34: case 35: Conv_TextosMin(c, SEC(i), 161); break;
        case 36: Conv_FichasPersonagem(c, SEC(i)); break;
        case 37: Conv_TextosMin(c, SEC(i), 351); break;
        case 38: Conv_JanelaSenha(c, base + ent[i], ent[i + 1] - ent[i]); break;
        case 39: Conv_TextosMin(c, SEC(i), 2); break;
        default: Conv_TexFileMin(c, SEC(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu do Evolution Z", (unsigned long)i);
    }
}
