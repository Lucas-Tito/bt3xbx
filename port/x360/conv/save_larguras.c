/*
 * Mapa de larguras do save: a tabela única (ver save_larguras.h e docs/x360/formatos/save-larguras.md, que tem a
 * evidência de cada linha: arquivo:linha de onde o código acessa o número).
 *
 * A tabela é escrita em modelos: {desl, qtd, larg, cat, rep, passo, nome} vale `rep` vezes, a cada `passo` bytes
 * a partir de `desl` (rep 1 = uma vez). SaveLarg_Faixas expande os modelos em faixas planas, na primeira chamada.
 * Cobertura sem buraco nem sobreposição é conferida por port/x360/tools/save_larguras_tool.c (confere).
 */
#include "save_larguras.h"

#define T SAVELARG_TIPADO
#define R SAVELARG_RESOLVIDO
#define S SAVELARG_SEM_ACESSO
#define D SAVELARG_DISCO

typedef struct SaveModelo {
    unsigned long desl, qtd;
    int larg, cat;
    int rep;
    unsigned long passo;
    const char *nome;
} SaveModelo;

/* ---- arquivo de dados: SaveData (include/sys/save.h), 0x4000 bytes ---- */
static const SaveModelo sDados[] = {
    {0x0000, 2, 4, T, 1, 0, "soma (unk0, unk4)"},
    {0x0008, 1, 4, T, 1, 0, "unlockFlags"},
    {0x000C, 1, 4, T, 1, 0, "unkC (dificuldade da História)"},
    {0x0010, 36, 4, T, 1, 0, "slot[9] (flags, val[3])"},
    {0x00A0, 0x168, 1, S, 1, 0, "unkA0"},
    {0x0208, 2, 4, T, 1, 0, "unk208, unk20C"},
    /* 0x210..0x77C: bloco opaco do save.h, resolvido pelos views de menu_o.h, menu_p.h, menu_q.h, menu_r.h, menu_s.h */
    {0x0210, 2, 4, R, 10, 12, "rank[i].chara, score"},
    {0x0218, 1, 1, R, 10, 12, "rank[i].cleared"},
    {0x0219, 3, 1, S, 10, 12, "rank[i] (enchimento)"},
    {0x0288, 1, 1, R, 1, 0, "simCleared"},
    {0x0289, 3, 1, S, 1, 0, "0x289 (enchimento)"},
    {0x028C, 5, 1, R, 100, 12, "mission[i].cleared, rank, tempo"},
    {0x0291, 3, 1, S, 100, 12, "mission[i] (enchimento)"},
    {0x0294, 1, 4, R, 100, 12, "mission[i].score"},
    {0x073C, 2, 4, R, 5, 12, "bestB[i].value, total"},
    {0x0744, 4, 1, R, 5, 12, "bestB[i].rank, tempo"},
    {0x0778, 4, 1, S, 1, 0, "unk778[0] (declarado s32, sem acesso)"},
    {0x077C, 1, 4, T, 1, 0, "unk77C (posição na escada)"},
    /* 0x780..0xA08: bloco opaco do save.h, bestC[54] */
    {0x0780, 5, 1, R, 54, 12, "bestC[i].cleared, rank, tempo"},
    {0x0785, 3, 1, S, 54, 12, "bestC[i] (enchimento)"},
    {0x0788, 1, 4, R, 54, 12, "bestC[i].total"},
    {0x0A08, 3, 4, T, 1, 0, "unkA08, unkA0C, unkA10"},
    {0x0A14, 0x1FC, 1, S, 1, 0, "unkA14"},
    {0x0C10, 4, 8, T, 1, 0, "charaBits[3], stageBits"},
    {0x0C30, 1, 4, T, 1, 0, "bgmBits"},
    {0x0C34, 6, 4, T, 1, 0, "rule[6]"},
    /* 0xC4C..0x1008: bloco opaco do save.h; só trainClear (menu_h_d.c) é acessado */
    {0x0C4C, 0x1C0, 1, S, 1, 0, "unkC4C (antes de trainClear)"},
    {0x0E0C, 3, 4, R, 1, 0, "trainClear[3]"},
    {0x0E18, 0x1F0, 1, S, 1, 0, "unkC4C (depois de trainClear)"},
    {0x1008, 2, 4, T, 1, 0, "unk1008, unk100C"},
    {0x1010, 0x1F8, 1, S, 1, 0, "unk1010"},
    {0x1208, 1, 4, T, 1, 0, "unk1208 (dcFlags)"},
    {0x120C, 0x3FC, 1, S, 1, 0, "unk120C"},
    {0x1608, 42, 4, T, 1, 0, "flags, key, keyEdit, unk168C, opções, volumes"},
    {0x16B0, 0x158, 1, S, 1, 0, "unk16B0"},
    /* SaveCustom: u16 item[3][8], s32 unk30, u16 level, u16 unk36 */
    {0x1808, 24, 2, T, 97, 0x38, "custom[i].item[3][8]"},
    {0x1838, 1, 4, T, 97, 0x38, "custom[i].unk30 (exp)"},
    {0x183C, 2, 2, T, 97, 0x38, "custom[i].level, unk36"},
    /* SaveRec: u8 unk0[0x14] (opaco), u16 level, u16 unk16, s32 chara */
    {0x2D40, 8, 2, R, 14, 0x1C, "rec[i].item[8]"},
    {0x2D50, 4, 1, S, 14, 0x1C, "rec[i] +0x10 (declarado s32, sem acesso)"},
    {0x2D54, 2, 2, T, 14, 0x1C, "rec[i].level, unk16"},
    {0x2D58, 1, 4, T, 14, 0x1C, "rec[i].chara"},
    {0x2EC8, 350, 1, T, 1, 0, "item[350]"},
    {0x3026, 2, 1, S, 1, 0, "unk3026"},
    {0x3028, 1, 4, T, 1, 0, "money"},
    {0x302C, 0xFD4, 1, S, 1, 0, "unk302C"}
};

/* ---- icon.sys: McCardIconSys (include/battle/stgm_a_b.h), 0x3C4 bytes ---- */
static const SaveModelo sIconSys[] = {
    {0x000, 4, 1, T, 1, 0, "head \"PS2D\""},
    {0x004, 2, 2, T, 1, 0, "unk4, titleBreak"},
    {0x008, 2, 4, T, 1, 0, "unk8, transRate"},
    {0x010, 44, 4, T, 1, 0, "bgColor s32, lightDir/lightColor/ambient f32"},
    {0x0C0, 0x304, 1, T, 1, 0, "title, viewName, copyName, delName, unk1C4 (textos e zeros)"}
};

/* ---- dbzsm.ico: 0x9C9A bytes copiados do pacote 00477 do disco (McFlow_SetupRequest, mcflow_a.c:160) ---- */
static const SaveModelo sIco[] = {
    {0x0000, 40090, 1, D, 1, 0, "ícone do PS2 (cópia do disco)"}
};

#undef T
#undef R
#undef S
#undef D

#define NUM(v) ((int)(sizeof(v) / sizeof((v)[0])))
#define MAX_FAIXAS 1200

static SaveFaixa sFaixas[SAVELARG_ARQ_NUM][MAX_FAIXAS];
static int sNum[SAVELARG_ARQ_NUM];
static int sPronto[SAVELARG_ARQ_NUM];

static void expande(int arq, const SaveModelo *m, int nm) {
    int i, r, n = 0;
    for (i = 0; i < nm; i++) {
        for (r = 0; r < m[i].rep && n < MAX_FAIXAS; r++) {
            sFaixas[arq][n].desl = m[i].desl + (unsigned long)r * m[i].passo;
            sFaixas[arq][n].qtd = m[i].qtd;
            sFaixas[arq][n].larg = m[i].larg;
            sFaixas[arq][n].cat = m[i].cat;
            sFaixas[arq][n].nome = m[i].nome;
            n++;
        }
    }
    /* ordena por deslocamento (inserção: os modelos intercalam os campos de um registro repetido) */
    for (i = 1; i < n; i++) {
        SaveFaixa x = sFaixas[arq][i];
        int j = i - 1;
        while (j >= 0 && sFaixas[arq][j].desl > x.desl) {
            sFaixas[arq][j + 1] = sFaixas[arq][j];
            j--;
        }
        sFaixas[arq][j + 1] = x;
    }
    sNum[arq] = n;
    sPronto[arq] = 1;
}

const char *SaveLarg_NomeArquivo(int arquivo) {
    switch (arquivo) {
    case SAVELARG_ARQ_DADOS: return "BASLUS-21678DBZT3";
    case SAVELARG_ARQ_ICONSYS: return "icon.sys";
    case SAVELARG_ARQ_ICO: return "dbzsm.ico";
    }
    return 0;
}

unsigned long SaveLarg_Tamanho(int arquivo) {
    switch (arquivo) {
    case SAVELARG_ARQ_DADOS: return SAVELARG_TAM_DADOS;
    case SAVELARG_ARQ_ICONSYS: return SAVELARG_TAM_ICONSYS;
    case SAVELARG_ARQ_ICO: return SAVELARG_TAM_ICO;
    }
    return 0;
}

const SaveFaixa *SaveLarg_Faixas(int arquivo, int *n) {
    if (arquivo < 0 || arquivo >= SAVELARG_ARQ_NUM) {
        return 0;
    }
    if (!sPronto[arquivo]) {
        switch (arquivo) {
        case SAVELARG_ARQ_DADOS: expande(arquivo, sDados, NUM(sDados)); break;
        case SAVELARG_ARQ_ICONSYS: expande(arquivo, sIconSys, NUM(sIconSys)); break;
        default: expande(arquivo, sIco, NUM(sIco)); break;
        }
    }
    *n = sNum[arquivo];
    return sFaixas[arquivo];
}

int SaveLarg_Inverte(unsigned char *buf, unsigned long tam, int arquivo) {
    const SaveFaixa *f;
    int n, i, k;
    unsigned long q, p;
    unsigned char t;

    if (SaveLarg_Tamanho(arquivo) == 0 || tam != SaveLarg_Tamanho(arquivo)) {
        return -1;
    }
    f = SaveLarg_Faixas(arquivo, &n);
    for (i = 0; i < n; i++) {
        if (f[i].larg == 1) {
            continue;
        }
        for (q = 0; q < f[i].qtd; q++) {
            p = f[i].desl + q * (unsigned long)f[i].larg;
            for (k = 0; k < f[i].larg / 2; k++) {
                t = buf[p + k];
                buf[p + k] = buf[p + f[i].larg - 1 - k];
                buf[p + f[i].larg - 1 - k] = t;
            }
        }
    }
    return 0;
}
