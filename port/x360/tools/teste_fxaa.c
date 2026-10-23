/*
 * F5, etapa 8, 720p (docs/x360/plano/F5/08-720p.md, seção 12): o modelo do FXAA da BT3_FXAA no Linux, sobre imagens
 * sintéticas (nenhum dado do jogo). As contas são as do ps_fxaa do gs_d3d.hlsl em float, com o bilinear do sampler preso na
 * borda (o centro do texel em .5, como a GPU com o HALFPIXELOFFSET), na resolução interna da 1,33 (a parte usada de
 * 688 x 600 cortada a 160 x 120 por imagem, que basta: o FXAA só olha até 4 texels em volta).
 *
 * O que se confere:
 *   - uma imagem lisa e um degradê suave (abaixo do limiar) saem iguais, pixel a pixel ("igual": o mesmo byte, menos de meio
 *     passo de 1/255);
 *   - um ponto isolado sai igual (a direção zero do ps_fxaa: as leituras caem no centro);
 *   - uma borda reta horizontal ou vertical sai igual, e uma caixa com a moldura de 1 pixel (a interface) fora dos cantos;
 *   - longe das bordas (mais de 5 pixels de qualquer borda), nada muda;
 *   - cada pixel de saída fica entre o mínimo e o máximo da vizinhança de 9 x 9 (a conta é uma média de leituras);
 *   - a borda de um semiplano desenhado sem antisserrilhado (o centro do pixel dentro ou fora), em 16 ângulos de 3 a 87 graus
 *     e 4 deslocamentos: o erro médio contra a cobertura de verdade (a fração de cada pixel dentro do semiplano, por 16 x 16
 *     amostras) cai nos ângulos entre 15 e 75 graus para no máximo 75% do erro sem o FXAA, e em nenhum ângulo sobe mais de 5%;
 *   - nas mesmas bordas, a luma de nenhum pixel sai do intervalo da luma dos 3 x 3 em volta por mais de meio passo de byte
 *     (nada de halo).
 * E imprime o erro por ângulo, com e sem o FXAA.
 *
 *   gcc -std=c89 -pedantic -Wall -Wextra -Werror port/x360/tools/teste_fxaa.c -lm -o /tmp/teste_fxaa
 *   /tmp/teste_fxaa            # 0: tudo bate
 *   /tmp/teste_fxaa --estraga  # o controle: a direção girada de 90 graus (mistura atravessando a borda); TEM de sair com 1
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 160
#define H 120
#define PI 3.14159265358979

typedef struct { float r, g, b, a; } Cor;

static Cor sImg[H][W], sSai[H][W];
static float sCob[H][W]; /* a cobertura de verdade (0 a 1) do semiplano */
static int sCasos, sFalhas, sEstraga, sVerboso;

#define FXAA_LIMIAR 0.125f
#define FXAA_LIMIAR_MIN 0.05f
#define FXAA_NITIDEZ 8.0f

static float luma(Cor c) { return c.r * 0.299f + c.g * 0.587f + c.b * 0.114f; }

static Cor cor_soma(Cor a, Cor b) { Cor c; c.r = a.r + b.r; c.g = a.g + b.g; c.b = a.b + b.b; c.a = a.a + b.a; return c; }
static Cor cor_vezes(Cor a, float k) { Cor c; c.r = a.r * k; c.g = a.g * k; c.b = a.b * k; c.a = a.a * k; return c; }

static int prende(int i, int n) { return i < 0 ? 0 : i >= n ? n - 1 : i; }

/* o tex2Dlod do sampler linear preso na borda, em uv (o texel i tem o centro em (i + 0,5) / W) */
static Cor le(float u, float v) {
    float x = u * W - 0.5f, y = v * H - 0.5f, fx, fy;
    int x0 = (int)floor(x), y0 = (int)floor(y);
    Cor a, b, c, d;
    fx = x - (float)x0;
    fy = y - (float)y0;
    a = sImg[prende(y0, H)][prende(x0, W)];
    b = sImg[prende(y0, H)][prende(x0 + 1, W)];
    c = sImg[prende(y0 + 1, H)][prende(x0, W)];
    d = sImg[prende(y0 + 1, H)][prende(x0 + 1, W)];
    return cor_soma(cor_soma(cor_vezes(a, (1 - fx) * (1 - fy)), cor_vezes(b, fx * (1 - fy))),
                    cor_soma(cor_vezes(c, (1 - fx) * fy), cor_vezes(d, fx * fy)));
}

static float fmaxf2(float a, float b) { return a > b ? a : b; }
static float fminf2(float a, float b) { return a < b ? a : b; }
static float prendef(float x, float a, float b) { return x < a ? a : x > b ? b : x; }

/* o ps_fxaa, linha a linha */
static Cor ps_fxaa(float u, float v) {
    float tx = 1.0f / W, ty = 1.0f / H;
    Cor m = le(u, v), perto, longe, s;
    float nw = luma(le(u - 0.5f * tx, v - 0.5f * ty));
    float ne = luma(le(u + 0.5f * tx, v - 0.5f * ty));
    float sw = luma(le(u - 0.5f * tx, v + 0.5f * ty));
    float se = luma(le(u + 0.5f * tx, v + 0.5f * ty));
    float lm = luma(m);
    float maxC = fmaxf2(fmaxf2(nw, ne), fmaxf2(sw, se));
    float minC = fminf2(fminf2(nw, ne), fminf2(sw, se));
    float d, lb, dx, dy, k, d2x, d2y;
    if (fmaxf2(maxC, lm) - fminf2(minC, lm) < fmaxf2(FXAA_LIMIAR_MIN, maxC * FXAA_LIMIAR)) {
        return m;
    }
    d = sw - ne;
    lb = se - nw;
    dx = d + lb;
    dy = d - lb;
    if (sEstraga) { /* o controle: a perpendicular (a mistura atravessa a borda) */
        float t = dx;
        dx = -dy;
        dy = t;
    }
    k = 1.0f / (float)sqrt(fmaxf2(dx * dx + dy * dy, 1.0e-8f));
    dx *= k;
    dy *= k;
    perto = cor_soma(le(u - dx * 0.5f * tx, v - dy * 0.5f * ty), le(u + dx * 0.5f * tx, v + dy * 0.5f * ty));
    k = fmaxf2(fminf2((float)fabs(dx), (float)fabs(dy)) * FXAA_NITIDEZ, 1.0f / 1024.0f);
    d2x = prendef(dx / k, -2.0f, 2.0f);
    d2y = prendef(dy / k, -2.0f, 2.0f);
    longe = cor_soma(cor_vezes(cor_soma(le(u - d2x * 2.0f * tx, v - d2y * 2.0f * ty), le(u + d2x * 2.0f * tx, v + d2y * 2.0f * ty)),
                               0.25f),
                     cor_vezes(perto, 0.25f));
    lb = luma(longe);
    if (lb < minC || lb > maxC) {
        longe = cor_vezes(perto, 0.5f);
    }
    s = longe;
    s.a = m.a;
    return s;
}

static void passa(void) {
    int x, y;
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            sSai[y][x] = ps_fxaa(((float)x + 0.5f) / W, ((float)y + 0.5f) / H);
        }
    }
}

static void falha(const char *caso, int x, int y, const char *o_que) {
    sFalhas++;
    if (sFalhas <= 12 || sVerboso) {
        printf("  FALHA: %s, pixel %d,%d: %s\n", caso, x, y, o_que);
    }
}

/* o mesmo byte: menos de meio passo de 1/255 (a saída é RGBA8; a conta em float deixa uns 2e-5 numa borda reta, da direção
   quase zero num eixo) */
#define MEIO_BYTE (0.5 / 255.0)
static int igual(Cor a, Cor b) {
    return fabs(a.r - b.r) < MEIO_BYTE && fabs(a.g - b.g) < MEIO_BYTE && fabs(a.b - b.b) < MEIO_BYTE && fabs(a.a - b.a) < MEIO_BYTE;
}

/* toda a saída igual à entrada */
static void confere_igual(const char *caso) {
    int x, y, n = 0;
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            sCasos++;
            if (!igual(sSai[y][x], sImg[y][x]) && n++ == 0) {
                falha(caso, x, y, "mudou");
            }
        }
    }
    if (n > 1) {
        sFalhas += 0; /* (contada uma vez por imagem) */
    }
}

static Cor cor(float r, float g, float b) { Cor c; c.r = r; c.g = g; c.b = b; c.a = 0.5f; return c; }
static const float kClaro[3] = {0.9f, 0.8f, 0.7f}, kEscuro[3] = {0.1f, 0.15f, 0.2f};

static void lisas(void) {
    int x, y;
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) sImg[y][x] = cor(0.4f, 0.5f, 0.6f);
    passa();
    confere_igual("imagem lisa");
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) sImg[y][x] = cor(0.2f + 0.003f * x, 0.3f + 0.002f * y, 0.5f);
    passa();
    confere_igual("degradê suave");
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) sImg[y][x] = cor(kEscuro[0], kEscuro[1], kEscuro[2]);
    sImg[60][80] = cor(kClaro[0], kClaro[1], kClaro[2]);
    passa();
    confere_igual("ponto isolado");
    for (y = 0; y < H; y++) for (x = 0; x < W; x++)
        sImg[y][x] = y < 50 ? cor(kClaro[0], kClaro[1], kClaro[2]) : cor(kEscuro[0], kEscuro[1], kEscuro[2]);
    passa();
    confere_igual("borda horizontal");
    for (y = 0; y < H; y++) for (x = 0; x < W; x++)
        sImg[y][x] = x < 70 ? cor(kEscuro[0], kEscuro[1], kEscuro[2]) : cor(kClaro[0], kClaro[1], kClaro[2]);
    passa();
    confere_igual("borda vertical");
    /* uma caixa da interface: o retângulo claro com a borda de 1 pixel escura */
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
        int dentro = x >= 30 && x < 130 && y >= 20 && y < 100, borda = dentro && (x == 30 || x == 129 || y == 20 || y == 99);
        sImg[y][x] = borda ? cor(kEscuro[0], kEscuro[1], kEscuro[2]) : dentro ? cor(kClaro[0], kClaro[1], kClaro[2])
                                                                           : cor(0.4f, 0.5f, 0.6f);
    }
    passa();
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            /* os cantos da caixa podem mudar (o FXAA vê uma borda em L); as retas longe dos cantos, não */
            int canto = (abs(x - 30) <= 4 || abs(x - 129) <= 4) && (abs(y - 20) <= 4 || abs(y - 99) <= 4);
            if (canto) continue;
            sCasos++;
            if (!igual(sSai[y][x], sImg[y][x])) {
                falha("caixa da interface (fora dos cantos)", x, y, "mudou");
                x = W;
                y = H;
            }
        }
    }
}

/* o semiplano cos(a) x + sin(a) y < c, pelo centro do pixel (a imagem serrilhada) e por 16 x 16 amostras (a cobertura) */
static void semiplano(double ang, double c) {
    int x, y, i, j;
    double ca = cos(ang), sa = sin(ang);
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            int dentro = ca * (x + 0.5) + sa * (y + 0.5) < c, n = 0;
            float k;
            for (j = 0; j < 16; j++) {
                for (i = 0; i < 16; i++) {
                    n += ca * (x + (i + 0.5) / 16.0) + sa * (y + (j + 0.5) / 16.0) < c;
                }
            }
            sCob[y][x] = (float)n / 256.0f;
            k = dentro ? 1.0f : 0.0f;
            sImg[y][x] = cor(kEscuro[0] + (kClaro[0] - kEscuro[0]) * k, kEscuro[1] + (kClaro[1] - kEscuro[1]) * k,
                             kEscuro[2] + (kClaro[2] - kEscuro[2]) * k);
        }
    }
}

/* a distância (em pixels) do centro do pixel à reta */
static double dist(double ang, double c, int x, int y) { return fabs(cos(ang) * (x + 0.5) + sin(ang) * (y + 0.5) - c); }

static void bordas(void) {
    int a, o, x, y, xx, yy;
    printf("A borda de um semiplano (claro sobre escuro), erro médio da luma contra a cobertura nos pixels da borda:\n");
    printf("  ângulo   sem FXAA   com FXAA   razão\n");
    for (a = 0; a < 16; a++) {
        double ang = (3.0 + a * 84.0 / 15.0) * PI / 180.0, eSem = 0.0, eCom = 0.0, razao;
        int n = 0;
        char caso[96];
        for (o = 0; o < 4; o++) {
            double c = cos(ang) * 80.0 + sin(ang) * 60.0 + 0.25 * o;
            semiplano(ang, c);
            passa();
            sprintf(caso, "semiplano a %.1f graus, deslocamento %d", ang * 180.0 / PI, o);
            for (y = 5; y < H - 5; y++) {
                for (x = 5; x < W - 5; x++) {
                    float lr = luma(cor(kEscuro[0] + (kClaro[0] - kEscuro[0]) * sCob[y][x],
                                        kEscuro[1] + (kClaro[1] - kEscuro[1]) * sCob[y][x],
                                        kEscuro[2] + (kClaro[2] - kEscuro[2]) * sCob[y][x]));
                    float lo = luma(sSai[y][x]), mn = 1.0f, mx = 0.0f, mn9 = 1.0f, mx9 = 0.0f;
                    for (yy = -4; yy <= 4; yy++) {
                        for (xx = -4; xx <= 4; xx++) {
                            float l = luma(sImg[y + yy][x + xx]);
                            if (abs(xx) <= 1 && abs(yy) <= 1) {
                                mn = fminf2(mn, l);
                                mx = fmaxf2(mx, l);
                            }
                            mn9 = fminf2(mn9, l);
                            mx9 = fmaxf2(mx9, l);
                        }
                    }
                    sCasos++;
                    if (lo < mn9 - 1.0e-5f || lo > mx9 + 1.0e-5f) {
                        falha(caso, x, y, "fora do mínimo e do máximo da vizinhança de 9 x 9");
                    }
                    if (lo < mn - 0.5f / 255.0f || lo > mx + 0.5f / 255.0f) {
                        falha(caso, x, y, "a luma saiu do intervalo dos 3 x 3 (halo)");
                    }
                    if (dist(ang, c, x, y) > 5.0) {
                        sCasos++;
                        if (!igual(sSai[y][x], sImg[y][x])) {
                            falha(caso, x, y, "mudou longe da borda");
                        }
                    }
                    if (sCob[y][x] > 0.0f && sCob[y][x] < 1.0f) {
                        eSem += fabs(luma(sImg[y][x]) - lr);
                        eCom += fabs(lo - lr);
                        n++;
                    }
                }
            }
        }
        eSem /= n;
        eCom /= n;
        razao = eCom / eSem;
        printf("  %5.1f    %.4f     %.4f     %.3f\n", ang * 180.0 / PI, eSem, eCom, razao);
        sCasos++;
        sprintf(caso, "semiplano a %.1f graus", ang * 180.0 / PI);
        if (ang * 180.0 / PI >= 15.0 && ang * 180.0 / PI <= 75.0 && razao > 0.75) {
            falha(caso, 0, 0, "o erro da borda não caiu para 75% ou menos");
        }
        if (razao > 1.05) {
            falha(caso, 0, 0, "o erro da borda subiu mais de 5%");
        }
    }
}

int main(int argc, char **argv) {
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--estraga") == 0) sEstraga = 1;
        if (strcmp(argv[i], "-v") == 0) sVerboso = 1;
    }
    lisas();
    bordas();
    printf("%d casos, %d falhas%s\n", sCasos, sFalhas, sEstraga ? " (com --estraga: a direção girada de 90 graus)" : "");
    return sFalhas != 0;
}
