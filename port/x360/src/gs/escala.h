/*
 * F5, etapa 8 (a resolução interna maior, etapa 2; docs/x360/plano/F5/08-720p.md): as contas da área dos alvos e da
 * escala, em C89 puro e sem o XDK. O gs_d3d.cpp as usa; port/x360/tools/teste_escala.c as testa no Linux com o gcc.
 *
 * A área é em pixels do GS (o alvo lógico do gs_draw.c é 1024 x 1024; o jogo desenha no máximo 640 x 448); a escala, em
 * meios (2 é 1, 3 é 1,5, 4 é 2); a área vezes a escala, em pixels de saída, é o tamanho dos alvos, das profundidades e
 * do par na EDRAM. As regras de uma área aceita:
 *   - de 640 x 448 (a imagem do jogo: a cópia dos ids e as capturas a leem inteira) até 1024 x 1024 (o alvo lógico);
 *   - largura e altura em pixels de saída múltiplas de 32: o pitch da textura tiled é a largura exata (o autoteste da
 *     profundidade compara pitch / 4 x altura palavras com largura x altura) e as linhas de 0 a altura - 1 são as
 *     primeiras pitch x altura bytes (blocos de 32 linhas); múltiplas de 32 também são de 8, a exigência do Resolve;
 *   - cor e profundidade cabem juntas nos 2.048 tiles da EDRAM (80 x 16 pixels de 32 bits cada), sem tiling predicado.
 */
#ifndef BT3_ESCALA_H
#define BT3_ESCALA_H

#define ESC_GS_LADO 1024     /* o alvo lógico do GS (GS_W e GS_H do gs_draw.h) */
#define ESC_IMG_W 640        /* a imagem do jogo: FBW 10, 448 linhas */
#define ESC_IMG_H 448
#define ESC_TILE_W 80        /* um tile da EDRAM, 32 bits por pixel, sem MSAA: 5.120 bytes */
#define ESC_TILE_H 16
#define ESC_EDRAM_TILES 2048 /* 10 MB */

typedef struct EscArea {
    int w, h;       /* a área, em pixels do GS */
    int meios;      /* a escala em meios */
    int sw, sh;     /* em pixels de saída: os alvos, as profundidades e o par da EDRAM */
    int swz;        /* sw arredondada a 80: a profundidade da EDRAM vista como cor (as metades de 40 colunas trocadas) */
    unsigned tilesCor, tilesProf;
} EscArea;

/* tiles da EDRAM de uma superfície de 32 bits por pixel (o que o XGSurfaceSize dá para A8R8G8B8 e D24S8) */
static unsigned esc_tiles(int w, int h) {
    return (unsigned)((w + ESC_TILE_W - 1) / ESC_TILE_W) * (unsigned)((h + ESC_TILE_H - 1) / ESC_TILE_H);
}

/* um inteiro decimal positivo no começo de s; *fim aponta depois dele. 0: nenhum dígito ou grande demais */
static int esc_num(const char *s, const char **fim) {
    int v = 0, n = 0;
    while (*s >= '0' && *s <= '9' && n < 6) {
        v = v * 10 + (*s - '0');
        s++;
        n++;
    }
    *fim = s;
    return n > 0 && n < 6 ? v : 0;
}

/* "LxA" (x ou X, sem espaços); 1: lida */
static int esc_le_area(const char *s, int *w, int *h) {
    const char *p;
    int a, b;
    if (s == NULL) {
        return 0;
    }
    a = esc_num(s, &p);
    if (a == 0 || (*p != 'x' && *p != 'X')) {
        return 0;
    }
    b = esc_num(p + 1, &p);
    if (b == 0 || *p != '\0') {
        return 0;
    }
    *w = a;
    *h = b;
    return 1;
}

/* "1", "1.5" (ou "1,5") e "2" em meios; 0: outro valor */
static int esc_le_escala(const char *s) {
    if (s == NULL) {
        return 0;
    }
    if (s[0] == '1' && s[1] == '\0') {
        return 2;
    }
    if (s[0] == '1' && (s[1] == '.' || s[1] == ',') && s[2] == '5' && s[3] == '\0') {
        return 3;
    }
    if (s[0] == '2' && s[1] == '\0') {
        return 4;
    }
    return 0;
}

/* Monta a área w x h (pixels do GS) na escala `meios`. NULL: aceita (a preenchida); senão o motivo da recusa (a fica
   como estava). */
static const char *esc_monta(EscArea *a, int w, int h, int meios) {
    EscArea n;
    if (meios < 2 || meios > 4) {
        return "escala fora de 1, 1,5 e 2";
    }
    if (w < ESC_IMG_W || h < ESC_IMG_H) {
        return "menor que a imagem do jogo (640 x 448)";
    }
    if (w > ESC_GS_LADO || h > ESC_GS_LADO) {
        return "maior que o alvo do GS (1024 x 1024)";
    }
    if ((w * meios) % 2 != 0 || (h * meios) % 2 != 0) {
        return "não inteira na escala";
    }
    n.w = w;
    n.h = h;
    n.meios = meios;
    n.sw = w * meios / 2;
    n.sh = h * meios / 2;
    if (n.sw % 32 != 0 || n.sh % 32 != 0) {
        return "largura ou altura (na escala) não múltipla de 32";
    }
    n.swz = (n.sw + ESC_TILE_W - 1) / ESC_TILE_W * ESC_TILE_W;
    n.tilesCor = esc_tiles(n.sw, n.sh);
    n.tilesProf = esc_tiles(n.sw, n.sh);
    if (n.tilesCor + n.tilesProf > ESC_EDRAM_TILES) {
        return "cor e profundidade não cabem na EDRAM";
    }
    *a = n;
    return 0;
}

/* o retângulo [x0, x1) x [y0, y1) em múltiplos de 8 (exigência do Resolve), dentro de w x h */
static void esc_alinha8(int *x0, int *y0, int *x1, int *y1, int w, int h) {
    *x0 &= ~7;
    *y0 &= ~7;
    *x1 = (*x1 + 7) & ~7;
    *y1 = (*y1 + 7) & ~7;
    if (*x1 > w) *x1 = w;
    if (*y1 > h) *y1 = h;
}

/* (a profundidade) e o x em múltiplos de 80, a largura de um tile da EDRAM: o restauro como cor lê o pixel x + 40 ou
   x - 40 do mesmo tile, que assim fica sempre dentro do que foi resolvido. A borda direita fica presa em w (o restauro
   leva a última coluna até a swz, ver prof_poe) */
static void esc_alinha80(int *x0, int *y0, int *x1, int *y1, int w, int h) {
    esc_alinha8(x0, y0, x1, y1, w, h);
    *x0 -= *x0 % ESC_TILE_W;
    *x1 = (*x1 + ESC_TILE_W - 1) / ESC_TILE_W * ESC_TILE_W;
    if (*x1 > w) *x1 = w;
}

/* A tesoura [x0, x1) x [y0, y1) presa em 0..w x 0..h; devolve 1 se algo dela passava da área (o desenho foi cortado) */
static int esc_prende(int *x0, int *y0, int *x1, int *y1, int w, int h) {
    int cortou = *x1 > w || *y1 > h;
    if (*x0 < 0) *x0 = 0;
    if (*y0 < 0) *y0 = 0;
    if (*x1 > w) *x1 = w;
    if (*y1 > h) *y1 = h;
    return cortou;
}

#endif
