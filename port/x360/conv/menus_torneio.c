/* Torneio: docs/x360/formatos/menus-torneio.md. Um formato cobre todas as variantes de cada pacote. */
#include "conv.h"
#include "conv_menus.h"
#include "conv_pecas.h"

#define SEC(n) (base + ent[n]), (base + ent[(n) + 1])

/* TourInfo e LTourPrize: 25 s32 por registro, pelo menos os cinco torneios; depois, zeros. */
static void tour_premios(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_ofs r;
    if (c->erro) {
        return;
    }
    if (b - a < 5 * 0x64) {
        Conv_Erro(c, "prêmios: 0x%lX bytes, o leitor usa cinco registros de 0x64", (unsigned long)(b - a));
        return;
    }
    for (r = a; r + 0x64 <= b && !c->erro; r += 0x64) {
        Conv_U32s(c, r, 25);
    }
    Conv_Zeros(c, a, b, "depois dos prêmios");
}

/* LTourCpu: três níveis e cinco rodadas, cada registro tem s32 cpuLevel e oito u16 de LItemSet. */
static void tour_cpu(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 i;
    if (c->erro) {
        return;
    }
    if (b - a < 15 * 0x14) {
        Conv_Erro(c, "CPU: 0x%lX bytes, o leitor usa quinze registros de 0x14", (unsigned long)(b - a));
        return;
    }
    for (i = 0; i < 15 && !c->erro; i++) {
        Conv_U32(c, a + i * 0x14);
        Conv_U16s(c, a + i * 0x14 + 4, 8);
    }
    Conv_Zeros(c, a, b, "depois da CPU");
}

void Conv_MenuTourMenu(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[28] = {0, 8, 4, 4, 3, 2, 1, 1, 3, 1, 1, 1, 3, 2, 1, 0, 2, 2, 0, 0, 0, 0, 2, 0, 0, 3, 2, 0};
    conv_u32 ent[29], i;
    if (!Conv_Secoes(c, base, tam, ent, 27, "menu do Torneio")) {
        return;
    }
    for (i = 1; i <= 27 && !c->erro; i++) {
        switch (i) {
        case 15: case 18: Conv_Flash(c, SEC(i)); break;
        case 19: Conv_TextosMin(c, SEC(i), 34); break;
        case 20: Conv_JanelaMsg(c, SEC(i)); break;
        case 21: Conv_JanelaIcone(c, SEC(i)); break;
        case 23: Conv_LegendasMin(c, SEC(i), 34); break;
        case 24: Conv_RestoBytes(c, SEC(i)); break; /* sem leitor */
        case 27: tour_premios(c, SEC(i)); break;
        default: Conv_TexFileMin(c, SEC(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu do menu do Torneio", (unsigned long)i);
    }
}

void Conv_MenuEntrySel(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[29] = {0, 1, 3, 0, 1, 3, 2, 5, 1, 2, 0, 2, 2, 2, 2, 2, 0, 2, 0, 4, 0, 4, 0, 2, 0, 0, 4, 0, 4};
    conv_u32 ent[41], i;
    if (!Conv_Secoes(c, base, tam, ent, 39, "seleção do Torneio")) {
        return;
    }
    for (i = 1; i <= 39 && !c->erro; i++) {
        switch (i) {
        case 3: case 10: case 16: case 18: case 20: case 22: case 24: case 25:
            Conv_Flash(c, SEC(i)); break;
        case 27: case 37: case 38: Conv_RestoBytes(c, SEC(i)); break; /* sem leitor */
        case 29: Conv_Celulas(c, SEC(i)); break;
        case 30: case 31: Conv_TextosMin(c, SEC(i), 161); break;
        case 32: Conv_FichasPersonagem(c, SEC(i)); break;
        case 33: Conv_TextosMin(c, SEC(i), 205); break;
        case 34: Conv_JanelaMsg(c, SEC(i)); break;
        case 35: Conv_ItemHelp(c, SEC(i)); break;
        case 36: Conv_LegendasMin(c, SEC(i), 205); break;
        case 39: Conv_ItemPanel(c, SEC(i)); break;
        default: Conv_TexFileMin(c, SEC(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu da seleção do Torneio", (unsigned long)i);
    }
}

void Conv_MenuTourBg(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_u32 ent[5], n;
    if (!Conv_Secoes(c, base, tam, ent, 3, "fundo do Torneio")) {
        return;
    }
    Conv_TexFileMin(c, SEC(1), 8);
    Conv_Onde(c, "seção 1 do fundo do Torneio", 0);
    if (ent[3] - ent[2] < 4) {
        Conv_Erro(c, "seção 2 do fundo do Torneio sem contagem de texturas");
        return;
    }
    n = Conv_Ler32(c, base + ent[2]);
    if (!c->erro && n != 3 && n != 7) {
        Conv_Erro(c, "seção 2 do fundo do Torneio: %lu texturas (esperadas 3 ou 7)", (unsigned long)n);
        return;
    }
    Conv_TexFileMin(c, SEC(2), n);
    Conv_Onde(c, "seção 2 do fundo do Torneio", 0);
    Conv_Flash(c, SEC(3));
    Conv_Onde(c, "seção 3 do fundo do Torneio", 0);
}

void Conv_MenuBracket(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[46] = {0, 1, 6, 4, 0, 1, 2, 1, 2, 2, 0, 2, 1, 0, 0, 2, 0, 1, 4, 2, 1, 0, 0,
                                      0, 0, 0, 0, 2, 9, 0, 4, 0, 1, 0, 4, 2, 0, 1, 0, 0, 0, 0, 0, 0, 4, 4};
    conv_u32 ent[47], i, n;
    n = Conv_Ler32(c, base);
    if (!c->erro && n != 43 && n != 44 && n != 45) {
        Conv_Erro(c, "chave do Torneio: %lu seções (esperadas 43, 44 ou 45)", (unsigned long)n);
        return;
    }
    if (!Conv_Secoes(c, base, tam, ent, n, "chave do Torneio")) {
        return;
    }
    for (i = 1; i <= n && !c->erro; i++) {
        switch (i) {
        case 4: case 10: case 13: case 14: case 16: case 29: Conv_Flash(c, SEC(i)); break;
        case 21: case 22: Conv_TextosMin(c, SEC(i), 161); break;
        case 23: Conv_FichasPersonagem(c, SEC(i)); break;
        case 24: Conv_Celulas(c, SEC(i)); break;
        case 25: Conv_TextosMin(c, SEC(i), 246); break;
        case 26: Conv_JanelaMsg(c, SEC(i)); break;
        case 31: tour_cpu(c, SEC(i)); break;
        case 33: case 38: case 40: Conv_RestoBytes(c, SEC(i)); break; /* sem leitor */
        case 36: Conv_Dialogo(c, SEC(i)); break;
        case 39: Conv_LegendasMin(c, SEC(i), 246); break;
        case 41: tour_premios(c, SEC(i)); break;
        case 42: Conv_GetWin(c, SEC(i)); break;
        case 43: Conv_TexFileMin(c, SEC(i), n == 43 ? 2 : 4); break;
        default: Conv_TexFileMin(c, SEC(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu da chave do Torneio", (unsigned long)i);
    }
}
