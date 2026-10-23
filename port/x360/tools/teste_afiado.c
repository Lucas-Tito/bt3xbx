/*
 * F5, etapa 8, 720p (docs/x360/plano/F5/08-720p.md, seção 10): o modelo do bilinear afiado da BT3_AFIADO no Linux, sobre
 * uma linha sintética de texels (nenhum dado do jogo). As contas são as do gs_d3d.hlsl em float: afiado_eixo (a arte 2D
 * no ps_gs_af, por eixo) e afiado_copia_eixo (a cópia para a tela no ps_copia_af).
 *
 * A arte 2D: um sprite de N texels no GS a partir de X0 (pixels do GS, com as frações de 1/16 do GS), com 1 pixel do GS
 * por texel (a arte desenhada texel a texel) ou 0,75 (o HUD estreitado do 16:9), desenhado na escala s: o pixel de saída
 * i entra se o centro i + 0,5 cai em [X0 s, X1 s) e lê o uv do centro, como a GPU interpola. Os casos: as escalas 1, 4/3,
 * 3/2 e 8/5, as 16 frações de X0, o filtro do jogo no ponto e linear. O que se confere:
 *   - na escala 1 com a arte alinhada, cada pixel lê o texel que o ponto lê (o do caminho de hoje, no canto do pixel do
 *     GS): nenhuma mudança (e o gs_d3d.cpp nem liga o ps_gs_af na escala 1);
 *   - na arte nítida (o ponto, ou um texel por pixel do GS) ampliada: no máximo 1 pixel misturado em cada borda entre dois
 *     texels, a mistura só entre texels vizinhos, o pixel sem mistura lê o texel do seu centro, e cada texel de dentro
 *     recebe peso total 1 / p (a média do pixel sobre os texels: nada some, nada engorda);
 *   - um texel por pixel de saída (o HUD estreitado na 1,33): cada texel de dentro em exatamente um pixel, sem mistura;
 *   - a cópia para a tela, nas razões de cada escala e forma: ampliada, no máximo 1 pixel misturado por borda e o peso
 *     certo; reduzida, o bilinear de sempre.
 * E imprime, contra o caminho de hoje (a leitura por pixel do GS), as colunas perdidas e as larguras dos texels.
 *
 *   gcc -std=c89 -pedantic -Wall -Wextra -Werror port/x360/tools/teste_afiado.c -lm -o /tmp/teste_afiado
 *   /tmp/teste_afiado            # 0: tudo bate
 *   /tmp/teste_afiado --estraga  # o controle: o bilinear comum no lugar do afiado; TEM de sair com 1
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#define N 37    /* texels do sprite */
#define MAXPX 1024
/* um pixel é misturado quando o peso do outro texel muda um byte em meio passo ou mais: abaixo disso é o erro do float na
   conta (o centro do pixel a exatamente meio pixel da borda dá peso 0 na conta exata e uns 1e-4 em float) */
#define MISTURA (1.0f / 512.0f)

static int sFalhas, sCasos, sEstraga, sVerboso;

static void confere(int ok, const char *o_que) {
    sCasos++;
    if (!ok) {
        sFalhas++;
        if (sFalhas <= 40) {
            printf("FALHA: %s\n", o_que);
        }
    }
}

static float fl(float x) { return (float)floor(x); }
static float cl(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
static float sg(float x) { return x > 0.0f ? 1.0f : x < 0.0f ? -1.0f : 0.0f; }

/* o afiado_eixo do gs_d3d.hlsl (jogo: o afiadoJogo.x, 1 com o filtro linear); com --estraga, o bilinear comum */
static void afiado_eixo(float t, float p, float pGs, int jogo, float *a, float *b, float *w) {
    float i = fl(t);
    float c = t - i - 0.5f;
    float r = 0.5f - 0.5f * p;
    float g = (c - cl(c, -r, r)) / (p > 1.0f / 4096.0f ? p : 1.0f / 4096.0f);
    int um = fabs(p - 1.0f) < 0.02f;
    int nitido = jogo == 0 || fabs(pGs - 1.0f) < 0.02f;
    int afia = !um && nitido && p < 1.0f;
    int lin = !um && !nitido;
    float il = fl(t - 0.5f);
    if (sEstraga) {
        lin = 1;
    }
    *a = lin ? il + 0.5f : i + 0.5f;
    *b = lin ? il + 1.5f : afia ? i + 0.5f + sg(g) : *a;
    *w = lin ? t - 0.5f - il : afia ? (float)fabs(g) : 0.0f;
}

/* o afiado_copia_eixo do gs_d3d.hlsl: o ponto que o bilinear da cópia lê */
static float afiado_copia_eixo(float t, float p) {
    float i = fl(t);
    float c = t - i - 0.5f;
    float r = 0.5f - 0.5f * p > 0.0f ? 0.5f - 0.5f * p : 0.0f;
    if (sEstraga) {
        return t;
    }
    return i + 0.5f + (c - cl(c, -r, r)) / cl(p, 1.0f / 4096.0f, 1.0f);
}

/* um pixel de saída: os dois texels lidos (índices) e o peso do segundo */
typedef struct { int px, ta, tb; float w, t; } Px;

/* O sprite na escala s: os pixels que entram e o que cada um lê pelo afiado (hoje = 0) ou pelo caminho de hoje (hoje = 1:
   o canto do pixel do GS do pixel, no ponto; com um texel por pixel do GS, o texel inteiro com o 1/64 do shader). */
static int desenha(double s, double x0, double gsPorTexel, int jogo, int hoje, Px *o) {
    double e0 = x0 * s, e1 = (x0 + N * gsPorTexel) * s;
    float p = (float)(1.0 / (gsPorTexel * s)), sf = (float)s;
    int i, n = 0;
    for (i = (int)floor(e0) - 1; i <= (int)ceil(e1) + 1; i++) {
        double c = i + 0.5;
        float t, a, b, w;
        if (c < e0 || c >= e1) {
            continue;
        }
        t = (float)((c / s - x0) / gsPorTexel);
        if (hoje) {
            /* gs_pixel sem a chave: g = floor(f / s) * s, uv no canto; passo = texels por pixel do GS */
            float f = (float)c, g = fl(f / sf) * sf, tc = t - p * (f - g), passo = p * sf;
            a = fabs(passo - 1.0f) < 0.02f ? fl(tc + 1.0f / 64.0f) + 0.5f : fl(tc) + 0.5f; /* (o jogo no ponto) */
            b = a;
            w = 0.0f;
        } else {
            afiado_eixo(t, p, p * sf, jogo, &a, &b, &w);
        }
        o[n].px = i;
        o[n].ta = (int)floor(a);
        o[n].tb = (int)floor(b);
        o[n].w = w;
        o[n].t = t;
        n++;
    }
    return n;
}

static const double kEsc[4] = {1.0, 4.0 / 3.0, 1.5, 1.6};
static const char *const kEscNome[4] = {"1", "1,33", "1,5", "1,6"};

static void arte(void) {
    static Px o[MAXPX], h[MAXPX];
    int ie, k, jogo, larg, n, i, j;
    char txt[200];
    printf("A arte 2D (N = %d texels; colunas perdidas e pixels sem mistura por texel de dentro, hoje e com o afiado):\n", N);
    for (ie = 0; ie < 4; ie++) {
        double s = kEsc[ie];
        for (larg = 0; larg < 2; larg++) {
            double gpt = larg ? 0.75 : 1.0;
            int perdHoje = 0, perdAf = 0, minH = 99, maxH = 0, minA = 99, maxA = 0, mistMax = 0, foraMax = 0;
            float pesoFora = 0.0f;
            for (jogo = 0; jogo < 2; jogo++) {
                for (k = 0; k < 16; k++) {
                    double x0 = 100.0 + k / 16.0;
                    float p = (float)(1.0 / (gpt * s)), pGs = p * (float)s;
                    int nitido = jogo == 0 || fabs(pGs - 1.0f) < 0.02f, um = fabs(p - 1.0f) < 0.02f;
                    int bordas[N + 2], cont[N + 2], puros[N + 2], contH[N + 2];
                    double peso[N + 2];
                    n = desenha(s, x0, gpt, jogo, 0, o);
                    memset(bordas, 0, sizeof(bordas));
                    memset(cont, 0, sizeof(cont));
                    memset(puros, 0, sizeof(puros));
                    memset(contH, 0, sizeof(contH));
                    for (j = 0; j < N + 2; j++) {
                        peso[j] = 0.0;
                    }
                    for (i = 0; i < n; i++) {
                        int misturado = o[i].w > MISTURA && o[i].ta != o[i].tb;
                        sprintf(txt, "escala %s, %s, jogo %s, X0 %.4f: pixel %d lê %d e %d (peso %.3f), só vizinhos",
                                kEscNome[ie], larg ? "estreitada" : "1 por pixel do GS", jogo ? "linear" : "ponto", x0,
                                o[i].px, o[i].ta, o[i].tb, o[i].w);
                        confere(o[i].tb - o[i].ta >= -1 && o[i].tb - o[i].ta <= 1, txt);
                        if (o[i].ta >= 0 && o[i].ta < N) {
                            peso[o[i].ta + 1] += 1.0 - o[i].w;
                            cont[o[i].ta + 1] += !misturado;
                        }
                        if (o[i].tb >= 0 && o[i].tb < N) {
                            peso[o[i].tb + 1] += o[i].w;
                        }
                        if ((o[i].ta < 0 || o[i].ta >= N || o[i].tb < 0 || o[i].tb >= N) && misturado) {
                            /* a borda da peça mistura com o texel de fora (numa folha, a peça vizinha) */
                            foraMax = 1;
                            if (o[i].w > pesoFora && (o[i].tb < 0 || o[i].tb >= N)) pesoFora = o[i].w;
                            if (1.0f - o[i].w > pesoFora && (o[i].ta < 0 || o[i].ta >= N)) pesoFora = 1.0f - o[i].w;
                        }
                        if (misturado) {
                            int bi = (o[i].ta < o[i].tb ? o[i].ta : o[i].tb) + 1;
                            if (bi >= 0 && bi < N + 2) {
                                bordas[bi]++;
                            }
                        } else if (nitido || um) {
                            sprintf(txt, "escala %s, %s, jogo %s, X0 %.4f: pixel %d sem mistura lê o texel do centro (%d, t %.4f)",
                                    kEscNome[ie], larg ? "estreitada" : "1 por pixel do GS", jogo ? "linear" : "ponto", x0,
                                    o[i].px, o[i].ta, o[i].t);
                            confere(o[i].ta == (int)floor(o[i].t), txt);
                        }
                    }
                    for (j = 1; j <= N && (nitido || um); j++) { /* (texel j - 1; o jogo linear fora: o bilinear de sempre) */
                        if (bordas[j] > mistMax) mistMax = bordas[j];
                    }
                    if (nitido || um) {
                        sprintf(txt, "escala %s, %s, jogo %s, X0 %.4f: no máximo 1 pixel misturado por borda",
                                kEscNome[ie], larg ? "estreitada" : "1 por pixel do GS", jogo ? "linear" : "ponto", x0);
                        for (j = 0; j < N + 2; j++) {
                            confere(bordas[j] <= 1, txt);
                        }
                    }
                    for (j = 2; j <= N - 1; j++) { /* os texels de dentro: 1 a N - 2 */
                        if (peso[j] < 1e-6) perdAf++;
                        if (cont[j] < minA) minA = cont[j];
                        if (cont[j] > maxA) maxA = cont[j];
                        if (nitido && p < 1.0f && !um) {
                            sprintf(txt, "escala %s, %s, jogo %s, X0 %.4f: texel %d com peso %.5f, esperado %.5f",
                                    kEscNome[ie], larg ? "estreitada" : "1 por pixel do GS", jogo ? "linear" : "ponto", x0,
                                    j - 1, peso[j], 1.0 / p);
                            confere(fabs(peso[j] - 1.0 / p) < 2e-3, txt);
                        }
                        if (um) {
                            sprintf(txt, "escala %s, %s, X0 %.4f: um texel por pixel, o texel %d em %d pixels sem mistura",
                                    kEscNome[ie], larg ? "estreitada" : "1 por pixel do GS", x0, j - 1, cont[j]);
                            confere(cont[j] == 1 && peso[j] > 0.999 && peso[j] < 1.001, txt);
                        }
                    }
                    /* hoje (o jogo no ponto) */
                    if (jogo == 0) {
                        int nh = desenha(s, x0, gpt, 0, 1, h);
                        for (i = 0; i < nh; i++) {
                            if (h[i].ta >= 0 && h[i].ta < N) contH[h[i].ta + 1]++;
                        }
                        for (j = 2; j <= N - 1; j++) {
                            if (contH[j] == 0) perdHoje++;
                            if (contH[j] < minH) minH = contH[j];
                            if (contH[j] > maxH) maxH = contH[j];
                        }
                        if (ie == 0 && larg == 0 && k == 0) {
                            /* a escala 1, alinhada: o afiado é o ponto de hoje, pixel a pixel */
                            sprintf(txt, "escala 1, alinhada: %d pixels com o afiado e %d hoje", n, nh);
                            confere(n == nh, txt);
                            for (i = 0; i < n && i < nh; i++) {
                                sprintf(txt, "escala 1, alinhada, jogo ponto: pixel %d lê %d (peso %.3f) com o afiado e %d hoje",
                                        o[i].px, o[i].ta, o[i].w, h[i].ta);
                                confere(o[i].px == h[i].px && o[i].ta == h[i].ta && o[i].w == 0.0f, txt);
                            }
                        }
                    }
                    if (ie == 0 && larg == 0 && k == 0 && jogo == 1) {
                        /* e com o jogo no linear: o mesmo ponto */
                        n = desenha(s, x0, gpt, 0, 1, h);
                        for (i = 0; i < n; i++) {
                            sprintf(txt, "escala 1, alinhada, jogo linear: pixel %d lê %d (peso %.3f), o ponto lê %d",
                                    o[i].px, o[i].ta, o[i].w, h[i].ta);
                            confere(o[i].ta == h[i].ta && o[i].w == 0.0f, txt);
                        }
                    }
                }
            }
            if (ie == 0 && larg == 1) {
                printf("  escala 1, estreitada: o gs_d3d.cpp não liga o afiado na escala 1 (hoje: %d colunas perdidas em 16 "
                       "posições, %d a %d pixels por texel)\n", perdHoje, minH, maxH);
                continue;
            }
            printf("  escala %-4s %-18s hoje: %3d colunas perdidas em 16 posições, %d a %d pixels por texel | afiado: %d "
                   "perdidas, %d a %d pixels sem mistura por texel, até %d misturado por borda na arte nítida%s\n",
                   kEscNome[ie], larg ? "estreitada (0,75):" : "1 por pixel do GS:", perdHoje, minH, maxH, perdAf, minA, maxA,
                   mistMax, foraMax ? "" : "");
            if (foraMax) {
                printf("               a borda da peça puxa o texel de fora com até %.0f%% (numa folha, a peça vizinha)\n",
                       pesoFora * 100.0);
            }
        }
    }
}

/* A cópia para a tela: src texels da imagem do GS para dst pixels; o bilinear do hardware no ponto do afiado_copia_eixo */
static void copia(const char *nome, double src, int dst) {
    float p = (float)(src / dst);
    int i, j, bordas[2048], misturados = 0, maxb = 0, puros = 0;
    double peso[2048];
    char txt[200];
    memset(bordas, 0, sizeof(bordas));
    for (j = 0; j < 2048; j++) {
        peso[j] = 0.0;
    }
    for (i = 0; i < dst; i++) {
        float t = (float)((i + 0.5) * src / dst), q = afiado_copia_eixo(t, p);
        float a = fl(q - 0.5f), w = q - 0.5f - a;
        int ia = (int)a;
        if (w <= MISTURA) {
            w = 0.0f;
        } else if (w >= 1.0f - MISTURA) {
            w = 0.0f;
            ia++;
        }
        if (ia >= 0 && ia < 2047) {
            peso[ia] += 1.0 - w;
            peso[ia + 1] += w;
            if (w > 0.0f) {
                bordas[ia]++;
                misturados++;
            } else {
                puros++;
            }
        }
        if (p >= 1.0f) {
            sprintf(txt, "cópia %s: reduzida, o bilinear de sempre no pixel %d (%.5f e %.5f)", nome, i, q, t);
            confere(q == t, txt); /* (a mesma conta: o ponto da leitura é o t) */
        }
    }
    if (p < 1.0f) {
        for (j = 0; j < 2048; j++) {
            if (bordas[j] > maxb) maxb = bordas[j];
            sprintf(txt, "cópia %s: borda %d com %d pixels misturados", nome, j, bordas[j]);
            confere(bordas[j] <= 1, txt);
        }
        for (j = 2; j < (int)src - 2; j++) {
            sprintf(txt, "cópia %s: texel %d com peso %.5f, esperado %.5f", nome, j, peso[j], 1.0 / p);
            confere(fabs(peso[j] - 1.0 / p) < 2e-3, txt);
        }
        printf("  %-34s %7.2f -> %4d (%.3f): %4d pixels sem mistura, %4d misturados, até %d por borda\n", nome, src, dst,
               1.0 / p, puros, misturados, maxb);
    } else {
        printf("  %-34s %7.2f -> %4d (%.3f): reduzida, o bilinear de sempre\n", nome, src, dst, 1.0 / p);
    }
}

int main(int argc, char **argv) {
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--estraga") == 0) sEstraga = 1;
        if (strcmp(argv[i], "-v") == 0) sVerboso = 1;
    }
    arte();
    printf("A cópia para a tela (a área de 512 x 448 do GS na escala, para 1280 x 720 no 16:9 ou 960 x 720 no 4:3):\n");
    copia("escala 1, 16:9, horizontal", 512.0, 1280);
    copia("escala 1, 16:9 e 4:3, vertical", 448.0, 720);
    copia("escala 1, 4:3, horizontal", 512.0, 960);
    copia("escala 1,33, 16:9, horizontal", 512.0 * 4.0 / 3.0, 1280);
    copia("escala 1,33, vertical", 448.0 * 4.0 / 3.0, 720);
    copia("escala 1,5, 16:9, horizontal", 768.0, 1280);
    copia("escala 1,5, vertical", 672.0, 720);
    copia("escala 1,6, 16:9, horizontal", 819.2, 1280);
    copia("escala 1,6, vertical", 716.8, 720);
    copia("escala 2, 16:9, horizontal", 1024.0, 1280);
    copia("escala 2, vertical", 896.0, 720);
    printf("%d casos, %d falhas%s\n", sCasos, sFalhas, sEstraga ? " (com --estraga: o bilinear comum)" : "");
    return sFalhas != 0;
}
