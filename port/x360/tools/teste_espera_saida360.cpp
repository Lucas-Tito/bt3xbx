/* Corpo real da espera do GS: a queda do desenho deve liberar o jogo fora da trava. */
#include <cstdio>
#include <stdexcept>
#include <stdint.h>
static int sLock, sDone, sJob, sParar, gPicos;
static unsigned gPicosJEspera, gPicosJEsperas, esperas;
static int gTerminou;
static unsigned gThreadJogo=2, threadAtual=2;
struct FimJogo {};
static uint64_t __mftb() { return 0; }
static void pthread_mutex_lock(int *m) { if (*m) throw std::runtime_error("trava já tomada"); *m=1; }
static void pthread_mutex_unlock(int *m) { if (!*m) throw std::runtime_error("trava solta"); *m=0; }
static unsigned GetCurrentThreadId() { return threadAtual; }
static void Port_Sair(int codigo) {
    if (sLock || codigo != 5) throw std::runtime_error("encerrou com a trava do GS tomada ou código errado");
    throw FimJogo();
}
#include "interromper.inc"
static int Port_X360CondEsperaMs(int *, int *, unsigned ms) {
    if (ms != 50) throw std::runtime_error("espera sem teto de 50 ms");
    esperas++; gTerminou=1; sParar=1;
    if (esperas > 3) sJob=0; /* um mutante sem checkpoint devolve sem encerrar e precisa ser acusado */
    return 1;
}
static unsigned juncoes;
static void parar_desenho() { juncoes++; }
static void (*gGsDesenhoParar)(void);
static void GsD3d_Soltar() {}
static void parada_gravacao() {
#include "parada.inc"
}
#include "espera.inc"
int main() {
    try {
        sLock=1; sParar=0; gGsDesenhoParar=parar_desenho;
        parada_gravacao();
        if (juncoes != 1) throw std::runtime_error("perdeu a parada síncrona");
        sParar=1; gGsDesenhoParar=parar_desenho;
        parada_gravacao();
        if (juncoes != 1) throw std::runtime_error("fechou o handle do desenho na queda antes da main");
        sParar=0; sLock=0;
        gTerminou=1; threadAtual=1;
        try { Port_GsInterromperJogo360(); } catch (const FimJogo &) { throw std::runtime_error("encerrou uma thread que não é a do jogo"); }
        gTerminou=0; threadAtual=2;
        render_wait();
        sJob=1;
        try { render_wait(); } catch (const FimJogo &) {
            if (sLock || esperas != 1) throw std::runtime_error("não encerrou na primeira espera, fora da trava");
            puts("espera do GS: tudo certo; jogo interrompido sem trava"); return 0;
        }
        throw std::runtime_error("jogo continuou depois da queda");
    } catch (const std::exception &e) { fprintf(stderr, "FALHA: %s\n", e.what()); return 1; }
}
