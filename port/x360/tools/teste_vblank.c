/*
 * F5, etapa 8 (docs/x360/plano/F5/08-gravacao.md, seção 20): a simulação da conta da espera pelo vblank de hardware
 * (port/x360/src/gs/vblank_alvo.h, a do gs_d3d.cpp) contra a conta de antes (a do 3ae20e6, sem o recomeço pela dívida
 * parada, copiada aqui), no Linux com o gcc. Só tempo simulado: nenhum byte do jogo.
 *
 * O modelo: o vblank de hardware a cada 16,635 ms (o medido no console, seção 19), com uma fase. Um passo da luta: a
 * thread do jogo lê o controle (o começo do passo), trabalha g, entrega a lista à gravação (espera se a gravação ainda não
 * tomou a anterior), e chama a espera de vblank duas vezes (um passo, dois vblanks). A gravação trabalha r e entrega ao
 * desenho (espera se o desenho está ocupado); o desenho trabalha d e faz o Present; a imagem entra no primeiro vblank
 * depois do Present, uma por vblank no máximo, e o desenho só começa a seguinte quando ela entra (com o desenho acima de
 * um vblank, como o frame_end de ~17 ms dos picos da janela 1, cada imagem fica exatamente dois vblanks na tela, as 685
 * imagens de 2 vblanks da janela 1 do vb1). O caminho do controle à tela de
 * um passo: do começo dele até o vblank em que a imagem dele entra.
 *
 * As cenas:
 *   A. a janela 1 do vb1 (seção 19), em dez fases do vídeo: 20 passos lentos pela gravação (~45 ms), depois a fila do
 *      desenho segura cada passo em exatamente dois vblanks (o jogo leve, a gravação de 20 a 24 ms, o desenho de 16,8 a
 *      18,5) e um pico da gravação no passo 120 (60 a 87 ms; o máximo da janela 1 foi 84). A conta de antes fica sem
 *      espera nenhuma, com o jogo um passo à frente da tela (o controle lido 6 vblanks antes de a imagem entrar); a nova
 *      recomeça o alvo, volta a esperar e o caminho do controle à tela cai, com as mesmas imagens na tela.
 *   B. o passo comum: 20 mil passos do jogo sem fila que segure (5% de picos de 34 a 60 ms), três fases do vídeo. As duas
 *      contas dão as mesmas esperas (0,5%), o mesmo período (0,05%), o mesmo caminho do controle à tela e as mesmas imagens
 *      de 3 vblanks (até 0,05% dos passos: os poucos recomeços numa sequência de passos de 30 a 33 ms custam uma ou outra).
 *   C. a dívida paga devagar: um pico de 90 ms e depois passos de 1,9 vblank. A conta nova paga a dívida como a de antes,
 *      sem recomeçar.
 *
 *   gcc -std=c89 -pedantic -Wall -Wextra -Werror -Iport/x360/src/gs port/x360/tools/teste_vblank.c -lm -o /tmp/teste_vblank
 *   /tmp/teste_vblank      # 0: tudo bate; 1: uma conferência falhou (as linhas FALHA dizem qual)
 * Os mutantes: uma cópia do vblank_alvo.h mudada (sed) numa pasta à parte, posta antes no -I; cada um TEM de sair com 1.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "vblank_alvo.h"

#define P 16.635        /* ms: o vblank do console (seção 19) */
#define DPC 0.05        /* ms: do vblank até a thread do jogo acordar */
#define NMAX 21000

static int sFalhas, sCasos;

static void confere(int ok, const char *o_que) {
    sCasos++;
    if (!ok) {
        sFalhas++;
        printf("FALHA: %s\n", o_que);
    }
}

/* um gerador simples e igual em toda máquina */
static unsigned long sSemente;
static double uni(double a, double b) {
    sSemente = (sSemente * 1103515245ul + 12345ul) & 0x7ffffffful;
    return a + (b - a) * (double)sSemente / 2147483648.0;
}

/* a conta de antes (a do 3ae20e6 no gs_d3d.cpp): igual à nova sem o recomeço pela dívida parada */
typedef struct Antes {
    unsigned alvo;
    int tem;
} Antes;

static int antes_decide(Antes *a, unsigned n) {
    if (!a->tem || (int)(n - a->alvo) >= VB_LONGE) {
        a->alvo = n + 1;
        a->tem = 1;
        return 1;
    }
    if ((int)(n - a->alvo) >= 0) {
        a->alvo++;
        return 1;
    }
    return 0;
}

/* o resultado de uma rodada, de um passo de começo em diante (a janela) */
typedef struct Res {
    int passos, esperas, passosComEspera, tela[4], recomecos;
    double parede, esperado, lat50, lat99, latMax, latMedia;
} Res;

static double sG[NMAX], sR[NMAX], sD[NMAX], sLat[NMAX];

static int cmp(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y;
}

static unsigned vb_em(double t, double fase) {
    return t < fase ? 0u : (unsigned)floor((t - fase) / P) + 1u; /* (o 1 é o vblank em fase + 0) */
}
static double vb_t(unsigned k, double fase) {
    return fase + (double)(k - 1u) * P;
}

/* n passos com os tempos sG/sR/sD; nova: 1 a conta do vblank_alvo.h, 0 a de antes; a janela a partir do passo j0 */
static void roda(int n, int j0, double fase, int nova, Res *o) {
    VbAlvo va;
    Antes aa;
    double t = 0.0, gLivre = 0.0, dLivre = 0.0, tPrev = 0.0, s0 = 0.0;
    unsigned kPrev = 0, k;
    int i, c, esperou, m = 0;
    memset(&va, 0, sizeof(va));
    vb_solta(&va);
    memset(&aa, 0, sizeof(aa));
    memset(o, 0, sizeof(*o));
    for (i = 0; i < n; i++) {
        double s = t, h, gFim, pres;
        if (i == j0) {
            s0 = s;
        }
        /* o jogo trabalha e entrega à gravação (espera a gravação tomar a anterior) */
        h = s + sG[i];
        if (h < gLivre) {
            h = gLivre;
        }
        /* a gravação: trabalha e entrega ao desenho (espera o desenho ficar livre) */
        gFim = h + sR[i];
        if (gFim < dLivre) {
            gFim = dLivre;
        }
        gLivre = gFim;
        /* o desenho: trabalha e faz o Present, que espera a imagem anterior entrar na tela */
        pres = gFim + sD[i];
        k = vb_em(pres, fase);
        if (vb_t(k, fase) < pres) {
            k++;
        }
        if (kPrev != 0 && k <= kPrev) {
            k = kPrev + 1;
        }
        dLivre = vb_t(k, fase); /* (o Present segura o desenho até a imagem entrar na tela) */
        if (i >= j0) {
            if (kPrev != 0 && i > j0) {
                unsigned v = k - kPrev;
                o->tela[v >= 4 ? 3 : v - 1]++;
            }
            sLat[m++] = vb_t(k, fase) - s;
        }
        kPrev = k;
        /* as duas esperas de vblank do passo */
        t = h;
        esperou = 0;
        for (c = 0; c < 2; c++) {
            unsigned nn = vb_em(t, fase);
            int r = nova ? vb_decide(&va, nn) : antes_decide(&aa, nn);
            if (r == 0) {
                unsigned alvo = nova ? va.alvo : aa.alvo;
                t = vb_t(alvo, fase) + DPC;
                if (nova) {
                    vb_esperou(&va);
                } else {
                    aa.alvo++;
                }
                esperou = 1;
                if (i >= j0) {
                    o->esperas++;
                }
            }
        }
        if (i >= j0) {
            o->passosComEspera += esperou;
        }
        tPrev = t;
    }
    o->passos = n - j0;
    o->parede = tPrev - s0;
    o->esperado = (double)o->passos * 2.0 * P;
    o->recomecos = nova ? (int)va.recomecos : 0;
    qsort(sLat, (size_t)m, sizeof(double), cmp);
    o->lat50 = sLat[m / 2];
    o->lat99 = sLat[(int)(m * 0.99)];
    o->latMax = sLat[m - 1];
    for (i = 0; i < m; i++) {
        o->latMedia += sLat[i] / m;
    }
}

static void mostra(const char *nome, const Res *o) {
    printf("  %-6s %d passos: %d esperas (%d passos com espera), recomeços %d; tela 1/2/3/4+ %d/%d/%d/%d; parede %.1f ms "
           "(razão %.4f); controle à tela média %.2f p50 %.2f p99 %.2f máximo %.2f ms\n", nome, o->passos, o->esperas,
           o->passosComEspera, o->recomecos, o->tela[0], o->tela[1], o->tela[2], o->tela[3], o->parede,
           o->parede / o->esperado, o->latMedia, o->lat50, o->lat99, o->latMax);
}

int main(void) {
    Res ra, rn;
    char t[200];
    int i, f, fases = 0, travadas = 0;
    double fase;

    /* A. a cena lenta e a janela 1 do vb1, em dez fases do vídeo: 20 passos de ~45 ms pela gravação, depois a fila do
       desenho segura cada passo em dois vblanks, e no passo 120 um pico da gravação (o máximo de 84 ms da janela 1);
       a janela de 684 passos começa no 100. Contados também os passos depois do pico (do 130 em diante) */
    printf("A. a cena lenta, a fila do desenho em dois vblanks e um pico da gravação (684 passos)\n");
    for (f = 0; f < 10; f++) {
        Res pa, pn;
        fase = 0.3 + 1.6 * f;
        sSemente = 1000ul + (unsigned long)f;
        for (i = 0; i < 784; i++) {
            if (i < 20) {
                sG[i] = uni(10, 14), sR[i] = uni(42, 46), sD[i] = uni(16.8, 18.5);
            } else {
                sG[i] = uni(9, 13), sR[i] = uni(20, 24), sD[i] = uni(16.8, 18.5);
            }
        }
        sR[120] = 60.0 + 3.0 * f;
        roda(784, 100, fase, 0, &ra);
        roda(784, 100, fase, 1, &rn);
        roda(784, 130, fase, 0, &pa);
        roda(784, 130, fase, 1, &pn);
        if (f < 3) {
            printf(" fase %.1f ms, pico de %.0f ms\n", fase, sR[120]);
            mostra("antes", &ra);
            mostra("nova", &rn);
            printf("  depois do pico: antes %d de %d passos com espera, nova %d\n", pa.passosComEspera, pa.passos,
                   pn.passosComEspera);
        }
        fases++;
        travadas += pa.passosComEspera == 0;
        sprintf(t, "A fase %.1f: a nova espera depois do pico (%d passos com espera de %d)", fase, pn.passosComEspera,
                pn.passos);
        confere(pn.passosComEspera >= pn.passos * 95 / 100, t);
        sprintf(t, "A fase %.1f: recomeços entre 1 e 2 quando a de antes travou (%d)", fase, rn.recomecos);
        confere(pa.passosComEspera != 0 || (rn.recomecos >= 1 && rn.recomecos <= 2), t);
        sprintf(t, "A fase %.1f: o controle à tela não piora (p50 %.2f contra %.2f, p99 %.2f contra %.2f, máximo %.2f contra "
                "%.2f)", fase, rn.lat50, ra.lat50, rn.lat99, ra.lat99, rn.latMax, ra.latMax);
        confere(rn.lat50 <= ra.lat50 + 0.01 && rn.lat99 <= ra.lat99 + 0.01 && rn.latMax <= ra.latMax + 0.01, t);
        sprintf(t, "A fase %.1f: a tela igual (1/2/3/4+ %d/%d/%d/%d contra %d/%d/%d/%d)", fase, rn.tela[0], rn.tela[1],
                rn.tela[2], rn.tela[3], ra.tela[0], ra.tela[1], ra.tela[2], ra.tela[3]);
        confere(memcmp(rn.tela, ra.tela, sizeof(ra.tela)) == 0, t);
        sprintf(t, "A fase %.1f: a parede igual até um passo (%.1f contra %.1f ms)", fase, rn.parede, ra.parede);
        confere(fabs(rn.parede - ra.parede) <= 2.0 * P, t); /* (a de antes acaba a janela parada na entrega, um passo
                                                               à frente da tela; a nova, no vblank: a tela é a mesma) */
    }
    printf("  a conta de antes ficou sem espera nenhuma depois do pico em %d de %d fases\n", travadas, fases);
    sprintf(t, "A: a cena reproduz a janela 1 (a conta de antes sem espera depois do pico em %d de %d fases)", travadas, fases);
    confere(travadas >= fases / 2, t);

    /* B. o passo comum, sem fila que segure */
    printf("B. o passo comum: 20 mil passos, 5%% de picos de 34 a 60 ms, a gravação e o desenho leves\n");
    for (f = 0; f < 3; f++) {
        fase = 2.0 + 5.0 * f;
        sSemente = 77ul + (unsigned long)f;
        for (i = 0; i < 20000; i++) {
            double u = uni(0, 1);
            sG[i] = u < 0.70 ? uni(15, 30) : u < 0.95 ? uni(30, 33) : uni(34, 60);
            sR[i] = uni(2, 5), sD[i] = uni(3, 6);
        }
        roda(20000, 0, fase, 0, &ra);
        roda(20000, 0, fase, 1, &rn);
        printf(" fase %.1f ms\n", fase);
        mostra("antes", &ra);
        mostra("nova", &rn);
        sprintf(t, "B fase %.1f: as mesmas esperas (%d contra %d)", fase, rn.esperas, ra.esperas);
        confere(fabs((double)rn.esperas - ra.esperas) <= 0.005 * ra.esperas, t);
        sprintf(t, "B fase %.1f: o mesmo período (%.4f contra %.4f ms)", fase, rn.parede / rn.passos, ra.parede / ra.passos);
        confere(fabs(rn.parede - ra.parede) <= 0.0005 * ra.parede, t);
        sprintf(t, "B fase %.1f: o controle à tela não piora (média %.3f contra %.3f, máximo %.2f contra %.2f)", fase,
                rn.latMedia, ra.latMedia, rn.latMax, ra.latMax);
        confere(rn.latMedia <= ra.latMedia + 0.05 && rn.latMax <= ra.latMax + 0.01, t);
        sprintf(t, "B fase %.1f: as imagens de 3 vblanks ou mais iguais até 0,05%% dos passos (%d contra %d)", fase, rn.tela[2] + rn.tela[3],
                ra.tela[2] + ra.tela[3]);
        confere(rn.tela[2] + rn.tela[3] <= ra.tela[2] + ra.tela[3] + rn.passos / 2000, t);
    }

    /* C. a dívida paga devagar: um pico de 90 ms e depois passos de 1,9 vblank */
    printf("C. a dívida paga devagar: um pico de 90 ms e depois 200 passos de 1,9 vblank\n");
    for (f = 0; f < 3; f++) {
        fase = 1.0 + 6.0 * f;
        for (i = 0; i < 230; i++) {
            sG[i] = i == 10 ? 90.0 : 1.9 * P - 0.2;
            sR[i] = 1.0, sD[i] = 1.0;
        }
        roda(230, 0, fase, 0, &ra);
        roda(230, 0, fase, 1, &rn);
        printf(" fase %.1f ms\n", fase);
        mostra("antes", &ra);
        mostra("nova", &rn);
        sprintf(t, "C fase %.1f: nenhum recomeço (%d)", fase, rn.recomecos);
        confere(rn.recomecos == 0, t);
        sprintf(t, "C fase %.1f: a mesma parede (%.1f contra %.1f ms)", fase, rn.parede, ra.parede);
        confere(fabs(rn.parede - ra.parede) < 0.01, t);
    }

    printf("%d conferências, %d falhas\n", sCasos, sFalhas);
    return sFalhas != 0;
}
