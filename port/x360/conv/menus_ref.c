/* Referência e Shenlong: docs/x360/formatos/menus-ref.md. */
#include "conv.h"
#include "conv_menus.h"
#include "conv_pecas.h"

#define SEC(n) (base + ent[n]), (base + ent[(n) + 1])

/* Tabelas s32 com o número de entradas alcançado pelo leitor, seguidas de enchimento zero. */
static void ref_tabela(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 n) {
    if (c->erro) {
        return;
    }
    if (b - a < (conv_ofs)n * 4) {
        Conv_Erro(c, "tabela: %lu bytes, o leitor usa %lu s32", (unsigned long)(b - a), (unsigned long)n);
        return;
    }
    Conv_U32s(c, a, n);
    Conv_Zeros(c, a, b, "depois da tabela");
}

void Conv_MenuCharRef(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[29] = {0, 0, 1, 2, 2, 4, 0, 1, 2, 6, 1, 3, 9, 4, 1, 8, 6};
    conv_u32 ent[30], i, linha, num, poses = 0;
    if (!Conv_Secoes(c, base, tam, ent, 28, "Referência")) {
        return;
    }
    /* Lê as duas tabelas antes de trocá-las: a última fala e pose dependem dos 161 personagens. */
    if (ent[27] - ent[26] < 161 * 4 || ent[29] - ent[28] < 161 * 4) {
        Conv_Erro(c, "Referência: tabelas de comentários menores que 161 s32");
        return;
    }
    for (i = 0; i < 161 && !c->erro; i++) {
        linha = Conv_Ler32(c, base + ent[26] + i * 4);
        num = Conv_Ler32(c, base + ent[28] + i * 4);
        if (linha > 0x7ffffff6 || num > 0x7fffffff - 9 - linha) {
            Conv_Erro(c, "Referência: intervalo de comentário inválido no personagem %lu", (unsigned long)i);
            return;
        }
        if (linha + num > poses) {
            poses = linha + num;
        }
    }
    for (i = 1; i <= 28 && !c->erro; i++) {
        switch (i) {
        case 1: Conv_Flash(c, SEC(i)); break;
        case 6: Conv_RestoBytes(c, SEC(i)); break; /* sem leitor */
        case 17: case 26: case 28: ref_tabela(c, SEC(i), 161); break;
        case 27: ref_tabela(c, SEC(i), poses); break;
        case 18: Conv_JanelaMsg(c, SEC(i)); break;
        case 19: Conv_JanelaIcone(c, SEC(i)); break;
        case 20: Conv_LegendasMin(c, SEC(i), 9 + poses); break;
        case 21: Conv_TextosMin(c, SEC(i), 9 + poses); break;
        case 22: case 23: Conv_TextosMin(c, SEC(i), 161); break;
        case 24: Conv_TexFileMin(c, SEC(i), 1); break;
        case 25: Conv_TextosMin(c, SEC(i), 322); break;
        default: Conv_TexFileMin(c, SEC(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu da Referência", (unsigned long)i);
    }
}

/* Shen_BuildList: 4 + 7 + 4 registros de oito bytes. Os leitores usam id s32 e kind u32. */
static void shen_desejos(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 i, id, kind;
    if (c->erro) {
        return;
    }
    if (b - a < 0x78) {
        Conv_Erro(c, "desejos: menos de quinze registros de oito bytes");
        return;
    }
    for (i = 0; i < 15 && !c->erro; i++) {
        id = Conv_Ler32(c, a + i * 8);
        kind = Conv_Ler32(c, a + i * 8 + 4);
        if (kind > 3 || id == 0 || id > (kind == 0 ? 350 : kind == 1 ? 35 : kind == 2 ? 161 : 0x7fffffff)) {
            Conv_Erro(c, "desejo %lu: id ou tipo fora da faixa do leitor", (unsigned long)i);
            return;
        }
    }
    Conv_U32s(c, a, 30);
    Conv_Zeros(c, a, b, "depois dos desejos");
}

static void shen_confirmacao(ConvCtx *c, conv_ofs base, conv_ofs fim) {
    conv_u32 ent[6];
    if (!Conv_Secoes(c, base, fim - base, ent, 4, "confirmação do Shenlong")) {
        return;
    }
    Conv_Flash(c, SEC(1));
    Conv_TexFileMin(c, SEC(2), 4);
    Conv_TexFileMin(c, SEC(3), 2);
    Conv_ItemHelp(c, SEC(4));
}

void Conv_MenuShenlong(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_u32 ent[15], i;
    if (!Conv_Secoes(c, base, tam, ent, 13, "Shenlong")) {
        return;
    }
    for (i = 1; i <= 13 && !c->erro; i++) {
        switch (i) {
        case 1: Conv_TextosMin(c, SEC(i), 16); break;
        case 2: shen_desejos(c, SEC(i)); break;
        case 3: Conv_Flash(c, SEC(i)); break;
        case 4: Conv_TexFileMin(c, SEC(i), 7); break;
        case 5: Conv_TexFileMin(c, SEC(i), 2); break;
        case 6: shen_confirmacao(c, SEC(i)); break;
        case 7: /* Shen_DrawList só lê ShenItem.type, um byte por registro; o resto não tem leitor. */
            if (ent[8] - ent[7] < 350 * 0x28) {
                Conv_Erro(c, "tabela de itens menor que 350 registros");
            }
            Conv_RestoBytes(c, SEC(i)); break;
        case 8: Conv_JanelaIcone(c, SEC(i)); break;
        case 9: Conv_JanelaMsg(c, SEC(i)); break;
        case 10: Conv_Dialogo(c, SEC(i)); break;
        case 11: Conv_GetWin(c, SEC(i)); break;
        case 12: Conv_RestoBytes(c, SEC(i)); break; /* sem leitor */
        case 13: Conv_TextosMin(c, SEC(i), 15); break;
        }
        Conv_Onde(c, "seção %lu do Shenlong", (unsigned long)i);
    }
}

/* Conteúdos sem chamada no inventário: não se infere tipo pela aparência. Só a extensão é conhecida no disco. */
void Conv_MenuInativo(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    if (tam != 551808 && tam != 99584) {
        Conv_Erro(c, "conteúdo inativo: extensão desconhecida (%lu bytes)", (unsigned long)tam);
        return;
    }
    Conv_Bytes(c, base, tam);
}
