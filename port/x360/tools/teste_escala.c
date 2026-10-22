/*
 * F5, etapa 8 (a resolução interna maior, etapas 2 e 3): o teste das contas do port/x360/src/gs/escala.h no Linux, contra a
 * tabela da seção 3 de docs/x360/plano/F5/08-720p.md (tiles da EDRAM por área e escala), as recusas, os alinhamentos do
 * Resolve e da profundidade, a tesoura presa na área, a leitura das chaves e (etapa 3) as bordas na escala (esc_px: a
 * regra do rasterizador, retângulos vizinhos sem fresta nem sobreposição) e a redução das capturas (esc_reduz).
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

/* (etapa 3) as bordas na escala `meios`, de 0 a `lado` pixels do GS: cada pixel do GS com pelo menos um de saída, cada
   pixel de saída k no pixel do GS que o rasterizador dá com os centros em .5, floor((k + 0,5) / s) = (2k + 1) / meios, e
   retângulos vizinhos (cortados em pontos quaisquer) somando a largura inteira, sem fresta nem sobreposição */
static void bordas(int meios, int lado) {
    int g, k, a, b, soma = 0, vazios = 0, errados = 0, n = 0;
    char t[128];
    for (g = 0; g < lado; g++) {
        vazios += esc_px(g + 1, meios) <= esc_px(g, meios);
        for (k = esc_px(g, meios); k < esc_px(g + 1, meios); k++) {
            errados += (2 * k + 1) / meios + sEstraga != g;
            n++;
        }
    }
    sprintf(t, "escala de %d meios em %d pixels do GS: %d de saída, %d pixels do GS vazios, %d de saída fora do raster",
            meios, lado, n, vazios, errados);
    confere(vazios == 0 && errados == 0 && n == esc_px(lado, meios), t);
    for (a = 0, b = 7; a < lado; a = b, b += 1 + (b * 13) % 37) { /* larguras de 1 a 37, ímpares e pares */
        if (b > lado) {
            b = lado;
        }
        soma += esc_px(b, meios) - esc_px(a, meios);
    }
    sprintf(t, "escala de %d meios: retângulos vizinhos somam %d de %d", meios, soma, esc_px(lado, meios));
    confere(soma == esc_px(lado, meios), t);
}

/* (etapa 3) a redução: uma imagem de w x h pixels do GS ampliada para a escala (cada pixel do GS repetido nos pixels de
   saída dele) e reduzida volta igual; numa de 2 x 2 diferentes, a média arredondada */
static void reduz(int meios) {
    static unsigned grande[64 * 48], volta[32 * 24];
    unsigned orig[32 * 24];
    int w = 32, h = 24, x, y, iguais = 0;
    char t[96];
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            orig[y * w + x] = (unsigned)(x * 7 + y * 131) * 2654435761u;
        }
    }
    for (y = 0; y < esc_px(h, meios); y++) {
        for (x = 0; x < esc_px(w, meios); x++) {
            grande[y * 64 + x] = orig[((2 * y + 1) / meios) * w + (2 * x + 1) / meios];
        }
    }
    esc_reduz(grande, 64, w, h, meios, volta, 32);
    for (x = 0; x < w * h; x++) {
        iguais += volta[x] == orig[x] + (unsigned)sEstraga;
    }
    sprintf(t, "redução de %d meios: %d de %d pixels iguais ao original", meios, iguais, w * h);
    confere(iguais == w * h, t);
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

    /* (etapa 3) as bordas na escala, os tamanhos e a redução das capturas */
    confere(esc_px(640, 3) == 960 && esc_px(448, 3) == 672 && esc_px(640, 4) == 1280 && esc_px(448, 4) == 896 &&
                esc_px(512, 3) == 768 && esc_px(333, 2) == 333,
            "esc_px: 640 x 448 em 960 x 672 e 1280 x 896; 1 na escala 1");
    confere(esc_px(1, 3) == 1 && esc_px(2, 3) == 3 && esc_px(3, 3) == 4 && esc_px(5, 3) == 7 + sEstraga,
            "esc_px na escala 1,5: as bordas 1, 3, 4 e 7 (floor, não floor + 0,5)");
    confere(esc_gs(960, 3) == 640 && esc_gs(1280, 4) == 640 && esc_gs(1, 3) == 1 && esc_gs(3, 4) == 2 && esc_gs(448, 2) == 448,
            "esc_gs: o inverso arredondado para cima");
    bordas(2, 1024);
    bordas(3, 1024);
    bordas(4, 1024);
    reduz(2);
    reduz(3);
    reduz(4);
    {
        unsigned q[4], m;
        q[0] = 0x0A141E29u; q[1] = 0x0A141E29u; q[2] = 0x14281E2Au; q[3] = 0x14281E2Au; /* 2 x 2 na escala 2 (pitch 2) */
        esc_reduz(q, 2, 1, 1, 4, &m, 1);
        confere(m == 0x0F1E1E2Au - (unsigned)sEstraga, "redução de 2 x 2: a média por canal, arredondada (0x29 e 0x2A: 0x2A)");
    }

    printf("teste_escala: %d casos, %d falhas%s\n", sCasos, sFalhas, sEstraga ? " (com --estraga: o controle)" : "");
    return sFalhas != 0;
}
