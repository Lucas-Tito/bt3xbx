/*
 * F5, etapa 8 (a resolução interna maior, etapas 2 e 3; docs/x360/plano/F5/08-720p.md): as contas da área dos alvos e
 * da escala, em C89 puro e sem o XDK. O gs_d3d.cpp as usa; port/x360/tools/teste_escala.c as testa no Linux com o gcc.
 *
 * A área é em pixels do GS (o alvo lógico do gs_draw.c é 1024 x 1024; o jogo desenha no máximo 640 x 448); a escala, uma
 * fração num / den (720p, retomada: 1, 1,25 = 5/4, 1,33 = 4/3, 1,5 = 3/2, 1,6 = 8/5 e 2; antes, em meios); a área vezes a
 * escala, em pixels de saída e inteira, é o tamanho dos alvos, das profundidades e do par na EDRAM. As regras de uma área
 * aceita:
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
    int num, den;   /* a escala, num / den */
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

/* (720p, retomada) As escalas aceitas: 1, 1,25, 1,33, 1,5, 1,6 e 2, com o texto do log ("1,33" é 4/3). k: 0 a 5 (a
   escala k em *num / *den); NULL depois da última. */
static const char *esc_lista(int k, int *num, int *den) {
    static const char *const nome[6] = {"1", "1,25", "1,33", "1,5", "1,6", "2"};
    static const int fr[6][2] = {{1, 1}, {5, 4}, {4, 3}, {3, 2}, {8, 5}, {2, 1}};
    if (k < 0 || k >= 6) {
        return 0;
    }
    *num = fr[k][0];
    *den = fr[k][1];
    return nome[k];
}

/* o texto da escala num / den ("1,5"); NULL: não é uma das aceitas */
static const char *esc_nome(int num, int den) {
    int k, n, d;
    const char *t;
    for (k = 0; (t = esc_lista(k, &n, &d)) != 0; k++) {
        if (n == num && d == den) {
            return t;
        }
    }
    return 0;
}

/* BT3_ESCALA: "1", "1.25", "1.33", "1.5", "1.6" ou "2" (com ponto ou vírgula) em *num / *den; 0: outro valor */
static int esc_le_escala(const char *s, int *num, int *den) {
    int k, i, n, d;
    const char *t;
    if (s == NULL) {
        return 0;
    }
    for (k = 0; (t = esc_lista(k, &n, &d)) != 0; k++) {
        for (i = 0; t[i] != '\0' && (s[i] == t[i] || (s[i] == '.' && t[i] == ',')); i++) {
        }
        if (t[i] == '\0' && s[i] == '\0') {
            *num = n;
            *den = d;
            return 1;
        }
    }
    return 0;
}

/* Monta a área w x h (pixels do GS) na escala num / den. NULL: aceita (a preenchida); senão o motivo da recusa (a fica
   como estava). */
static const char *esc_monta(EscArea *a, int w, int h, int num, int den) {
    EscArea n;
    if (esc_nome(num, den) == 0) {
        return "escala fora de 1, 1,25, 1,33, 1,5, 1,6 e 2";
    }
    if (w < ESC_IMG_W || h < ESC_IMG_H) {
        return "menor que a imagem do jogo (640 x 448)";
    }
    if (w > ESC_GS_LADO || h > ESC_GS_LADO) {
        return "maior que o alvo do GS (1024 x 1024)";
    }
    if ((w * num) % den != 0 || (h * num) % den != 0) {
        return "não inteira na escala";
    }
    n.w = w;
    n.h = h;
    n.num = num;
    n.den = den;
    n.sw = w * num / den;
    n.sh = h * num / den;
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

/* (720p, etapa 3; retomada) Uma borda x em pixels do GS levada a pixels de saída na escala s = num / den, x >= 0:
   ceil(x * s - 0,5), em inteiros (2 x num + den - 1) / (2 den). É a regra do rasterizador com os centros dos pixels em .5
   (HALFPIXELOFFSET ligado): o pixel de saída k fica no pixel do GS floor((k + 0,5) / s), ou seja, no x com
   x * s - 0,5 <= k < (x + 1) * s - 0,5, então o pixel x do GS cobre de esc_px(x) a esc_px(x + 1) - 1 (na escala 1,5, 1 e 2
   pixels de saída alternados; o agrupamento da arte 2D, floor(f / s) * s no gs_pixel, dá o mesmo). As tesouras, os
   retângulos das nativas e a redução das capturas usam esta conta nas duas bordas: retângulos vizinhos do GS continuam
   vizinhos, sem fresta nem sobreposição. A etapa 3 usava floor(x * s), que só é a mesma conta enquanto a fração de x * s
   não passa de 0,5: vale nas escalas 1, 1,5 e 2 (os mesmos números de antes), mas na 1,6 o pixel 1 do GS começaria no
   pixel de saída 1, cujo centro (1,5 / 1,6 = 0,94) é do pixel 0. (A seção 4 do plano propunha floor(x * s + 0,5): na 1,5
   a tesoura ficaria um pixel de saída fora do que a primitiva da mesma borda cobre.) */
static int esc_px(int x, int num, int den) {
    return (2 * x * num + den - 1) / (2 * den);
}

/* o inverso, para os logs: quantos pixels do GS cobrem v pixels de saída (v / s arredondado para cima) */
static int esc_gs(int v, int num, int den) {
    return (v * den + num - 1) / num;
}

/* (720p, etapa 3) A captura reduzida: w x h pixels do GS em `saida` (`spitch` pixels por linha), cada um a média, canal a
   canal e arredondada, dos pixels de saída que ele cobre (esc_px) em `lin` (`pitch` pixels por linha, pelo menos
   esc_px(w) x esc_px(h)). Os pixels são palavras de 32 bits com 4 canais de 8 (A8R8G8B8 como u32). Na escala 1, a cópia. */
static void esc_reduz(const unsigned *lin, int pitch, int w, int h, int num, int den, unsigned *saida, int spitch) {
    int x, y, i, j, c;
    for (y = 0; y < h; y++) {
        int j0 = esc_px(y, num, den), j1 = esc_px(y + 1, num, den);
        for (x = 0; x < w; x++) {
            int i0 = esc_px(x, num, den), i1 = esc_px(x + 1, num, den);
            unsigned n = (unsigned)((i1 - i0) * (j1 - j0)), soma[4], r = 0;
            soma[0] = soma[1] = soma[2] = soma[3] = 0;
            for (j = j0; j < j1; j++) {
                for (i = i0; i < i1; i++) {
                    unsigned p = lin[j * pitch + i];
                    for (c = 0; c < 4; c++) {
                        soma[c] += (p >> (8 * c)) & 255u;
                    }
                }
            }
            for (c = 0; c < 4; c++) {
                r |= ((soma[c] + n / 2) / n) << (8 * c);
            }
            saida[y * spitch + x] = r;
        }
    }
}

#endif
