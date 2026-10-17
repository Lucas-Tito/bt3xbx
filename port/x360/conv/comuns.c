/*
 * Arquivos comuns da luta: 00001 (tabelas da luta e IA), 00002 (efeitos comuns), 00003 (personagens e itens),
 * 00004 (fontes, ícones, avisos do controle), 00005/00006 (HUD e menu de pausa, um por idioma).
 * Especificação: docs/x360/formatos/comuns.md (as seções citadas abaixo).
 */
#include "conv.h"
#include "conv_pecas.h"

/* entrada n de uma tabela já lida; b == a quando vazia */
#define ENT(n) (base + ent[n]), (base + ent[(n) + 1])

static void vazia(ConvCtx *c, const conv_u32 *ent, conv_u32 n) {
    if (ent[n] != ent[n + 1]) {
        Conv_Erro(c, "entrada %lu: esperada vazia (sem leitor)", (unsigned long)n);
    }
}

/* ------------------------------------------------------------------ 00001 */

/* §2.6: dados da IA da CPU (cabeçalho com 41 tamanhos, tabela de ações, 8 listas de regras, 32 perfis) */
static void ia(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 tam[41], i;
    conv_ofs p, t;
    if (b - a < 0x148) {
        Conv_Erro(c, "IA em 0x%lX: entrada curta", (unsigned long)a);
        return;
    }
    for (i = 0; i < 41; i++) {
        tam[i] = Conv_U32(c, a + i * 4);
    }
    Conv_U32s(c, a + 0xA4, 41); /* ponteiros gravados na ligação (zero no arquivo) */
    p = a + 0x148;
    /* §2.6.2: tabela de ações */
    if (tam[0] < 0x788 || (tam[0] - 0x788) % 0x14 != 0 || p + tam[0] > b) {
        Conv_Erro(c, "IA em 0x%lX: tabela de ações de 0x%lX bytes", (unsigned long)a, (unsigned long)tam[0]);
        return;
    }
    Conv_U32s(c, p, 2);         /* unk0 (sem leitor, s32 nas visões), steps (ponteiro) */
    Conv_U16s(c, p + 0x008, 0x80); /* first */
    Conv_Bytes(c, p + 0x108, 0x180); /* count, pick, timeout */
    Conv_U16s(c, p + 0x288, 0x80); /* flags */
    Conv_Bytes(c, p + 0x388, 0x400); /* stateClass, stateFlags */
    for (t = p + 0x788; t < p + tam[0] && !c->erro; t += 0x14) {
        Conv_Bytes(c, t, 4);
        Conv_U16s(c, t + 4, 2);
        Conv_U32s(c, t + 8, 3);
    }
    p += tam[0];
    /* §2.6.3: listas de regras */
    for (i = 1; i <= 8 && !c->erro; i++) {
        conv_u32 n, k;
        if (tam[i] < 8 || p + tam[i] > b) {
            Conv_Erro(c, "IA em 0x%lX: lista %lu de 0x%lX bytes", (unsigned long)a, (unsigned long)i, (unsigned long)tam[i]);
            return;
        }
        n = Conv_U16(c, p);
        Conv_Bytes(c, p + 2, 2);
        Conv_U32(c, p + 4);
        if (8 + n * 0x1C > tam[i]) {
            Conv_Erro(c, "IA em 0x%lX: lista %lu com %lu regras em 0x%lX bytes", (unsigned long)a, (unsigned long)i,
                      (unsigned long)n, (unsigned long)tam[i]);
            return;
        }
        for (k = 0; k < n && !c->erro; k++) {
            Conv_U16(c, p + 8 + k * 0x1C);
            Conv_Bytes(c, p + 8 + k * 0x1C + 2, 0x1A);
        }
        Conv_Zeros(c, p, p + tam[i], "lista de regras da IA");
        p += tam[i];
    }
    /* §2.6.4: perfis, só bytes (0x580 no arquivo; a struct da decomp diz 0x584) */
    for (i = 9; i < 41 && !c->erro; i++) {
        if (p + tam[i] > b) {
            Conv_Erro(c, "IA em 0x%lX: perfil %lu passa do fim", (unsigned long)a, (unsigned long)(i - 9));
            return;
        }
        Conv_Bytes(c, p, tam[i]);
        p += tam[i];
    }
    Conv_Zeros(c, a, b, "IA");
}

/* §2.8: vozes, tamanho / 8 registros */
static void vozes(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_ofs r;
    for (r = a; r + 8 <= b && !c->erro; r += 8) {
        Conv_U16s(c, r, 2);
        Conv_U32(c, r + 4);
    }
    Conv_Zeros(c, a, b, "vozes");
}

/* §2.9: golpes por ação de ataque, tamanho / 0x18 registros */
static void golpes_acao(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_ofs r;
    for (r = a; r + 0x18 <= b && !c->erro; r += 0x18) {
        Conv_Bytes(c, r, 6);
        Conv_U16(c, r + 6);
        Conv_U32(c, r + 8);
        Conv_Bytes(c, r + 0x0C, 8);
        Conv_U16(c, r + 0x14);
        Conv_Bytes(c, r + 0x16, 2);
    }
    Conv_Zeros(c, a, b, "golpes por ação");
}

static void comum1(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_u32 ent[0x100 + 2], n, i;
    n = Conv_Tabela(c, base, tam, ent, 0xFF);
    if (!c->erro && n != 213) {
        Conv_Erro(c, "00001 com %lu entradas (esperado 213)", (unsigned long)n);
        return;
    }
    Conv_TexFile(c, ENT(1), 0);
    vazia(c, ent, 2);
    Conv_U32Entrada(c, ENT(3));
    ia(c, ENT(4));
    Conv_Cortes(c, ENT(5));
    vozes(c, ENT(6));
    golpes_acao(c, ENT(7));
    Conv_U32Entrada(c, ENT(8));
    Conv_BytesEntrada(c, ENT(9));
    vazia(c, ent, 10);
    vazia(c, ent, 11);
    Conv_Camera(c, ENT(12));
    Conv_U32Entrada(c, ENT(13));
    for (i = 14; i <= 213 && !c->erro; i++) {
        Conv_Labio(c, ENT(i));
        Conv_Onde(c, "entrada %lu", (unsigned long)i);
    }
}

/* ------------------------------------------------------------------ 00002 */

/* §3.7.5: definição de rastro (EftStreakDef, 0x4A bytes) */
static void rastro(ConvCtx *c, conv_ofs a, conv_ofs b) {
    Conv_U32s(c, a, 0x48 / 4); /* f32 x7, unk1C (sem leitor; f32 pelos dados), f32 x8, endMode, space */
    Conv_Bytes(c, a + 0x48, 2);
    Conv_Zeros(c, a, b, "rastro");
}

/* §3.7.6: parâmetros da cauda do orbe (EftOrbTailParam) */
static void cauda(ConvCtx *c, conv_ofs a, conv_ofs b) {
    Conv_U32s(c, a, 29);
    Conv_Bytes(c, a + 0x74, 8);
    Conv_Zeros(c, a, b, "cauda do orbe");
}

static void texturas(ConvCtx *c, conv_ofs a, conv_ofs b) {
    Conv_TexFile(c, a, b, 0);
}

static void comum2(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static conv_u32 ent[0x300 + 2];
    conv_u32 n, k, i;
    n = Conv_Tabela(c, base, tam, ent, 0x300);
    if (!c->erro && n != 637) {
        Conv_Erro(c, "00002 com %lu entradas (esperado 637)", (unsigned long)n);
        return;
    }
    /* §3.1, §3.7: as entradas diretas dos gerenciadores de efeito */
    Conv_U32Entrada(c, ENT(1)); /* parâmetros da aura: tudo palavra de 4 bytes */
    texturas(c, ENT(2));
    texturas(c, ENT(3));
    Conv_U32Entrada(c, ENT(4)); /* pares de nós dos raios */
    texturas(c, ENT(5));
    texturas(c, ENT(6));
    Conv_U32Entrada(c, ENT(7)); /* parâmetros do brilho */
    texturas(c, ENT(8));
    texturas(c, ENT(9));
    texturas(c, ENT(0xA));
    texturas(c, ENT(0xB));
    /* §3.4: os conjuntos de emissores de impacto, rajada e transformação, contíguos de 0x0C a 0x236 */
    for (k = 0x0C; k <= 0x236 && !c->erro;) {
        conv_u32 prox = Conv_Conjunto(c, base, ent, n, k);
        Conv_Onde(c, "conjunto em %lu", (unsigned long)k);
        if (prox <= k) {
            Conv_Erro(c, "conjunto em %lu não consumiu entradas", (unsigned long)k);
        }
        k = prox;
    }
    if (!c->erro && k != 0x237) {
        Conv_Erro(c, "os conjuntos de 0x0C terminam em 0x%lX (esperado 0x237)", (unsigned long)k);
    }
    texturas(c, ENT(0x237));
    texturas(c, ENT(0x238));
    Conv_MalhaEfeito(c, ENT(0x239));
    texturas(c, ENT(0x23A));
    Conv_MalhaEfeito(c, ENT(0x23B));
    texturas(c, ENT(0x23C));            /* sem leitor: TexFile pela forma */
    Conv_U32Entrada(c, ENT(0x23D));     /* sem leitor: palavras de 4 bytes pela forma */
    Conv_U32Entrada(c, ENT(0x23E));
    if (!c->erro && Conv_Conjunto(c, base, ent, n, 0x23F) != 0x24D) {
        Conv_Erro(c, "conjunto de 0x23F não termina em 0x24D");
    }
    texturas(c, ENT(0x24D));
    Conv_U32Entrada(c, ENT(0x24E)); /* cores dos rastros */
    for (i = 0x24F; i <= 0x258 && !c->erro; i++) {
        rastro(c, ENT(i));
    }
    texturas(c, ENT(0x259));
    Conv_U32Entrada(c, ENT(0x25A)); /* cores da cauda do orbe */
    cauda(c, ENT(0x25B));
    texturas(c, ENT(0x25C));
    Conv_ParteEfeito(c, 5, 0, ENT(0x25D));
    texturas(c, ENT(0x25E));
    Conv_ParteEfeito(c, 5, 0, ENT(0x25F));
    Conv_ParteEfeito(c, 5, 0, ENT(0x260));
    for (i = 0x261; i <= 0x267 && !c->erro; i++) {
        Conv_ParteEfeito(c, 5, 1, ENT(i));
    }
    if (!c->erro && Conv_Conjunto(c, base, ent, n, 0x268) != n + 1) {
        Conv_Erro(c, "conjunto de 0x268 não termina no fim da tabela");
    }
}

/* ------------------------------------------------------------------ 00003 */

/* §4.3, §4.4: personagens (0x3C) e itens (0x28) */
static void comum3(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_u32 ent[4], n;
    conv_ofs r;
    n = Conv_Tabela(c, base, tam, ent, 2);
    if (c->erro || n != 2) {
        Conv_Erro(c, "00003 com %lu seções (esperado 2)", (unsigned long)n);
        return;
    }
    for (r = base + ent[1]; r + 0x3C <= base + ent[2] && !c->erro; r += 0x3C) {
        Conv_U32s(c, r, 2);
        Conv_U16s(c, r + 8, 4);
        Conv_U32s(c, r + 0x10, 7);
        Conv_Bytes(c, r + 0x2C, 4);
        Conv_U32s(c, r + 0x30, 3); /* targetY, dist, unk38 (sem leitor; f32 pelos dados) */
    }
    for (r = base + ent[2]; r + 0x28 <= base + ent[3] && !c->erro; r += 0x28) {
        Conv_Bytes(c, r, 4);
        Conv_U32s(c, r + 4, 2);
        Conv_U16s(c, r + 0x0C, 4);
        Conv_U32s(c, r + 0x14, 5);
    }
    Conv_Zeros(c, base, base + tam, "00003");
}

/* ------------------------------------------------------------------ 00004 */

/* §5.3: fonte (FontData + FontTex + pacotes + glifos); deslocamentos em BYTES a partir de a */
static void fonte(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 tex, glifos, n, pix, clut, ptam, ctam;
    Conv_Bytes(c, a, 8);
    Conv_U32(c, a + 8);
    tex = Conv_U32(c, a + 0x0C);
    Conv_U32(c, a + 0x10);
    glifos = Conv_U32(c, a + 0x14);
    n = Conv_U32(c, a + 0x18);
    Conv_U32s(c, a + 0x1C, 2);
    if (c->erro || a + tex + 0x40 > b || a + glifos + n * 6 > b) {
        Conv_Erro(c, "fonte em 0x%lX: textura em +0x%lX, %lu glifos em +0x%lX", (unsigned long)a, (unsigned long)tex,
                  (unsigned long)n, (unsigned long)glifos);
        return;
    }
    pix = Conv_U32(c, a + tex);
    clut = Conv_U32(c, a + tex + 4);
    ptam = Conv_U32(c, a + tex + 8);
    ctam = Conv_U32(c, a + tex + 12);
    Conv_U32s(c, a + tex + 0x10, 6);
    Conv_Bytes(c, a + tex + 0x28, 8);
    Conv_U64(c, a + tex + 0x30);
    Conv_U32s(c, a + tex + 0x38, 2);
    if (pix != 0) {
        Conv_PacoteImagem(c, a + pix, ptam);
    }
    if (clut != 0) {
        Conv_PacoteImagem(c, a + clut, ctam);
    }
    Conv_U16s(c, a + glifos, n * 3);
    Conv_Zeros(c, a, b, "fonte");
}

static void comum4(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_u32 ent[16], n, i;
    n = Conv_Tabela(c, base, tam, ent, 11);
    if (c->erro || n != 11) {
        Conv_Erro(c, "00004 com %lu entradas (esperado 11)", (unsigned long)n);
        return;
    }
    fonte(c, ENT(1));
    fonte(c, ENT(2));
    for (i = 3; i <= 7; i++) {
        vazia(c, ent, i);
    }
    /* A HUD usa também a folha de ícones como HudCRes: HudRes_UploadTex grava mark (+0x28) como s32. */
    Conv_TexFile(c, ENT(8), 1);
    for (i = 9; i <= 11 && !c->erro; i++) {
        Conv_Texto(c, ENT(i));
    }
}

/* ------------------------------------------------------------------ 00005, 00006 */

/* §6: folhas de sprites do HUD e do menu de pausa, e os textos */
static void hud(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_u32 ent[0x100 + 2], n, i;
    n = Conv_Tabela(c, base, tam, ent, 0xFF);
    if (c->erro || n != 197) {
        Conv_Erro(c, "HUD com %lu entradas (esperado 197)", (unsigned long)n);
        return;
    }
    for (i = 1; i <= 7 && !c->erro; i++) {
        if (i == 6) {
            vazia(c, ent, 6);
        } else {
            Conv_TexFile(c, ENT(i), 1);
        }
        Conv_Onde(c, "entrada %lu", (unsigned long)i);
    }
    for (i = 8; i <= n && !c->erro; i++) {
        Conv_Texto(c, ENT(i));
    }
}

/* ------------------------------------------------------------------ registro */

void Conv_Comum(ConvCtx *c, conv_ofs base, conv_ofs tam, int qual) {
    switch (qual) {
    case 1: comum1(c, base, tam); break;
    case 2: comum2(c, base, tam); break;
    case 3: comum3(c, base, tam); break;
    case 4: comum4(c, base, tam); break;
    default: hud(c, base, tam); break;
    }
}

void Conv_Comum1(ConvCtx *c, conv_ofs base, conv_ofs tam) { Conv_Comum(c, base, tam, 1); }
void Conv_Comum2(ConvCtx *c, conv_ofs base, conv_ofs tam) { Conv_Comum(c, base, tam, 2); }
void Conv_Comum3(ConvCtx *c, conv_ofs base, conv_ofs tam) { Conv_Comum(c, base, tam, 3); }
void Conv_Comum4(ConvCtx *c, conv_ofs base, conv_ofs tam) { Conv_Comum(c, base, tam, 4); }
void Conv_Hud(ConvCtx *c, conv_ofs base, conv_ofs tam) { Conv_Comum(c, base, tam, 5); }
