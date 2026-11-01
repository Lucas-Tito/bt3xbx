/* Desenho da tela de conversão: fonte 5x7 própria e retângulos Clear, sem shader nem arquivo de fonte.
   Separado de iso_tela.cpp para a prova host (port/x360/tools/prova_tela.py) compilar este mesmo desenho
   com um D3D falso que pinta num buffer. Depende de D3DDevice, D3DRECT, LONG, sprintf e de IcContas. */
#ifndef ISO_TELA_DESENHO_H
#define ISO_TELA_DESENHO_H
#include "iso_avisos.h"
#include "iso_conv.h"
/* Fonte mínima 5x7 do port. Retângulos Clear dispensam shaders, fontes externas e qualquer leitura de
   arquivo pela thread que apresenta. Ordem: A-Z, 0-9, . % / - \ | [ ] # : > ? , < */
static const unsigned char letras[][7] = {
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14},
    {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17}, {14, 4, 4, 4, 4, 4, 14},
    {7, 2, 2, 2, 18, 18, 12},     {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17}, {14, 17, 17, 17, 17, 17, 14},
    {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},       {17, 17, 17, 17, 17, 17, 14},
    {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31},     {14, 17, 19, 21, 25, 17, 14},
    {4, 12, 4, 4, 4, 4, 14},      {14, 17, 1, 2, 4, 8, 31},     {30, 1, 1, 14, 1, 1, 30},
    {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},   {14, 16, 16, 30, 17, 17, 14},
    {31, 1, 2, 4, 8, 8, 8},       {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14},
    {0, 0, 0, 0, 0, 12, 12},      {17, 2, 4, 8, 16, 17, 0},
    /* 38 */ {1, 1, 2, 4, 8, 16, 16}, /* 39 - */ {0, 0, 0, 31, 0, 0, 0}, /* 40 \ */ {16, 16, 8, 4, 2, 1, 1},
    /* 41 | */ {4, 4, 4, 4, 4, 4, 4}, /* 42 [ */ {14, 8, 8, 8, 8, 8, 14}, /* 43 ] */ {14, 2, 2, 2, 2, 2, 14},
    /* 44 # */ {10, 10, 31, 10, 31, 10, 10}, /* 45 : */ {0, 12, 12, 0, 12, 12, 0},
    /* 46 > */ {16, 8, 4, 2, 4, 8, 16},
    /* 47 ? */ {14, 17, 1, 2, 4, 0, 4}, /* 48 , */ {0, 0, 0, 0, 12, 4, 8}, /* 49 < */ {1, 2, 4, 8, 4, 2, 1}};
static int indiceLetra(int c) {
    if (c >= 'a' && c <= 'z')
        c -= 32;
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= '0' && c <= '9')
        return 26 + c - '0';
    switch (c) {
    case '.': return 36;
    case '%': return 37;
    case '/': return 38;
    case '-': return 39;
    case '\\': return 40;
    case '|': return 41;
    case '[': return 42;
    case ']': return 43;
    case '#': return 44;
    case ':': return 45;
    case '>': return 46;
    case '?': return 47;
    case ',': return 48;
    case '<': return 49;
    }
    return -1; /* o espaço e o que a fonte não tem só avançam */
}
/* Um retângulo por trecho contíguo de pixels acesos numa linha da letra: bem menos Clear por quadro. */
static void texto(D3DDevice *dev, const char *s, int x, int y, int escala, unsigned cor) {
    for (; *s; s++, x += 6 * escala) {
        int indice = indiceLetra(*s);
        if (indice < 0)
            continue;
        for (int j = 0; j < 7; j++) {
            int i = 0;
            while (i < 5) {
                if (letras[indice][j] & (16 >> i)) {
                    int fim = i;
                    while (fim < 5 && (letras[indice][j] & (16 >> fim)))
                        fim++;
                    D3DRECT r = {x + i * escala, y + j * escala, x + fim * escala, y + (j + 1) * escala};
                    dev->Clear(1, &r, D3DCLEAR_TARGET, cor, 1, 0);
                    i = fim;
                } else {
                    i++;
                }
            }
        }
    }
}
#define TELA_FUNDO 0xff000000u
#define TELA_TEXTO 0xffc0c0c0u
#define TELA_OK 0xff40c040u
#define TELA_FALHA 0xffd04040u
#define TELA_ESCALA 3
#define TELA_X 80
#define TELA_Y 90
#define TELA_PASSO 32
#define TELA_COLUNA 42 /* o contador termina nesta coluna; o OK, o giro ou o FAILED vêm logo depois */

struct TelaEstado {
    int fase;
    IcContas c;
    unsigned pc;
    int falhou;
    unsigned ms;
};

/* As linhas: 0 = conferência, 1 = a passagem (cópia), 2..6 = categorias, 7 = verificação. */
#define TELA_L_COPIA 1
#define TELA_L_CAT 2
#define TELA_L_VERIF 7
/* A categoria em conversão agora, ou -1 se a passagem está só copiando. */
static int telaCategoriaAtual(const TelaEstado &e) {
    if (e.fase == IC_CONVERTER && e.c.atual >= 0 && e.c.atual < IC_NCATS && e.c.feito[e.c.atual] < e.c.total[e.c.atual])
        return e.c.atual;
    return -1;
}
/* A linha da etapa em andamento (onde vai o FAILED). */
static int telaEtapaAtual(const TelaEstado &e) {
    if (e.fase <= IC_COPIAR)
        return 0;
    if (e.fase == IC_CONVERTER)
        return telaCategoriaAtual(e) >= 0 ? TELA_L_CAT + telaCategoriaAtual(e) : TELA_L_COPIA;
    return TELA_L_VERIF;
}
/* Rótulo, pontinhos até a coluna fixa e o contador alinhado à direita. */
static void telaLinha(char *out, const char *rotulo, const char *contador) {
    int n = sprintf(out, "> %s ", rotulo), k = (int)strlen(contador), pontos = TELA_COLUNA - n - (k ? k + 1 : 0);
    while (pontos-- > 0)
        out[n++] = '.';
    if (k)
        n += sprintf(out + n, " %s", contador);
    out[n] = 0;
}
/* Cada linha, depois do texto: o marcador na coluna seguinte. */
static void telaMarca(D3DDevice *dev, int linha, const char *s, unsigned cor) {
    texto(dev, s, TELA_X + (TELA_COLUNA + 1) * 6 * TELA_ESCALA, TELA_Y + linha * TELA_PASSO, TELA_ESCALA, cor);
}
/* O giro avança uma posição por batida (entrada convertida), nunca por tempo: se o conversor travar, ele para. */
static char telaGiro(unsigned batidas) { return "|/-\\"[batidas & 3]; }
static void telaDesenhar(D3DDevice *dev, const TelaEstado &e, const char *versao) {
    char buf[64], cnt[24], gira[2];
    int i, l = 0, falha = e.falhou ? telaEtapaAtual(e) : -1;
    unsigned pc = e.pc > 100 ? 100 : e.pc, seg = e.ms / 1000;
    dev->Clear(0, NULL, D3DCLEAR_TARGET, TELA_FUNDO, 1, 0);
    sprintf(buf, ISO_CONV_T_TITULO, versao);
    texto(dev, buf, TELA_X, TELA_Y + (l++) * TELA_PASSO, TELA_ESCALA, TELA_TEXTO);
    texto(dev, ISO_CONV_T_PRAZO, TELA_X, TELA_Y + (l++) * TELA_PASSO, TELA_ESCALA, TELA_TEXTO);
    texto(dev, ISO_CONV_T_AVISO, TELA_X, TELA_Y + (l++) * TELA_PASSO, TELA_ESCALA, TELA_TEXTO);
    l++;
    gira[1] = 0;
    for (i = 0; i <= TELA_L_VERIF; i++, l++) {
        const char *rotulo = i == 0 ? ISO_CONV_T_CONFERINDO : i == TELA_L_COPIA ? ISO_CONV_T_COPIANDO
                           : i == TELA_L_VERIF ? ISO_CONV_T_VERIFICANDO : ISO_CONV_T_CATEGORIAS[i - TELA_L_CAT];
        bool ok = false, gira_agora = false;
        unsigned giro = e.c.batidas;
        cnt[0] = 0;
        if (i == 0) {
            ok = e.fase >= IC_COPIAR;
        } else if (i == TELA_L_COPIA) {
            /* MiB gravados na passagem; gira por lote gravado quando nenhuma categoria está convertendo. */
            if (e.c.passe_total_mb)
                sprintf(cnt, "%u/%u MB", (unsigned)e.c.passe_mb, (unsigned)e.c.passe_total_mb);
            ok = e.fase >= IC_VERIFICAR;
            gira_agora = !ok && e.fase == IC_CONVERTER && telaCategoriaAtual(e) < 0;
            giro = e.c.passe_blocos;
        } else if (i == TELA_L_VERIF) {
            /* MiB relidos da ISO convertida; o giro avança um por bloco relido. */
            if (e.c.verif_total_mb)
                sprintf(cnt, "%u/%u MB", (unsigned)e.c.verif_mb, (unsigned)e.c.verif_total_mb);
            ok = e.fase >= IC_PUBLICAR;
            gira_agora = !ok && e.fase == IC_VERIFICAR;
            giro = e.c.verif_blocos;
        } else {
            unsigned total = e.c.total[i - TELA_L_CAT], feito = e.c.feito[i - TELA_L_CAT];
            if (total)
                sprintf(cnt, "%u/%u", feito, total);
            ok = total > 0 && feito == total;
            gira_agora = !ok && telaCategoriaAtual(e) == i - TELA_L_CAT;
        }
        telaLinha(buf, rotulo, cnt);
        texto(dev, buf, TELA_X, TELA_Y + l * TELA_PASSO, TELA_ESCALA, TELA_TEXTO);
        if (i == falha)
            telaMarca(dev, l, ISO_CONV_T_FALHOU, TELA_FALHA);
        else if (ok)
            telaMarca(dev, l, ISO_CONV_T_OK, TELA_OK);
        else if (gira_agora) {
            gira[0] = telaGiro(giro);
            telaMarca(dev, l, gira, TELA_TEXTO);
        }
    }
    /* A barra é o processo inteiro, pesado pelo trabalho (iso_conv.h, IC_PESO_MENU). */
    {
        int cheias = (int)(pc * 40 / 100);
        buf[0] = ' ';
        buf[1] = ' ';
        buf[2] = '[';
        for (i = 0; i < 40; i++)
            buf[3 + i] = i < cheias ? '#' : '-';
        sprintf(buf + 43, "] %u%%", pc);
        texto(dev, buf, TELA_X, TELA_Y + (l++) * TELA_PASSO, TELA_ESCALA, TELA_TEXTO);
    }
    sprintf(buf, "  %s %02u:%02u", ISO_CONV_T_DECORRIDO, seg / 60, seg % 60);
    texto(dev, buf, TELA_X, TELA_Y + l * TELA_PASSO, TELA_ESCALA, TELA_TEXTO);
}
#endif
