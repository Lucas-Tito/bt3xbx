/*
 * F2: no lugar de port/src/gs/state.c (checksum de tudo, salvar e voltar estado, rede), só o que a medição usa:
 *   - Port_StateLog: a linha por quadro de BT3_HASH, no formato da build de PC (vblank, dois checksums, e numa
 *     luta vida, posições, relógio e gerador), para port/x360/tools/compara.py; os checksums vão como 0;
 *   - o tempo: a build sem janela não espera o vblank, então o intervalo entre duas chamadas (uma por vblank) é o
 *     que o jogo levou. A luta faz um quadro a cada 2 vblanks (30 fps, decisão 15): o trabalho cai num intervalo
 *     do par e o outro fica quase vazio, então o que vale contra o orçamento de 33,3 ms é a soma de cada par.
 *     Resumo no log a cada 600 vblanks e no fim (Estado_Resumo), com a razão contra o tempo real (16,683 ms por
 *     vblank: abaixo de 1 o jogo cabe no tempo).
 */
#include <xtl.h>
#include <stdio.h>
#include <stdlib.h>
#include "diario.h"

FILE *Port_X360Fopen(const char *caminho, const char *modo);

/* o resto do que state.c fornece e o resto do port chama, sem salvar estado nem rede no 360 */
int gPortResim;                    /* quadros sendo refeitos (rollback): nunca */
char __data_start[4], _end[4];     /* o despejo das globais do headless.c (BT3_DUMP): vazio */
void Port_StateTouch(void *p, unsigned long n) { (void)p; (void)n; }
void Port_SessionPoll(void) {}
void Port_SyncTest(unsigned vblank) { (void)vblank; }
long Port_PadPlayPos(void);

static FILE *sArq;
static int sModo = -1;
static LARGE_INTEGER sAntes, sFreq;
#define VBLANK_MS 16.683
static double sTotal, sTotalTrecho, sPiorPar, sPiorParTrecho, sPar, sLutaTotal;
static unsigned sVblanks, sVblanksTrecho, sPares, sParesAcima, sLuta, sLutaParesAcima;

static void resumo(const char *quando, double total, unsigned n, double piorPar) {
    if (n == 0) {
        return;
    }
    Diario_Escrever("tempo %s: %u vblanks em %.0f ms (%.2f do tempo real), pior par de vblanks %.2f ms", quando, n,
                    total, total / (n * VBLANK_MS), piorPar);
}

void Estado_Resumo(void) {
    resumo("total", sTotal, sVblanks, sPiorPar);
    Diario_Escrever("pares de vblanks (quadros a 30 fps) acima de 33,3 ms: %u de %u; na luta: %u vblanks, %.2f do "
                    "tempo real, %u pares acima de 33,3 ms", sParesAcima, sPares, sLuta,
                    sLuta ? sLutaTotal / (sLuta * VBLANK_MS) : 0.0, sLutaParesAcima);
    if (sArq != NULL) {
        fclose(sArq); /* fechado: só o fflush não garante os bytes no FATX (port/x360/src/f2/posix360.c) */
        sArq = NULL;
        sModo = 0;
    }
}

void Port_StateLog(unsigned vblank, const void *fight, unsigned fightSize) {
    LARGE_INTEGER agora;
    long pos;
    (void)fightSize;
    QueryPerformanceCounter(&agora);
    if (sModo < 0) {
        const char *caminho = getenv("BT3_HASH");
        QueryPerformanceFrequency(&sFreq);
        sModo = 0;
        if (caminho != NULL && (sArq = Port_X360Fopen(caminho, "w")) != NULL) {
            sModo = 1;
        }
        Diario_Escrever("hash por quadro: %s", sModo ? caminho : "desligado");
    } else {
        double ms = (double)(agora.QuadPart - sAntes.QuadPart) * 1000.0 / (double)sFreq.QuadPart;
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
            resumo(quando, sTotalTrecho, sVblanksTrecho, sPiorParTrecho);
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
    QueryPerformanceCounter(&sAntes); /* sem o tempo da própria gravação */
}
