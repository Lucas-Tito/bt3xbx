/*
 * F5, etapa 8 (a resolução interna maior, etapa 2): o teste das contas do port/x360/src/gs/escala.h no Linux, contra a
 * tabela da seção 3 de docs/x360/plano/F5/08-720p.md (tiles da EDRAM por área e escala), as recusas, os alinhamentos do
 * Resolve e da profundidade, a tesoura presa na área e a leitura das chaves.
 *
 *   gcc -std=c89 -pedantic -Wall -Wextra -Werror -Iport/x360/src/gs port/x360/tools/teste_escala.c -o /tmp/teste_escala
 *   /tmp/teste_escala            # 0: tudo bate
 *   /tmp/teste_escala --estraga  # o controle: casos com o valor esperado errado de propósito; TEM de sair com 1
 */
#include <stdio.h>
#include <string.h>
#include "escala.h"

static int sFalhas, sCasos, sEstraga;

static void confere(int ok, const char *o_que) {
    sCasos++;
    if (!ok) {
        sFalhas++;
        printf("FALHA: %s\n", o_que);
    }
}

/* uma área aceita, com os tamanhos e os tiles esperados (o esperado de tiles + 1 com --estraga) */
static void aceita(int w, int h, int meios, int sw, int sh, int swz, unsigned tiles) {
    EscArea a;
    const char *e = esc_monta(&a, w, h, meios);
    char t[160];
    sprintf(t, "%dx%d em %d meios: aceita, %dx%d (largura vista como cor %d), %u + %u tiles", w, h, meios, sw, sh, swz, tiles,
            tiles);
    if (sEstraga) {
        tiles++;
    }
    confere(e == NULL && a.sw == sw && a.sh == sh && a.swz == swz && a.tilesCor == tiles && a.tilesProf == tiles, t);
    if (e != NULL) {
        printf("   (recusada: %s)\n", e);
    }
}

/* uma área recusada (aceita, com --estraga: o controle diz que uma recusa trocada por aceite é acusada) */
static void recusa(int w, int h, int meios) {
    EscArea a;
    const char *e;
    char t[96];
    memset(&a, 0, sizeof(a));
    e = esc_monta(&a, w, h, meios);
    sprintf(t, "%dx%d em %d meios: recusada", w, h, meios);
    confere(sEstraga ? e == NULL : e != NULL && a.sw == 0, t);
}

static void alinha(int x0, int y0, int x1, int y1, int w, int h, int oito, int e0, int e1, int e2, int e3) {
    char t[128];
    if (oito) {
        esc_alinha8(&x0, &y0, &x1, &y1, w, h);
    } else {
        esc_alinha80(&x0, &y0, &x1, &y1, w, h);
    }
    if (sEstraga) {
        e2 += 8;
    }
    sprintf(t, "alinha%s em %dx%d: %d,%d %d,%d (esperado %d,%d %d,%d)", oito ? "8" : "80", w, h, x0, y0, x1, y1, e0, e1, e2,
            e3);
    confere(x0 == e0 && y0 == e1 && x1 == e2 && y1 == e3, t);
}

int main(int argc, char **argv) {
    int w, h, x0, y0, x1, y1, c;
    sEstraga = argc > 1 && strcmp(argv[1], "--estraga") == 0;

    /* a tabela da seção 3 (tiles de 80 x 16) */
    aceita(1024, 1024, 2, 1024, 1024, 1040, 832); /* hoje: 832 + 832 */
    aceita(640, 448, 2, 640, 448, 640, 224);      /* a área: 224 + 224 */
    aceita(640, 448, 3, 960, 672, 960, 504);      /* 1,5 */
    aceita(640, 448, 4, 1280, 896, 1280, 896);    /* 2: 896 + 896 de 2.048 */
    aceita(704, 480, 2, 704, 480, 720, 270);      /* uma largura que não é múltipla de 80: a vista como cor arredonda */
    confere(esc_tiles(1200, 720) == 675, "1200 x 720 (1,875 x 1,607): 675 tiles");
    confere(esc_tiles(1024, 720) == 585, "1024 x 720 (1,6): 585 tiles");

    /* as recusas */
    recusa(1024, 1024, 4); /* 2048 x 2048: 3.328 + 3.328 tiles, não cabe na EDRAM */
    recusa(1024, 1024, 3); /* 1536 x 1536: 1.920 + 1.920 */
    recusa(512, 448, 2);   /* menor que a imagem do jogo (a cópia dos ids e as capturas leem 640 x 448) */
    recusa(640, 440, 2);   /* idem na altura */
    recusa(2048, 448, 2);  /* maior que o alvo do GS */
    recusa(656, 448, 2);   /* 656 não é múltiplo de 32 (o pitch da textura tiled) */
    recusa(640, 464, 2);   /* 464 não é múltiplo de 32 */
    recusa(672, 480, 3);   /* 1008 x 720: 1008 não é múltiplo de 32 */
    recusa(640, 448, 1);   /* escala 0,5 */
    recusa(640, 448, 5);   /* escala 2,5 */

    /* os alinhamentos: o Resolve (8) e a profundidade (80), presos na área */
    alinha(3, 5, 17, 9, 640, 448, 1, 0, 0, 24, 16);
    alinha(630, 440, 645, 452, 640, 448, 1, 624, 440, 640, 448);
    alinha(85, 0, 161, 8, 640, 448, 0, 80, 0, 240, 8);
    alinha(600, 0, 640, 448, 640, 448, 0, 560, 0, 640, 448);
    alinha(990, 0, 1024, 1024, 1024, 1024, 0, 960, 0, 1024, 1024); /* (1024 fica em 1024: o restauro leva até a 1040) */

    /* a tesoura presa na área: cortou quando passava dela */
    x0 = -4; y0 = 0; x1 = 512; y1 = 448;
    c = esc_prende(&x0, &y0, &x1, &y1, 640, 448);
    confere((sEstraga ? c == 1 : c == 0) && x0 == 0 && x1 == 512, "tesoura 512 x 448 dentro de 640 x 448: não cortada");
    x0 = 0; y0 = 0; x1 = 1024; y1 = 448;
    c = esc_prende(&x0, &y0, &x1, &y1, 640, 448);
    confere(c == 1 && x1 == 640 && y1 == 448, "tesoura de 1024 de largura em 640 x 448: cortada em 640");
    x0 = 0; y0 = 0; x1 = 1024; y1 = 1024;
    c = esc_prende(&x0, &y0, &x1, &y1, 1024, 1024);
    confere(c == 0 && x1 == 1024 && y1 == 1024, "tesoura de 1024 em 1024 x 1024: não cortada");

    /* as chaves */
    confere(esc_le_area("640x448", &w, &h) && w == 640 && h == 448, "BT3_ALVO_AREA=640x448");
    confere(esc_le_area("1024X1024", &w, &h) && w == 1024 && h == 1024, "BT3_ALVO_AREA=1024X1024");
    confere(!esc_le_area("640", &w, &h) && !esc_le_area("640x", &w, &h) && !esc_le_area("x448", &w, &h) &&
                !esc_le_area("640x448 ", &w, &h) && !esc_le_area("640*448", &w, &h) && !esc_le_area("1000000x448", &w, &h),
            "BT3_ALVO_AREA mal escrita: recusada");
    confere(esc_le_escala("1") == 2 && esc_le_escala("1.5") == 3 && esc_le_escala("1,5") == 3 && esc_le_escala("2") == 4,
            "BT3_ESCALA 1, 1.5, 1,5 e 2");
    confere(esc_le_escala("1.6") == 0 && esc_le_escala("3") == 0 && esc_le_escala("") == 0 && esc_le_escala("15") == 0,
            "BT3_ESCALA fora: 0");

    printf("teste_escala: %d casos, %d falhas%s\n", sCasos, sFalhas, sEstraga ? " (com --estraga: o controle)" : "");
    return sFalhas != 0;
}
