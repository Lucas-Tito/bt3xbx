/* Conversão do save do BT3 entre PS2 e 360. Ver save_conv.h e docs/x360/formatos/save-conversao.md. */
#include <string.h>
#include "save_conv.h"

#define BT3_PASTA "BASLUS-21678DBZT3"

static unsigned long le32(const unsigned char *p) {
    return (unsigned long)p[0] | (unsigned long)p[1] << 8 | (unsigned long)p[2] << 16 | (unsigned long)p[3] << 24;
}
static unsigned long be32(const unsigned char *p) {
    return (unsigned long)p[3] | (unsigned long)p[2] << 8 | (unsigned long)p[1] << 16 | (unsigned long)p[0] << 24;
}
static void grava32(unsigned char *p, unsigned long v, int ordem) {
    int i;
    for (i = 0; i < 4; i++) {
        p[ordem == SAVECONV_360 ? 3 - i : i] = (unsigned char)((v >> (8 * i)) & 255UL);
    }
}

unsigned SaveConv_Soma(const unsigned char *dados) {
    unsigned long sum = 0, i;
    for (i = 8; i < SAVELARG_TAM_DADOS; i++) {
        sum += (i - 8) & 1 ? (unsigned long)dados[i] << 8 : (unsigned long)dados[i];
        sum &= 0xFFFFUL;
    }
    return (unsigned)sum;
}

unsigned SaveConv_GravaSoma(unsigned char *dados, int ordem) {
    unsigned s;
    memset(dados, 0, 8);
    s = SaveConv_Soma(dados);
    grava32(dados, s >> 8, ordem);
    grava32(dados + 4, s & 0xFFU, ordem);
    return s;
}

int SaveConv_ConferirSoma(const unsigned char *dados, int ordem) {
    unsigned s;
    unsigned long w0, w1;
    /* a soma não olha os 8 primeiros bytes: calcula-se direto */
    s = SaveConv_Soma(dados);
    w0 = ordem == SAVECONV_360 ? be32(dados) : le32(dados);
    w1 = ordem == SAVECONV_360 ? be32(dados + 4) : le32(dados + 4);
    return w0 == (unsigned long)(s >> 8) && w1 == (unsigned long)(s & 0xFFU);
}

static const PsuArquivo *acha(const PsuSave *s, const char *nome) {
    unsigned i;
    for (i = 0; i < s->nArquivos; i++) {
        if (strcmp(s->arquivos[i].nome, nome) == 0) return &s->arquivos[i];
    }
    return 0;
}

int SaveConv_Ps2Para360(const PsuSave *s, SaveConvTres *saida) {
    const PsuArquivo *ic, *co, *da;
    int r;
    if (!s || !saida) return SAVECONV_E_ARGS;
    r = Psu_ConferirBT3(s);
    if (r != PSU_OK) return r;
    ic = acha(s, "icon.sys");
    co = acha(s, "dbzsm.ico");
    da = acha(s, BT3_PASTA);
    if (!ic || !co || !da) return PSU_E_BT3_FALTA;
    if (ic->tamanho != SAVELARG_TAM_ICONSYS || co->tamanho != SAVELARG_TAM_ICO || da->tamanho != SAVELARG_TAM_DADOS)
        return SAVECONV_E_TAMANHO;
    if (!SaveConv_ConferirSoma(da->dados, SAVECONV_PS2)) return SAVECONV_E_SOMA_PS2;
    memcpy(saida->iconsys, ic->dados, SAVELARG_TAM_ICONSYS);
    memcpy(saida->ico, co->dados, SAVELARG_TAM_ICO);
    memcpy(saida->dados, da->dados, SAVELARG_TAM_DADOS);
    if (SaveLarg_Inverte(saida->dados, SAVELARG_TAM_DADOS, SAVELARG_ARQ_DADOS) != 0) return SAVECONV_E_TAMANHO;
    if (SaveLarg_Inverte(saida->iconsys, SAVELARG_TAM_ICONSYS, SAVELARG_ARQ_ICONSYS) != 0) return SAVECONV_E_TAMANHO;
    SaveConv_GravaSoma(saida->dados, SAVECONV_360);
    return SAVECONV_OK;
}

static void preenche(PsuArquivo *a, const char *nome, const unsigned char *dados, unsigned long tam,
                     const PsuData *c, const PsuData *m) {
    memset(a, 0, sizeof(*a));
    strcpy(a->nome, nome);
    a->modo = PSU_MODO_ARQUIVO;
    a->criado = *c;
    a->modificado = *m;
    a->dados = dados;
    a->tamanho = tam;
}

int SaveConv_360ParaPs2(const SaveConvTres *entrada, SaveConvTres *tmp, PsuArquivo *vet, PsuSave *s,
                        const PsuData *criada, const PsuData *modificada) {
    if (!entrada || !tmp || !vet || !s || !criada || !modificada || entrada == tmp) return SAVECONV_E_ARGS;
    if (!SaveConv_ConferirSoma(entrada->dados, SAVECONV_360)) return SAVECONV_E_SOMA;
    memcpy(tmp->iconsys, entrada->iconsys, SAVELARG_TAM_ICONSYS);
    memcpy(tmp->ico, entrada->ico, SAVELARG_TAM_ICO);
    memcpy(tmp->dados, entrada->dados, SAVELARG_TAM_DADOS);
    if (SaveLarg_Inverte(tmp->dados, SAVELARG_TAM_DADOS, SAVELARG_ARQ_DADOS) != 0) return SAVECONV_E_TAMANHO;
    if (SaveLarg_Inverte(tmp->iconsys, SAVELARG_TAM_ICONSYS, SAVELARG_ARQ_ICONSYS) != 0) return SAVECONV_E_TAMANHO;
    SaveConv_GravaSoma(tmp->dados, SAVECONV_PS2);
    preenche(&vet[0], "icon.sys", tmp->iconsys, SAVELARG_TAM_ICONSYS, criada, modificada);
    preenche(&vet[1], "dbzsm.ico", tmp->ico, SAVELARG_TAM_ICO, criada, modificada);
    preenche(&vet[2], BT3_PASTA, tmp->dados, SAVELARG_TAM_DADOS, criada, modificada);
    memset(s, 0, sizeof(*s));
    strcpy(s->pasta, BT3_PASTA);
    s->modo = PSU_MODO_PASTA;
    s->criada = *criada;
    s->modificada = *modificada;
    s->nArquivos = 3;
    s->arquivos = vet;
    return SAVECONV_OK;
}

const char *SaveConv_TextoErro(int c) {
    switch (c) {
    case SAVECONV_OK: return "ok";
    case SAVECONV_E_ARGS: return "bad arguments";
    case SAVECONV_E_SOMA_PS2: return "PS2 save is damaged (checksum)";
    case SAVECONV_E_SOMA: return "save is damaged (checksum)";
    case SAVECONV_E_TAMANHO: return "save file has the wrong size";
    }
    return Psu_TextoErro(c);
}
