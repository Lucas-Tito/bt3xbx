/*
 * F2: no lugar de port/src/gs/state.c (checksum de tudo, salvar e voltar estado, rede), só o que a medição usa:
 *   - Port_StateLog: a linha por quadro de BT3_HASH, no formato da build de PC (vblank, dois checksums, e numa
 *     luta vida, posições, relógio e gerador), para port/x360/tools/compara.py; os checksums vão como 0. Desligada por
 *     padrão no console (sem BT3_HASH no environment.txt; F5/08.md);
 *   - o tempo: a build sem janela não espera o vblank, então o intervalo entre duas chamadas (uma por vblank) é o
 *     que o jogo levou. A luta faz um quadro a cada 2 vblanks (30 fps, decisão 15): o trabalho cai num intervalo
 *     do par e o outro fica quase vazio, então o que vale contra o orçamento de 33,3 ms é a soma de cada par.
 *     Resumo no log a cada 600 vblanks e no fim (Estado_Resumo), com a razão contra o tempo real (16,683 ms por
 *     vblank: abaixo de 1 o jogo cabe no tempo).
 *   - BT3_ATE=<vblank>: encerra o jogo nesse vblank (os menus rodam em ciclo, título e demo, sem fim próprio);
 *   - cada troca de tela (gProgress->mode, include/menu/menu_a.h) e cada começo e fim de luta vão para o log.
 */
#include <xtl.h>
#include <stdio.h>
#include <stdlib.h>
#include "diario.h"
#include "iso360.h"

FILE *Port_X360Fopen(const char *caminho, const char *modo);

/* o resto do que state.c fornece e o resto do port chama, sem salvar estado nem rede no 360 */
int gPortResim;                    /* quadros sendo refeitos (rollback): nunca */
char __data_start[4], _end[4];     /* o despejo das globais do headless.c (BT3_DUMP): vazio */
void Port_StateTouch(void *p, unsigned long n) { (void)p; (void)n; }
void Port_SessionPoll(void) {}
void Port_SyncTest(unsigned vblank) { (void)vblank; }
long Port_PadPlayPos(void);

volatile unsigned gX360Luta; /* o vblank em que a luta começou, 0 fora dela (o backend do GS registra os primeiros quadros) */
static FILE *sArq;
static int sModo = -1;
static int sMedir;
static LARGE_INTEGER sAntes, sFreq;
#define VBLANK_MS 16.683
static double sTotal, sTotalTrecho, sPiorPar, sPiorParTrecho, sPar, sLutaTotal;
static unsigned sVblanks, sVblanksTrecho, sPares, sParesAcima, sLuta, sLutaParesAcima;

extern struct CharSel *gCharSel; /* src/menu/menu_c_e.c: a seleção de personagens, NULL fora dela */
static int sPreAtiva, sPreViuSel;

static int precarga_ligada(void) {
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("BT3_PRECARGA");
        v = !(e != NULL && e[0] == '0');
    }
    return v;
}

static void resumo(const char *quando, double total, unsigned n, double piorPar) {
    if (n == 0) {
        return;
    }
    Diario_Escrever("tempo %s: %u vblanks em %.0f ms (%.2f do tempo real), pior par de vblanks %.2f ms", quando, n,
                    total, total / (n * VBLANK_MS), piorPar);
}

void Estado_Resumo(void) {
    if (!gX360Log) return;
    resumo("total", sTotal, sVblanks, sPiorPar);
    Diario_Escrever("pares de vblanks (quadros a 30 fps) acima de 33,3 ms: %u de %u; na luta: %u vblanks, %.2f do "
                    "tempo real, %u pares acima de 33,3 ms", sParesAcima, sPares, sLuta,
                    sLuta ? sLutaTotal / (sLuta * VBLANK_MS) : 0.0, sLutaParesAcima);
    if (sArq != NULL) {
        fclose(sArq);
        sArq = NULL;
        sModo = 0;
    }
}

void Port_Sair(int codigo);
void Port_QuedaTeste360(int desenho, int luta, unsigned quadro);
void Port_GsInterromperJogo360(void);
extern unsigned gGsFrame;
void Port_X360FecharSaida(void); /* posix360.c */
int Passos_Medindo(void);        /* port/x360/headless/passos.c */
extern const char *volatile gPortStage;
extern int *gProgress; /* include/menu/menu_a.h, MenuProgress: o modo da tela está em +0x18 */

void Port_StateLog(unsigned vblank, const void *fight, unsigned fightSize) {
    LARGE_INTEGER agora;
    long pos;
    static unsigned ate = 0xFFFFFFFFu;
    static int modo = -1, luta = 0;
    (void)fightSize;
    Port_GsInterromperJogo360();
    gPortStage = fight != NULL ? "battle" : "menus or loading";
    Port_QuedaTeste360(0, fight != NULL, gGsFrame);
    gX360Luta = fight != NULL ? (gX360Luta ? gX360Luta : vblank) : 0;
    if (!gX360Log) return;
    if (sModo < 0) {
        const char *caminho = getenv("BT3_HASH");
        const char *medida = getenv("BT3_ESTADO_TEMPOS");
        sMedir = medida != NULL && atoi(medida) == 1;
        QueryPerformanceFrequency(&sFreq);
        sModo = 0;
        if (caminho != NULL && (sArq = Port_X360Fopen(caminho, "w")) != NULL) {
            sModo = 1;
        }
        Diario_Escrever("hash por quadro: %s", sModo ? caminho : "desligado");
        if (getenv("BT3_ATE") != NULL) {
            ate = (unsigned)atoi(getenv("BT3_ATE"));
            Diario_Escrever("encerra no vblank %u", ate);
        }
    } else if (sMedir) {
        double ms;
        QueryPerformanceCounter(&agora);
        ms = (double)(agora.QuadPart - sAntes.QuadPart) * 1000.0 / (double)sFreq.QuadPart;
        sTotal += ms;
        sTotalTrecho += ms;
        sVblanks++;
        sVblanksTrecho++;
        if (fight != NULL) {
            sLuta++;
            sLutaTotal += ms;
        }
        sPar += ms;
        if ((sVblanks & 1) == 0) { /* fecha um par */
            sPares++;
            sParesAcima += sPar > 2 * VBLANK_MS;
            if (fight != NULL) {
                sLutaParesAcima += sPar > 2 * VBLANK_MS;
            }
            if (sPar > sPiorPar) {
                sPiorPar = sPar;
            }
            if (sPar > sPiorParTrecho) {
                sPiorParTrecho = sPar;
            }
            sPar = 0;
        }
        if (sVblanksTrecho == 600) {
            char quando[32];
            sprintf(quando, "até o vblank %u", vblank);
            if (!Passos_Medindo()) { /* (F5, etapa 3: nada de log durante a janela medida, port/x360/headless/passos.c) */
                resumo(quando, sTotalTrecho, sVblanksTrecho, sPiorParTrecho);
            }
            sTotalTrecho = sPiorParTrecho = 0;
            sVblanksTrecho = 0;
        }
    }
    if (sModo == 1) {
        fprintf(sArq, "%u %016llx %016llx", vblank, 0ull, 0ull);
        if (fight != NULL) {
            const int *w = (const int *)fight;
            fprintf(sArq, " hp %d %d pos %08x %08x %08x  %08x %08x %08x clock %d rnd %08x%08x", w[0], w[1], w[2], w[3],
                    w[4], w[5], w[6], w[7], w[8], w[11], w[10]);
        }
        pos = Port_PadPlayPos();
        if (pos >= 0) {
            fprintf(sArq, " pad %ld", pos);
        }
        fputc('\n', sArq);
    }
    if (gProgress != NULL && gProgress[0x18 / 4] != modo) {
        int antes = modo;
        modo = gProgress[0x18 / 4];
        Diario_Escrever("vblank %u: tela %d", vblank, modo);
        /* (pendência 34) a seleção de personagens e de arenas (tela 39) lê os retratos (0x2F9 + personagem) e as
           prévias das arenas (0x39D a 0x3C0) do pzs3us1 a cada movimento do cursor (include/menu/menu_d.h,
           CHARSEL_FACE_FILE e CHARSEL_STAGE_FILE): o bloco inteiro vai para a memória na entrada e sai na saída.
           BT3_PRECARGA=0 volta ao disco a cada movimento (para comparar no console). */
        if (modo == 39 && precarga_ligada()) {
            Iso_Precarregar(1, 0x2F9, 0x3C0);
            sPreViuSel = 0;
            sPreAtiva = 1;
        } else if (antes == 39 && sPreAtiva) {
            Iso_PrecargaLiberar();
            sPreAtiva = 0;
        }
    }
    /* (pendência 34) a luta carrega ainda na tela 39: a pré-carga (uns 16 MB) sai quando a seleção termina, no
       CharSel_Term, que zera o gCharSel (src/menu/menu_c_e.c), e não espera o jogo trocar de tela. Na integração do BT4,
       com os buffers maiores do upstream, ela ocupada durante a carga deixava alvos sem textura (12/11/2026). */
    if (sPreAtiva) {
        if (gCharSel != NULL) {
            sPreViuSel = 1;
        } else if (sPreViuSel) {
            Iso_PrecargaLiberar();
            sPreAtiva = 0;
            Diario_Escrever("vblank %u: pré-carga da seleção liberada (a seleção terminou)", vblank);
        }
    }
    if ((fight != NULL) != luta) {
        luta = fight != NULL;
        Diario_Escrever("vblank %u: %s", vblank, luta ? "luta começou" : "luta terminou");
        gX360Luta = luta ? vblank : 0;
    }
    if (!Diario_Assincrono()) {
        /* o fprintf(stderr) do jogo chega ao disco mesmo num travamento sem exceção; com a thread do diário é ela quem
           faz isto (Diario_Gancho, a cada 50 ms), e o commit sai da thread do jogo */
        Port_X360FecharSaida();
    }
    if (sMedir) QueryPerformanceCounter(&sAntes); /* sem o tempo da própria gravação */
    if (vblank >= ate) {
        Diario_Escrever("BT3_ATE: vblank %u", vblank);
        Port_Sair(0);
    }
}
