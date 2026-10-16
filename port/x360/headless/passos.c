/*
 * F5, etapa 3: o tempo por passo da luta e os contadores de descarte (plano/F5/00-escopo.md, seção 3, "Tempo,
 * memória e uso do console"; o que ler no console está em plano/F5/03.md e no port/x360/tools/passos.py).
 *
 * A fronteira de passo é a leitura do controle do jogador 1 (Port_PadRead da porta 0: sem_janela.c na x86 e na PPC,
 * pad360.c no 360): o jogo lê o controle uma vez por quadro dele (Pad_Update, passo 6 do Battle_Loop), e na luta um
 * quadro é um passo da lógica, dois vblanks. A mesma fronteira nas três builds, com ou sem roteiro.
 *
 * Uma janela é um trecho contínuo de passos com o combate andando: o estado 3 da sequência (BtlSeqFight) sem a
 * pausa (BATTLE_FLAG_PAUSE, que o menu de pausa e a Guide levantam). As cargas durante o combate ficam dentro (o
 * critério as conta). Por passo, num vetor de tamanho fixo (nada se escreve durante a janela):
 *   - o intervalo entre duas fronteiras (o relógio monotônico do host: clock_gettime; no 360, o posix360.c sobre o
 *     QueryPerformanceCounter);
 *   - a espera deliberada de vblank dentro dele (gPortSleptNs, que o vblank_wait do plat_stub.c soma): o trabalho do
 *     passo é o intervalo menos essa espera, e a espera nunca conta como trabalho;
 *   - o tempo dentro do Present e cada apresentação (Passos_Apresentou, do backend D3D9 do 360: o instante e o quadro
 *     do GS mostrado, para separar imagem nova de repetida);
 *   - os vblanks do passo (2 na luta).
 * Ao fechar a janela (pausa, fim do combate, saída da luta, "fim" do roteiro, vetor cheio), fora do trecho medido:
 *   - no rastro do roteiro (Roteiro_Rastro, só com roteiro): "roteiro: janela K passos N vblanks V ..." (o que é
 *     determinístico e se compara entre a x86 e a PPC);
 *   - "passos: janela K: ..." com a parede contra o esperado (2 vblanks de 16,6833 ms por passo), os percentis do
 *     trabalho (p50, p95, p99, máximo), os passos acima de 30 ms e acima de um período (33,37 ms), a espera, o
 *     Present, os intervalos entre imagens novas e os acima de um período mais 1 ms e de três vblanks;
 *   - "passos: janela K descartes: ...": o que o backend deixou de desenhar durante a janela (Passos_Descartes).
 * Na x86 e na PPC sem janela não há espera de vblank nem Present: o número é do emulador, não do console, e só prova
 * que a medida existe e anda; o número de verdade vem do 360.
 *
 * Nada aqui é estado do jogo (os objetos x360_* ficam fora do checksum e do dump de memória: mem.py).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* de que commit a build saiu (o carimbo do headless.py e do build_f2.sh: "COMMIT diff:HASH"), no começo do log: o
   passos.py recusa o log de outro commit. Num vetor de tamanho fixo, para o resto do programa não mudar de lugar com
   o tamanho do texto */
#ifndef BT3_CARIMBO
#define BT3_CARIMBO "sem carimbo"
#endif
static const char kCarimbo[64] = BT3_CARIMBO;

extern unsigned gPortVBlanks;           /* plat_stub.c */
extern unsigned long long gPortSleptNs; /* plat_stub.c: a espera de vblank até agora */
extern int gPortMenuMode;               /* headless.c: 1 enquanto os menus rodam */
extern struct { int state; } *gBtlSeq;  /* include/battle/btl_seq.h */
void *Battle_GetWork(void);             /* include/battle/battle.h: as bandeiras (u64) em +0x19F0 */
#define BTL_FLAGS 0x19F0
#define BTL_PAUSA 0x100ull              /* BATTLE_FLAG_PAUSE */
#define BTL_PAUSA_MENU 0x4000ull        /* BATTLE_FLAG_PAUSE_MENU */
#define LUTANDO 3                       /* BTL_SEQ_FIGHT */
void Roteiro_Rastro(const char *linha); /* roteiro.c */
/* o backend: os descartes acumulados desde o começo (gs_d3d.cpp no 360; 0 sem backend, sem_janela.c) */
int Passos_Descartes(unsigned *v, int n);
#define N_DESC 8

#ifdef PORT_X360
void Diario_Escrever(const char *formato, ...);
#endif

#define PASSOS_MAX 16384
#define VBLANK_MS 16.68335       /* 59,94 Hz */
#define PERIODO_MS (2 * VBLANK_MS) /* um passo da luta */

typedef struct Passo {
    unsigned intervaloUs, esperaUs, presentUs;
    unsigned short vblanks, imagens;
} Passo;

static Passo sP[PASSOS_MAX];
static unsigned sIntUs[PASSOS_MAX];   /* intervalos entre imagens novas na janela */
static unsigned sOrdem[PASSOS_MAX];   /* (cópia para ordenar) */
static unsigned sN, sNInt, sRepetidas, sJanela;
static int sAberta, sTemAnterior, sDito;
static unsigned long long sT0, sEspera0;
static unsigned sVb0, sVbAbriu, sPresentUs, sImagens;
static unsigned long long sUltImagem;
static int sTemImagem;
static unsigned sUltGs;
static unsigned sDesc0[N_DESC];

static void escreve(const char *linha) {
#ifdef PORT_X360
    Diario_Escrever("%s", linha);
#else
    printf("%s\n", linha);
    fflush(stdout);
#endif
}

static unsigned long long agora_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000000000ull + (unsigned long long)ts.tv_nsec;
}

static int cmp(const void *a, const void *b) {
    unsigned x = *(const unsigned *)a, y = *(const unsigned *)b;
    return x < y ? -1 : x > y;
}

/* o percentil q (0..100) de v[0..n), já ordenado: o menor valor com pelo menos q% dos valores até ele */
static double pct(const unsigned *v, unsigned n, unsigned q) {
    unsigned k;
    if (n == 0) {
        return 0.0;
    }
    k = (n * q + 99) / 100;
    k = k == 0 ? 0 : k - 1;
    return v[k >= n ? n - 1 : k] / 1000.0;
}

static int combate(int *pausa) {
    unsigned long long f;
    *pausa = 0;
    if (gPortMenuMode || gBtlSeq == NULL || gBtlSeq->state != LUTANDO) {
        return 0;
    }
    f = *(const unsigned long long *)((const unsigned char *)Battle_GetWork() + BTL_FLAGS);
    *pausa = (f & BTL_PAUSA) != 0;
    return !*pausa;
}

static void abre(void) {
    sAberta = 1;
    sJanela++;
    sN = sNInt = sRepetidas = 0;
    sTemImagem = 0;
    sVbAbriu = gPortVBlanks;
    if (Passos_Descartes(sDesc0, N_DESC) <= 0) {
        memset(sDesc0, 0, sizeof(sDesc0));
    }
}

static void fecha(const char *motivo) {
    char s[600];
    unsigned i, vb = 0, dois = 0, acima30 = 0, acima33 = 0, int34 = 0, int50 = 0, desc[N_DESC];
    double parede = 0, espera = 0, present = 0, presentPior = 0;
    int nd;
    sAberta = 0;
    for (i = 0; i < sN; i++) {
        unsigned t = sP[i].intervaloUs > sP[i].esperaUs ? sP[i].intervaloUs - sP[i].esperaUs : 0;
        sOrdem[i] = t;
        vb += sP[i].vblanks;
        dois += sP[i].vblanks == 2;
        parede += sP[i].intervaloUs / 1000.0;
        espera += sP[i].esperaUs / 1000.0;
        present += sP[i].presentUs / 1000.0;
        if (sP[i].presentUs / 1000.0 > presentPior) {
            presentPior = sP[i].presentUs / 1000.0;
        }
        acima30 += t > 30000;
        acima33 += t / 1000.0 > PERIODO_MS;
    }
    /* o determinístico vai ao rastro (com o roteiro): passos e vblanks da janela e por que ela fechou ("fecha", não
       "fim": o roteiros.sh procura o fim do roteiro por "^roteiro: .* fim ") */
    sprintf(s, "roteiro: janela %u passos %u vblanks %u de 2 vblanks %u outros %u fecha %s vblank %u", sJanela, sN, vb,
            dois, sN - dois, motivo, gPortVBlanks);
    Roteiro_Rastro(s);
    qsort(sOrdem, sN, sizeof(sOrdem[0]), cmp);
    sprintf(s, "passos: janela %u (vblank %u a %u, fim %s): %u passos, parede %.1f ms, esperado %.1f ms (razão %.4f); "
            "trabalho p50 %.2f p95 %.2f p99 %.2f máximo %.2f ms; acima de 30 ms %u, acima de %.2f ms %u; espera de "
            "vblank %.1f ms; Present %.1f ms, pior %.2f ms",
            sJanela, sVbAbriu, gPortVBlanks, motivo, sN, parede, sN * PERIODO_MS,
            sN ? parede / (sN * PERIODO_MS) : 0.0, pct(sOrdem, sN, 50), pct(sOrdem, sN, 95), pct(sOrdem, sN, 99),
            sN ? sOrdem[sN - 1] / 1000.0 : 0.0, acima30, PERIODO_MS, acima33, espera, present, presentPior);
    escreve(s);
    for (i = 0; i < sNInt; i++) {
        int34 += sIntUs[i] / 1000.0 > PERIODO_MS + 1.0;
        int50 += sIntUs[i] / 1000.0 > 3 * VBLANK_MS;
        sOrdem[i] = sIntUs[i];
    }
    qsort(sOrdem, sNInt, sizeof(sOrdem[0]), cmp);
    if (sNInt == 0 && sRepetidas == 0) {
        sprintf(s, "passos: janela %u apresentação: nenhuma imagem (sem backend de vídeo)", sJanela);
    } else {
        sprintf(s, "passos: janela %u apresentação: %u intervalos entre imagens novas, %u repetidas; p50 %.2f p99 "
                "%.2f máximo %.2f ms; acima de %.2f ms %u (%.2f%%), acima de %.2f ms %u",
                sJanela, sNInt, sRepetidas, pct(sOrdem, sNInt, 50), pct(sOrdem, sNInt, 99),
                sNInt ? sOrdem[sNInt - 1] / 1000.0 : 0.0, PERIODO_MS + 1.0, int34, sNInt ? 100.0 * int34 / sNInt : 0.0,
                3 * VBLANK_MS, int50);
    }
    escreve(s);
    nd = Passos_Descartes(desc, N_DESC);
    if (nd <= 0) {
        sprintf(s, "passos: janela %u descartes: sem backend D3D (build sem janela)", sJanela);
    } else {
        for (i = 0; i < N_DESC; i++) {
            desc[i] -= sDesc0[i];
        }
        sprintf(s, "passos: janela %u descartes: VU1 %u, nativos %u %u %u %u (contorno, chave, tabela, ids), "
                "direto no quadro %u, texturas recusadas %u, primitivas só do PS2 %u",
                sJanela, desc[0], desc[1], desc[2], desc[3], desc[4], desc[5], desc[6], desc[7]);
    }
    escreve(s);
}

/* a fronteira de passo: a leitura do controle do jogador 1 */
void Passos_Leitura(void) {
    unsigned long long t = agora_ns(), espera = gPortSleptNs;
    unsigned vb = gPortVBlanks;
    int pausa, luta;
    if (!sDito) {
        char s[160];
        sDito = 1;
        sprintf(s, "passos: origem %.63s (tempo por passo da luta, F5 etapa 3)", kCarimbo);
        escreve(s);
    }
    if (sAberta && sTemAnterior) {
        Passo *p = &sP[sN++];
        unsigned long long d = t - sT0, e = espera - sEspera0;
        p->intervaloUs = (unsigned)(d / 1000ull);
        p->esperaUs = (unsigned)(e / 1000ull);
        p->presentUs = sPresentUs;
        p->vblanks = (unsigned short)(vb - sVb0);
        p->imagens = (unsigned short)sImagens;
    }
    sPresentUs = 0;
    sImagens = 0;
    luta = combate(&pausa);
    if (sAberta && (!luta || sN == PASSOS_MAX)) {
        fecha(sN == PASSOS_MAX && luta ? "cheia" : pausa ? "pausa" : gPortMenuMode || gBtlSeq == NULL ? "menus" : "combate");
    }
    if (!sAberta && luta) {
        abre();
    }
    sT0 = t;
    sEspera0 = espera;
    sVb0 = vb;
    sTemAnterior = 1;
}

/* uma imagem apresentada (o backend, logo depois do Present): o quadro do GS e o tempo dentro do Present */
void Passos_Apresentou(unsigned quadroGs, unsigned presentUs) {
    unsigned long long t = agora_ns();
    sPresentUs += presentUs;
    sImagens++;
    if (!sAberta) {
        sTemImagem = 0;
        return;
    }
    if (sTemImagem && quadroGs == sUltGs) {
        sRepetidas++;
        return;
    }
    if (sTemImagem && sNInt < PASSOS_MAX) {
        sIntUs[sNInt++] = (unsigned)((t - sUltImagem) / 1000ull);
    }
    sTemImagem = 1;
    sUltImagem = t;
    sUltGs = quadroGs;
}

/* os passos da janela aberta até aqui (0 sem janela): o "repete ate janela N" do roteiro */
unsigned Passos_NaJanela(void) {
    return sAberta ? sN : 0;
}

/* uma janela aberta: o resto do port não escreve resumos periódicos no log enquanto isso (gs_d3d.cpp, estado360.c) */
int Passos_Medindo(void) {
    return sAberta;
}

/* o "fim" do roteiro: a janela aberta fecha antes do programa sair */
void Passos_Fim(const char *motivo) {
    if (sAberta) {
        fecha(motivo);
    }
}
