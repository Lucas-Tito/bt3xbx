/*
 * F1, build PowerPC: o que os arquivos de janela, controle e rede do port (gs_gpu.c, gs_draw.c,
 * gs_input.c, net.c) fornecem, no estado em que ficam numa build de PC rodando sem janela (BT3_GS=none)
 * e sem rede. Tem de devolver exatamente o que eles devolvem nesse caso, senão as duas builds divergem
 * por isso e não pela ordem dos bytes.
 */
#include <stdint.h>
#include <stdlib.h>

/* gs_gpu.c: só é chamado com o GS em modo gpu. Na build do 360 com o backend D3D9 (F3, etapa 7: c89.py define
   X360_GS_D3D), estas entradas e o letreiro vêm do gs_draw.c de verdade. */
#ifndef X360_GS_D3D
int GsGpu_Init(void) { return 0; }
void GsGpu_Draw(int type, int ctx, const void *v) { (void)type; (void)ctx; (void)v; }
void GsGpu_DrawVu0(int layer, int ctx, const float *v, uint32_t n, const float *c) { (void)layer; (void)ctx; (void)v; (void)n; (void)c; }
void GsGpu_DrawVu4(int ctx, const float *v, uint32_t n, const float *c) { (void)ctx; (void)v; (void)n; (void)c; }
void GsGpu_DrawVu6(int ctx, const float *v, uint32_t n, const float *c) { (void)ctx; (void)v; (void)n; (void)c; }
void GsGpu_FbUpload(int second) { (void)second; }
void GsGpu_FrameEnd(void) {}
void GsGpu_InterpPump(int ahead_ms, int all) { (void)ahead_ms; (void)all; }
void GsGpu_Native(int effect) { (void)effect; }
void GsGpu_PagesMoved(uint32_t delta) { (void)delta; }
uint64_t gGpuTexNs, gGpuPipeNs, gGpuEndNs;
int gGpuNewPipes, gGpuNewTex, gGpuNewTexPixels;

/* gs_draw.c: o letreiro de fases e músicas de fora do disco, desenhado pela janela */
volatile int gUiNameX, gUiNameY, gUiNameW, gUiNameH, gUiNameIdx, gUiNameReady;
volatile int gUiSongX, gUiSongY, gUiSongW, gUiSongH, gUiSongIdx, gUiSongLit, gUiSongReady;
#endif

/* gs_input.c: sem janela, Port_PadRead devolve 0 (nenhum controle), a não ser com um roteiro de controle
   (BT3_ROTEIRO, roteiro.c, F4 etapa 4). No 360 os controles vêm do XInput (port/x360/src/f2/pad360.c, F3 etapa 8),
   que também passa pelo roteiro. */
extern int Port_Setting(const char *name, int def);
#ifndef PORT_X360
extern int Roteiro_Pad(int socket, unsigned char *data);
int Port_PadRead(int socket, unsigned char *data) { return Roteiro_Pad(socket, data); }
#endif
void Port_ClashLog(int player, const char *what, int fromInput, int total, int frame) {
    (void)player; (void)what; (void)fromInput; (void)total; (void)frame;
}
int Port_ClashSquare(void) {
    static int on = -1;
    if (on < 0) {
        on = getenv("BT3_CLASH_SQUARE") != NULL ? atoi(getenv("BT3_CLASH_SQUARE")) != 0 : Port_Setting("clash_square", 0) != 0;
    }
    return on;
}

/* net.c: sem rede */
int Port_NetActive(void) { return 0; }
int Port_NetMe(void) { return 0; }
int Port_NetSession(void) { return 0; }
int Port_NetResim(void) { return 0; }
int Port_NetWarp(void) { return 0; }
int Port_NetView(void) {
    const char *e = getenv("BT3_VIEW");
    return e != NULL && atoi(e) >= 0 ? atoi(e) & 1 : -1;
}
int Port_HudSwap(void) { return getenv("BT3_HUD_SWAP") != NULL && atoi(getenv("BT3_HUD_SWAP")) != 0; }
void Port_NetRules(int *type, int *dp, int *time) { (void)type; (void)dp; (void)time; }
void Port_NetBeginTick(unsigned tick) { (void)tick; }
void Port_NetInput(int player, unsigned char *data) { (void)player; (void)data; }
void Port_NetArrived(void) {}
void Port_NetLeave(void) {}
void Port_LobbyAuto(void) {}
int Port_NetVoiceAlt(void) { return 0; } /* só vale com a rede ativa */
