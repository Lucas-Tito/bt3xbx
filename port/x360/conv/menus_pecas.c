/*
 * Peças comuns dos pacotes de tela dos menus: as janelas de mensagem e de ícone, as legendas com o movimento da boca
 * e as das telas de seleção, do Treino e das Opções (ItemPanel, ItemHelp, ListaIds, Células, SubpacoteTex).
 * Especificação: docs/x360/formatos/menus-pecas.md (as seções citadas abaixo). Cada peça é uma função que converte
 * [a, b), para os conversores de pacote (menus.c) a reaproveitarem; conv_tool --pecas valida cada uma no disco.
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

/* ================================================================== as peças das telas de seleção (§5) */

/* teto da grade de personagens: ChrGrid_Build completa até múltiplo de 7 num buffer de 165 células (menus-pecas.md 5.4.4) */
#define CEL_BUFFER 165
#define CEL_TAM 0x24
#define CEL_FORMAS 7

/* ------------------------------------------------------------------ auxiliares */

/* o início de uma lista lida por MPACK_AT tem de ser múltiplo de 4 (a macro arredonda para baixo; o buffer
   descomprimido começa alinhado) */
static int alinhada(ConvCtx *c, conv_ofs a, const char *oque) {
    if (!c->erro && a % 4 != 0) {
        Conv_Erro(c, "%s em 0x%lX: início fora de múltiplo de 4 (MPACK_AT arredonda)", oque, (unsigned long)a);
    }
    return !c->erro;
}

/* TexFile com pelo menos `minimo` registros (o maior índice de MTEX + 1), conferido antes de converter: o código não
   confere, mas um índice além da contagem daria ao filme um registro não relocado */
void Conv_TexFileMin(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 minimo) {
    conv_u32 n;
    if (c->erro) {
        return;
    }
    if (b - a < 0x14) {
        Conv_Erro(c, "texturas em 0x%lX: seção de 0x%lX bytes", (unsigned long)a, (unsigned long)(b - a));
        return;
    }
    n = Conv_Ler32(c, a);
    if (!c->erro && n < minimo) {
        Conv_Erro(c, "texturas em 0x%lX: %lu registros, o leitor usa %lu", (unsigned long)a, (unsigned long)n,
                  (unsigned long)minimo);
        return;
    }
    Conv_TexFile(c, a, b, 0);
}

/* nenhuma seção de 1 a n vazia (todas têm leitor; uma vazia faria o leitor pegar a seguinte) */
static int cheias(ConvCtx *c, const conv_u32 *ent, conv_u32 n, const char *oque) {
    conv_u32 i;
    for (i = 1; i <= n && !c->erro; i++) {
        if (ent[i] == ent[i + 1]) {
            Conv_Erro(c, "%s: seção %lu vazia (tem leitor)", oque, (unsigned long)i);
        }
    }
    return !c->erro;
}

/* ------------------------------------------------------------------ ItemPanel (menus-pecas.md 5.1) */

/* ItemPanel_Init (src/menu/menu_g.c:39) e ItemPanel_Draw (:103): N = 16 (hipótese: o código lê 1..16 e nunca a 17);
   1, 2, 3, 6..12 e 16 TexFile, 4 e 5 Flash (um por lado), 13 e 14 textos de personagem, 15 textos de item */
void Conv_ItemPanel(ConvCtx *c, conv_ofs a, conv_ofs b) {
    /* registros mínimos por seção TexFile: 1 + o maior índice de MTEX (menu_g.c:47..:76) */
    static const conv_u32 minimo[17] = {0, 4, 2, 2, 0, 0, 4, 1, 4, 1, 5, 1, 2, 0, 0, 0, 1};
    conv_u32 ent[16 + 2], i;
    if (!Conv_Secoes(c, a, b - a, ent, 16, "ItemPanel") || !cheias(c, ent, 16, "ItemPanel")) {
        return;
    }
    for (i = 1; i <= 16 && !c->erro; i++) {
        switch (i) {
        case 4: /* filme do lado 0 (:77) */
            Conv_Flash(c, SEC(4));
            break;
        case 5:
            /* filme do lado 1 (:77 com side = 1): só o CharSel e o TeamSel o leem; no EntrySel, SoloSel e UbTeamSel
               fica sem leitor; nos cinco pacotes do disco ela é filme (conv_tool --pecas), então Flash sempre */
            Conv_Flash(c, SEC(5));
            break;
        case 13: /* nomes de personagem, linha = id 0..0xA0 (:134) */
        case 14: /* nomes de forma (:136) */
            Conv_Textos(c, SEC(i));
            break;
        case 15: /* nomes de item, linha = id - 1 (:206): uma entrada por item */
            Conv_Textos(c, SEC(15));
            break;
        default:
            Conv_TexFileMin(c, SEC(i), minimo[i]);
            break;
        }
        Conv_Onde(c, "seção %lu do ItemPanel", (unsigned long)i);
    }
}

/* ------------------------------------------------------------------ ItemHelp (menus-pecas.md 5.2) */

/* ItemHelp_Init (src/menu/menu_v_c.c:14) e ItemHelp_Draw (:64): N = 6 (hipótese); 1 TexFile (registros até 9),
   2 Flash, 3 TexFile (até 1), 5 descrições, 6 nomes (uma entrada por item); 4 sem leitor */
void Conv_ItemHelp(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 ent[6 + 2];
    if (!Conv_Secoes(c, a, b - a, ent, 6, "ItemHelp")) {
        return;
    }
    if (ent[1] == ent[2] || ent[2] == ent[3] || ent[3] == ent[4] || ent[5] == ent[6] || ent[6] == ent[7]) {
        Conv_Erro(c, "ItemHelp: seção lida vazia");
        return;
    }
    Conv_TexFileMin(c, SEC(1), 10); /* MTEX 0, 4, 8, 9 (:23..:26) */
    Conv_Onde(c, "seção 1 do ItemHelp", 0);
    Conv_Flash(c, SEC(2));
    Conv_Onde(c, "seção 2 do ItemHelp", 0);
    Conv_TexFileMin(c, SEC(3), 2); /* MTEX 0, 1 (:30..:31) */
    Conv_Onde(c, "seção 3 do ItemHelp", 0);
    /* seção 4: nenhum leitor em nenhum dos nove chamadores (menus-pecas.md 5.2.3), e o rastreio dos roteiros não a vê
       ler. Não tem a forma de nenhuma peça conhecida (nem filme, nem TexFile, nem tabela): fica como está, em bytes,
       o que só vale porque o jogo não a lê */
    if (!c->erro && ent[4] != ent[5]) {
        Conv_RestoBytes(c, SEC(4));
        Conv_Onde(c, "seção 4 do ItemHelp", 0);
    }
    Conv_Textos(c, SEC(5)); /* descrições, linha = item (:78) */
    Conv_Onde(c, "seção 5 do ItemHelp", 0);
    Conv_Textos(c, SEC(6)); /* nomes, linha = item (:76) */
    Conv_Onde(c, "seção 6 do ItemHelp", 0);
}

/* ------------------------------------------------------------------ ListaIds (menus-pecas.md 5.3) */

/* contagem s32 em +0 (MPACK_WORD, TS_PACK_WORD, res[res[n] >> 2], *(u32 *)), +4..+0xF sem leitor, ids s32 a partir de
   +0x10; depois, zeros. `minimo`: contagem mínima que o leitor exige; `teto`: maior id aceito (0x7FFFFFFF = sem teto).
   O jogo GRAVA ids nesta lista depois da conversão (StgGrid_ApplyUnlocks, BgmList_ApplyUnlocks, bgmIds[n - 5] = 0x18),
   já em ordem nativa: nada a fazer aqui. */
void Conv_ListaIdsFaixa(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 minimo, conv_u32 teto) {
    conv_u32 n, i, id;
    if (!alinhada(c, a, "lista de ids")) {
        return;
    }
    if (b - a < 0x10) {
        Conv_Erro(c, "lista de ids em 0x%lX: seção de 0x%lX bytes", (unsigned long)a, (unsigned long)(b - a));
        return;
    }
    n = Conv_Ler32(c, a);
    if (!c->erro && (n > 0x7FFFFFFFu || (conv_ofs)n > (b - a - 0x10) / 4 || n < minimo)) {
        Conv_Erro(c, "lista de ids em 0x%lX: %lu ids em 0x%lX bytes (mínimo %lu)", (unsigned long)a, (unsigned long)n,
                  (unsigned long)(b - a), (unsigned long)minimo);
        return;
    }
    Conv_U32(c, a);
    /* +4..+0xF: ninguém lê (todos os leitores vão de +0 direto a +0x10); zero em todas as listas do disco */
    Conv_Zeros(c, a + 4, a + 0x10, "cabeçalho da lista de ids");
    for (i = 0; i < n && !c->erro; i++) {
        id = Conv_U32(c, a + 0x10 + (conv_ofs)i * 4);
        if (id > teto) {
            Conv_Erro(c, "lista de ids em 0x%lX: id %lu = 0x%lX acima de 0x%lX", (unsigned long)a, (unsigned long)i,
                      (unsigned long)id, (unsigned long)teto);
        }
    }
    /* depois da lista até o fim da seção: zeros (hipótese). Atenção à grade de estágios, que lê 36 ids sem olhar a
       contagem: quem chama para a seção 39 da seleção passa minimo = 36 */
    Conv_Zeros(c, a, b, "depois da lista de ids");
}

/* a peça sem os limites de cada uso (menus-pecas.md 5.3.4 dá os de cada seção):
     seleção 39 (estágios): Conv_ListaIdsFaixa(c, SEC(39), 36, 0x25)
     seleção 56 (músicas):  Conv_ListaIdsFaixa(c, SEC(56), 5, 0x19)
     Opções 37 (músicas):   Conv_ListaIdsFaixa(c, SEC(37), 11, 0x19)
     Treino 24 (páginas):   Conv_ListaIdsFaixa(c, SEC(24), 0, 397)
     Treino 26:             Conv_ListaIds */
void Conv_ListaIds(ConvCtx *c, conv_ofs a, conv_ofs b) {
    Conv_ListaIdsFaixa(c, a, b, 0, 0x7FFFFFFFu);
}

/* ------------------------------------------------------------------ Células (menus-pecas.md 5.4) */

/* lista mestra da grade de personagens (ChrGrid_Build, src/battle/view_a_e.c:776; DcPass_IsListed, menu_z_d.c:435):
   contagem s32 em +0, +4..+0xF sem leitor (s32 unk4[3] nas structs), células de 0x24 a partir de +0x10 (s32 id, s32
   formCount, s32 form[7]: a célula inteira é copiada como struct, então as 9 palavras são u32); depois, zeros */
void Conv_Celulas(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 n, i, k, id, nf, f;
    conv_ofs e;
    if (!alinhada(c, a, "células")) {
        return;
    }
    if (b - a < 0x10) {
        Conv_Erro(c, "células em 0x%lX: seção de 0x%lX bytes", (unsigned long)a, (unsigned long)(b - a));
        return;
    }
    n = Conv_Ler32(c, a);
    if (!c->erro && ((conv_ofs)n > (b - a - 0x10) / CEL_TAM || (n + CEL_FORMAS - 1) / CEL_FORMAS * CEL_FORMAS > CEL_BUFFER)) {
        Conv_Erro(c, "células em 0x%lX: %lu células em 0x%lX bytes (a grade cabe em %d)", (unsigned long)a,
                  (unsigned long)n, (unsigned long)(b - a), CEL_BUFFER);
        return;
    }
    Conv_U32(c, a);
    /* +4..+0xF: ninguém lê; zero em todas as listas de células do disco */
    Conv_Zeros(c, a + 4, a + 0x10, "cabeçalho das células");
    for (i = 0; i < n && !c->erro; i++) {
        e = a + 0x10 + (conv_ofs)i * CEL_TAM;
        id = Conv_U32(c, e);
        nf = Conv_U32(c, e + 4);
        if (nf > CEL_FORMAS) {
            Conv_Erro(c, "célula %lu em 0x%lX: %lu formas (máximo 7)", (unsigned long)i, (unsigned long)e, (unsigned long)nf);
            break;
        }
        if (nf == 0 && id > 0xA4) { /* personagem 0..0xA0 ou marcador 0xA1..0xA4; id + 1 indexa as 165 fichas */
            Conv_Erro(c, "célula %lu em 0x%lX: id 0x%lX", (unsigned long)i, (unsigned long)e, (unsigned long)id);
            break;
        }
        for (k = 0; k < CEL_FORMAS && !c->erro; k++) {
            f = Conv_U32(c, e + 8 + (conv_ofs)k * 4);
            if (k < nf && f > 0xA0) { /* hipótese: forma é sempre personagem */
                Conv_Erro(c, "célula %lu em 0x%lX: forma %lu = 0x%lX", (unsigned long)i, (unsigned long)e,
                          (unsigned long)k, (unsigned long)f);
            }
        }
    }
    Conv_Zeros(c, a, b, "depois das células"); /* hipótese: nada depois da última célula */
}

/* ------------------------------------------------------------------ SubpacoteTex (menus-pecas.md 5.5) */

/* tabela de seções (menus.md 2.1) em que cada subseção é um TexFile com base no início da subseção (o laço de
   Res_RelocateOffsets(&res, res, res) sobre MPACK_AT(sub, i + 1)); os leitores por id usam o registro 0.
   `esperado`: N exato (165 fichas, 38 ícones de estágio; hipótese, o código nunca lê a entrada N + 1); 0 = qualquer */
void Conv_SubpacoteTexN(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 esperado) {
    static conv_u32 ent[0x100 + 2];
    conv_u32 n, i;
    if (!alinhada(c, a, "subpacote de texturas")) {
        return;
    }
    n = Conv_Tabela(c, a, b - a, ent, 0x100);
    if (!c->erro && esperado != 0 && n != esperado) {
        Conv_Erro(c, "subpacote de texturas com %lu subseções (o laço reloca %lu)", (unsigned long)n,
                  (unsigned long)esperado);
    }
    for (i = 1; i <= n && !c->erro; i++) {
        /* subseção vazia: o código não impede (relocaria a seguinte de novo), mas nenhum subpacote do disco tem
           (conv_tool --pecas): recusada, para um caso novo vir com nome */
        if (ent[i] == ent[i + 1]) {
            Conv_Erro(c, "subpacote de texturas: subseção %lu vazia", (unsigned long)i);
            break;
        }
        if (ent[i] % 4 != 0) { /* MPACK_AT arredonda o deslocamento */
            Conv_Erro(c, "subpacote de texturas: subseção %lu em 0x%lX", (unsigned long)i, (unsigned long)ent[i]);
            break;
        }
        Conv_TexFileMin(c, SEC(i), 1);
        Conv_Onde(c, "subseção %lu", (unsigned long)i);
    }
}

void Conv_SubpacoteTex(ConvCtx *c, conv_ofs a, conv_ofs b) {
    Conv_SubpacoteTexN(c, a, b, 0);
}

/* os usos com os limites de cada leitor (menus-pecas.md 5.3.4 e 5.5.3) */
void Conv_ListaEstagios(ConvCtx *c, conv_ofs a, conv_ofs b) { /* seleção 39: a grade lê 36 ids sem olhar a contagem */
    Conv_ListaIdsFaixa(c, a, b, 36, 0x25);
}
void Conv_ListaMusicas(ConvCtx *c, conv_ofs a, conv_ofs b) { /* seleção 56: o jogo grava em ids[n - 5] */
    Conv_ListaIdsFaixa(c, a, b, 5, 0x19);
}
void Conv_ListaMusicasOpcoes(ConvCtx *c, conv_ofs a, conv_ofs b) { /* Opções 37: seis linhas sobre n - 5 */
    Conv_ListaIdsFaixa(c, a, b, 11, 0x19);
}
void Conv_FichasPersonagem(ConvCtx *c, conv_ofs a, conv_ofs b) { /* 165 fichas: id 0..0xA4, MPACK_AT(sub, id + 1) */
    Conv_SubpacoteTexN(c, a, b, 165);
}
void Conv_IconesEstagio(ConvCtx *c, conv_ofs a, conv_ofs b) { /* 36 estágios, travado e vazio */
    Conv_SubpacoteTexN(c, a, b, 38);
}
