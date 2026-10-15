/* Data Center e Replay: docs/x360/formatos/menus-dc.md. Tabelas em conv.c, ganchos por chamador. */
#include "conv.h"
#include "conv_menus.h"
#include "conv_pecas.h"

#define SEC(n) (base + ent[n]), (base + ent[(n) + 1])

/* As falas do guia que cada tela toca (DcMenu_Say, DcList_Say, DcPass_Say): a maior + 1. */
#define DC_FALAS_TOPO 26
#define DC_FALAS_LISTA 28
#define DC_FALAS_SENHA 22
#define DC_PERSONAGENS 161 /* nomes e formas, TextBox_AttachLine com o id 0..160 */
#define DC_ITENS 350       /* SAVE_ITEM_COUNT: itemTbl[id - 1] e a linha id - 1 do nome */

/* menus-dc.md 6: a tabela de itens (ItemTblEntry, comuns.md 4.4), registros de 0x28 e depois zeros */
static void dc_itens(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_ofs r;
    if (c->erro) {
        return;
    }
    if (b - a < (conv_ofs)DC_ITENS * 0x28) {
        Conv_Erro(c, "itens em 0x%lX: 0x%lX bytes, o leitor indexa %d registros de 0x28", (unsigned long)a,
                  (unsigned long)(b - a), DC_ITENS);
        return;
    }
    for (r = a; r + 0x28 <= b && !c->erro; r += 0x28) {
        Conv_Bytes(c, r, 4);          /* type, kind, picture, slots */
        Conv_U32s(c, r + 4, 2);       /* price, stockLevel */
        Conv_U16s(c, r + 0x0C, 4);    /* stat */
        Conv_U32s(c, r + 0x14, 5);    /* flags, ability */
    }
    Conv_Zeros(c, a, b, "depois dos itens");
}

/* menus-dc.md 5: uma tabela de registros de `largura` bytes, cada um em palavras de `w` bytes (w = 1: bytes); o leitor
   alcança pelo menos n registros, e a seção inteira tem a forma da tabela (todos os registros inteiros), depois zeros */
static void dc_tabela(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 n, conv_u32 largura, conv_u32 w, const char *oque) {
    conv_u32 i, total;
    if (c->erro) {
        return;
    }
    if (b - a < (conv_ofs)n * largura) {
        Conv_Erro(c, "%s: 0x%lX bytes, o leitor alcança %lu registros", oque, (unsigned long)(b - a), (unsigned long)n);
        return;
    }
    total = (conv_u32)((b - a) / largura);
    for (i = 0; i < total && !c->erro; i++) {
        switch (w) {
        case 1: Conv_Bytes(c, a + (conv_ofs)i * largura, largura); break;
        case 2: Conv_U16s(c, a + (conv_ofs)i * largura, largura / 2); break;
        default: Conv_U32s(c, a + (conv_ofs)i * largura, largura / 4); break;
        }
    }
    Conv_Zeros(c, a, b, oque);
}

/* menus-dc.md 5: o pacote do PassChk (PassChk_Init, menu_za_c.c), N = 5 */
static void dc_passchk(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_u32 ent[7];
    if (!Conv_Secoes(c, base, tam, ent, 5, "PassChk")) {
        return;
    }
    dc_tabela(c, SEC(1), 0xCA, 4, 4, "PassChk 1, personagens do jogo anterior");  /* s32, charId < 0xCA */
    dc_tabela(c, SEC(2), 0x109, 4, 1, "PassChk 2, itens do jogo anterior");      /* PassOldItem, 4 x u8 */
    dc_tabela(c, SEC(3), 6 * 3 * 7, 4, 4, "PassChk 3, itens por classe");        /* s32 [6][3][7] */
    dc_tabela(c, SEC(4), 0x15F, 2, 1, "PassChk 4, itens");                       /* PassItem, 2 x u8 */
    /* PassChara (u16 mask, u16 valid): o índice do leitor passa da seção (menus-dc.md 5); pelo menos um registro */
    dc_tabela(c, SEC(5), 1, 4, 2, "PassChk 5, personagens");
    Conv_Onde(c, "PassChk", 0);
}

/* DcMenu_Init (menu_z_c.c): 459 s1, N = 12; a seção 12 é o diálogo do DcSave */
void Conv_MenuDcTopo(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[13] = {0, 0, 0, 0, 0, 0, 8, 4, 2, 2, 4, 7, 0};
    conv_u32 ent[14], i;
    if (!Conv_Secoes(c, base, tam, ent, 12, "topo do Data Center")) {
        return;
    }
    for (i = 1; i <= 12 && !c->erro; i++) {
        switch (i) {
        case 1: Conv_LegendasMin(c, SEC(i), DC_FALAS_TOPO); break;
        case 2: Conv_JanelaMsg(c, SEC(i)); break;
        case 3: Conv_JanelaIcone(c, SEC(i)); break;
        case 4: Conv_TextosMin(c, SEC(i), DC_FALAS_TOPO); break;
        case 5: Conv_Flash(c, SEC(i)); break;
        case 12: Conv_Dialogo(c, SEC(i)); break;
        default: Conv_TexFileMin(c, SEC(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu do topo do Data Center", (unsigned long)i);
    }
}

/* DcList_Init (menu_y_b.c): 459 s2, N = 22 */
void Conv_MenuDcLista(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[23] = {0, 8, 4, 17, 5, 0, 1, 11, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 1, 0, 0};
    conv_u32 ent[24], i;
    if (!Conv_Secoes(c, base, tam, ent, 22, "lista do Data Center")) {
        return;
    }
    for (i = 1; i <= 22 && !c->erro; i++) {
        switch (i) {
        case 5: case 9: Conv_Flash(c, SEC(i)); break;
        case 10: Conv_JanelaSenha(c, base + ent[i], ent[i + 1] - ent[i]); break;
        case 11: Conv_JanelaMsg(c, SEC(i)); break;
        case 12: Conv_Dialogo(c, SEC(i)); break;
        case 13: Conv_TextosMin(c, SEC(i), DC_FALAS_LISTA); break;
        case 14: Conv_LegendasMin(c, SEC(i), DC_FALAS_LISTA); break;
        case 15: dc_itens(c, SEC(i)); break;
        case 16: Conv_ItemHelp(c, SEC(i)); break;
        case 17: case 18: Conv_TextosMin(c, SEC(i), DC_PERSONAGENS); break;
        case 21: Conv_TextosMin(c, SEC(i), DC_ITENS); break;
        case 22: Conv_TextosMin(c, SEC(i), 3); break; /* Dialog_SetMsg(2) */
        default: Conv_TexFileMin(c, SEC(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu da lista do Data Center", (unsigned long)i);
    }
}

/* DcPass_Init (menu_z_d.c): 459 s3, N = 21 */
void Conv_MenuDcSenha(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[22] = {0, 10, 7, 14, 4, 1, 0, 0, 0, 0, 8, 4, 0, 0, 0, 7, 3, 0, 0, 0, 0, 0};
    conv_u32 ent[23], i;
    if (!Conv_Secoes(c, base, tam, ent, 21, "senha do Data Center")) {
        return;
    }
    for (i = 1; i <= 21 && !c->erro; i++) {
        switch (i) {
        case 6: Conv_JanelaMsg(c, SEC(i)); break;
        case 7: Conv_Dialogo(c, SEC(i)); break;
        case 8: Conv_TextosMin(c, SEC(i), DC_FALAS_SENHA); break;
        case 9: Conv_LegendasMin(c, SEC(i), DC_FALAS_SENHA); break;
        case 12: dc_itens(c, SEC(i)); break;
        case 13: case 14: Conv_TextosMin(c, SEC(i), DC_PERSONAGENS); break;
        case 17: case 18: Conv_Flash(c, SEC(i)); break;
        case 19: dc_passchk(c, base + ent[i], ent[i + 1] - ent[i]); break;
        case 20: Conv_Celulas(c, SEC(i)); break;
        case 21: Conv_TextosMin(c, SEC(i), 2); break; /* Dialog_SetMsg(0) e (1) */
        default: Conv_TexFileMin(c, SEC(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu da senha do Data Center", (unsigned long)i);
    }
}

/* ReplayMenu_Init (menu_za_d.c): 459 s4, N = 9; a seção 9 não tem leitor */
void Conv_MenuReplay(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[10] = {0, 0, 1, 14, 7, 0, 8, 7, 0, 0};
    conv_u32 ent[11], i;
    if (!Conv_Secoes(c, base, tam, ent, 9, "replay")) {
        return;
    }
    for (i = 1; i <= 9 && !c->erro; i++) {
        switch (i) {
        case 1: Conv_Flash(c, SEC(i)); break;
        case 5: Conv_Dialogo(c, SEC(i)); break;
        case 8: Conv_FichasPersonagem(c, SEC(i)); break;
        case 9: Conv_RestoBytes(c, SEC(i)); break; /* gReplayMenu->font: guardado, nunca lido */
        default: Conv_TexFileMin(c, SEC(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu do replay", (unsigned long)i);
    }
}
