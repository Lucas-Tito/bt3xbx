/* Shared between the GS front end (gs_core.c: DMA chain -> VIF1 -> GIF -> registers, GS memory, software
   rasteriser) and the GPU back end (gs_gpu.c). */
#ifndef GS_INTERNAL_H
#define GS_INTERNAL_H
#include <stdint.h>

/* Byte order (docs/x360/formatos/gpu.md). What the game hands the GS is 32-bit words of the host's order (DMA and
   VIF codes, VU memory) with every 64-bit field as two of them, the low one first, as on the PS2: PS2_PKT64 turns
   such a field loaded as a native 64-bit number into its value (and a value into the field). GS memory (sVram) and
   the IMAGE data of an upload are the PS2's bytes: GS_LE32 reads or writes a 32-bit pixel of GS memory. Both are
   nothing on a little-endian host. */
#ifdef PORT_BIG_ENDIAN
static inline uint64_t Gs_Pkt64(uint64_t v) {
    return v << 32 | v >> 32;
}
static inline uint32_t Gs_Le32(uint32_t v) {
    return v << 24 | (v & 0xFF00u) << 8 | (v >> 8 & 0xFF00u) | v >> 24;
}
#ifndef PS2_PKT64 /* (the game's own, port/include/port_compat.h, does the same) */
#define PS2_PKT64(x) Gs_Pkt64((uint64_t)(x))
#endif
#define GS_LE32(x) Gs_Le32(x)
#else
#ifndef PS2_PKT64
#define PS2_PKT64(x) (x)
#endif
#define GS_LE32(x) (x)
#endif

typedef struct GsVertex {
    float x, y;       /* frame-buffer pixels */
    uint32_t z;
    float s, t, q;    /* texture coordinates when PRIM.FST = 0 */
    int u, v;         /* texel coordinates, 12.4, when PRIM.FST = 1 */
    uint8_t r, g, b, a;
} GsVertex;

typedef struct GsState {
    uint64_t prim;    /* the EFFECTIVE primitive attributes: PRIM, or PRIM's type with PRMODE's attributes (PRMODECONT) */
    uint64_t primRaw; /* PRIM as written */
    uint64_t rgbaq, st, uv, texa, prmodecont, prmode;
    uint64_t tex0[2], tex1[2], clamp[2], xyoffset[2], scissor[2], alpha[2], test[2], frame[2], zbuf[2];
    uint64_t fba[2]; /* FBA: 1 = every pixel written gets alpha bit 7 set (the masks of the HUD bars) */
    uint64_t bitbltbuf, trxpos, trxreg, trxdir;
    float q;
    GsVertex vtx[3];
    int vcount; /* vertices in the queue */
    int strip;  /* vertices seen since the last PRIM write */
    /* host -> GS transfer in progress */
    int transfer;
    uint32_t tx, ty, tw, th, tpos;
} GsState;

extern GsState gGs;
extern uint32_t gGsPageGen[512]; /* bumped whenever an upload writes into that 8 KB page of GS memory */
#if defined(PORT_BIG_ENDIAN) || defined(BT3_TESTE_VU1)
extern uint32_t gGsPageEpoch; /* changes whenever any gGsPageGen entry does (F5, etapa 8) */
#endif
#if (defined(PORT_BIG_ENDIAN) || defined(BT3_TESTE_VU1)) && !defined(BT3_GS_ESPERAS)
#define BT3_GS_ESPERAS 1 /* (F5, etapa 8, degrau 1) a thread de gravação com as esperas que o PS2 já fazia */
#endif
#ifdef BT3_GS_ESPERAS
/* (degrau 2, etapa 3) gs_core.c: 1 se a gravação roda na sua thread (BT3_GS_THREAD=1); e quem para a thread de desenho
   (gs_draw.c a põe ao ligar a divisão; o Port_GsParar a chama depois de a gravação parar) */
int Port_GsComThread(void);
extern void (*gGsDesenhoParar)(void);
#endif
#ifdef BT3_GS_ESPERAS
/* (F5, etapa 8, gravação) O tempo da thread de gravação por família, somado aqui e escrito a cada 60 quadros pelo
   gs_draw.c (Gs_FamTrecho), também dentro da janela do passos (o log do gs_d3d fica mudo nela). O relógio: no 360 o
   mftb, em linha (são umas 24 mil leituras por quadro de luta: o Unpack e a VU1 rodam milhares de vezes, e uma chamada
   ao QueryPerformanceCounter em cada uma pesaria na medida), calibrado a cada trecho contra o relógio do sistema; fora
   dele o CLOCK_MONOTONIC em ns. No 360 vem ligado (BT3_TEMPOS=0 desliga); fora dele só com BT3_TEMPOS=1 (no qemu cada
   leitura do relógio é uma chamada de sistema emulada, e a rodada da PPC dobrava); BT3_TEMPOS é a mesma chave
   (a mesma chave dos tempos do backend). O hash de página pedido de dentro da VU1 soma também em GS_FAM_HASH_VU1, para
   a soma das famílias não o contar duas vezes. */
enum {
    GS_FAM_ENVIO, GS_FAM_HASH, GS_FAM_HASH_VU1, GS_FAM_VU1, GS_FAM_UNPACK, GS_FAM_ESPERA, GS_FAM_COPIA, GS_FAM_BACKEND,
    /* (por dentro, fora da soma) o gancho do backend (gs_d3d.cpp, entrega): o vu_plano com a regra do cleared, as
       profundidades e as vagas sem textura, a medida da memória; e o Unpack pelo caminho do desempacotar (a cópia V4-32, o
       laço rápido dos outros formatos sem máscara, o laço geral) */
    GS_FAM_E_PLANO, GS_FAM_E_ALVOS, GS_FAM_E_MEM, GS_FAM_UNP_COPIA, GS_FAM_UNP_RAPIDO, GS_FAM_UNP_GERAL, GS_FAM_N
};
extern int gGsFam;           /* 1 medindo; 0 desligado (BT3_TEMPOS, ou antes do primeiro fim de quadro) */
extern int gGsFamNaVu1;      /* 1 dentro do GsVu1_Call */
extern uint64_t gGsFamTiques[GS_FAM_N];
extern uint32_t gGsFamVezes[GS_FAM_N];
extern uint64_t gGsFamBytes; /* os bytes que o envio recebeu */
extern uint32_t gGsFamUnpVet[3]; /* os vetores do Unpack por caminho (GS_FAM_UNP_COPIA em diante) */
uint64_t Gs_FamTique(void);  /* gs_core.c */
double Gs_FamMs(uint64_t tiques);
#if defined(PORT_X360) && !defined(__cplusplus)
uint64_t __mftb(); /* o intrínseco do cl (o mesmo do ppcintrinsics.h): o cl o emite em linha, uma instrução, sem chamada */
extern double gGsFamTbMs; /* tiques do mftb por ms, do último trecho (gs_draw.c) */
#define GS_FAM_TIQUE() __mftb()
#else
#define GS_FAM_TIQUE() Gs_FamTique()
#endif
#define GS_FAM_INI(t) ((t) = gGsFam ? GS_FAM_TIQUE() : 0)
#define GS_FAM_FIM(f, t) do { if (gGsFam) { gGsFamTiques[f] += GS_FAM_TIQUE() - (t); gGsFamVezes[f]++; } } while (0)
/* fecha a parte f e abre a seguinte com a mesma leitura do relógio */
#define GS_FAM_PASSO(f, t) do { if (gGsFam) { uint64_t t_ = GS_FAM_TIQUE(); gGsFamTiques[f] += t_ - (t); gGsFamVezes[f]++; (t) = t_; } } while (0)
#endif
extern int gGsMainFbp; /* gs_sony.c: o buffer do quadro, escrito pelo jogo no sceGsSwapDBuff */
#ifdef BT3_GS_ESPERAS
/* (F5, etapa 8, degrau 2, etapa 2) o gGsMainFbp capturado na entrega da lista (gs_core.c, render_post) e posto pela thread
   que grava ao pegar o trabalho: com BT3_GS_THREAD=1 o jogo já pode ter escrito o do quadro seguinte. Tudo o que grava ou
   fecha o quadro lê o GS_FBP_QUADRO; sem a thread, ele recebe o gGsMainFbp logo antes de a lista andar (o mesmo valor) */
extern int gGsFbpEntregue;
#define GS_FBP_QUADRO gGsFbpEntregue
#else
#define GS_FBP_QUADRO gGsMainFbp
#endif
extern unsigned gGsFrame;

uint32_t Gs_PageHash(uint32_t page); /* content hash of an 8 KB page of GS memory */
uint32_t Gs_VramRead(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y);
/* The 256-byte block of GS memory that holds the pixel (x, y) of that buffer (formats 0x00, 0x13, 0x14). */
const uint8_t *Gs_BlockPtr(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y);
extern unsigned gGsFbUploads;
void GsGpu_FbUpload(int second);
int Gs_PsmBits(uint32_t psm);
uint32_t Gs_Expand(uint32_t c, uint32_t psm); /* stored colour -> R | G << 8 | B << 16 | A << 24 */

void Gs_Gif(const uint8_t *p, uint32_t qwc); /* GIF data for the GS (from VIF1 DIRECT or a VU1 kick) */

/* VU1 (gs_vu1.c): what VIF1 does to it, and running its programs. */
void GsVu1_SetCycle(uint32_t cl, uint32_t wl);
void GsVu1_SetBase(uint32_t v);
void GsVu1_SetOffset(uint32_t v);
void GsVu1_SetItop(uint32_t v);
void GsVu1_SetMode(uint32_t v);
void GsVu1_SetMask(uint32_t v);
void GsVu1_SetRow(const uint32_t *v);
void GsVu1_SetCol(const uint32_t *v);
void GsVu1_Program(uint32_t addr, const uint32_t *words, uint32_t count);
uint32_t GsVu1_UnpackWords(uint32_t cmd, uint32_t num);
void GsVu1_Unpack(uint32_t cmd, uint32_t num, uint32_t imm, const uint32_t *data);
void GsVu1_Call(int addr);
void GsVu1_FrameEnd(void);

/* GPU back end (gs_gpu.c). Primitive = PRIM type: 0 point, 1 line, 3 triangle, 6 sprite (strips and fans arrive
   as single lines / triangles). The state to draw with is gGs. */
int GsGpu_Init(void);
void GsGpu_Draw(int type, int ctx, const GsVertex *v);
/* A vertex program run as a shader: `count` strip vertices of 48 bytes (position + weight, normal, texture
   coordinates) with the program's constants (VU memory quadwords 0..23); layer 0 or 1. The GS state is gGs. */
int GsGpu_Enabled(void);
void GsGpu_DrawVu0(int layer, int ctx, const float *vertices, uint32_t count, const float *consts);
void GsGpu_DrawVu4(int ctx, const float *vertices, uint32_t count, const float *consts);
void GsGpu_DrawVu6(int ctx, const float *vertices, uint32_t count, const float *consts);
void Gs_RegWrite(uint32_t addr, uint64_t d);
void GsGpu_Native(int effect); /* marker register 0x7F: a native effect goes here */
void GsGpu_FrameEnd(void);
/* The game is re-running frames it has already shown (online play after a late input; the save / restore test):
   nothing of them is to be seen or heard. The frame's list is still walked, because it also carries the texture
   uploads and vertex programs that later frames count on, but no draw is recorded, nothing is shown, and no
   sound starts. (gs/state.c) */
extern int gPortResim;

/* Settings are read from the environment all over the renderer, some of them for every batch a model sends
   (tens of thousands of times per frame). The C library's getenv is cheap on Linux and slow on Windows (a lock
   and a case-insensitive search of a much larger environment): there it cost more than half of a frame. In the
   renderer's files getenv is this instead: the answer for a name is looked up once and remembered. (Nothing here
   changes the environment while the game runs.) */
#include <stdlib.h>
const char *Port_GetEnv(const char *name);
/* Each place a variable is asked for remembers its answer (the names are literals; the environment does not
   change while the game runs). Asked in the middle of drawing, the search in Port_GetEnv was 3% of a frame. */
#ifdef PORT_X360 /* the Xbox 360's compiler is C89 (no statement expressions); its getenv is a small table anyway */
#define getenv(name) Port_GetEnv(name)
#else
#define getenv(name) (__extension__({ static const char *v_; static int k_; if (!k_) { v_ = Port_GetEnv(name); k_ = 1; } v_; }))
#endif

#endif
