/* The capture side of the GPU back ends: what a frame records (draws, vertices, textures, targets), the
 * GsBackend interface each back end implements, and the state both sides share. gs_draw.c owns the
 * recording; gs_gpu.c (SDL3 GPU / Vulkan) and gs_gl.c (OpenGL) own the GPU objects, the replay of the list
 * at the end of the frame, the present, the settings window and the screenshots. gs_core.c only ever sees
 * the GsGpu_* entry points and the gGpu* counters, exactly as before the split. */
#ifndef GS_DRAW_H
#define GS_DRAW_H
#include <stdint.h>

/* The internal resolution multiplier: BT3_SCALE=1..8 (default 2). Every render target is GS_W x GS_H GS
   pixels at SCALE times the PS2's resolution, about 9 MB of video memory each at 1x, 36 MB at 2x, 144 MB at
   4x; a fight uses 7 to 16 of them. Shared: the capture scales coordinates with it, the back ends size
   their attachments with it. */
extern int gsScale;
#define SCALE gsScale
#define GS_W 1024
#define GS_H 1024

#if (defined(PORT_BIG_ENDIAN) || defined(BT3_TESTE_VU1)) && !defined(BT3_GS_ESPERAS)
#define BT3_GS_ESPERAS 1 /* (o mesmo do gs_internal.h, para o backend que inclui só este) */
#endif
#ifdef BT3_GS_ESPERAS
/* (F5, etapa 8, gravação) Com a divisão, dois jogos destas listas que se alternam entre a gravação e o desenho (gs_draw.c,
   desenho_entrega), cada um com uns 31 MiB no 360: perto de nove vezes o maior quadro medido (13.182 vértices de CPU, 53.618
   da VU, 128.907 índices, 2.253 constantes, 2.517 desenhos), menos as constantes, as de antes. O que não cabe é pulado na
   gravação, como antes. */
#define MAX_VERTS 470000
#define MAX_DRAWS (1 << 14)
#define MAX_VU_VERTS 260000
#define MAX_VU_IDX (1 << 19)
#else
#define MAX_VERTS (1 << 20)
#define MAX_DRAWS (1 << 16)
#define MAX_VU_VERTS (1 << 19)
#define MAX_VU_IDX (1 << 20)
#endif
#define MAX_TARGETS 24
#define MAX_VU_UNIFORMS 8192
#define MAX_TEX 8192
#define TEX_BUCKETS 32768 /* a power of two, four times MAX_TEX */
#define MAX_CLUTS 64
/* Strength of glare and glow in percent of what the game's passes give (BT3_GLOW=<percent>; F6 / F7 change
   it by 10). Default 60: the user's choice on 2026-10-06 (100 looked too strong at the glare's peaks). */
#define GLOW_DEFAULT 60

/* A vertex of a draw list, interleaved the way every back end receives it. */
typedef struct Vtx {
    float x, y, z;
    uint8_t r, g, b, a;
    float s, t, q;
} Vtx;

/* A texture or a frame-buffer attachment inside a back end. What a draw samples from is stored as this:
   the Vulkan back end keeps its SDL_GPUTexture * in it, the OpenGL back end its GLuint. 0 = none (the
   callers fall back to the white texture). */
typedef uintptr_t GsTex;

/* std140 layout of the uniform block the vertex programs take (shaders/vu0.vert and friends). */
typedef struct Vu0Uniform {
    float boneA[16], boneB[16], pivotA[4], pivotB[4], screen[16], light[4], color0[4], color1[4], misc[4];
    float light2[4]; /* program 1: the y components of 10..13 (the second texture coordinate of its layer 1) */
} Vu0Uniform;

/* The shared side of a render target: everything that is GS state (which frame-buffer address it is,
   whether it holds a picture, when it was last used). The colour / aux / depth attachments are the back
   end's, in arrays parallel to gsTargets[]. */
typedef struct GsTarget {
    uint32_t fbp;
    int cleared;
    unsigned draws; /* this frame */
    uint32_t gen;   /* upload generation of its first page when it was last drawn to */
    int stale;      /* a pass that should have filled it was dropped: its contents are not what the game expects */
    unsigned last;  /* frame it was last asked for */
} GsTarget;

/* One recorded primitive. Everything inside is GS state or a handle a back end made (`tex`, `pipeline`); a
   back end replays the list by rebuilding its passes around the current target and calling the pipeline. */
typedef struct GsDraw {
    int vu;      /* 1, 2, 3: vertex programs' vertices (gsVuVerts, uniform block at `uniform`); 0: gsVerts */
    int uniform;
    int native; /* 0: primitives; otherwise a native full-screen effect (PORT_FX_*) at this point of the frame;
                   3: a table pass (dclut.frag) with `tex` = its table and `src` = which byte indexes it */
    int src;
    uint32_t first, count;
    int target;
    GsTex tex;
    int tex_is_target;
    int sampler;
    int pipeline; /* the back end's pipeline index (GsBackend.pipeGet) */
    int32_t mode[4];
    float misc[4];
    float rect[4];  /* a frame buffer as texture: the part of it the GS would address (u0, v0, u1, v1 in its uv);
                       a texture pack's replacement drawn as 2D art: the piece's own rectangle of the sheet */
    float orig[4];  /* a texture pack's replacement: xy = the original texture's size in texels, z = its largest alpha */
    float blendc;
    int scissor[4]; /* x, y, width, height, at SCALE */
} GsDraw;

/* The formats a texture pack's replacement can be in (matches gs_texpack.h's TexPackImage.format). */
#define GS_TEXFMT_RGBA8 0
#define GS_TEXFMT_BC1 1
#define GS_TEXFMT_BC2 2
#define GS_TEXFMT_BC3 3

/* The frame being recorded. A back end's frameEnd reads these, uploads the vertices, replays the list and
   clears the counters for the next frame. */
extern GsDraw *gsDraws;
extern uint32_t gsDrawCount;
extern Vtx *gsVerts;            /* MAX_VERTS of them, malloc'd in GsGpu_Init */
extern uint32_t gsVertCount;
extern float *gsVuVerts;        /* 12 floats per vertex, as the vertex programs get them */
extern uint32_t gsVuVertCount;
/* The triangles of the vertex programs' draws: three indices into gsVuVerts each. A draw's `first` and `count`
   are a range of these. (A strip's vertices are stored once; as a list of whole triangles they were stored three
   times, 11 MB a frame in a two-player fight, built here and copied to the graphics card.) */
extern uint32_t *gsVuIdx;
extern uint32_t gsVuIdxCount;
extern Vu0Uniform *gsVuUni;
extern uint32_t gsVuUniCount;
extern GsTarget gsTargets[MAX_TARGETS];
extern int gsTargetCount;
extern int gsTexCount;          /* textures in the shared decode cache */
extern GsTex gsWhite;           /* the back end's 1x1 white texture */
extern int gsPendingScale;      /* the user asked for a new SCALE: applied between two frames */
extern unsigned gsFxOff;        /* effects the user can switch off (BT3_FX_OFF, or F1..F5 while running) */
extern int gsGlowPercent;
extern int gsAnchor;            /* widescreen: which part of the screen the 2D pieces being drawn belong to */
extern int gsDepthByteIsFog;    /* the top byte of the depth page as the game last left it */
extern unsigned gsNative;       /* native effect markers seen this frame */
extern unsigned gsSkipped;      /* primitives of PS2-only passes dropped this frame */
extern int gGsMainFbp;          /* the frame buffer the game shows (gs_sony.c), -1 before the first swap */
/* created / spent since the front end last cleared them (gs_core.c's slow-frame report) */
extern unsigned gGpuNewTex, gGpuNewTexPixels, gGpuNewPipes;
extern unsigned gGpuTexReplaced; /* textures taken from a texture pack so far */
extern uint64_t gGpuTexNs, gGpuPipeNs, gGpuEndNs;

/* The colour write mask FRAME.FBMSK turns into, in the bit values every back end's masks use (R=1, G=2,
   B=4, A=8: the SDL_GPU_COLORCOMPONENT_* values, and the same bits behind GL's glColorMask). */
#define GS_CC_R 1u
#define GS_CC_G 2u
#define GS_CC_B 4u
#define GS_CC_A 8u

#ifdef BT3_GS_ESPERAS
/* (F5, etapa 8, degrau 2, etapa 3) A divisão entre gravação e desenho (BT3_GS_DESENHO=1, com BT3_GS_THREAD=1): a gravação
   entrega um retrato do quadro (gs_draw.c, desenho_entrega) e segue com o seguinte; uma terceira thread chama o frameEnd do
   backend sobre o retrato. O frameEnd lê o quadro por gsQuadro: sem a divisão ele aponta para as listas vivas, com as
   contagens de agora (vivo 1, dividido 0), e o backend zera os contadores como antes; com a divisão, para o retrato (o jogo
   de listas em que a gravação gravou o quadro, sem cópia desde a etapa 8 da gravação; ela segue no outro), e quem zera é a
   gravação na entrega. */
typedef struct GsSolta {
    GsTex tex;       /* a textura a soltar; com alvo >= 0, a cor do alvo cuja vaga a gravação reaproveitou */
    int alvo;        /* -1: textura */
    unsigned quadro; /* o gGsFrame da gravação no pedido */
} GsSolta;
typedef struct GsQuadro {
    const GsDraw *draws;
    uint32_t drawCount;
    const Vtx *verts;
    uint32_t vertCount;
    const float *vuVerts;
    uint32_t vuVertCount;
    const uint32_t *vuIdx;
    uint32_t vuIdxCount;
    const uint32_t *vuFaixa; /* o menor e o maior índice de cada desenho da VU1, dois por desenho (gs_draw.c, vu_faixa) */
    const Vu0Uniform *vuUni;
    uint32_t vuUniCount;
    GsTarget *targets;  /* a tabela: a viva sem a divisão; com ela, a cópia da entrega (o cleared de antes de a gravação
                           ligar o seu), em que o desenho liga o dele: nunca a viva, e nunca o GsDraw_TargetGet (o last) */
    int targetCount;
    unsigned quadro;    /* o gGsFrame do quadro */
    uint64_t frame[2];  /* gGs.frame[0..1] no fim da gravação */
    int fbp;            /* GS_FBP_QUADRO */
    int anchor, texCount;
    unsigned skipped, native;
    double vu1Ms;       /* gGsVu1Ms do quadro (com a divisão, zerado na entrega) */
    const GsSolta *soltas; /* o que a gravação pediu para soltar até a entrega: o desenho solta depois de desenhar este quadro */
    uint32_t soltaCount;
    int vivo;           /* 1: draws, verts e as listas da VU são as que a gravação está gravando (sem a divisão) */
    int dividido;       /* 1: desenhado na thread de desenho; o backend não mexe em nada da gravação */
} GsQuadro;
extern const GsQuadro *gsQuadro;
#endif

/* The interface a back end implements (gs_gpu.c today, gs_gl.c in time). All GPU state stays the back
   end's: the shared layer only hands it handles and asks for the few numbers it must know (whether it can
   still take a texture this frame, a target's attachments). */
typedef struct GsBackend {
    const char *name;                     /* "vulkan", "opengl" */
    int (*init)(void);                    /* window + device + fixed pipelines + white texture; 0 = failed */
    void (*frameEnd)(void);               /* events, uploads, replay the list, present, UI, screenshots */
    void (*scaleChanged)(void);           /* gsScale just changed: wait idle, drop and rebuild what is sized */
    GsTex (*whiteTex)(void);              /* the 1x1 white texture made in init */
    GsTex (*texCreate)(uint32_t w, uint32_t h, int format, int levels); /* 0 = at capacity / unsupported */
    int (*texUpload)(GsTex tex, int level, uint32_t w, uint32_t h, int format, const void *px, uint32_t bytes); /* takes ownership of px; 0 = dropped */
    void (*texDestroy)(GsTex tex);
    int (*texSlotsLeft)(void);            /* how many more textures it can take this frame */
    void (*targetEnsure)(int i);          /* create the attachments of shared target i */
    void (*targetDrop)(int i);            /* release them */
    GsTex (*targetColor)(int i);
    GsTex (*targetAux)(int i);
    GsTex (*targetDepth)(int i);
    int (*pipeGet)(uint32_t key);         /* the back end's pipeline for a key, creating it if new */
#ifdef BT3_GS_ESPERAS
    /* (degrau 2, etapa 3) só com a divisão; sem os dois últimos o BT3_GS_DESENHO=1 é recusado. entrega: na gravação, o
       retrato pronto (antes de zerar os contadores): o que o backend leva do lado da gravação, e limpa[i] (já 1 para todo
       alvo com desenho não nativo) diz para que alvos a gravação liga o seu cleared, a regra do backend. soltaPede: na
       gravação, o pedido de soltar uma textura (alvo -1) ou o alvo de uma vaga reaproveitada (no lugar de texDestroy e
       targetDrop); solta: no desenho, depois de ter desenhado o quadro do pedido. */
    void (*entrega)(const GsQuadro *q, unsigned char *limpa);
    void (*soltaPede)(GsTex tex, int alvo);
    void (*solta)(GsTex tex, int alvo);
#endif
} GsBackend;

extern GsBackend sVulkanBackend; /* gs_gpu.c */
extern GsBackend sGlBackend;     /* gs_gl.c */

/* gs_draw.c: the shared target table's entry for a frame-buffer address (create: one is made if absent,
   with the back end's attachments through GsBackend.targetEnsure). */
int GsDraw_TargetGet(uint32_t fbp, int create);
#ifdef BT3_GS_ESPERAS
void GsDraw_DesenhoEspera(void); /* (degrau 2) a thread de desenho acabou o retrato entregue (nada sem a divisão) */
#ifdef PORT_X360
int GsDraw_Dividido(void);        /* (degrau 2, etapa 4) 1: o desenho numa terceira thread, dona do dispositivo */
void GsDraw_DesenhoAcorda(void);  /* acorda o desenho ocioso (o pedido de memória da gravação) */
void GsDraw_DesenhoPararJa(void); /* uma queda: o desenho sai sem esperar e solta o dispositivo */
void GsDraw_DesenhoCaiu(void);    /* a thread de desenho caiu (no AoMorrer dela) */
#endif
#endif

#endif
