/*
 * F5, etapa 8 (a resolução interna maior, etapas 2 e 3): o teste das contas do port/x360/src/gs/escala.h no Linux, contra a
 * tabela da seção 3 de docs/x360/plano/F5/08-720p.md (tiles da EDRAM por área e escala), as recusas, os alinhamentos do
 * Resolve e da profundidade, a tesoura presa na área, a leitura das chaves e (etapa 3) as bordas na escala (esc_px: a
 * regra do rasterizador, retângulos vizinhos sem fresta nem sobreposição) e a redução das capturas (esc_reduz). Na retomada
 * do 720p (seção 9), a escala em num / den: 1,6 (640 x 460 em 1024 x 736), 1,33 (648 x 456 em 864 x 608) e 1,25 (640 x 512
 * em 800 x 640), a regra ceil(x * s - 0,5) e as escalas 1, 1,5 e 2 com os números da regra de antes, floor(x * meios / 2).
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
static void aceita(int w, int h, int num, int den, int sw, int sh, int swz, unsigned tiles) {
    EscArea a;
    const char *e = esc_monta(&a, w, h, num, den);
    char t[160];
    sprintf(t, "%dx%d na escala %d/%d: aceita, %dx%d (largura vista como cor %d), %u + %u tiles", w, h, num, den, sw, sh, swz,
            tiles, tiles);
    if (sEstraga) {
        tiles++;
    }
    confere(e == NULL && a.sw == sw && a.sh == sh && a.swz == swz && a.tilesCor == tiles && a.tilesProf == tiles, t);
    if (e != NULL) {
        printf("   (recusada: %s)\n", e);
    }
}

/* uma área recusada (aceita, com --estraga: o controle diz que uma recusa trocada por aceite é acusada) */
static void recusa(int w, int h, int num, int den) {
    EscArea a;
    const char *e;
    char t[96];
    memset(&a, 0, sizeof(a));
    e = esc_monta(&a, w, h, num, den);
    sprintf(t, "%dx%d na escala %d/%d: recusada", w, h, num, den);
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

/* (etapa 3) as bordas na escala num / den, de 0 a `lado` pixels do GS: cada pixel do GS com pelo menos um de saída, cada
   pixel de saída k no pixel do GS que o rasterizador dá com os centros em .5, floor((k + 0,5) / s) = (2k + 1) den / (2 num),
   e retângulos vizinhos (cortados em pontos quaisquer) somando a largura inteira, sem fresta nem sobreposição */
static void bordas(int num, int den, int lado) {
    int g, k, a, b, soma = 0, vazios = 0, errados = 0, n = 0;
    char t[128];
    for (g = 0; g < lado; g++) {
        vazios += esc_px(g + 1, num, den) <= esc_px(g, num, den);
        for (k = esc_px(g, num, den); k < esc_px(g + 1, num, den); k++) {
            errados += (2 * k + 1) * den / (2 * num) + sEstraga != g;
            n++;
        }
    }
    sprintf(t, "escala %d/%d em %d pixels do GS: %d de saída, %d pixels do GS vazios, %d de saída fora do raster", num, den,
            lado, n, vazios, errados);
    confere(vazios == 0 && errados == 0 && n == esc_px(lado, num, den), t);
    for (a = 0, b = 7; a < lado; a = b, b += 1 + (b * 13) % 37) { /* larguras de 1 a 37, ímpares e pares */
        if (b > lado) {
            b = lado;
        }
        soma += esc_px(b, num, den) - esc_px(a, num, den);
    }
    sprintf(t, "escala %d/%d: retângulos vizinhos somam %d de %d", num, den, soma, esc_px(lado, num, den));
    confere(soma == esc_px(lado, num, den), t);
}

/* (retomada) nas escalas da etapa 3 (meios 2, 3 e 4), os números da regra de antes: a borda floor(x * meios / 2) e o
   inverso (v * 2 + meios - 1) / meios, de 0 a 4.096 */
static void antes(int meios) {
    int x, difPx = 0, difGs = 0, num = meios % 2 == 0 ? meios / 2 : meios, den = meios % 2 == 0 ? 1 : 2;
    char t[128];
    for (x = 0; x <= 4096; x++) {
        difPx += esc_px(x, num, den) != x * meios / 2 + (sEstraga && x == 4096);
        difGs += esc_gs(x, num, den) != (x * 2 + meios - 1) / meios;
    }
    sprintf(t, "escala %d/%d (%d meios): esc_px e esc_gs de 0 a 4096 iguais aos de antes (%d e %d diferentes)", num, den,
            meios, difPx, difGs);
    confere(difPx == 0 && difGs == 0, t);
}

/* (etapa 3) a redução: uma imagem de w x h pixels do GS ampliada para a escala (cada pixel do GS repetido nos pixels de
   saída dele) e reduzida volta igual; numa de 2 x 2 diferentes, a média arredondada */
static void reduz(int num, int den) {
    static unsigned grande[64 * 48], volta[32 * 24];
    unsigned orig[32 * 24];
    int w = 32, h = 24, x, y, iguais = 0;
    char t[96];
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            orig[y * w + x] = (unsigned)(x * 7 + y * 131) * 2654435761u;
        }
    }
    for (y = 0; y < esc_px(h, num, den); y++) {
        for (x = 0; x < esc_px(w, num, den); x++) {
            grande[y * 64 + x] = orig[((2 * y + 1) * den / (2 * num)) * w + (2 * x + 1) * den / (2 * num)];
        }
    }
    esc_reduz(grande, 64, w, h, num, den, volta, 32);
    for (x = 0; x < w * h; x++) {
        iguais += volta[x] == orig[x] + (unsigned)sEstraga;
    }
    sprintf(t, "redução na escala %d/%d: %d de %d pixels iguais ao original", num, den, iguais, w * h);
    confere(iguais == w * h, t);
}

int main(int argc, char **argv) {
    int w, h, x0, y0, x1, y1, c, n, d;
    sEstraga = argc > 1 && strcmp(argv[1], "--estraga") == 0;

    /* a tabela da seção 3 (tiles de 80 x 16) */
    aceita(1024, 1024, 1, 1, 1024, 1024, 1040, 832); /* hoje: 832 + 832 */
    aceita(640, 448, 1, 1, 640, 448, 640, 224);      /* a área: 224 + 224 */
    aceita(640, 448, 3, 2, 960, 672, 960, 504);      /* 1,5 */
    aceita(640, 448, 2, 1, 1280, 896, 1280, 896);    /* 2: 896 + 896 de 2.048 */
    aceita(704, 480, 1, 1, 704, 480, 720, 270);      /* uma largura que não é múltipla de 80: a vista como cor arredonda */
    /* (retomada) as escalas não inteiras com a área em que a escala dá inteiro e múltiplo de 32 */
    aceita(640, 460, 8, 5, 1024, 736, 1040, 598);    /* 1,6: 598 + 598 */
    aceita(648, 456, 4, 3, 864, 608, 880, 418);      /* 1,33: 418 + 418 */
    aceita(640, 512, 5, 4, 800, 640, 800, 400);      /* 1,25: 400 + 400 */
    confere(esc_tiles(1200, 720) == 675, "1200 x 720 (1,875 x 1,607): 675 tiles");
    confere(esc_tiles(1024, 720) == 585, "1024 x 720 (1,6): 585 tiles");

    /* as recusas */
    recusa(1024, 1024, 2, 1); /* 2048 x 2048: 3.328 + 3.328 tiles, não cabe na EDRAM */
    recusa(1024, 1024, 3, 2); /* 1536 x 1536: 1.920 + 1.920 */
    recusa(512, 448, 1, 1);   /* menor que a imagem do jogo (a cópia dos ids e as capturas leem 640 x 448) */
    recusa(640, 440, 1, 1);   /* idem na altura */
    recusa(2048, 448, 1, 1);  /* maior que o alvo do GS */
    recusa(656, 448, 1, 1);   /* 656 não é múltiplo de 32 (o pitch da textura tiled) */
    recusa(640, 464, 1, 1);   /* 464 não é múltiplo de 32 */
    recusa(672, 480, 3, 2);   /* 1008 x 720: 1008 não é múltiplo de 32 */
    recusa(640, 448, 1, 2);   /* escala 0,5 */
    recusa(640, 448, 5, 2);   /* escala 2,5 */
    recusa(640, 448, 8, 5);   /* 1,6: 716,8 linhas, não inteira */
    recusa(640, 448, 4, 3);   /* 1,33: 853,3 x 597,3 */
    recusa(640, 448, 5, 4);   /* 1,25: 800 x 560, 560 não é múltiplo de 32 */
    recusa(640, 450, 8, 5);   /* 1,6: 1024 x 720, as 720 linhas não são múltiplo de 32 */
    recusa(640, 460, 1, 1);   /* a área da 1,6 na escala 1: 460 não é múltiplo de 32 */
    recusa(1024, 1024, 8, 5); /* 1638,4 */
    recusa(960, 640, 8, 5);   /* 1536 x 1024: 1.240 + 1.240 tiles, não cabe na EDRAM */
    recusa(640, 448, 7, 5);   /* 1,4: fora da lista */

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
    confere(esc_le_escala("1", &n, &d) && n == 1 && d == 1 && esc_le_escala("1.5", &n, &d) && n == 3 && d == 2 &&
                esc_le_escala("1,5", &n, &d) && n == 3 && d == 2 && esc_le_escala("2", &n, &d) && n == 2 && d == 1,
            "BT3_ESCALA 1, 1.5, 1,5 e 2");
    confere(esc_le_escala("1.6", &n, &d) && n == 8 && d == 5 && esc_le_escala("1,6", &n, &d) && n == 8 && d == 5 &&
                esc_le_escala("1.33", &n, &d) && n == 4 && d == 3 && esc_le_escala("1,33", &n, &d) && n == 4 && d == 3 &&
                esc_le_escala("1.25", &n, &d) && n == 5 && d == 4 && esc_le_escala("1,25", &n, &d) && n == 5 && d == 4,
            "BT3_ESCALA 1.6, 1.33 e 1.25 (ponto ou vírgula)");
    confere(!esc_le_escala("3", &n, &d) && !esc_le_escala("", &n, &d) && !esc_le_escala("15", &n, &d) &&
                !esc_le_escala("1.3", &n, &d) && !esc_le_escala("1.333", &n, &d) && !esc_le_escala("1.7", &n, &d) &&
                !esc_le_escala("1.50", &n, &d) && !esc_le_escala("1.6 ", &n, &d) && !esc_le_escala("1;6", &n, &d) &&
                !esc_le_escala(NULL, &n, &d),
            "BT3_ESCALA fora: recusada");
    confere(strcmp(esc_nome(1, 1), "1") == 0 && strcmp(esc_nome(4, 3), "1,33") == 0 && strcmp(esc_nome(8, 5), "1,6") == 0 &&
                esc_nome(7, 5) == NULL,
            "esc_nome: o texto do log");

    /* (etapa 3) as bordas na escala, os tamanhos e a redução das capturas */
    confere(esc_px(640, 3, 2) == 960 && esc_px(448, 3, 2) == 672 && esc_px(640, 2, 1) == 1280 && esc_px(448, 2, 1) == 896 &&
                esc_px(512, 3, 2) == 768 && esc_px(333, 1, 1) == 333,
            "esc_px: 640 x 448 em 960 x 672 e 1280 x 896; 1 na escala 1");
    confere(esc_px(1, 3, 2) == 1 && esc_px(2, 3, 2) == 3 && esc_px(3, 3, 2) == 4 && esc_px(5, 3, 2) == 7 + sEstraga,
            "esc_px na escala 1,5: as bordas 1, 3, 4 e 7 (floor, não floor + 0,5)");
    confere(esc_gs(960, 3, 2) == 640 && esc_gs(1280, 2, 1) == 640 && esc_gs(1, 3, 2) == 1 && esc_gs(3, 2, 1) == 2 &&
                esc_gs(448, 1, 1) == 448,
            "esc_gs: o inverso arredondado para cima");
    /* (retomada) as não inteiras: ceil(x * s - 0,5); na 1,6 o pixel 1 do GS começa no de saída 2 (o centro do 1, 1,5 / 1,6,
       é do pixel 0), na 1,25 o pixel 2 começa no 2 (o centro, 2,5, cai exatamente na borda 2 x 1,25: é do da direita) */
    confere(esc_px(1, 8, 5) == 2 && esc_px(2, 8, 5) == 3 && esc_px(3, 8, 5) == 5 && esc_px(5, 8, 5) == 8 &&
                esc_px(640, 8, 5) == 1024 && esc_px(460, 8, 5) == 736 && esc_px(448, 8, 5) == 717 + sEstraga,
            "esc_px na escala 1,6: as bordas 2, 3, 5 e 8; 640 x 460 em 1024 x 736, 448 linhas em 717");
    confere(esc_px(1, 4, 3) == 1 && esc_px(2, 4, 3) == 3 && esc_px(3, 4, 3) == 4 && esc_px(648, 4, 3) == 864 &&
                esc_px(456, 4, 3) == 608 && esc_px(640, 4, 3) == 853 && esc_px(448, 4, 3) == 597,
            "esc_px na escala 1,33: as bordas 1, 3 e 4; 648 x 456 em 864 x 608, 640 x 448 em 853 x 597");
    confere(esc_px(1, 5, 4) == 1 && esc_px(2, 5, 4) == 2 && esc_px(3, 5, 4) == 4 && esc_px(4, 5, 4) == 5 &&
                esc_px(640, 5, 4) == 800 && esc_px(512, 5, 4) == 640 && esc_px(448, 5, 4) == 560,
            "esc_px na escala 1,25: as bordas 1, 2, 4 e 5; 640 x 512 em 800 x 640");
    confere(esc_gs(1024, 8, 5) == 640 && esc_gs(717, 8, 5) == 449 && esc_gs(864, 4, 3) == 648 && esc_gs(800, 5, 4) == 640,
            "esc_gs nas não inteiras");
    antes(2);
    antes(3);
    antes(4);
    bordas(1, 1, 1024);
    bordas(3, 2, 1024);
    bordas(2, 1, 1024);
    bordas(8, 5, 1024);
    bordas(4, 3, 1024);
    bordas(5, 4, 1024);
    reduz(1, 1);
    reduz(3, 2);
    reduz(2, 1);
    reduz(8, 5);
    reduz(4, 3);
    reduz(5, 4);
    {
        unsigned q[4], m;
        q[0] = 0x0A141E29u; q[1] = 0x0A141E29u; q[2] = 0x14281E2Au; q[3] = 0x14281E2Au; /* 2 x 2 na escala 2 (pitch 2) */
        esc_reduz(q, 2, 1, 1, 2, 1, &m, 1);
        confere(m == 0x0F1E1E2Au - (unsigned)sEstraga, "redução de 2 x 2: a média por canal, arredondada (0x29 e 0x2A: 0x2A)");
    }

    printf("teste_escala: %d casos, %d falhas%s\n", sCasos, sFalhas, sEstraga ? " (com --estraga: o controle)" : "");
    return sFalhas != 0;
}
