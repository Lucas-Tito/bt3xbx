/* Os pacotes de tela do Ultimate Battle (F4, etapa 9b). Especificação: docs/x360/formatos/menus-ub.md, uma parte por
   grupo de telas (as seções de cada parte estão nos comentários). Registrados em sFormatos e sFaixas (conv.c). Cada
   parte tem o prefixo dos seus nomes internos (ubA_ a ubE_), da linha de produção em que foi escrita. */
#include "conv.h"
#include "conv_pecas.h"
#include "conv_menus.h"

/* as duas pontas da seção n de uma tabela de seções já lida (ent[], Conv_Secoes) */
#define ENT(n) (base + ent[n]), (base + ent[(n) + 1])

/* ================================================================== o menu, o topo do Sim Dragon e o Disc Fusion (00451 seções 1, 5 e 6); parte A de menus-ub.md (os § são dela) */
/* uma tabela de textos (menus.md 6.1) de que o leitor pede as linhas 0..minimo - 1 (TextBox_AttachLine lê
   text[linha + 1], view_b.c:243; o diálogo, msgTbl[msgIdx]): a contagem tem de cobrir a linha mais alta */
static void ubA_textos(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 minimo) {
    conv_u32 n;
    if (c->erro) {
        return;
    }
    if (b - a < 4) {
        Conv_Erro(c, "textos em 0x%lX: seção de 0x%lX bytes", (unsigned long)a, (unsigned long)(b - a));
        return;
    }
    n = Conv_Ler32(c, a);
    if (!c->erro && n < minimo) {
        Conv_Erro(c, "textos em 0x%lX: %lu linhas, o leitor usa %lu", (unsigned long)a, (unsigned long)n,
                  (unsigned long)minimo);
        return;
    }
    Conv_Textos(c, a, b);
}

/* legendas com a boca (LipPack, menus-pecas.md 4) de que a tela toca as falas 0..falas - 1
   (Voice_PlayWithSubtitle lê pack[fala * 2 + 1] e pack[fala * 2 + 2], view_b_c.c:38, :43): 2 * falas entradas */
static void ubA_legendas(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 falas) {
    conv_u32 n;
    if (c->erro) {
        return;
    }
    if (b - a < 4) {
        Conv_Erro(c, "legendas em 0x%lX: seção de 0x%lX bytes", (unsigned long)a, (unsigned long)(b - a));
        return;
    }
    n = Conv_Ler32(c, a);
    if (!c->erro && n < 2 * falas) {
        Conv_Erro(c, "legendas em 0x%lX: %lu entradas, a tela toca %lu falas", (unsigned long)a, (unsigned long)n,
                  (unsigned long)falas);
        return;
    }
    Conv_LipPack(c, a, b);
}

/* §2: o menu do Ultimate Battle (UbMenu_Init, src/menu/menu_p_c.c:63, modo 13; seção 1 de 0x1C4), N = 9. 1 o filme
   (Flash_Create, :92), 2 o fundo (UM_RES(2), UbMenu_Draw: Sprite_DrawPicture, 8 registros), 3 TexFile (registros
   0..10, :81..:91), 4 os ícones da janela de ícone (IconWin_Init usa os registros 0 e 1), 5 TexFile (0, 1, 3, 5, 6 e
   8, :74..:79), 6 a janela de mensagem, 7 a de ícone, 8 os textos da janela de mensagem (MsgWin_Draw com a fala
   0..16: 17 linhas), 9 as legendas com a boca (falas 0..16, UbMenu_UpdateVoice e UM_SAY_PLATE) */
void Conv_MenuUbMenu(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[6] = {0, 0, 8, 11, 2, 9};
    conv_u32 ent[11], i;
    if (!Conv_Secoes(c, base, tam, ent, 9, "menu do Ultimate Battle")) {
        return;
    }
    for (i = 1; i <= 9 && !c->erro; i++) {
        switch (i) {
        case 1: Conv_Flash(c, ENT(1)); break;
        case 6: Conv_JanelaMsg(c, ENT(6)); break;
        case 7: Conv_JanelaIcone(c, ENT(7)); break;
        case 8: ubA_textos(c, ENT(8), 17); break;
        case 9: ubA_legendas(c, ENT(9), 17); break;
        default: Conv_TexFileMin(c, ENT(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu do menu do Ultimate Battle", (unsigned long)i);
    }
}

/* §3: o topo do Sim Dragon (SimTop_Init, src/menu/menu_r_d.c:46, modo 20; seção 5 de 0x1C4), N = 9. 1 o filme
   (:111), 2 TexFile (registros 0..29, :74..:103), 3 os ícones da janela de ícone (0 e 1), 4 o fundo (SimTop_Draw:
   Sprite_DrawPicture, 8 registros), 5 TexFile (5, 6 e 8, :61..:63), 6 a janela de ícone, 7 os retratos do ranking
   (SubpacoteTex de 165, SIMTOP_FACE_NUM: o laço reloca as 165 e SimTop_Draw lê o registro 0 de MPACK_AT(faces,
   chara + 1)), 8 os textos de "como jogar" (TextBox_AttachLine com a página 0..10, SIMTOP_HELP_PAGES: 11 linhas), 9
   as legendas com a boca (falas 0x11..0x15, SimTop_UpdateVoice: 0x16 falas) */
void Conv_MenuSimInicio(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[6] = {0, 0, 30, 2, 8, 9};
    conv_u32 ent[11], i;
    if (!Conv_Secoes(c, base, tam, ent, 9, "topo do Sim Dragon")) {
        return;
    }
    for (i = 1; i <= 9 && !c->erro; i++) {
        switch (i) {
        case 1: Conv_Flash(c, ENT(1)); break;
        case 6: Conv_JanelaIcone(c, ENT(6)); break;
        case 7: Conv_FichasPersonagem(c, ENT(7)); break;
        case 8: ubA_textos(c, ENT(8), 11); break;
        case 9: ubA_legendas(c, ENT(9), 0x16); break;
        default: Conv_TexFileMin(c, ENT(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu do topo do Sim Dragon", (unsigned long)i);
    }
}

/* §4: o Disc Fusion (DiscFusion_Init, src/menu/menu_o_b.c:56, modo 24; seção 6 de 0x1C4), N = 12. 1 o filme da tela
   (texA, :90), 2 o filme de "disco reconhecido" (texB, :92), 3 TexFile (registros 0..18, :71..:89), 4 os ícones da
   janela de ícone (0 e 1), 5 o diálogo da troca de disco (Dialog_Init com os textos da 12, :105), 6 a janela de
   ícone, 7 a de mensagem, 8 o fundo (DiscFusion_Draw: Sprite_DrawPicture, 8 registros), 9 TexFile (0, 1 e 3,
   :67..:69), 10 os textos da janela de mensagem (MsgWin_Draw com a fala 0x40..0x53: 0x54 linhas), 11 as legendas com
   a boca (falas 0x40..0x53, DiscFusion_UpdateTalk), 12 os textos do diálogo e das placas (Dialog_SetMsg 0x14..0x1F,
   DiscFusion_UpdateDisc; TextBox_AttachLine 0x15A..0x15D, DiscFusion_Draw: 0x15E linhas) */
void Conv_MenuDiscFusion(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[10] = {0, 0, 0, 19, 2, 0, 0, 0, 8, 4};
    conv_u32 ent[14], i;
    if (!Conv_Secoes(c, base, tam, ent, 12, "Disc Fusion")) {
        return;
    }
    for (i = 1; i <= 12 && !c->erro; i++) {
        switch (i) {
        case 1: case 2: Conv_Flash(c, ENT(i)); break;
        case 5: Conv_Dialogo(c, ENT(5)); break;
        case 6: Conv_JanelaIcone(c, ENT(6)); break;
        case 7: Conv_JanelaMsg(c, ENT(7)); break;
        case 10: ubA_textos(c, ENT(10), 0x54); break;
        case 11: ubA_legendas(c, ENT(11), 0x54); break;
        case 12: ubA_textos(c, ENT(12), 0x15E); break;
        default: Conv_TexFileMin(c, ENT(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu do Disc Fusion", (unsigned long)i);
    }
}

/* ================================================================== a seleção da Missão 100 e da Sobrevivência e os resultados (00451 seções 2 e 3); parte B de menus-ub.md (os § são dela) */
#define UBB_NENHUM 0x3E7     /* MISSEL_NONE, SURV_NONE: fim do time, item vazio */
#define UBB_ALEATORIO 0x3E6  /* MISSEL_RANDOM, SURV_RANDOM: sorteado pelo leitor */
#define UBB_PERSONAGENS 0xA1 /* SAVE_CHARA_COUNT: personagens 0..0xA0 */
#define UBB_ESTAGIOS 0x24    /* estágios 0..0x23 */
#define UBB_ITENS 350        /* SAVE_ITEM_COUNT: ids de item 0..349 (o leitor grava id + 1) */
#define UBB_MISSOES 100      /* MisSel: page * 5 + row, 20 páginas (UoSave.mission[100]) */
#define UBB_CURSOS 3         /* SURV_COURSE_NUM */
#define UBB_OPP_CURSO 50     /* SURV_OPP_NUM */
#define UBB_SORTEIO 0x66     /* SURV_RANDOM_NUM: Rand_Range(0x66) sobre a lista */
#define UBB_PRECOS 101       /* UbScore_CalcPoints: price[valor] com valor 0..100 */

/* o início de uma tabela lida por MPACK_AT tem de ser múltiplo de 4, e a tabela tem de ter `minimo` registros de
   `tam` bytes; devolve o número de registros inteiros que cabem na seção (o resto, enchimento, fica para Conv_Zeros) */
static conv_u32 ubB_registros(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 tam, conv_u32 minimo, const char *oque) {
    conv_u32 n;
    if (c->erro) {
        return 0;
    }
    if (a % 4 != 0) {
        Conv_Erro(c, "%s em 0x%lX: início fora de múltiplo de 4 (MPACK_AT arredonda)", oque, (unsigned long)a);
        return 0;
    }
    n = (conv_u32)((b - a) / tam);
    if (n < minimo) {
        Conv_Erro(c, "%s: %lu registros de 0x%lX, o leitor usa %lu", oque, (unsigned long)n, (unsigned long)tam,
                  (unsigned long)minimo);
        return 0;
    }
    return n;
}

/* um id de item como o leitor o usa: UBB_NENHUM ou 0..349 (o leitor grava id + 1 num u16 de BattleItemSet) */
static void ubB_item(ConvCtx *c, conv_u32 id, conv_u32 reg, const char *oque) {
    if (!c->erro && id != UBB_NENHUM && id >= UBB_ITENS) {
        Conv_Erro(c, "%s %lu: item %lu (os ids vão a %lu)", oque, (unsigned long)reg, (unsigned long)id,
                  (unsigned long)(UBB_ITENS - 1));
    }
}

/* o estágio de uma regra: UBB_ALEATORIO (Rand_Range(0x23)) ou um estágio */
static void ubB_estagio(ConvCtx *c, conv_u32 st, conv_u32 reg, const char *oque) {
    if (!c->erro && st != UBB_ALEATORIO && st >= UBB_ESTAGIOS) {
        Conv_Erro(c, "%s %lu: estágio %lu", oque, (unsigned long)reg, (unsigned long)st);
    }
}

/* um adversário (MisSelOpp e SurvOpp, include/menu/menu_o.h e menu_s.h; 0x2C bytes, onze s32): chara, costume,
   cpuLevel, item7, item[7]. aceito = o marcador que o leitor trata além dos personagens (UBB_NENHUM na Missão 100,
   que encerra o time e escolhe a ficha 165; UBB_ALEATORIO na Sobrevivência, sorteado da seção 14) */
static void ubB_adversario(ConvCtx *c, conv_ofs r, conv_u32 reg, conv_u32 aceito, const char *oque) {
    conv_u32 chara = Conv_U32(c, r), k;
    Conv_U32s(c, r + 4, 2); /* costume, cpuLevel: BattleSetup_SetMember / SetPoolMember */
    ubB_item(c, Conv_U32(c, r + 0xC), reg, oque);
    for (k = 0; k < 7; k++) {
        ubB_item(c, Conv_U32(c, r + 0x10 + k * 4), reg, oque);
    }
    if (!c->erro && chara != aceito && chara >= UBB_PERSONAGENS) {
        Conv_Erro(c, "%s %lu: personagem %lu", oque, (unsigned long)reg, (unsigned long)chara);
    }
}

/* §3.2: os adversários (seção 11 para a Missão 100, 13 para a Sobrevivência): todos os registros inteiros da seção
   (os índices das missões e dos cursos apontam para eles); depois, zeros. Devolve o número de registros */
static conv_u32 ubB_adversarios(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 aceito, const char *oque) {
    conv_u32 n = ubB_registros(c, a, b, 0x2C, 1, oque), i;
    for (i = 0; i < n && !c->erro; i++) {
        ubB_adversario(c, a + (conv_ofs)i * 0x2C, i, aceito, oque);
    }
    Conv_Zeros(c, a, b, oque);
    return n;
}

/* §3.1: as missões (MisSelDef, include/menu/menu_o.h; seção 10): 100 registros de 0x34, treze s32 (announcer, unk4,
   timeLimit, stage, bgm, unk14, kind, dpRule, opp[5]). MisSel_SetupBattle trata UBB_ALEATORIO em announcer, stage e
   bgm, dá o tamanho do time pelo kind (0, 1, 2, 3 = 1..4 lutadores; 6, 7, 10 = 5; os outros, 0) e anda opp[] até o
   adversário UBB_NENHUM; MisSel_Draw mostra a linha 0x148 + kind e 0x156 + dpRule e lê os cinco opp[] de toda
   missão (a ficha de cada um). nopp = registros da seção 11 */
static void ubB_missoes(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 nopp) {
    conv_u32 n = ubB_registros(c, a, b, 0x34, UBB_MISSOES, "missões"), i, k, kind, dp, opp;
    for (i = 0; i < n && !c->erro; i++) {
        conv_ofs r = a + (conv_ofs)i * 0x34;
        Conv_U32s(c, r, 3); /* announcer, unk4, timeLimit */
        ubB_estagio(c, Conv_U32(c, r + 0xC), i, "missão");
        Conv_U32s(c, r + 0x10, 2); /* bgm, unk14 */
        kind = Conv_U32(c, r + 0x18);
        dp = Conv_U32(c, r + 0x1C);
        if (!c->erro && (kind > 10 || (kind > 3 && kind != 6 && kind != 7 && kind != 10))) {
            Conv_Erro(c, "missão %lu: kind %lu (o time do jogador sairia com 0 lutadores)", (unsigned long)i,
                      (unsigned long)kind);
        }
        if (!c->erro && dp > 3) {
            Conv_Erro(c, "missão %lu: regra de DP %lu (vai a 3)", (unsigned long)i, (unsigned long)dp);
        }
        for (k = 0; k < 5 && !c->erro; k++) {
            opp = Conv_U32(c, r + 0x20 + k * 4);
            if (!c->erro && opp >= nopp) {
                Conv_Erro(c, "missão %lu: adversário %lu é o registro %lu (a seção 11 tem %lu)", (unsigned long)i,
                          (unsigned long)k, (unsigned long)opp, (unsigned long)nopp);
            }
        }
    }
    Conv_Zeros(c, a, b, "depois das missões");
}

/* §3.3: os cursos da Sobrevivência (SurvCourse, include/menu/menu_s.h; seção 12): 3 registros de 0xE0, a cabeça da
   MisSelDef (seis s32) e opp[50] s32, índices na seção 13 (SurvSel_SetupBattle lê os 50 do curso escolhido) */
static void ubB_cursos(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 nopp) {
    conv_u32 n = ubB_registros(c, a, b, 0xE0, UBB_CURSOS, "cursos"), i, k, opp;
    for (i = 0; i < n && !c->erro; i++) {
        conv_ofs r = a + (conv_ofs)i * 0xE0;
        Conv_U32s(c, r, 3); /* announcer, unk4, timeLimit */
        ubB_estagio(c, Conv_U32(c, r + 0xC), i, "curso");
        Conv_U32s(c, r + 0x10, 2); /* bgm, unk14 */
        for (k = 0; k < UBB_OPP_CURSO && !c->erro; k++) {
            opp = Conv_U32(c, r + 0x18 + k * 4);
            if (!c->erro && opp >= nopp) {
                Conv_Erro(c, "curso %lu: adversário %lu é o registro %lu (a seção 13 tem %lu)", (unsigned long)i,
                          (unsigned long)k, (unsigned long)opp, (unsigned long)nopp);
            }
        }
    }
    Conv_Zeros(c, a, b, "depois dos cursos");
}

/* §3.4: os personagens do sorteio (seção 14, s32): SurvSel_SetupBattle lê randomChara[Rand_Range(0x66)], as entradas
   0..0x65. No disco a lista tem uma entrada a mais (sem leitor, um personagem também): as entradas até a última
   palavra não nula são ids s32; depois, zeros */
static void ubB_sorteio(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 n = ubB_registros(c, a, b, 4, UBB_SORTEIO, "personagens do sorteio"), i, ult = 0, id;
    for (i = 0; i < n && !c->erro; i++) {
        if (Conv_Ler32(c, a + (conv_ofs)i * 4) != 0) {
            ult = i + 1;
        }
    }
    if (ult < UBB_SORTEIO) {
        ult = UBB_SORTEIO;
    }
    for (i = 0; i < ult && !c->erro; i++) {
        id = Conv_U32(c, a + (conv_ofs)i * 4);
        if (!c->erro && id >= UBB_PERSONAGENS) {
            Conv_Erro(c, "personagens do sorteio: entrada %lu é %lu", (unsigned long)i, (unsigned long)id);
        }
    }
    Conv_Zeros(c, a, b, "depois dos personagens do sorteio");
}

/* §2: a seleção da Missão 100 e da Sobrevivência (MisSel_Init, src/menu/menu_o_d.c:139; SurvSel_Init,
   src/menu/menu_s_b.c:86; seção 2 de 0x1C4), N = 18. 1 o filme da MisSel, 2 o da SurvSel; 3 TexFile (MisSel 0..27,
   SurvSel até 27); 4 o fundo (Sprite_DrawPicture, 8); 5 TexFile (0, 1, 3); 6 sem leitor, com a forma de janela de
   mensagem; 7 a janela de ícone das duas; 8 os ícones dela na MisSel, 9 na SurvSel (registros 0 e 1); 10 as missões,
   11 os adversários delas; 12 os cursos, 13 os adversários deles, 14 os personagens do sorteio; 15 as 165 fichas
   (MisSel); 16 sem leitor, com a forma de tabela de textos; 17 as legendas com a boca, 18 os textos (das duas) */
void Conv_MenuUbSelecao(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[10] = {0, 0, 0, 28, 8, 4, 0, 0, 2, 2};
    conv_u32 ent[20], i, nmis = 0, nsurv = 0;
    if (!Conv_Secoes(c, base, tam, ent, 18, "seleção do Ultimate Battle")) {
        return;
    }
    /* os adversários antes das tabelas que apontam para eles (a ordem não muda nada no buffer) */
    nmis = ubB_adversarios(c, ENT(11), UBB_NENHUM, "adversário da missão");
    Conv_Onde(c, "seção 11 da seleção do Ultimate Battle", 0);
    if (c->erro) {
        return;
    }
    nsurv = ubB_adversarios(c, ENT(13), UBB_ALEATORIO, "adversário da Sobrevivência");
    Conv_Onde(c, "seção 13 da seleção do Ultimate Battle", 0);
    for (i = 1; i <= 18 && !c->erro; i++) {
        switch (i) {
        case 1: case 2: Conv_Flash(c, ENT(i)); break;
        case 6: Conv_JanelaMsg(c, ENT(6)); break; /* sem leitor: a forma de janela de mensagem */
        case 7: Conv_JanelaIcone(c, ENT(7)); break;
        case 10: ubB_missoes(c, ENT(10), nmis); break;
        case 11: case 13: break; /* já convertidas */
        case 12: ubB_cursos(c, ENT(12), nsurv); break;
        case 14: ubB_sorteio(c, ENT(14)); break;
        case 15: Conv_FichasPersonagem(c, ENT(15)); break;
        case 16: /* sem leitor: a forma de tabela de textos (88, uma por fala da 17) */
        case 18: Conv_Textos(c, ENT(i)); break;
        case 17: Conv_LipPack(c, ENT(17)); break;
        default: Conv_TexFileMin(c, ENT(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu da seleção do Ultimate Battle", (unsigned long)i);
    }
}

/* §4.2: a tabela de preços (UbScorePrice, include/menu/menu_p.h; seção 13): registros de 0x18, seis s32 (line[4],
   bonus, unk14). UbScore_CalcPoints lê price[valor].line[i] com o valor da linha (vida e a segunda linha até 100; a
   quarta, os adversários vencidos na Sobrevivência, até 50), price[degrau].line[2] (degrau 0..99) e price[bônus].bonus
   (bônus 0..47): 101 registros. Depois, zeros */
static void ubB_precos(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 n = ubB_registros(c, a, b, 0x18, UBB_PRECOS, "preços");
    Conv_U32s(c, a, (conv_ofs)n * 6);
    Conv_Zeros(c, a, b, "depois dos preços");
}

/* §4: o resultado (MisResult_Init, src/menu/menu_p_b.c:59; SurvResult_Init, src/menu/menu_s_c.c:22; SimResult_Init,
   src/menu/menu_r_c.c:31; UbResult_Init, src/menu/menu_n_d.c:46; seção 3 de 0x1C4), N = 14. 1 o filme da MisResult e
   da UbResult, 2 o da SurvResult, 3 o da SimResult; 4 TexFile (até 47); 5 o fundo da MisResult, SurvResult e
   UbResult, 6 o da SimResult (Sprite_DrawPicture, 8); 7 TexFile (0, 1, 3; na SimResult 5, 6, 8); 8 os ícones da
   janela de ícone (registros 0 e 1 + ícone, IconWin_SetIcon até 4); 9 a janela de ícone; 10 sem leitor, com a forma
   de tabela de textos; 11 as legendas com a boca; 12 os textos; 13 os preços; 14 os nomes dos itens */
void Conv_MenuUbResultado(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[9] = {0, 0, 0, 0, 48, 8, 8, 9, 6};
    conv_u32 ent[16], i;
    if (!Conv_Secoes(c, base, tam, ent, 14, "resultado do Ultimate Battle")) {
        return;
    }
    for (i = 1; i <= 14 && !c->erro; i++) {
        switch (i) {
        case 1: case 2: case 3: Conv_Flash(c, ENT(i)); break;
        case 9: Conv_JanelaIcone(c, ENT(9)); break;
        case 10: /* sem leitor: a forma de tabela de textos (88, uma por fala da 11) */
        case 12: case 14: Conv_Textos(c, ENT(i)); break;
        case 11: Conv_LipPack(c, ENT(11)); break;
        case 13: ubB_precos(c, ENT(13)); break;
        default: Conv_TexFileMin(c, ENT(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu do resultado do Ultimate Battle", (unsigned long)i);
    }
}

/* ================================================================== a seleção de um lutador e de um time (00451 seção 4); parte C de menus-ub.md (os § são dela) */
/* uma tabela de textos (menus.md 6.1) que o leitor indexa até a linha minimo - 1 (TextBox_AttachLine lê a entrada
   linha + 1): o N da tabela, lido antes de converter, tem de ser pelo menos `minimo` */
static void ubC_textos(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 minimo) {
    conv_u32 n;
    if (c->erro || !Conv_Cabe(c, a, 4, "textos")) {
        return;
    }
    n = Conv_Ler32(c, a);
    if (n < minimo) {
        Conv_Erro(c, "textos em 0x%lX: %lu linhas, o leitor usa %lu", (unsigned long)a, (unsigned long)n,
                  (unsigned long)minimo);
        return;
    }
    Conv_Textos(c, a, b);
}

/* as legendas com a boca (LipPack, menus-pecas.md 4): Voice_PlayWithSubtitle lê pack[fala * 2 + 1] e pack[fala * 2 +
   2], então a fala f pede pelo menos 2 * f + 2 entradas */
static void ubC_legendas(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 maior_fala) {
    conv_u32 n;
    if (c->erro || !Conv_Cabe(c, a, 4, "legendas")) {
        return;
    }
    n = Conv_Ler32(c, a);
    if (n < 2 * maior_fala + 2) {
        Conv_Erro(c, "legendas em 0x%lX: %lu entradas, a fala 0x%lX usa %lu", (unsigned long)a, (unsigned long)n,
                  (unsigned long)maior_fala, (unsigned long)(2 * maior_fala + 2));
        return;
    }
    Conv_LipPack(c, a, b);
}

/* §2: as seleções do Ultimate Battle (SoloSel_Init, src/menu/menu_l_b.c:148, modos 15 com um lutador, 18, 21, 25 e
   29; UbTeamSel_Init, src/menu/menu_m_b.c:266, modo 15 com time; as duas com section = 4, Ub_Main menu_o_c.c), N = 34.
   A união das duas telas: TexFile 1..10, 12..16, 18..20 e 32..34 (SS_RES, UT_RES; 32 o fundo de Sprite_DrawPicture;
   3..5 só o UbTeamSel com a regra de DP, 18..20 só o UbTeamSel), filmes 11, 17, 31 e 21 (só o UbTeamSel), 22 o
   ItemPanel (lado 0), 23 a janela de mensagem, 24 o ItemHelp, 25 os textos da janela (falas 0x16..0x18 e 0x36,
   0x37), 26 as legendas com a boca (as mesmas falas), 27 as 165 fichas, 28 e 29 os nomes e as formas (linha =
   personagem ou marcador até 0xA3, o maior id em que o cursor da grade para), 30 as células da grade. Todas têm leitor
   em pelo menos uma das telas */
void Conv_MenuUbSel(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[35] = {
        0, 1, 3, 1, 2, 3, 2, 5, 1, 1, 2, 0, 2, 2, 2, 2, 2, 0, 1, 3, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 5, 9};
    conv_u32 ent[36], i;
    if (!Conv_Secoes(c, base, tam, ent, 34, "seleção do Ultimate Battle")) {
        return;
    }
    for (i = 1; i <= 34 && !c->erro; i++) {
        switch (i) {
        case 11: case 17: case 21: case 31: Conv_Flash(c, ENT(i)); break;
        case 22: Conv_ItemPanel(c, ENT(22)); break;
        case 23: Conv_JanelaMsg(c, ENT(23)); break;
        case 24: Conv_ItemHelp(c, ENT(24)); break;
        case 25: ubC_textos(c, ENT(25), 0x38); break;   /* MsgWin_Draw(.., voiceLine): a maior fala é 0x37 */
        case 26: ubC_legendas(c, ENT(26), 0x37); break;
        case 27: Conv_FichasPersonagem(c, ENT(27)); break; /* MPACK_AT(chips, id + 1), id até 0xA4 */
        case 28: case 29: ubC_textos(c, ENT(i), 0xA4); break; /* TextBox_AttachLine(.., image / chara, ..) */
        case 30: Conv_Celulas(c, ENT(30)); break;
        default: Conv_TexFileMin(c, ENT(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu da seleção do Ultimate Battle", (unsigned long)i);
    }
}

/* ================================================================== o tabuleiro do Sim Dragon (00474); parte D de menus-ub.md (os § são dela) */
#define UBD_NENHUM 999   /* SIMDAY_NONE (include/menu/menu_q.h) */
#define UBD_SORTEIO 998  /* SIMDAY_RANDOM */
#define UBD_RODADAS 7    /* turn / 10 com turn <= 69 (SIM_LAST_TURN, include/menu/menu_r.h): rodadas 0..6 */
#define UBD_PERSONAGEM_MAX 0xA3 /* o retrato é 0x2F9 + personagem, até 0x39C (os 164 arquivos 760..923) */
#define UBD_ITENS 350    /* Q_ITEM_NUM (0x15E): ids de item 0..349 */

/* s32 em o, já trocado */
static long ubD_s32(ConvCtx *c, conv_ofs o) {
    return (long)(int)Conv_U32(c, o);
}

/* a tabela [a, b) tem n registros de tam bytes: cabe, alinhada a 4; o resto é enchimento (exigido zero no fim) */
static int ubD_cabe(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 n, conv_u32 tam, const char *oque) {
    if (c->erro) {
        return 0;
    }
    if (a % 4 != 0 || b < a || (b - a) / tam < n) {
        Conv_Erro(c, "%s em 0x%lX: 0x%lX bytes, o leitor usa %lu registros de 0x%lX", oque, (unsigned long)a,
                  (unsigned long)(b - a), (unsigned long)n, (unsigned long)tam);
        return 0;
    }
    return 1;
}

/* um personagem do adversário: o sorteio (só em SimEnemy.chara) ou um id com retrato */
static void ubD_personagem(ConvCtx *c, long v, int sorteio, const char *oque, conv_u32 k) {
    if (!c->erro && !(sorteio && v == UBD_SORTEIO) && (v < 0 || v > UBD_PERSONAGEM_MAX)) {
        Conv_Erro(c, "%s %lu: personagem %ld sem retrato (0x2F9 + personagem vai até 0x39C)", oque, (unsigned long)k, v);
    }
}

/* um item do adversário: nenhum ou um id de item */
static void ubD_item(ConvCtx *c, long v, conv_u32 k) {
    if (!c->erro && v != UBD_NENHUM && (v < 0 || v >= UBD_ITENS)) {
        Conv_Erro(c, "adversário %lu: item %ld fora dos %d", (unsigned long)k, v, UBD_ITENS);
    }
}

/* seção 25, as rodadas (SimRound, include/menu/menu_q.h): 7 registros de 0x1C, sete s32 (announcer, unk4, timeLimit,
   stage, bgm, unk14, enemy), lidos por SimDay_SetupBattle (menu_q_b.c:92) com round = turn / 10; depois, zeros.
   Devolve 1 + o maior enemy (os registros que a tabela de adversários precisa ter) */
static conv_u32 ubD_rodadas(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 k, precisa = 0;
    long inimigo;
    if (!ubD_cabe(c, a, b, UBD_RODADAS, 0x1C, "rodadas")) {
        return 0;
    }
    for (k = 0; k < UBD_RODADAS && !c->erro; k++) {
        Conv_U32s(c, a + k * 0x1C, 6);
        inimigo = ubD_s32(c, a + k * 0x1C + 0x18);
        if (!c->erro && inimigo < 0) {
            Conv_Erro(c, "rodada %lu: adversário %ld", (unsigned long)k, inimigo);
        }
        if (!c->erro && (conv_u32)inimigo + 1 > precisa) {
            precisa = (conv_u32)inimigo + 1;
        }
    }
    Conv_Zeros(c, a, b, "depois das rodadas");
    return precisa;
}

/* seção 26, os adversários (SimEnemy): n registros de 0x2C (n = 1 + o maior SimRound.enemy), onze s32 (chara, color,
   cpuLevel, lastItem, item[7]), lidos por SimDay_SetupBattle; depois, zeros */
static void ubD_adversarios(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 n) {
    conv_u32 k, j;
    conv_ofs r;
    if (!ubD_cabe(c, a, b, n, 0x2C, "adversários")) {
        return;
    }
    for (k = 0; k < n && !c->erro; k++) {
        r = a + k * 0x2C;
        ubD_personagem(c, ubD_s32(c, r), 1, "adversário", k);
        Conv_U32s(c, r + 4, 2);
        ubD_item(c, ubD_s32(c, r + 0xC), k);
        for (j = 0; j < 7 && !c->erro; j++) {
            ubD_item(c, ubD_s32(c, r + 0x10 + j * 4), k);
        }
    }
    Conv_Zeros(c, a, b, "depois dos adversários");
}

/* seção 27, os sorteios de adversário (SimPool): 3 registros de dezesseis s32 (o pool 0, o 1 na rodada 3, o 2 na 5),
   lidos até o primeiro 999 ou até o 16º; Rand_Range(n) precisa de n >= 1. Depois do 999, s32 pela forma */
static void ubD_sorteios(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 k, j;
    long v;
    int fim;
    if (!ubD_cabe(c, a, b, 3, 0x40, "sorteios de adversário")) {
        return;
    }
    for (k = 0; k < 3 && !c->erro; k++) {
        fim = 0;
        for (j = 0; j < 16 && !c->erro; j++) {
            v = ubD_s32(c, a + k * 0x40 + j * 4);
            if (!fim && v == UBD_NENHUM) {
                fim = 1;
                if (j == 0) {
                    Conv_Erro(c, "sorteio %lu vazio (Rand_Range(0))", (unsigned long)k);
                }
            } else if (!fim) {
                ubD_personagem(c, v, 0, "sorteio", k);
            }
        }
    }
    Conv_Zeros(c, a, b, "depois dos sorteios");
}

/* seção 28, os eventos ao acaso (SimEventRow): 32 linhas de cinco s32; SimDay_PickEvent (menu_q_b.c:801) lê a coluna
   eventSet (0..3, SimDay_ListItems); a coluna 4 não tem leitor (s32 pela forma) */
static void ubD_eventos(ConvCtx *c, conv_ofs a, conv_ofs b) {
    if (ubD_cabe(c, a, b, 32, 0x14, "eventos ao acaso")) {
        Conv_U32s(c, a, 32 * 5);
        Conv_Zeros(c, a, b, "depois dos eventos");
    }
}

/* seção 29, os treinos (SimTrainTbl, include/menu/menu_t.h:61; TSimDay.train, que é SimDay.unkB5C): 3 registros de
   0xC0, 48 s32 (weight[3][6], hp[6], atkMin[6], atkMax[6], defMin[6], defMax[6]), lidos por SimEv00..02 e
   SimEv00_Roll..02 (src/menu/menu_s_d.c); Rand_Range(max - min + 1) precisa de min <= max */
static void ubD_treinos(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 k, i;
    conv_ofs r;
    if (!ubD_cabe(c, a, b, 3, 0xC0, "treinos")) {
        return;
    }
    for (k = 0; k < 3 && !c->erro; k++) {
        r = a + k * 0xC0;
        /* confere antes de trocar: Conv_Ler32 lê como está no disco */
        for (i = 0; i < 6 && !c->erro; i++) {
            if ((int)Conv_Ler32(c, r + 0x60 + i * 4) > (int)Conv_Ler32(c, r + 0x78 + i * 4) ||
                (int)Conv_Ler32(c, r + 0x90 + i * 4) > (int)Conv_Ler32(c, r + 0xA8 + i * 4)) {
                Conv_Erro(c, "treino %lu, resultado %lu: mínimo acima do máximo", (unsigned long)k, (unsigned long)i);
            }
        }
        Conv_U32s(c, r, 48);
    }
    Conv_Zeros(c, a, b, "depois dos treinos");
}

/* seção 30, os níveis (SimLevel): 8 registros de dois s32 (max, need); o jogo lê level[nível] (0..6) e
   level[nível + 1] (até 7: SimDay_RefreshBoard :860, SimDay_Update :1532, SimDay_Cmd(33) :666) */
static void ubD_niveis(ConvCtx *c, conv_ofs a, conv_ofs b) {
    if (ubD_cabe(c, a, b, 8, 8, "níveis")) {
        Conv_U32s(c, a, 16);
        Conv_Zeros(c, a, b, "depois dos níveis");
    }
}

/* uma tabela de textos com pelo menos `minimo` linhas (a linha i é a entrada i + 1, menus.md 6.1) */
static void ubD_textos(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 minimo) {
    conv_u32 n;
    if (c->erro) {
        return;
    }
    if (b - a < 4) {
        Conv_Erro(c, "textos em 0x%lX: 0x%lX bytes", (unsigned long)a, (unsigned long)(b - a));
        return;
    }
    n = Conv_Ler32(c, a);
    if (n < minimo) {
        Conv_Erro(c, "textos em 0x%lX: %lu linhas, o leitor usa %lu", (unsigned long)a, (unsigned long)n,
                  (unsigned long)minimo);
        return;
    }
    Conv_Textos(c, a, b);
}

/* O tabuleiro (SimDay_Init, src/menu/menu_q_b.c:877; 0x1DB inteiro), N = 33, todas com leitor. Filmes: 1 o tabuleiro,
   2 o número da rodada, 10..12 o jogo das cartas (seção gSimTrain0[rank].param + 5, param 5..7), 13 e 14 os outros
   dois treinos, 15 a janela de três linhas, 18 o evento. TexFile (SD_RES e MPACK_AT direto): 3 (0..59), 4 o fundo
   (Sprite_DrawPicture, 8), 5 (0..13), 6 e 7 os retratos do monitor (faceA e faceB, 0..11), 8 o lutador do jogador
   (MTEX(res, member.chara): 0..0xA0, 161), 16 (0..27), 17 (0..5), 19 (0..7), 20, 21 e 31 (0), 22 os números (0, 1),
   23 (0..4). 9 a janela de mensagem, 24 os textos dela e do diálogo (linhas até 0xD7), 32 os nomes dos itens
   (TextBox_AttachLine com o id do item, até 0x70 pelos itens dos eventos), 33 o diálogo de sair. Tabelas: 25 rodadas,
   26 adversários, 27 sorteios, 28 eventos, 29 treinos, 30 níveis */
void Conv_MenuSimDia(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[34] = {0, 0, 0, 60, 8, 14, 12, 12, 161, 0, 0, 0, 0, 0, 0, 0, 28, 6, 0, 8, 1, 1, 2, 5,
                                       0, 0, 0, 0, 0, 0, 0, 1, 0, 0};
    conv_u32 ent[35], i, adversarios = 0;
    if (!Conv_Secoes(c, base, tam, ent, 33, "tabuleiro do Sim Dragon")) {
        return;
    }
    for (i = 1; i <= 33 && !c->erro; i++) {
        switch (i) {
        case 1: case 2: case 10: case 11: case 12: case 13: case 14: case 15: case 18: Conv_Flash(c, ENT(i)); break;
        case 9: Conv_JanelaMsg(c, ENT(9)); break;
        case 24: ubD_textos(c, ENT(24), 0xD8); break;
        case 25: adversarios = ubD_rodadas(c, ENT(25)); break;
        case 26: ubD_adversarios(c, ENT(26), adversarios); break;
        case 27: ubD_sorteios(c, ENT(27)); break;
        case 28: ubD_eventos(c, ENT(28)); break;
        case 29: ubD_treinos(c, ENT(29)); break;
        case 30: ubD_niveis(c, ENT(30)); break;
        case 32: ubD_textos(c, ENT(32), 0x71); break;
        case 33: Conv_Dialogo(c, ENT(33)); break;
        default: Conv_TexFileMin(c, ENT(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu do tabuleiro do Sim Dragon", (unsigned long)i);
    }
}

/* ================================================================== os ramos do Disc Fusion: o ranking (00475) e os cursos (00476); parte E de menus-ub.md (os § são dela) */
#define UBE_NENHUM 999u         /* N_NONE (include/menu/menu_n.h:53): "sem lutador", o fim da lista de um curso */
#define UBE_PERSONAGEM_MAX 0xA0u /* o maior personagem jogável (SAVE_CHARA_COUNT = 161) */
#define UBE_REGRA 0x1C          /* sizeof(NRankRule) */
#define UBE_LUTADOR 0x2C        /* sizeof(NFoe) */
#define UBE_CURSO 0x38          /* sizeof(NCourse) */
#define UBE_CURSOS 5            /* N_COURSE_NUM */
#define UBE_LUGARES 99          /* UBRANK_PLACES - 1: as regras de 100 lugares sem o do jogador */
#define UBE_INTRUSOS 37         /* Rand_Range(0x25) em UbRank_PickFoe */
#define UBE_ESTAGIOS 28         /* Rand_Range(0x1C) em UbRank_SetupBattle */
#define UBE_MUSICAS 11          /* Rand_Range(0xB) em UbRank_SetupBattle */

/* o início de uma tabela lida por MPACK_AT tem de ser múltiplo de 4 (a macro arredonda para baixo) */
static int ubE_alinhada(ConvCtx *c, conv_ofs a, const char *oque) {
    if (!c->erro && a % 4 != 0) {
        Conv_Erro(c, "%s em 0x%lX: início fora de múltiplo de 4 (MPACK_AT arredonda)", oque, (unsigned long)a);
    }
    return !c->erro;
}

/* §3.3: tabela de textos com pelo menos `minimo` entradas: TextBox_AttachLine lê a entrada linha + 1
   (src/battle/view_b.c:240), então a linha L pede N >= L + 1 */
static void ubE_textos(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 minimo) {
    conv_u32 n;
    if (c->erro) {
        return;
    }
    if (b - a < 8) {
        Conv_Erro(c, "textos em 0x%lX: seção de 0x%lX bytes", (unsigned long)a, (unsigned long)(b - a));
        return;
    }
    n = Conv_Ler32(c, a);
    if (!c->erro && n < minimo) {
        Conv_Erro(c, "textos em 0x%lX: %lu entradas, o leitor usa %lu", (unsigned long)a, (unsigned long)n,
                  (unsigned long)minimo);
        return;
    }
    Conv_Textos(c, a, b);
}

/* §3.3: legendas com a boca (LipPack, menus-pecas.md 4) com pelo menos `minimo` entradas: a fala L lê as entradas
   2L + 1 e 2L + 2 (Voice_PlayWithSubtitle, src/battle/view_b_c.c:38, :43) */
static void ubE_legendas(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 minimo) {
    conv_u32 n;
    if (c->erro) {
        return;
    }
    if (b - a < 8) {
        Conv_Erro(c, "legendas em 0x%lX: seção de 0x%lX bytes", (unsigned long)a, (unsigned long)(b - a));
        return;
    }
    n = Conv_Ler32(c, a);
    if (!c->erro && n < minimo) {
        Conv_Erro(c, "legendas em 0x%lX: %lu entradas, o leitor usa %lu", (unsigned long)a, (unsigned long)n,
                  (unsigned long)minimo);
        return;
    }
    Conv_LipPack(c, a, b);
}

/* §3.4: n s32 (as listas de estágios e de músicas do ranking, lidas por índice sorteado); depois, zeros */
static void ubE_lista(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 n, const char *oque) {
    if (!ubE_alinhada(c, a, oque)) {
        return;
    }
    if ((conv_ofs)n * 4 > b - a) {
        Conv_Erro(c, "%s em 0x%lX: 0x%lX bytes, o leitor usa %lu s32", oque, (unsigned long)a, (unsigned long)(b - a),
                  (unsigned long)n);
        return;
    }
    Conv_U32s(c, a, n);
    Conv_Zeros(c, a, b, oque);
}

/* §3.1: NRankRule (include/menu/menu_n.h:131), n registros de 0x1C com sete s32 (announcer, unk4, timeLimit, stage,
   bgm, unk14, foe); depois, zeros. Devolve 1 + o maior foe: os registros de lutador que o leitor alcança
   (UbRank_SetFoe, menu_n_c.c:91 e :95) */
static conv_u32 ubE_regras(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 n, const char *oque) {
    conv_u32 i, foe, alcance = 0;
    conv_ofs r;
    if (!ubE_alinhada(c, a, oque)) {
        return 0;
    }
    if ((conv_ofs)n * UBE_REGRA > b - a) {
        Conv_Erro(c, "%s em 0x%lX: 0x%lX bytes, o leitor usa %lu registros de 0x1C", oque, (unsigned long)a,
                  (unsigned long)(b - a), (unsigned long)n);
        return 0;
    }
    for (i = 0; i < n && !c->erro; i++) {
        r = a + (conv_ofs)i * UBE_REGRA;
        Conv_U32s(c, r, 6);
        foe = Conv_U32(c, r + 0x18);
        if (!c->erro && foe > 0xFFFF) {
            Conv_Erro(c, "%s: regra %lu com o lutador %lu", oque, (unsigned long)i, (unsigned long)foe);
        } else if (foe >= alcance) {
            alcance = foe + 1;
        }
    }
    Conv_Zeros(c, a, b, oque);
    return alcance;
}

/* §3.2: NFoe (include/menu/menu_n.h:111), n registros de 0x2C com onze s32 (chara, color, cpuLevel, lastItem, item[7]);
   depois, zeros. chara de 0 a 0xA0 (o leitor a usa como a ficha chara + 1 do subpacote de 165, como a linha chara dos
   nomes e no BattleSetup_SetMember), ou 999 (N_NONE) onde `nenhum` deixa (os cursos) */
static void ubE_lutadores(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 n, int nenhum, const char *oque) {
    conv_u32 i, chara;
    conv_ofs r;
    if (!ubE_alinhada(c, a, oque)) {
        return;
    }
    if ((conv_ofs)n * UBE_LUTADOR > b - a) {
        Conv_Erro(c, "%s em 0x%lX: 0x%lX bytes, o leitor alcança %lu registros de 0x2C", oque, (unsigned long)a,
                  (unsigned long)(b - a), (unsigned long)n);
        return;
    }
    for (i = 0; i < n && !c->erro; i++) {
        r = a + (conv_ofs)i * UBE_LUTADOR;
        chara = Conv_U32(c, r);
        Conv_U32s(c, r + 4, 10);
        if (!c->erro && chara > UBE_PERSONAGEM_MAX && !(nenhum && chara == UBE_NENHUM)) {
            Conv_Erro(c, "%s: lutador %lu com o personagem %lu", oque, (unsigned long)i, (unsigned long)chara);
        }
    }
    Conv_Zeros(c, a, b, oque);
}

/* §4.1: NCourse (include/menu/menu_n.h:120), 5 registros de 0x38 com quatorze s32 (announcer, unk4, timeLimit, stage,
   bgm, unk14, foe[8]); depois, zeros. UbzSel_Draw lê foe[course.foe[i]] nos oito i (menu_n_b.c:218), mesmo depois de
   um N_NONE: devolve 1 + o maior foe, e em primeiro[k] o foe[0] do curso k */
static conv_u32 ubE_cursos(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 *primeiro) {
    conv_u32 k, j, foe, alcance = 0;
    conv_ofs r;
    if (!ubE_alinhada(c, a, "cursos")) {
        return 0;
    }
    if ((conv_ofs)UBE_CURSOS * UBE_CURSO > b - a) {
        Conv_Erro(c, "cursos em 0x%lX: 0x%lX bytes, o leitor usa 5 registros de 0x38", (unsigned long)a,
                  (unsigned long)(b - a));
        return 0;
    }
    for (k = 0; k < UBE_CURSOS && !c->erro; k++) {
        r = a + (conv_ofs)k * UBE_CURSO;
        Conv_U32s(c, r, 6);
        for (j = 0; j < 8 && !c->erro; j++) {
            foe = Conv_U32(c, r + 0x18 + (conv_ofs)j * 4);
            if (!c->erro && foe > 0xFFFF) {
                Conv_Erro(c, "cursos: curso %lu com o lutador %lu", (unsigned long)k, (unsigned long)foe);
            } else if (foe >= alcance) {
                alcance = foe + 1;
            }
            if (j == 0) {
                primeiro[k] = foe;
            }
        }
    }
    Conv_Zeros(c, a, b, "cursos");
    return alcance;
}

/* §3: o ranking do Disc Fusion (UbRank_Init, src/menu/menu_n_c.c:133; 0x1DC, o conteúdo inteiro), N = 18. 1, 15 e 16
   os filmes; 2 os ícones da janela de ícone (registros 0, 1), 3 TexFile (0..29), 4 o fundo (Sprite_DrawPicture, 8
   registros), 5 TexFile (0, 1, 3), 6 as 165 fichas de personagem, 7 a janela de ícone, 8 as regras dos 99 lugares, 9
   as dos 37 intrusos, 10 e 11 os lutadores de cada uma, 12 os 28 estágios sorteados, 13 as 11 músicas sorteadas, 14 os
   nomes (linha = personagem), 17 TexFile (0..10), 18 as legendas do guia (falas 0x35..0x51) */
void Conv_MenuUbRanking(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[18] = {0, 0, 2, 30, 8, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 11};
    conv_u32 ent[18 + 2], i, n;
    if (!Conv_Secoes(c, base, tam, ent, 18, "ranking do Disc Fusion")) {
        return;
    }
    for (i = 1; i <= 18 && !c->erro; i++) {
        switch (i) {
        case 1: case 15: case 16: Conv_Flash(c, ENT(i)); break;
        case 6: Conv_FichasPersonagem(c, ENT(6)); break;
        case 7: Conv_JanelaIcone(c, ENT(7)); break;
        case 8: case 9:
            /* as regras e, com o alcance delas, os lutadores (seção i + 2) */
            n = ubE_regras(c, ENT(i), i == 8 ? UBE_LUGARES : UBE_INTRUSOS, i == 8 ? "regras dos lugares" :
                           "regras dos intrusos");
            Conv_Onde(c, "seção %lu do ranking do Disc Fusion", (unsigned long)i);
            if (!c->erro) {
                ubE_lutadores(c, ENT(i + 2), n, 0, i == 8 ? "lutadores dos lugares" : "lutadores dos intrusos");
                Conv_Onde(c, "seção %lu do ranking do Disc Fusion", (unsigned long)(i + 2));
            }
            continue;
        case 10: case 11: continue; /* com a 8 e a 9 */
        case 12: ubE_lista(c, ENT(12), UBE_ESTAGIOS, "estágios"); break;
        case 13: ubE_lista(c, ENT(13), UBE_MUSICAS, "músicas"); break;
        case 14: ubE_textos(c, ENT(14), UBE_PERSONAGEM_MAX + 1); break;
        case 18: ubE_legendas(c, ENT(18), 2 * 0x51 + 2); break;
        default: Conv_TexFileMin(c, ENT(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu do ranking do Disc Fusion", (unsigned long)i);
    }
}

/* §4: os cursos do Disc Fusion (UbzSel_Init, src/menu/menu_n_b.c:91; 0x1DD, o conteúdo inteiro), N = 11. 1 o filme;
   2 os ícones da janela de ícone (0, 1), 3 TexFile (0..24), 4 o fundo (8), 5 TexFile (0, 1, 3), 6 as 165 fichas (a 165
   é a do "sem lutador"), 7 a janela de ícone, 8 os 5 cursos, 9 os lutadores, 10 os nomes dos cursos (linhas
   0x143..0x147), 11 sem leitor (o UbzSel toca as falas sem legenda): legendas pela forma, iguais às do ranking */
void Conv_MenuUbCursos(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[6] = {0, 0, 2, 25, 8, 4};
    conv_u32 ent[11 + 2], primeiro[UBE_CURSOS], i, k, n;
    if (!Conv_Secoes(c, base, tam, ent, 11, "cursos do Disc Fusion")) {
        return;
    }
    for (i = 1; i <= 11 && !c->erro; i++) {
        switch (i) {
        case 1: Conv_Flash(c, ENT(1)); break;
        case 6: Conv_FichasPersonagem(c, ENT(6)); break;
        case 7: Conv_JanelaIcone(c, ENT(7)); break;
        case 8:
            n = ubE_cursos(c, ENT(8), primeiro);
            /* cada curso começa com um lutador: UbzSel_SetupBattle põe foe[course.foe[0]] no lado 1 sem olhar o
               N_NONE (menu_n_b.c:63); o lutador ainda não foi trocado, então se lê como está no disco */
            for (k = 0; k < UBE_CURSOS && !c->erro; k++) {
                if ((conv_ofs)(primeiro[k] + 1) * UBE_LUTADOR <= ent[10] - ent[9] &&
                    Conv_Ler32(c, base + ent[9] + (conv_ofs)primeiro[k] * UBE_LUTADOR) == UBE_NENHUM) {
                    Conv_Erro(c, "cursos: o curso %lu começa sem lutador", (unsigned long)k);
                }
            }
            Conv_Onde(c, "seção 8 dos cursos do Disc Fusion", 0);
            if (!c->erro) {
                ubE_lutadores(c, ENT(9), n, 1, "lutadores dos cursos");
                Conv_Onde(c, "seção 9 dos cursos do Disc Fusion", 0);
            }
            continue;
        case 9: continue; /* com a 8 */
        case 10: ubE_textos(c, ENT(10), 0x147 + 1); break;
        case 11:
            if (ent[11] != ent[12]) {
                Conv_LipPack(c, ENT(11));
            }
            break;
        default: Conv_TexFileMin(c, ENT(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu dos cursos do Disc Fusion", (unsigned long)i);
    }
}
