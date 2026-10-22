/*
 * Graphics Synthesizer, the capture side of the GPU back ends (gs_gpu.c: Vulkan, gs_gl.c: OpenGL).
 *
 * gs_core.c still decodes what the game sends and keeps GS memory; this file takes over at "a primitive with
 * the current GS registers": it records every primitive of the frame as a GsDraw (what to draw, where, with
 * which texture and which pipeline), decodes GS memory into ordinary RGBA textures and keeps them cached.
 * The back end replays the list at the end of the frame.
 *   render targets   one per GS frame-buffer address in use, GS_W x GS_H GS pixels at SCALE times the PS2's
 *                    resolution; the shared table holds the GS state, each back end its attachments
 *   textures         GS memory decoded to ordinary RGBA textures, cached by TEX0 / TEXA and the upload
 *                    generation of the pages they occupy; a frame buffer used as a texture is sampled directly
 *   blending         the GS equation ((A - B) * C >> 7) + D mapped onto blend factors, as a key of pure GS
 *                    state bits the back end turns into its own blend state
 *   alpha            1.0 = 0x80, so that "source alpha" blending needs no extra scaling
 * Not handled yet: a target sampled while it is being drawn to, region clamp / repeat, destination-alpha tests,
 * frame-buffer masks, 16-bit targets' precision.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL3/SDL.h>   /* the one window both back ends share */
#include "gs_internal.h"
#include "gs_draw.h"
#include "ui.h"         /* PortVideo and the window helpers */
#include "gs_texpack.h" /* texture packs (replacement textures) */
#include <math.h>

/* Stage-name overlay: defined here (C) so both the renderer (gs_gpu.c) and the game's menu (menu_c_e.c) can
   reach them without C++ name mangling. Where they mean is in ui.h. */
volatile int gUiPresentX, gUiPresentY, gUiPresentW, gUiPresentH;
volatile int gUiNameX, gUiNameY, gUiNameW, gUiNameH;
volatile int gUiNameIdx = -1;
volatile int gUiNameReady;
volatile int gUiSongX, gUiSongY, gUiSongW, gUiSongH;
volatile int gUiSongIdx = -1;
volatile int gUiSongReady;
volatile int gUiSongLit; /* the added song's name is the one of the open music list (the game's second name style) */

extern int Port_Setting(const char *name, int def); /* plat_settings.c: the saved settings */
extern void Port_SettingSave(const char *name, int value);
extern int Port_IsWide(void);   /* plat_stub.c */
extern int Port_AspectMilli(void);
extern void Port_SetAspectMilli(int milli), Port_AudioRefresh(void), Port_SettingsWrite(void);
extern int gPortMenuMode;       /* headless.c: the menus are running */
extern int gPortMusicPercent, gPortSePercent;

static GsBackend *sBackend;
#ifdef BT3_GS_ESPERAS
#include <pthread.h>
/* (degrau 2, etapa 3) o pedido de soltar uma textura ou o alvo de uma vaga: direto ao backend sem a divisão, pela fila com
   ela (desenho_entrega, mais abaixo) */
static void solta_pede(GsTex tex, int alvo);
static void divisao_decide(void);
#ifdef PORT_X360
static int desenho_init(void);
#endif
static void backend_frame_end(void);
#define BACKEND_FRAME_END() backend_frame_end()
#define TEX_DESTROY(t) solta_pede((t), -1)
#else
#define TEX_DESTROY(t) sBackend->texDestroy(t)
#define BACKEND_FRAME_END() sBackend->frameEnd()
#endif

/* The one window both back ends share: shape (aspect), full screen and display are the renderer's, not a
   back end's, so the settings window and F11 work the same whichever back end is running. */
static SDL_Window *sWindow;
static int sFullscreen;
static float sWantAspect;
static int sDisplaySetting;

int gsScale = 2;
int gsPendingScale;
GsTex gsWhite;
GsDraw *gsDraws;
uint32_t gsDrawCount;
Vtx *gsVerts;
uint32_t gsVertCount;
float *gsVuVerts;
uint32_t gsVuVertCount;
uint32_t *gsVuIdx;
uint32_t gsVuIdxCount;
Vu0Uniform *gsVuUni;
uint32_t gsVuUniCount;
GsTarget gsTargets[MAX_TARGETS];
int gsTargetCount;
int gsTexCount;
unsigned gsFxOff;
int gsGlowPercent = GLOW_DEFAULT;
static int sTexPackOn = 1; /* the setting: use the texture pack (if one was found) */
int gsAnchor;
int gsDepthByteIsFog;
unsigned gsNative;
unsigned gsSkipped;
unsigned gGpuNewTex, gGpuNewTexPixels, gGpuNewPipes; /* created since the front end last cleared them (slow-frame report) */
unsigned gGpuTexReplaced; /* textures taken from a texture pack so far */
uint64_t gGpuTexNs, gGpuPipeNs, gGpuEndNs; /* time spent decoding textures, creating pipelines, in GsGpu_FrameEnd */
#if defined(BT3_GS_ESPERAS) && defined(BT3_GPU_FALSO)
/* (F5, etapa 8, gravação) BT3_GS_CAP_DIV=N, só de teste nas builds --gpu: as capacidades das listas divididas por N, para ver o
   que não cabe se comportar igual com a divisão ligada e desligada */
static uint32_t sCapDiv = 1;
#define CAP_VERTS (MAX_VERTS / sCapDiv)
#define CAP_DRAWS (MAX_DRAWS / sCapDiv)
#define CAP_VU_VERTS (MAX_VU_VERTS / sCapDiv)
#define CAP_VU_IDX (MAX_VU_IDX / sCapDiv)
#define CAP_VU_UNIFORMS (MAX_VU_UNIFORMS / sCapDiv)
#else
#define CAP_VERTS MAX_VERTS
#define CAP_DRAWS MAX_DRAWS
#define CAP_VU_VERTS MAX_VU_VERTS
#define CAP_VU_IDX MAX_VU_IDX
#define CAP_VU_UNIFORMS MAX_VU_UNIFORMS
#endif
#ifdef BT3_GS_ESPERAS
/* (F5, etapa 8, gravação) Os dois jogos de listas: gsDraws, gsVerts e as três da VU apontam para o jogo em que a gravação
   grava; na entrega, com a divisão, o jogo vai para o desenho como está e a gravação passa para o outro (desenho_entrega).
   Sem a divisão só existe o primeiro. */
typedef struct GsJogo {
    GsDraw *draws;
    Vtx *verts;
    float *vuVerts;
    uint32_t *vuIdx;
    Vu0Uniform *vuUni;
    uint32_t *vuFaixa; /* (vu_faixa) o menor e o maior índice de cada desenho da VU1, dois por desenho */
} GsJogo;
static GsJogo sJogos[2];
static int sJogo; /* o jogo da gravação */
static uint32_t *sVuFaixa; /* a vuFaixa do jogo da gravação */
#define JOGO_BYTES ((unsigned)(MAX_DRAWS * sizeof(GsDraw) + MAX_VERTS * sizeof(Vtx) + MAX_VU_VERTS * 48u + MAX_VU_IDX * 4u + \
                               MAX_VU_UNIFORMS * sizeof(Vu0Uniform) + MAX_DRAWS * 8u))
static int jogo_aloca(GsJogo *j) {
    j->verts = (Vtx *)malloc(MAX_VERTS * sizeof(Vtx));
    j->vuVerts = (float *)malloc(MAX_VU_VERTS * 48u);
    j->vuIdx = (uint32_t *)malloc(MAX_VU_IDX * sizeof(uint32_t));
    j->vuUni = (Vu0Uniform *)malloc(MAX_VU_UNIFORMS * sizeof(Vu0Uniform));
    j->draws = (GsDraw *)malloc(MAX_DRAWS * sizeof(GsDraw));
    j->vuFaixa = (uint32_t *)malloc(MAX_DRAWS * 8u);
    if (j->verts == NULL || j->vuVerts == NULL || j->vuIdx == NULL || j->vuUni == NULL || j->draws == NULL || j->vuFaixa == NULL) {
        free(j->verts);
        free(j->vuVerts);
        free(j->vuIdx);
        free(j->vuUni);
        free(j->draws);
        free(j->vuFaixa);
        memset(j, 0, sizeof(*j));
        return 0;
    }
    return 1;
}
static void jogo_usa(int k) {
    sJogo = k;
    gsDraws = sJogos[k].draws;
    gsVerts = sJogos[k].verts;
    gsVuVerts = sJogos[k].vuVerts;
    gsVuIdx = sJogos[k].vuIdx;
    gsVuUni = sJogos[k].vuUni;
    sVuFaixa = sJogos[k].vuFaixa;
}
/* (F5, etapa 8, gravação) O menor e o maior índice de um desenho da VU1, guardados quando a gravação gera os índices, para o
   vu_plano do backend não reler todos eles (uns 129 mil por quadro de luta no 360). Os três GsGpu_DrawVu* geram os índices de
   um trecho em ordem crescente (base + i + k, com i e k crescendo), então o menor é o primeiro do trecho e o maior o último;
   quando o trecho se junta ao desenho anterior (same_state), a faixa é a união das duas. Um desenho sem índices fica com a
   faixa vazia (menor 0xFFFFFFFF, maior 0), que não muda a união. A tabela é do jogo (sJogos), como as listas: o desenho lê a
   do retrato entregue e a gravação escreve na do outro. */
static void vu_faixa(uint32_t n, uint32_t first, uint32_t count, int junta) {
    uint32_t lo = 0xFFFFFFFFu, hi = 0;
    if (count != 0) {
        lo = gsVuIdx[first];
        hi = gsVuIdx[first + count - 1];
    }
    if (junta) {
        if (sVuFaixa[2 * n] < lo) lo = sVuFaixa[2 * n];
        if (sVuFaixa[2 * n + 1] > hi) hi = sVuFaixa[2 * n + 1];
    }
    sVuFaixa[2 * n] = lo;
    sVuFaixa[2 * n + 1] = hi;
}
#endif

static uint64_t gpu_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

/* GS memory -> an RGBA texture, cached by TEX0 / TEXA and the content of the pages it occupies. */
typedef struct Tex {
    uint64_t tex0, texa;
    uint32_t gen;
    GsTex tex;
    unsigned last; /* frame last used */
    int replaced;  /* from a texture pack: several times the size, so always filtered (texture_get) */
    uint32_t ow, oh, rw, rh; /* then: the original's size and the replacement's */
    uint32_t amax;           /* and the largest alpha byte of the original */
} Tex;
static Tex sTex[MAX_TEX];
static uint16_t sTexBucket[TEX_BUCKETS]; /* index + 1 of a cache entry, 0 = empty (open addressing) */
static Tex *sLast; /* the entry the previous lookup returned */

/* The 256-colour tables as textures, by hash of the palette entries they use (64 slots). */
static struct { uint32_t hash, last; GsTex tex; } sCluts[MAX_CLUTS];

static uint32_t tex_bucket(uint64_t t0, uint64_t texa, uint32_t gen) {
    uint64_t h = (t0 ^ (texa * 0x9E3779B97F4A7C15ull) ^ ((uint64_t)gen << 17)) * 0xD6E8FEB86659FD93ull;
    return (uint32_t)(h >> 40) & (TEX_BUCKETS - 1);
}

/* A hash of the palette entries a texture uses (16 or 256 at `cbp`), remembered until one of the palette's pages is
   written again. The page hashes are too coarse here: a page holds up to 32 palettes, and the game rewrites some
   of them every frame (lit palettes, the fighters' palettes painted for the see-through pass), which made every
   texture with a palette in the same page count as new and be decoded again, hundreds per frame on some stages. */
static uint32_t clut_hash(uint32_t cbp, uint32_t cpsm, uint32_t count) {
    static struct { uint32_t key, gen0, gen1, hash; } memo[1024];
    uint32_t key = cbp << 5 | cpsm << 1 | (count == 256), slot = (key * 2654435761u) >> 22, h = 2166136261u, i;
    uint32_t g0 = gGsPageGen[(cbp / 32) & 511], g1 = gGsPageGen[(cbp / 32 + 1) & 511];

    if (memo[slot].key == key + 1 && memo[slot].gen0 == g0 && memo[slot].gen1 == g1) {
        return memo[slot].hash;
    }
    for (i = 0; i < count; i++) {
        h = (h ^ Gs_VramRead(cbp, 1, cpsm, i & 15, i >> 4)) * 16777619u;
    }
    memo[slot].key = key + 1;
    memo[slot].gen0 = g0;
    memo[slot].gen1 = g1;
    memo[slot].hash = h;
    return h;
}

#ifdef PORT_X360
/* (F5, etapa 1) O descarte por uso do 360. O backend (port/x360/src/gs/gs_d3d.cpp) conta os bytes reais e diz quanto
   falta para um pedido caber (piso de memória livre, teto das texturas); este cache é o dono das texturas, então é ele
   que descarta: as entradas usadas há mais tempo, nunca as dos dois últimos quadros (a lista em gravação só aponta
   para texturas com last == gGsFrame, e gGsFrame só avança depois do frameEnd). O recurso solto espera a GPU na cova
   do backend. Contrato de posse: docs/x360/plano/F5/01.md. */
uint32_t GsD3d_Falta(uint32_t w, uint32_t h, int modo);
uint32_t GsD3d_TexBytes(GsTex tex);
int GsD3d_Descartou(uint32_t w, uint32_t h, int modo, unsigned n, uint32_t bytes, unsigned idade, unsigned emUso);
void GsD3d_Branca(int motivo, uint32_t w, uint32_t h);
static uint16_t sX360Ordem[MAX_TEX];
static unsigned char sX360Sai[MAX_TEX];

static int x360_mais_antiga(const void *a, const void *b) {
    unsigned la = sTex[*(const uint16_t *)a].last, lb = sTex[*(const uint16_t *)b].last;
    return la < lb ? -1 : la > lb ? 1 : 0;
}

/* modo 0: textura w x h; 1: alvo; 2: textura que o D3D recusou com o piso cumprido. 1: o pedido cabe. */
int GsDraw_X360Abre(uint32_t w, uint32_t h, int modo) {
    uint32_t falta = GsD3d_Falta(w, h, modo), soma = 0, i, n = 0, k;
    unsigned idade = 0;
    int m;

    if (falta == 0) {
        return 1;
    }
    for (i = 0; i < (uint32_t)gsTexCount; i++) {
        sX360Sai[i] = 0;
        if (sTex[i].last + 2 <= gGsFrame) {
            sX360Ordem[n++] = (uint16_t)i;
        }
    }
    qsort(sX360Ordem, n, sizeof(sX360Ordem[0]), x360_mais_antiga);
    for (k = 0; k < n && soma < falta; k++) {
        soma += GsD3d_TexBytes(sTex[sX360Ordem[k]].tex);
        sX360Sai[sX360Ordem[k]] = 1;
    }
    if (k > 0) {
        idade = gGsFrame - sTex[sX360Ordem[k - 1]].last;
        /* fora do cache e do balde antes de soltar: nada fica apontando para o recurso */
        for (m = 0, i = 0; i < (uint32_t)gsTexCount; i++) {
            if (sX360Sai[i]) {
                TEX_DESTROY(sTex[i].tex);
            } else {
                sTex[m++] = sTex[i];
            }
        }
        gsTexCount = m;
        sLast = NULL;
        memset(sTexBucket, 0, sizeof(sTexBucket));
        for (i = 0; i < (uint32_t)gsTexCount; i++) {
            uint32_t h2 = tex_bucket(sTex[i].tex0, sTex[i].texa, sTex[i].gen);
            while (sTexBucket[h2] != 0) {
                h2 = (h2 + 1) & (TEX_BUCKETS - 1);
            }
            sTexBucket[h2] = (uint16_t)(i + 1);
        }
    }
    return GsD3d_Descartou(w, h, modo, k, soma, idade, (unsigned)gsTexCount - (n - k));
}
#endif

#if defined(PORT_BIG_ENDIAN) || defined(BT3_TESTE_VU1)
/* (F5, etapa 8, degrau 1) Quantas páginas de 8 KB, a partir de tbp / 32, a decodificação de uma textura pode ler: as do
   canto de baixo e da direita (a página só cresce com x e com y), mais a página a que um bloco do fim da última página
   da grade vaza quando tbp não começa numa página. É a conta do vram_rw (as mesmas expressões, em páginas): o laço de antes,
   `i <= pages` sobre o tamanho linear, hasheava uma página a mais (e, nos formatos de 8 e 4 bits dentro de 32, a menos). O
   `gen` do texture_get só precisa do conteúdo do que a decodificação lê. */
static uint32_t tex_pages(uint32_t tbp, uint32_t bw, uint32_t psm, uint32_t tw, uint32_t th) {
    uint32_t x = tw - 1, y = th - 1, grid;

    if ((psm & 0x30) == 0x30) {
        psm &= 0x0F;
    }
    if (bw == 0) {
        bw = 1;
    }
    switch (psm) {
    case 0x02: case 0x0A: grid = (y >> 6) * bw + (x >> 6); break;        /* 16 bits: páginas de 64 x 64 */
    case 0x13: grid = (y >> 6) * (bw >> 1) + (x >> 7); break;            /* 8 bits: 128 x 64 */
    case 0x14: grid = (y >> 7) * (bw >> 1) + (x >> 7); break;            /* 4 bits: 128 x 128 */
    default: grid = (y >> 5) * bw + (x >> 6); break;                     /* 32 bits e os que moram num pixel de 32: 64 x 32 */
    }
    return grid + ((tbp & 31) != 0) + 1;
}
#endif

/* GS memory -> an RGBA texture for the current TEX0 (alpha as stored: 0x80 is opaque). The back end makes
   the texture and takes the decoded pixels (freed after it has uploaded them). */
static GsTex texture_get(int ctx) {
    uint64_t t0 = gGs.tex0[ctx] & 0x1FFFFFFFFFFFFFFFull, texa = gGs.texa;
    uint32_t tbp = t0 & 0x3FFF, tbw = (t0 >> 14) & 0x3F, psm = (t0 >> 20) & 0x3F, tw = 1u << ((t0 >> 26) & 15), th = 1u << ((t0 >> 30) & 15);
    uint32_t cbp = (t0 >> 37) & 0x3FFF, cpsm = (t0 >> 51) & 15;
    uint32_t bits = (uint32_t)Gs_PsmBits(psm), gen = 0, pages, i, x, y, *px;
    Tex *t;
    uint64_t tex_t0;
    TexPackImage pack;
    uint32_t packMaxAlpha = 255, rw = tw, rh = th;
    int packFormat = GS_TEXFMT_RGBA8, replaced = 0;

    if (tw > 1024) { tw = 1024; }
    if (th > 1024) { th = 1024; }
    pages = ((tbw ? tbw : 1) * 64 * th * (bits == 24 ? 32 : bits) / 8 + 8191) / 8192;
#if defined(PORT_BIG_ENDIAN) || defined(BT3_TESTE_VU1)
    /* (F5, etapa 8) `gen` depende só de t0 e do conteúdo da VRAM, e o conteúdo só muda quando uma gGsPageGen muda
       (todo lugar que a muda sobe gGsPageEpoch). Um desenho da VU1 pede a mesma textura que o anterior, ou uma das
       poucas da luta, centenas de vezes entre dois envios: o resultado do laço abaixo fica numa tabela por t0 e vale
       enquanto a época for a mesma. É o mesmo número que o laço dá, sem pedir o hash de cada página a cada desenho. */
    {
        static struct { uint64_t t0; uint32_t epoch, gen; } memo[16];
        uint32_t m = ((uint32_t)(t0 ^ (t0 >> 29) ^ (t0 >> 47)) * 2654435761u) >> 28;
        if (memo[m].epoch == gGsPageEpoch + 1 && memo[m].t0 == t0) {
            gen = memo[m].gen;
        } else {
            pages = tex_pages(tbp, tbw, psm, tw, th);
            for (i = 0; i < pages; i++) {
                gen = (gen ^ Gs_PageHash(tbp / 32 + i)) * 16777619u + i;
            }
            if (bits <= 8) {
                gen = (gen ^ clut_hash(cbp, cpsm, bits == 8 ? 256 : 16)) * 16777619u;
            }
            memo[m].t0 = t0;
            memo[m].epoch = gGsPageEpoch + 1;
            memo[m].gen = gen;
        }
    }
#else
    /* `gen` identifies the CONTENT: a hash over the pages the texture and its palette occupy */
    for (i = 0; i <= pages; i++) {
        gen = (gen ^ Gs_PageHash(tbp / 32 + i)) * 16777619u + i;
    }
    if (bits <= 8) {
        gen = (gen ^ clut_hash(cbp, cpsm, bits == 8 ? 256 : 16)) * 16777619u;
    }
#endif
    if (sTexPackOn && TexPack_Count() != 0) {
        gen ^= 0x5BD1E995u; /* with the pack and without it are different textures: switching it takes effect at once */
    }
    if (sLast != NULL && sLast->tex0 == t0 && sLast->texa == texa && sLast->gen == gen) {
        sLast->last = gGsFrame;
        return sLast->tex; /* the common case: the same texture as the previous primitive */
    }
    {
        uint32_t h = tex_bucket(t0, texa, gen);
        while (sTexBucket[h] != 0) {
            Tex *c = &sTex[sTexBucket[h] - 1];
            if (c->tex0 == t0 && c->texa == texa && c->gen == gen) {
                c->last = gGsFrame;
                sLast = c;
                return c->tex;
            }
            h = (h + 1) & (TEX_BUCKETS - 1);
        }
    }
#ifdef PORT_X360
    if (!GsDraw_X360Abre(tw, th, 0)) {
        GsD3d_Branca(1, tw, th);
        return gsWhite;
    }
#else
    if (sBackend->texSlotsLeft() <= 0) {
        return gsWhite;
    }
#endif
#if defined(BT3_GS_ESPERAS) && defined(BT3_GPU_FALSO)
    /* (degrau 2, etapa 3, só nas builds --gpu do WSL) BT3_GS_TEX_MAX=N: o cache "cheio" com N texturas, para a varredura
       soltar texturas do quadro anterior, as que o desenho ainda pode estar desenhando (a prova da fila de liberação) */
    static int texMax;
    if (texMax == 0) {
        texMax = getenv("BT3_GS_TEX_MAX") != NULL ? atoi(getenv("BT3_GS_TEX_MAX")) : MAX_TEX;
        texMax = texMax < 64 ? 64 : texMax > MAX_TEX ? MAX_TEX : texMax;
    }
#define TEX_CHEIO (gsTexCount >= texMax)
#else
#define TEX_CHEIO (gsTexCount == MAX_TEX)
#endif
    if (TEX_CHEIO) {
        /* full: drop everything not used for two seconds (or, failing that, not used this frame) in one sweep */
        unsigned keep = gGsFrame > 120 ? gGsFrame - 120 : 0;
        int pass, n;
        for (pass = 0; pass < 2 && TEX_CHEIO; pass++, keep = gGsFrame) {
            for (n = 0, i = 0; i < (uint32_t)gsTexCount; i++) {
                if (sTex[i].last >= keep) {
                    sTex[n++] = sTex[i];
                } else {
                    TEX_DESTROY(sTex[i].tex);
                }
            }
            gsTexCount = n;
        }
        sLast = NULL;
        memset(sTexBucket, 0, sizeof(sTexBucket));
        for (i = 0; i < (uint32_t)gsTexCount; i++) {
            uint32_t h = tex_bucket(sTex[i].tex0, sTex[i].texa, sTex[i].gen);
            while (sTexBucket[h] != 0) {
                h = (h + 1) & (TEX_BUCKETS - 1);
            }
            sTexBucket[h] = (uint16_t)(i + 1);
        }
        if (gsTexCount == MAX_TEX) {
#ifdef PORT_X360
            GsD3d_Branca(2, tw, th);
#endif
            return gsWhite; /* this one frame uses more textures than the cache holds */
        }
    }
    tex_t0 = gpu_now();
    /* a texture pack's replacement for this texture, if there is one (gs_texpack.c) */
    if (sTexPackOn && TexPack_Count() != 0) {
        const char *path = TexPack_Lookup(tbp, tbw, psm, tw, th, (uint32_t)((t0 >> 34) & 1), cbp, cpsm, &packMaxAlpha);
        if (path != NULL && TexPack_Load(path, &pack)) {
            packFormat = pack.format & 3;
            if (sBackend->texSlotsLeft() >= pack.levels) {
                replaced = 1;
                gGpuTexReplaced++;
                rw = pack.w;
                rh = pack.h;
            } else {
                TexPack_Free(&pack);
            }
        }
    }
    px = replaced ? NULL : malloc((size_t)tw * th * 4);
    for (y = 0; !replaced && y < th; y++) {
        for (x = 0; x < tw; x++) {
            uint32_t c = Gs_VramRead(tbp, tbw, psm, x, y), a;
            if (bits > 8) {
                c = Gs_Expand(c, psm);
            } else if (bits == 8) {
                c = (c & 0xE7) | ((c & 8) << 1) | ((c & 0x10) >> 1);
                c = Gs_Expand(Gs_VramRead(cbp, 1, cpsm, c & 15, c >> 4), cpsm);
            } else {
                c = Gs_Expand(Gs_VramRead(cbp, 1, cpsm, c & 7, c >> 3), cpsm);
            }
            (void)a; /* alpha is kept as the GS has it (0x80 = opaque, up to 0xFF): the shader rescales it. Values
                        above 0x80 matter: the see-through pass gives palettes alpha 0xF9 to write an id */
            px[y * tw + x] = c;
        }
    }
#ifdef PORT_X360
    {   /* (picos: a decodificação do quadro, gs_internal.h) */
        uint64_t d_ = gpu_now() - tex_t0;
        gGpuTexNs += d_;
        gPicosTexNs += d_;
    }
#else
    gGpuTexNs += gpu_now() - tex_t0;
#endif
#ifdef PORT_X360
    {
        /* o D3D recusou com o piso cumprido: descarta mais e tenta uma vez de novo (o descarte muda sTex: só depois
           pega a vaga) */
        GsTex nova = replaced ? sBackend->texCreate(rw, rh, packFormat, pack.levels) : sBackend->texCreate(tw, th, GS_TEXFMT_RGBA8, 1);
        if (nova == 0 && !replaced && GsDraw_X360Abre(tw, th, 2)) {
            nova = sBackend->texCreate(tw, th, GS_TEXFMT_RGBA8, 1);
        }
        if (nova == 0) {
            GsD3d_Branca(3, tw, th);
        }
        t = &sTex[gsTexCount];
        t->tex = nova;
    }
#else
    t = &sTex[gsTexCount];
    t->tex = replaced ? sBackend->texCreate(rw, rh, packFormat, pack.levels) : sBackend->texCreate(tw, th, GS_TEXFMT_RGBA8, 1);
#endif
    if (t->tex == 0) {
        free(px);
        if (replaced) {
            TexPack_Free(&pack);
        }
        return gsWhite;
    }
    if (replaced) {
        int level;
        for (level = 0; level < pack.levels; level++) {
            uint32_t lw = pack.w >> level ? pack.w >> level : 1, lh = pack.h >> level ? pack.h >> level : 1;
            void *buf = malloc(pack.bytes[level]);
            memcpy(buf, pack.data[level], pack.bytes[level]);
            sBackend->texUpload(t->tex, level, lw, lh, packFormat, buf, pack.bytes[level]);
        }
        TexPack_Free(&pack);
    } else {
        sBackend->texUpload(t->tex, 0, tw, th, GS_TEXFMT_RGBA8, px, tw * th * 4);
        if (getenv("BT3_TEX_DUMP") != NULL) { /* every texture as it is decoded, raw RGBA with a small header (looking at the game's art) */
            static unsigned n;
            char name[600];
            FILE *df;
            snprintf(name, sizeof(name), "%s/tex_%05u_f%06u_%ux%u.rgba", getenv("BT3_TEX_DUMP"), n++, gGsFrame, (unsigned)tw, (unsigned)th);
            df = fopen(name, "wb");
            if (df != NULL) {
                fwrite(px, 1, (size_t)tw * th * 4, df);
                fclose(df);
            }
        }
    }
    gsTexCount++;
    {
        uint32_t h = tex_bucket(t0, texa, gen);
        while (sTexBucket[h] != 0) {
            h = (h + 1) & (TEX_BUCKETS - 1);
        }
        sTexBucket[h] = (uint16_t)gsTexCount;
    }
    gGpuNewTex++;
    gGpuNewTexPixels += tw * th;
#ifdef PORT_X360
    gPicosTexN++;
    gPicosTexPx += tw * th;
#endif
    sLast = t;
    t->tex0 = t0;
    t->texa = texa;
    t->gen = gen;
    t->last = gGsFrame;
    t->replaced = replaced;
    t->ow = tw;
    t->oh = th;
    t->rw = rw;
    t->rh = rh;
    t->amax = replaced ? packMaxAlpha : 255u;
    return t->tex;
}

/* A 256-colour table at `cbp` (32-bit, the GS's index order undone) as a 256 x 1 texture; alpha as stored. */
static GsTex clut_texture(uint32_t cbp) {
    uint32_t px[256], hash = 2166136261u, i;
    GsTex tex;
    uint32_t *upload;
    int k, old = 0;
#ifdef BT3_GS_ESPERAS
    /* (F5, etapa 8, gravação) O hash da tabela lembrado por cbp e pela geração das duas páginas que ela ocupa (a mesma
       regra do clut_hash: 16 x 16 pixels de 32 bits com largura 1 são os blocos cbp a cbp + 3). As passadas de tabela
       pela névoa e pelos ids pedem a mesma tabela a cada primitiva, e o hash dela era refeito toda vez, 256 leituras da
       VRAM, para achar a mesma textura no sCluts. As 256 cores só se leem quando a memória não vale, ou para criar a
       textura. O sCluts e a escolha da vaga seguem iguais: a mesma textura volta, com o mesmo last. */
    static struct { uint32_t key, gen0, gen1, hash; } memo[64];
    uint32_t m = (cbp * 2654435761u) >> 26, g0 = gGsPageGen[(cbp / 32) & 511], g1 = gGsPageGen[(cbp / 32 + 1) & 511];
    int lido = 0;

    if (memo[m].key == cbp + 1 && memo[m].gen0 == g0 && memo[m].gen1 == g1) {
        hash = memo[m].hash;
    } else {
        for (i = 0; i < 256; i++) {
            uint32_t n = (i & 0xE7) | ((i & 8) << 1) | ((i & 0x10) >> 1);
            px[i] = Gs_VramRead(cbp, 1, 0, n & 15, n >> 4);
            hash = (hash ^ px[i]) * 16777619u;
        }
        lido = 1;
        memo[m].key = cbp + 1;
        memo[m].gen0 = g0;
        memo[m].gen1 = g1;
        memo[m].hash = hash;
    }
#else
    for (i = 0; i < 256; i++) {
        uint32_t n = (i & 0xE7) | ((i & 8) << 1) | ((i & 0x10) >> 1);
        px[i] = Gs_VramRead(cbp, 1, 0, n & 15, n >> 4);
        hash = (hash ^ px[i]) * 16777619u;
    }
#endif
    for (k = 0; k < MAX_CLUTS; k++) {
        if (sCluts[k].tex != 0 && sCluts[k].hash == hash) {
            sCluts[k].last = gGsFrame;
            return sCluts[k].tex;
        }
        if (sCluts[k].tex == 0 || sCluts[k].last < sCluts[old].last) {
            old = sCluts[k].tex == 0 && sCluts[old].tex == 0 ? old : k;
        }
    }
    if (sBackend->texSlotsLeft() <= 0 || (sCluts[old].tex != 0 && sCluts[old].last == gGsFrame)) {
        return 0;
    }
    if (sCluts[old].tex != 0) {
        TEX_DESTROY(sCluts[old].tex);
    }
#ifdef BT3_GS_ESPERAS
    if (!lido) { /* (as páginas não mudaram desde o hash lembrado: as cores são as que deram esse hash) */
        for (i = 0; i < 256; i++) {
            uint32_t n = (i & 0xE7) | ((i & 8) << 1) | ((i & 0x10) >> 1);
            px[i] = Gs_VramRead(cbp, 1, 0, n & 15, n >> 4);
        }
    }
#endif
    upload = malloc(sizeof(px));
    memcpy(upload, px, sizeof(px));
    tex = sBackend->texCreate(256, 1, GS_TEXFMT_RGBA8, 1);
    if (tex == 0) {
        free(upload);
        return 0;
    }
    sBackend->texUpload(tex, 0, 256, 1, GS_TEXFMT_RGBA8, upload, sizeof(px));
    sCluts[old].tex = tex;
    sCluts[old].hash = hash;
    sCluts[old].last = gGsFrame;
    return tex;
}

/* ((A - B) * C >> 7) + D as the pipeline key's blend bits: ks * Cs + kd * Cd with ks, kd from
   {0, 1, C, -C, 1 - C, 1 + C}. Each of A, B, D is source, destination or zero, so bit i of A/B decides
   which side is scaled. Returns the key of pure GS state (bits 0..8); the back end turns it back into
   blend factors. k[i]: 0 nothing, 1 one, 2 C, 3 -C, 4 1 - C, 5 1 + C; i = 0 source, 1 destination. */
static uint32_t blend_key(uint64_t alpha, int abe) {
    int A = alpha & 3, B = (alpha >> 2) & 3, C = (alpha >> 4) & 3, D = (alpha >> 6) & 3, k[2], i;

    if (!abe) {
        return 0;
    }
    for (i = 0; i < 2; i++) {
        int c = (A == i) - (B == i), one = D == i;
        k[i] = c == 0 ? one : c > 0 ? (one ? 5 : 2) : (one ? 4 : 3);
    }
    return 1u | (uint32_t)k[0] << 1 | (uint32_t)k[1] << 4 | (uint32_t)C << 7;
}

/* The pipeline a draw uses: everything a pipeline depends on is in the key (the back end makes and caches
   one pipeline per key, so this is pure GS state -> an index):
   bits 0..8 blend, 10..11 depth test, 12 depth write, 13..14 topology, 16..19 colour write mask,
   20..21 vertices: 0 GS vertices, 1 program 0, 2 program 4, 3 program 6, 22 full-screen table pass. */
static int pipeline_get(int ctx, int topo, int vu) {
    uint64_t test = gGs.test[ctx], zb = gGs.zbuf[ctx];
    int zte = (test >> 16) & 1, ztst = (test >> 17) & 3, zwrite = !((zb >> 32) & 1);
    uint32_t bkey, key, wmask, m = (uint32_t)(gGs.frame[ctx] >> 32);

    bkey = blend_key(gGs.alpha[ctx], (int)((gGs.prim >> 6) & 1));
    if (!zte) {
        ztst = 1;
    }
    /* FRAME.FBMSK, in the whole-channel forms ordinary drawing uses (alpha only, everything but alpha, ...):
       a channel whose eight mask bits are all set is not written. Partial masks are not representable. */
    wmask = ((m & 0xFF) != 0xFF ? GS_CC_R : 0) | ((m & 0xFF00) != 0xFF00 ? GS_CC_G : 0) |
            ((m & 0xFF0000) != 0xFF0000 ? GS_CC_B : 0) | ((m & 0xFF000000u) != 0xFF000000u ? GS_CC_A : 0);
    key = bkey | (uint32_t)ztst << 10 | (uint32_t)zwrite << 12 | (uint32_t)topo << 13 | wmask << 16 | (uint32_t)vu << 20;
    if (vu == 4) { /* asked for by depth_clut: the table pass, which only depends on blending and the write mask */
        key = bkey | wmask << 16 | 1u << 22;
    }
    return sBackend->pipeGet(key);
}

#ifdef BT3_GS_ESPERAS
/* (F5, etapa 8, gravação, seção 15 da nota) sobe quando uma vaga da tabela de alvos muda de dono (alvo novo, vaga tomada,
   escala nova): a memória do estado dos lotes da VU1 (draw_state_vu) só vale com a tabela que ela viu */
static uint32_t sAlvosMuda;
#endif
/* The shared target table's entry for a frame-buffer address. The attachments are the back end's: a new
   slot asks it to make them (GsBackend.targetEnsure), an evicted one to release them (targetDrop). */
int GsDraw_TargetGet(uint32_t fbp, int create) {
    int i, slot;

    for (i = 0; i < gsTargetCount; i++) {
        if (gsTargets[i].fbp == fbp) {
            gsTargets[i].last = gGsFrame;
            return i;
        }
    }
    if (!create) {
        return -1;
    }
    slot = gsTargetCount;
    if (gsTargetCount == MAX_TARGETS) {
        /* Every buffer address the game has ever drawn to holds three large textures. The menus and each stage
           use their own set, so one that has not been touched for two seconds gives its slot to the new one. */
        slot = -1;
        for (i = 0; i < gsTargetCount; i++) {
            if (gsTargets[i].last + 120 < gGsFrame && (slot < 0 || gsTargets[i].last < gsTargets[slot].last)) {
                slot = i;
            }
        }
        if (slot < 0) {
            return -1;
        }
#ifdef BT3_GS_ESPERAS
        solta_pede(0, slot);
#else
        sBackend->targetDrop(slot);
#endif
    }
    sBackend->targetEnsure(slot);
#ifdef BT3_GS_ESPERAS
    sAlvosMuda++;
#endif
    gsTargets[slot].fbp = fbp;
    gsTargets[slot].cleared = 0;
    gsTargets[slot].draws = 0;
    gsTargets[slot].stale = 0;
    gsTargets[slot].gen = 0;
    gsTargets[slot].last = gGsFrame;
    if (slot == gsTargetCount) {
        gsTargetCount++;
    }
    return slot;
}

static int pipeline_get(int ctx, int topo, int vu);

/* GfxPost_DrawDepthClut as one full-screen pass (the game sends 16 strips; they become one draw). */
static void depth_clut(int ctx) {
    uint64_t sc = gGs.scissor[ctx];
    uint32_t wm = (uint32_t)(gGs.frame[ctx] >> 32);
    GsDraw d, *last = gsDrawCount ? &gsDraws[gsDrawCount - 1] : NULL;

    if (gsDrawCount == CAP_DRAWS) {
        return;
    }
    memset(&d, 0, sizeof(d));
    d.native = 3;
    d.src = gsDepthByteIsFog ? 0 : 1;
    d.target = GsDraw_TargetGet((uint32_t)(gGs.frame[ctx] & 0x1FF), 0);
    /* switched off by the user: the passes that colour the picture (the ones that only write alpha feed the glow) */
    if (d.target < 0 || ((gsFxOff & 4) && d.src == 0 && wm != 0x00FFFFFFu)) {
        return;
    }
    {
        uint32_t cbp = (uint32_t)((gGs.tex0[ctx] >> 37) & 0x3FFF);
        if (cbp == 0x3E8C) {
            return; /* the outline's own passes: the outline is drawn natively at its marker (outline.frag) */
        }
        if (cbp == 0x3E94 && (gsFxOff & 2)) {
            return; /* the see-through tint, switched off */
        }
    }
    d.tex = clut_texture((uint32_t)((gGs.tex0[ctx] >> 37) & 0x3FFF));
    if (d.tex == 0) {
        return;
    }
    d.pipeline = pipeline_get(ctx, 0, 4);
    d.blendc = (float)((gGs.alpha[ctx] >> 32) & 0xFF) / 128.0f;
    d.misc[0] = ((gGs.zbuf[ctx] >> 24) & 15) == 0 ? 4294967295.0f : ((gGs.zbuf[ctx] >> 24) & 15) == 1 ? 16777215.0f : 65535.0f;
    d.scissor[0] = (int)(sc & 0x7FF) * SCALE;
    d.scissor[1] = (int)((sc >> 32) & 0x7FF) * SCALE;
    d.scissor[2] = ((int)((sc >> 16) & 0x7FF) + 1) * SCALE - d.scissor[0];
    d.scissor[3] = ((int)((sc >> 48) & 0x7FF) + 1) * SCALE - d.scissor[1];
    if (last != NULL && last->native == 3 && last->tex == d.tex && last->target == d.target && last->pipeline == d.pipeline && last->src == d.src) {
        return; /* the next strip of the same pass */
    }
    gsDraws[gsDrawCount++] = d;
}

static int draw_state(int ctx, int topo, int sprite, int vu, GsDraw *d, float *us, float *vs) {
    uint64_t prim = gGs.prim, t0 = gGs.tex0[ctx], test = gGs.test[ctx], cl = gGs.clamp[ctx], sc = gGs.scissor[ctx];
    int tme = (prim >> 4) & 1, src;
    float tw = (float)(1u << ((t0 >> 26) & 15)), th = (float)(1u << ((t0 >> 30) & 15));

    *us = *vs = 1.0f;
    /* Passes that only make sense on the PS2's memory layout are not drawn here: they belong to full-screen
       effects that get native versions (see docs/port/README.md). Dropped:
         - drawing through a 16-bit view of the frame buffer (the "channel shuffle" between halves of a pixel),
         - drawing INTO the depth buffer's memory as if it were a picture,
         - sampling the depth buffer's memory, or the top byte of a buffer as an 8-bit index (PSMT8H / T4HL / T4HH). */
    {
        uint32_t fpsm = (uint32_t)((gGs.frame[ctx] >> 24) & 0x3F), fbp = (uint32_t)(gGs.frame[ctx] & 0x1FF), zbp = (uint32_t)(gGs.zbuf[ctx] & 0x1FF);
        uint32_t tpsm = (uint32_t)((t0 >> 20) & 0x3F), tbp = (uint32_t)(t0 & 0x3FFF);
        if (Gs_PsmBits(fpsm) != 16 && fbp == zbp && tme) {
            /* The two passes that fill the depth page's spare byte: from the frame's top byte through the fog
               ramp (GfxDepthFog_Draw), or a plain copy of the frame's alpha (GfxPost_CopyAlphaToDepth). */
            gsDepthByteIsFog = tpsm == 0x1B;
            if (tpsm != 0x1B) { /* the copy of the ids: taken now, read by the passes that follow */
                GsDraw c, *last = gsDrawCount ? &gsDraws[gsDrawCount - 1] : NULL;
                memset(&c, 0, sizeof(c));
                c.native = 4;
                c.target = GsDraw_TargetGet(tbp / 32, 0);
                if (c.target >= 0 && gsDrawCount < CAP_DRAWS && !(last != NULL && last->native == 4)) {
                    gsDraws[gsDrawCount++] = c;
                }
            }
        }
        if (Gs_PsmBits(fpsm) == 16 || fbp == zbp) {
            if (fbp != zbp && fbp != (uint32_t)GS_FBP_QUADRO) {
                /* A work buffer drawn through a 16-bit view (the outline's edge image): not drawable here, but
                   the game has now written the buffer, so whatever samples it next means this buffer and not the
                   stage textures that share its address. Without this the outline's later passes read those
                   textures as its mask whenever nothing else had drawn into the buffer since the last upload
                   (split screen: blocks of noise over the picture). */
                int t = GsDraw_TargetGet(fbp, 1);
                if (t >= 0) {
                    gsTargets[t].stale = 1;
                    gsTargets[t].cleared = 1;
                    gsTargets[t].gen = gGsPageGen[fbp & 511];
                }
            }
            gsSkipped++;
            return 0;
        }
        if (tme && sprite && tpsm == 0x1B && tbp / 32 == zbp) {
            depth_clut(ctx); /* that byte through a table over the screen: a native pass */
            gsSkipped++;
            return 0;
        }
        if (tme && ((tpsm & 0x30) == 0x30 || tbp / 32 == zbp || tpsm == 0x1B || tpsm == 0x24 || tpsm == 0x2C)) {
            int t = GsDraw_TargetGet(fbp, 0);
            if (t >= 0 && fbp != (uint32_t)GS_FBP_QUADRO) {
                gsTargets[t].stale = 1; /* a work buffer missed a pass */
            }
            gsSkipped++;
            return 0;
        }
    }
    memset(d, 0, sizeof(*d));
    d->target = GsDraw_TargetGet((uint32_t)(gGs.frame[ctx] & 0x1FF), 1);
    if (d->target < 0) {
        return 0;
    }
    d->tex = gsWhite;
    if (!tme && sprite) {
        gsTargets[d->target].stale = 0; /* cleared by the game: what is drawn into it from here on is real */
    }
    if (tme) {
        src = GsDraw_TargetGet((uint32_t)((t0 & 0x3FFF) / 32), 0);
        /* a frame buffer used as a texture: only if nothing was uploaded over it since it was drawn */
        if (src >= 0 && (t0 & 0x1F) == 0 && gsTargets[src].cleared && gsTargets[src].gen == gGsPageGen[gsTargets[src].fbp & 511]) {
            uint32_t tp = (uint32_t)((t0 >> 20) & 0x3F);
            if (gsTargets[src].stale) {
                /* The buffer was meant to hold a copy or a step of an effect that was dropped; what it holds now
                   is whatever was drawn there before (seen as blocks of noise over the picture). */
                gsSkipped++;
                return 0;
            }
            /* One buffer drawn into another is how the game builds its screen effects: the glare and the object
               glow (the picture shrunk by its alpha into the 256 x 256 work buffer, down to 64 x 64, blurred
               between two buffers, added back), the blur by depth, the pan blur, the haze. They are ordinary
               textured sprites and strips and are drawn as such. Not drawable: a buffer sampled while it is the
               target, and a buffer read in another pixel format than it was drawn in (the 16-bit views). */
            /* The user's switches act on the pass that puts a work buffer back over the picture: added (the
               glare and the object glow, blend "source + destination") or mixed in by the destination's alpha
               (the blur of distant things). */
            if (sprite && (uint32_t)(gGs.frame[ctx] & 0x1FF) == (uint32_t)GS_FBP_QUADRO && ((prim >> 6) & 1) &&
                ((((gGs.alpha[ctx] & 0xFF) == 0x68) && (gsFxOff & 8)) || (((gGs.alpha[ctx] & 0xFF) == 0x54) && (gsFxOff & 16)))) {
                gsSkipped++;
                return 0;
            }
            if (src == d->target || tp > 1) {
                if (d->target != src && (uint32_t)(gGs.frame[ctx] & 0x1FF) != (uint32_t)GS_FBP_QUADRO) {
                    gsTargets[d->target].stale = 1; /* a work buffer missed a pass */
                }
                gsSkipped++;
                return 0;
            }
            d->tex = sBackend->targetColor(src);
            d->tex_is_target = 1;
            {
                /* The buffer is a corner of a larger texture here, so the GS's own edge handling has to be done
                   by hand: CLAMP gives the texel range per axis (0 repeat and 1 clamp: the texture's size; 2 and
                   3: the region MINU..MAXU). Sampling is held inside it, or the blur steps pull in whatever lies
                   next to the buffer (seen as a box with an edge in the glare). */
                int wms = (int)(cl & 3), wmt = (int)((cl >> 2) & 3);
                float u0 = 0.0f, u1 = tw - 1.0f, v0 = 0.0f, v1 = th - 1.0f;
                if (wms >= 2) { u0 = (float)((cl >> 4) & 0x3FF); u1 = (float)((cl >> 14) & 0x3FF); }
                if (wmt >= 2) { v0 = (float)((cl >> 24) & 0x3FF); v1 = (float)((cl >> 34) & 0x3FF); }
                d->rect[0] = (u0 + 0.5f) / (float)GS_W;
                d->rect[1] = (v0 + 0.5f) / (float)GS_H;
                d->rect[2] = (u1 + 0.5f) / (float)GS_W;
                d->rect[3] = (v1 + 0.5f) / (float)GS_H;
            }
            *us = tw / (float)GS_W;
            *vs = th / (float)GS_H;
        } else {
            d->tex = texture_get(ctx);
        }
    }
    d->sampler = (int)((gGs.tex1[ctx] >> 5) & 1) | ((cl & 3) ? 2 : 0) | (((cl >> 2) & 3) ? 4 : 0);
    /* A replacement is several times the original's size: with the nearest-texel sampling the game asks for on
       its text and 2D art (right for a texture drawn 1:1) it is shown smaller than it is, and its edges come out
       jagged. Replacements are always sampled with filtering. */
    if (d->tex != 0 && sLast != NULL && sLast->tex == d->tex && sLast->replaced) {
        d->sampler |= 1 | 8; /* (GsGpu_Draw takes the 8 off again for 2D art) */
    }
    d->pipeline = pipeline_get(ctx, topo, vu);
    d->mode[0] = !tme ? 0 : d->tex_is_target ? 2 : 1; /* 2: a frame buffer as texture, its alpha is already rescaled */
    /* (BT3_TEX_ALPHA=0 switches the replacement's alpha treatment off, for telling which one a fault comes from) */
    if (d->mode[0] == 1 && sLast != NULL && sLast->tex == d->tex && sLast->replaced &&
        !(getenv("BT3_TEX_ALPHA") != NULL && atoi(getenv("BT3_TEX_ALPHA")) == 0)) {
        d->mode[0] = 3; /* a texture pack's replacement: the alpha test allows for its filtered, compressed alpha (gs.frag) */
        d->orig[2] = (float)sLast->amax / 128.0f; /* its alpha is kept at or below the original's largest (1.0 = 0x80) */
    }
    d->mode[1] = (int32_t)((t0 >> 35) & 3);
    d->mode[2] = (int32_t)((t0 >> 34) & 1);
    d->mode[3] = (test & 1) && ((test >> 12) & 3) == 0 ? (int32_t)((test >> 1) & 7) + 1 : 0;
    d->misc[0] = (float)((test >> 4) & 0xFF);
    d->misc[1] = ((test >> 14) & 1) ? (float)(1 + (int)((test >> 15) & 1)) : 0.0f; /* DATE, DATM */
    d->misc[2] = (float)(gGs.fba[ctx] & 1);
    d->blendc = (float)((gGs.alpha[ctx] >> 32) & 0xFF) / 128.0f;
    d->scissor[0] = (int)(sc & 0x7FF) * SCALE;
    d->scissor[1] = (int)((sc >> 32) & 0x7FF) * SCALE;
    d->scissor[2] = ((int)((sc >> 16) & 0x7FF) + 1) * SCALE - d->scissor[0];
    d->scissor[3] = ((int)((sc >> 48) & 0x7FF) + 1) * SCALE - d->scissor[1];
    return 1;
}

static void put(const GsVertex *v, float x, float y, float s, float t, float q, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float zmax) {
    Vtx *o = &gsVerts[gsVertCount++];
    o->x = x;
    o->y = y;
    o->z = (float)((double)v->z / zmax);
    o->r = r; o->g = g; o->b = b; o->a = a;
    o->s = s; o->t = t; o->q = q;
}

/* Two draws can be merged into one call when everything they need is the same. `scissor` is compared as
   four ints (the scissor a draw has recorded). */
static int same_state(const GsDraw *a, const GsDraw *b) {
    return !a->native && a->vu == b->vu && a->target == b->target && a->tex == b->tex && a->sampler == b->sampler && a->pipeline == b->pipeline &&
           memcmp(a->mode, b->mode, sizeof(a->mode)) == 0 && a->misc[0] == b->misc[0] && a->misc[1] == b->misc[1] &&
           a->misc[2] == b->misc[2] && a->misc[3] == b->misc[3] && a->blendc == b->blendc &&
           memcmp(a->rect, b->rect, sizeof(a->rect)) == 0 && memcmp(a->orig, b->orig, sizeof(a->orig)) == 0 &&
           memcmp(a->scissor, b->scissor, sizeof(a->scissor)) == 0;
}

/* A draw that tests the alpha already in the frame buffer (TEST.DATE: the fill of a HUD bar cut to length by a
   mask, a word shown through a band, and also some of the stage's own layers): at the start of a run of such
   draws, a copy of the alpha bytes is taken for them to read. Called by every way a primitive can be recorded;
   left out of the vertex-program paths at first, the stage's layers tested against the copy the HUD had made the
   frame before, and showed a strip of different-looking scenery where an announcement's band had been. */
static void date_snapshot(const GsDraw *d) {
    GsDraw *prev = gsDrawCount ? &gsDraws[gsDrawCount - 1] : NULL;
    GsDraw c;

    if (d->misc[1] == 0.0f || (prev != NULL && !prev->native && prev->misc[1] != 0.0f && prev->target == d->target)) {
        return;
    }
    memset(&c, 0, sizeof(c));
    c.native = 5;
    c.target = d->target;
    if (gsDrawCount + 1 < CAP_DRAWS) {
        gsDraws[gsDrawCount++] = c;
    }
}

#ifdef BT3_GS_ESPERAS
/* (F5, etapa 8, gravação, seção 15 da nota) A memória do estado dos lotes da VU1. Os uns 4.500 lotes de um quadro de luta
   pedem o draw_state em seguida com a mesma entrada, e no 360 refazê-lo custava uns 4 ms por quadro fora do hash (a
   tabela de alvos andada duas vezes, o pipeGet com a sua busca linear, o memset e as conversões de inteiro para float).
   O resultado de um draw_state que deu certo fica aqui com a entrada que ele leu: os registradores do contexto, o PRIM
   e o TEXA, a tabela de alvos (sAlvosMuda), o quadro, a textura branca e a chave dos pacotes de textura. Com a mesma
   entrada o resultado é o mesmo, fora do que depende da memória do GS:
     - o alvo como textura (tex_is_target) nunca entra na memória; quando o alvo em tbp / 32 existe, a condição que
       escolheria esse caminho (cleared e gen da vaga contra a gGsPageGen) é refeita a cada acerto;
     - a textura: se a época das páginas é a mesma e a entrada do cache que o texture_get achou segue no lugar, com o
       mesmo TEX0, TEXA e gen, é ela que ele acharia de novo; senão o texture_get é chamado (o hash, quando as páginas
       mudaram, segue sendo feito) e, sendo uma entrada sem substituição de pacote ou a branca da falta de espaço, o
       resultado é o da memória com a textura nova (o draw_state só muda o sampler, o mode e o orig para a substituição).
   Os efeitos do draw_state que deu certo (o last do alvo, da vaga lida e da textura igual ao quadro, e o sLast) já valem
   num acerto, pois o quadro está na chave; o sLast é posto de novo. Um draw_state que recusou o desenho (gsSkipped, as
   vagas marcadas, o depth_clut) nunca entra na memória. */
typedef struct EstMem {
    uint64_t prim, t0, test, cl, sc, frame, zbuf, alpha, fba, tex1, texa;
    uint32_t quadro, alvos, epoch, gen;
    int valido, vu, pack, src;
    GsTex white;
    Tex *tex; /* a entrada do cache (com textura) ou NULL */
    float us, vs;
    GsDraw d;
} EstMem;
static EstMem sEst[2]; /* uma por contexto: as duas camadas dos programas 0 e 1 se alternam entre os dois */
#if defined(BT3_GPU_FALSO)
/* (só nas builds --gpu) BT3_VU_ESTADO_CONFERE=1: em todo acerto o draw_state completo também roda, e é o resultado dele que
   vale; as diferenças são contadas (uma linha a cada 1.000 quadros com o acumulado e as contagens dos lotes e do fecho, e
   uma por diferença, até 20) */
static int sEstConfere = -1;
static struct {
    uint64_t lotes, acertos, igualAnterior, mesmaTex, texChamada, texNova, diferentes, naoGuarda;
    uint64_t fecho, mesmoEstado, junta, novos, bytesIguais, bytesDiferentes, desenhoAnterior;
    uint64_t falhaChave, falhaSrc, falhaEntrada;
    uint64_t campo[20], vuLotes[4], vuAcertos[4], vuJunta[4], juntaAcerto;
} sEstN;
static int sEstAcertou;
static uint64_t sEstAntT0, sEstAntTexa;
static int sEstAntCtx = -1;
#endif

/* a vaga do alvo em fbp, sem efeito (o GsDraw_TargetGet(fbp, 0) põe o last, que num acerto já vale) */
static int alvo_acha(uint32_t fbp) {
    int i;
    for (i = 0; i < gsTargetCount; i++) {
        if (gsTargets[i].fbp == fbp) {
            return i;
        }
    }
    return -1;
}

#define EST_ENTRADA(e, ctx_, vu_) ((e)->valido && (e)->vu == (vu_) && (e)->quadro == gGsFrame && \
    (e)->alvos == sAlvosMuda && (e)->prim == gGs.prim && (e)->t0 == gGs.tex0[ctx_] && (e)->test == gGs.test[ctx_] && \
    (e)->cl == gGs.clamp[ctx_] && (e)->sc == gGs.scissor[ctx_] && (e)->frame == gGs.frame[ctx_] && \
    (e)->zbuf == gGs.zbuf[ctx_] && (e)->alpha == gGs.alpha[ctx_] && (e)->fba == gGs.fba[ctx_] && \
    (e)->tex1 == gGs.tex1[ctx_] && (e)->texa == gGs.texa && (e)->white == gsWhite && (e)->pack == sTexPackOn)

/* 1: o resultado da memória vale para este lote (o d já preenchido); 0: o draw_state completo tem de rodar */
static int est_acerta(int ctx, int vu, GsDraw *d) {
    EstMem *e = &sEst[ctx & 1];
    Tex *t;
    GsTex x;

    if (!EST_ENTRADA(e, ctx, vu)) {
#if defined(BT3_GPU_FALSO)
        sEstN.falhaChave++;
        if (sEstConfere) {
            int c = !e->valido ? 0 : e->vu != vu ? 2 : e->quadro != gGsFrame ? 3 : e->alvos != sAlvosMuda ? 4 :
                    e->prim != gGs.prim ? 5 : e->t0 != gGs.tex0[ctx] ? 6 : e->test != gGs.test[ctx] ? 7 : e->cl != gGs.clamp[ctx] ? 8 :
                    e->sc != gGs.scissor[ctx] ? 9 : e->frame != gGs.frame[ctx] ? 10 : e->zbuf != gGs.zbuf[ctx] ? 11 :
                    e->alpha != gGs.alpha[ctx] ? 12 : e->fba != gGs.fba[ctx] ? 13 : e->tex1 != gGs.tex1[ctx] ? 14 :
                    e->texa != gGs.texa ? 15 : 16;
            sEstN.campo[c]++;
            if (c == 5 && ((e->prim ^ gGs.prim) & 0x50) == 0) {
                sEstN.campo[17]++; /* (só bits do PRIM que o draw_state não lê) */
            }
        }
#endif
        return 0;
    }
    if (e->tex == NULL) {
        memcpy(d, &e->d, sizeof(*d));
        return 1;
    }
    if (e->src >= 0 && (e->t0 & 0x1F) == 0 && gsTargets[e->src].cleared &&
        gsTargets[e->src].gen == gGsPageGen[gsTargets[e->src].fbp & 511]) {
#if defined(BT3_GPU_FALSO)
        sEstN.falhaSrc++;
#endif
        return 0; /* o alvo em tbp / 32 viraria a textura */
    }
    t = e->tex;
    if (e->epoch == gGsPageEpoch && t < &sTex[gsTexCount] && t->tex == e->d.tex &&
        t->tex0 == (e->t0 & 0x1FFFFFFFFFFFFFFFull) && t->texa == e->texa && t->gen == e->gen) {
        sLast = t;
        memcpy(d, &e->d, sizeof(*d));
        return 1;
    }
#if defined(BT3_GPU_FALSO)
    sEstN.texChamada++;
#endif
    x = texture_get(ctx);
    if (x == gsWhite) { /* (sem espaço: o draw_state daria a branca; a próxima vez tenta de novo, pelo caminho completo) */
        memcpy(d, &e->d, sizeof(*d));
        d->tex = gsWhite;
        e->valido = 0;
        return 1;
    }
    if (sLast == NULL || sLast->tex != x || sLast->replaced) {
        /* (uma substituição de pacote: o draw_state completo, cujo texture_get acha a mesma entrada pelo sLast) */
#if defined(BT3_GPU_FALSO)
        sEstN.falhaEntrada++;
#endif
        return 0;
    }
#if defined(BT3_GPU_FALSO)
    if (x != e->d.tex) {
        sEstN.texNova++;
    }
#endif
    e->tex = sLast;
    e->gen = sLast->gen;
    e->epoch = gGsPageEpoch;
    e->d.tex = x;
    memcpy(d, &e->d, sizeof(*d));
    return 1;
}

/* guarda o resultado de um draw_state que deu certo, se ele pode ser lembrado */
static void est_guarda(int ctx, int vu, const GsDraw *d, float us, float vs) {
    EstMem *e = &sEst[ctx & 1];
    uint64_t t0 = gGs.tex0[ctx];
    int tme = (int)((gGs.prim >> 4) & 1);

    e->valido = 0;
    if (d->tex_is_target || (tme && (sLast == NULL || sLast->tex != d->tex || sLast->replaced || d->tex == gsWhite))) {
        return;
    }
    e->prim = gGs.prim;
    e->t0 = t0;
    e->test = gGs.test[ctx];
    e->cl = gGs.clamp[ctx];
    e->sc = gGs.scissor[ctx];
    e->frame = gGs.frame[ctx];
    e->zbuf = gGs.zbuf[ctx];
    e->alpha = gGs.alpha[ctx];
    e->fba = gGs.fba[ctx];
    e->tex1 = gGs.tex1[ctx];
    e->texa = gGs.texa;
    e->quadro = gGsFrame;
    e->alvos = sAlvosMuda;
    e->epoch = gGsPageEpoch;
    e->white = gsWhite;
    e->pack = sTexPackOn;
    e->vu = vu;
    e->tex = tme ? sLast : NULL;
    e->gen = tme ? sLast->gen : 0;
    e->src = tme ? alvo_acha((uint32_t)((t0 & 0x3FFF) / 32)) : -1;
    e->us = us;
    e->vs = vs;
    memcpy(&e->d, d, sizeof(*d));
    e->valido = 1;
}

/* o draw_state dos três GsGpu_DrawVu*, pela memória */
static int draw_state_vu(int ctx, int vu, GsDraw *d, float *us, float *vs) {
    EstMem *e = &sEst[ctx & 1];
    int r;
#if defined(BT3_GPU_FALSO)
    if (sEstConfere < 0) {
        sEstConfere = getenv("BT3_VU_ESTADO_CONFERE") != NULL && atoi(getenv("BT3_VU_ESTADO_CONFERE")) != 0;
    }
    if (sEstConfere) {
        sEstN.lotes++;
        if (sEstAntCtx == ctx && sEstAntT0 == gGs.tex0[ctx] && sEstAntTexa == gGs.texa) {
            sEstN.mesmaTex++;
        }
        sEstAntCtx = ctx;
        sEstAntT0 = gGs.tex0[ctx];
        sEstAntTexa = gGs.texa;
        if (EST_ENTRADA(e, ctx, vu)) {
            sEstN.igualAnterior++;
        }
    }
#endif
#if defined(BT3_GPU_FALSO)
    if (sEstConfere) {
        sEstN.vuLotes[vu & 3]++;
    }
    sEstAcertou = 0;
#endif
    if (est_acerta(ctx, vu, d)) {
#if defined(BT3_GPU_FALSO)
        sEstAcertou = 1;
        if (sEstConfere) {
            sEstN.vuAcertos[vu & 3]++;
        }
#endif
        *us = e->us;
        *vs = e->vs;
#if defined(BT3_GPU_FALSO)
        if (sEstConfere) {
            GsDraw c;
            float cu, cv;
            Tex *lembrado = sLast;
            sEstN.acertos++;
            r = draw_state(ctx, 0, 0, vu, &c, &cu, &cv);
            if (r != 1 || memcmp(&c, d, sizeof(c)) != 0 || cu != *us || cv != *vs || (e->tex != NULL && sLast != lembrado)) {
                if (sEstN.diferentes++ < 20) {
                    fprintf(stderr, "gs: BT3_VU_ESTADO_CONFERE: quadro %u, vu %d: completo %d tex %lx alvo %d pipeline %d sampler %d; "
                            "memória tex %lx alvo %d pipeline %d sampler %d\n", gGsFrame, vu, r, (unsigned long)c.tex, c.target,
                            c.pipeline, c.sampler, (unsigned long)d->tex, d->target, d->pipeline, d->sampler);
                }
            }
            if (r) {
                memcpy(d, &c, sizeof(c));
                *us = cu;
                *vs = cv;
            }
            if (r) {
                est_guarda(ctx, vu, d, *us, *vs);
            } else {
                e->valido = 0;
            }
            return r;
        }
#endif
        return 1;
    }
    r = draw_state(ctx, 0, 0, vu, d, us, vs);
    if (r) {
        est_guarda(ctx, vu, d, *us, *vs);
#if defined(BT3_GPU_FALSO)
        if (sEstConfere && !e->valido) {
            sEstN.naoGuarda++;
        }
#endif
    } else {
        e->valido = 0;
    }
    return r;
}
#define DRAW_STATE_VU(ctx, vu, d, us, vs) draw_state_vu(ctx, vu, d, us, vs)
#if defined(BT3_GPU_FALSO)
/* (BT3_VU_ESTADO_CONFERE) o que o fecho de cada desenho da VU1 faz: o mesmo estado que o desenho anterior (same_state), o
   bloco uniforme igual (junta) e quantos bytes o memcmp dele lê; e se o anterior é o desenho do lote anterior */
static uint32_t sFechoUlt = 0xFFFFFFFFu, sFechoQuadro;
static void est_conta_fecho(const GsDraw *last, const GsDraw *d, const Vu0Uniform *u) {
    int junta = 0;
    if (sEstConfere <= 0) {
        return;
    }
    sEstN.fecho++;
    if (last != NULL && sFechoQuadro == gGsFrame && sFechoUlt == gsDrawCount - 1) {
        sEstN.desenhoAnterior++;
    }
    if (last != NULL && same_state(last, d)) {
        const unsigned char *a = (const unsigned char *)&gsVuUni[last->uniform], *b = (const unsigned char *)u;
        size_t k;
        sEstN.mesmoEstado++;
        for (k = 0; k < sizeof(*u) && a[k] == b[k]; k++) {
        }
        if (k == sizeof(*u)) {
            junta = 1;
            sEstN.junta++;
            sEstN.vuJunta[d->vu & 3]++;
            sEstN.juntaAcerto += sEstAcertou;
            sEstN.bytesIguais += k;
        } else {
            sEstN.bytesDiferentes += k + 1;
        }
    }
    if (!junta) {
        sEstN.novos++;
    }
    sFechoQuadro = gGsFrame;
    sFechoUlt = junta ? gsDrawCount - 1 : gsDrawCount;
}
static void est_confere_linha(void) {
    static unsigned quadros;
    if (sEstConfere <= 0 || ++quadros % 1000 != 0) {
        return;
    }
    fprintf(stderr, "gs: BT3_VU_ESTADO_CONFERE: %u quadros, %llu lotes, %llu com a entrada do anterior, %llu acertos, %llu diferentes; "
            "mesma textura que o anterior %llu; texture_get no acerto %llu (textura nova %llu); falhas: chave %llu, alvo como textura "
            "%llu, substituição %llu; sem memória %llu; fecho %llu: o desenho do lote anterior %llu, mesmo estado %llu, junta %llu "
            "(%llu bytes), novos %llu (%llu bytes até a diferença)\n",
            quadros, (unsigned long long)sEstN.lotes, (unsigned long long)sEstN.igualAnterior, (unsigned long long)sEstN.acertos,
            (unsigned long long)sEstN.diferentes, (unsigned long long)sEstN.mesmaTex, (unsigned long long)sEstN.texChamada,
            (unsigned long long)sEstN.texNova, (unsigned long long)sEstN.falhaChave, (unsigned long long)sEstN.falhaSrc,
            (unsigned long long)sEstN.falhaEntrada, (unsigned long long)sEstN.naoGuarda, (unsigned long long)sEstN.fecho,
            (unsigned long long)sEstN.desenhoAnterior, (unsigned long long)sEstN.mesmoEstado, (unsigned long long)sEstN.junta,
            (unsigned long long)sEstN.bytesIguais, (unsigned long long)sEstN.novos, (unsigned long long)sEstN.bytesDiferentes);
    {
        int c;
        fprintf(stderr, "gs: BT3_VU_ESTADO_CONFERE: por vu (lotes acertos junta):");
        for (c = 1; c < 4; c++) {
            fprintf(stderr, " %d: %llu %llu %llu;", c, (unsigned long long)sEstN.vuLotes[c], (unsigned long long)sEstN.vuAcertos[c],
                    (unsigned long long)sEstN.vuJunta[c]);
        }
        fprintf(stderr, " junta com acerto %llu; a chave falhou por (vazia ctx vu quadro alvos prim tex0 test clamp scissor frame zbuf alpha fba tex1 texa outro prim-só-sem-uso):",
                (unsigned long long)sEstN.juntaAcerto);
        for (c = 0; c < 18; c++) {
            fprintf(stderr, " %llu", (unsigned long long)sEstN.campo[c]);
        }
        fprintf(stderr, "\n");
    }
}
#define EST_CONTA_FECHO(last, d, u) est_conta_fecho(last, d, u)
#else
#define EST_CONTA_FECHO(last, d, u) ((void)0)
#endif
#else
#define DRAW_STATE_VU(ctx, vu, d, us, vs) draw_state(ctx, 0, 0, vu, d, us, vs)
#endif

/* Gs_StateBack (gs_core.c) moved every page's upload generation on by `delta`, so that nothing decoded from the
   pages in between is taken for them. The buffers the game draws into keep their standing: one that was drawn
   since the last upload over it still is. */
void GsGpu_PagesMoved(uint32_t delta) {
    int i;
    for (i = 0; i < gsTargetCount; i++) {
        if (gsTargets[i].gen != 0) { /* (0: its page was never uploaded to, and stays 0) */
            gsTargets[i].gen += delta;
        }
    }
}

void GsGpu_Draw(int type, int ctx, const GsVertex *v) {
    if (gPortResim) { /* a frame that is only being re-run: nothing is drawn */
        return;
    }
    uint64_t prim = gGs.prim, t0 = gGs.tex0[ctx];
    int fst = (prim >> 8) & 1, gouraud = (prim >> 3) & 1, n = type == 6 ? 2 : type == 3 ? 3 : type == 1 ? 2 : 1;
    float tw = (float)(1u << ((t0 >> 26) & 15)), th = (float)(1u << ((t0 >> 30) & 15)), us, vs;
    float zmax = ((gGs.zbuf[ctx] >> 24) & 15) == 0 ? 4294967295.0f : ((gGs.zbuf[ctx] >> 24) & 15) == 1 ? 16777215.0f : 65535.0f;
    float s[3], t[3], q[3];
    const GsVertex *flat = &v[n - 1];
    GsDraw d, *last;
    int i, packed2d = 0;

    if (gsVertCount + 6 > CAP_VERTS || gsDrawCount == CAP_DRAWS) {
        return;
    }
    if (!draw_state(ctx, type == 1 ? 1 : type == 0 ? 2 : 0, type == 6, 0, &d, &us, &vs)) {
        return;
    }
    if (type == 6 || fst) {
        d.sampler &= 7; /* 2D art: never the smaller copies of a replacement */
    }
    if ((type == 6 || fst) && !d.tex_is_target) {
        /* 2D art: sprites, and triangles with whole-texel coordinates (the logo, HUD pieces drawn as quads):
           texture coordinates per GS pixel (gs.frag). Not for a texture pack's replacement: that has several
           texels per GS pixel, and sampling it once per GS pixel would show it at the original's resolution. */
        if (sLast != NULL && sLast->tex == d.tex && sLast->replaced && !(getenv("BT3_TEX_2D") != NULL && atoi(getenv("BT3_TEX_2D")) == 0)) {
            packed2d = 1; /* its rectangle is worked out below, from the vertices */
        } else {
            d.misc[3] = (float)SCALE;
        }
    }
    date_snapshot(&d);
    for (i = 0; i < n; i++) {
        if (fst) {
            s[i] = (float)v[i].u / 16.0f / tw * us;
            t[i] = (float)v[i].v / 16.0f / th * vs;
            q[i] = 1.0f;
        } else {
            s[i] = v[i].s * us;
            t[i] = v[i].t * vs;
            q[i] = v[i].q != 0.0f ? v[i].q : 1.0f;
        }
    }
    if (packed2d) {
        /* A texture pack's replacement as 2D art: sampled per output pixel (it has several texels per original
           texel), but only inside the rectangle of the sheet this piece shows on the console: the original texels
           from the one its first GS pixel takes to the one its last GS pixel takes. Without the limit the
           filtering reaches the neighbouring picture of the sheet (lines along HUD panels). */
        int lo[2] = {0, 0}, hi[2] = {0, 0}, k, axis;
        for (axis = 0; axis < 2; axis++) {
            float size = axis ? th : tw, pmin = 0, pmax = 0, cmin = 0, cmax = 0, cAtMin = 0, cAtMax = 0, step, first, lastc;
            for (k = 0; k < n; k++) {
                /* (a sprite's coordinates are both divided by the SECOND vertex's Q, as below) */
                float pos = axis ? v[k].y : v[k].x, c = (axis ? t[k] : s[k]) / (type == 6 ? q[1] : q[k]) * size;
                if (k == 0 || pos < pmin) { pmin = pos; cAtMin = c; }
                if (k == 0 || pos > pmax) { pmax = pos; cAtMax = c; }
                if (k == 0 || c < cmin) { cmin = c; }
                if (k == 0 || c > cmax) { cmax = c; }
            }
            step = pmax > pmin ? (cmax - cmin) / (pmax - pmin) : 0.0f; /* texels per GS pixel */
            first = cAtMax >= cAtMin ? cmin : cmin + step;             /* lowest coordinate a GS pixel takes */
            lastc = cAtMax >= cAtMin ? cmax - step : cmax;             /* highest */
            lo[axis] = (int)floorf(first + 1.0f / 64.0f);
            /* A mirrored piece (coordinates falling along the screen) starts exactly ON the boundary to the next
               picture of the sheet (16.0 down to 8.0): one GS pixel's worth on the console, but the filtered
               replacement blends that neighbour in over a visible stretch when the piece is drawn enlarged
               (a strip of the yellow layer at the end of the second player's health bar). The boundary itself
               does not count as inside. */
            hi[axis] = (int)floorf(lastc + (cAtMax >= cAtMin ? 1.0f / 64.0f : -1.0f / 64.0f)) + 1;
            if (hi[axis] <= lo[axis]) {
                hi[axis] = lo[axis] + 1;
            }
        }
        d.misc[3] = -(float)SCALE;
        d.rect[0] = (float)lo[0] / tw;
        d.rect[1] = (float)lo[1] / th;
        d.rect[2] = (float)hi[0] / tw;
        d.rect[3] = (float)hi[1] / th;
        d.orig[0] = tw;
        d.orig[1] = th;
    }
    d.first = gsVertCount;
    if (type == 6) { /* sprite: two corners, flat colour and depth of the second vertex */
        const GsVertex *a = &v[0], *b = &v[1];
        put(b, a->x, a->y, s[0], t[0], 1.0f, b->r, b->g, b->b, b->a, zmax);
        put(b, b->x, a->y, s[1], t[0], 1.0f, b->r, b->g, b->b, b->a, zmax);
        put(b, a->x, b->y, s[0], t[1], 1.0f, b->r, b->g, b->b, b->a, zmax);
        put(b, b->x, a->y, s[1], t[0], 1.0f, b->r, b->g, b->b, b->a, zmax);
        put(b, b->x, b->y, s[1], t[1], 1.0f, b->r, b->g, b->b, b->a, zmax);
        put(b, a->x, b->y, s[0], t[1], 1.0f, b->r, b->g, b->b, b->a, zmax);
        if (!fst) { /* a sprite's texture coordinates are not divided by Q per pixel: do it here */
            for (i = 0; i < 6; i++) {
                gsVerts[d.first + i].s /= q[1];
                gsVerts[d.first + i].t /= q[1];
            }
        }
    } else {
        for (i = 0; i < n; i++) {
            const GsVertex *c = gouraud ? &v[i] : flat;
            put(&v[i], v[i].x, v[i].y, s[i], t[i], q[i], c->r, c->g, c->b, c->a, zmax);
        }
    }
    if (d.tex_is_target && type == 6 && v[1].x != v[0].x && v[1].y != v[0].y) {
        /* The GS evaluates texture coordinates at whole pixel positions, a GPU at pixel centres, half a pixel
           later. For one buffer drawn into another that half pixel is not cosmetic: the blur steps of the glow
           chain sample one texel apart on purpose (each step should average two texels and move the picture
           back and forth by half a texel); evaluated at pixel centres they do not blur and move it a whole texel
           every round, which showed as a displaced copy of the scenery in the sky. The coordinates are taken
           half a pixel back instead. (Moving the sprite itself left its first row and column undrawn: a line
           of the wrong brightness along the top of the picture.) */
        float ds = 0.5f * (gsVerts[d.first + 1].s - gsVerts[d.first].s) / (v[1].x - v[0].x);
        float dt = 0.5f * (gsVerts[d.first + 2].t - gsVerts[d.first].t) / (v[1].y - v[0].y);
        uint32_t k;
        for (k = d.first; k < gsVertCount; k++) {
            gsVerts[k].s -= ds;
            gsVerts[k].t -= dt;
        }
        /* the user's strength for the glare and the glow: the pass that adds the blurred buffer to the picture */
        if (gsGlowPercent != 100 && (uint32_t)(gGs.frame[ctx] & 0x1FF) == (uint32_t)GS_FBP_QUADRO && ((gGs.prim >> 6) & 1) &&
            (gGs.alpha[ctx] & 0xFF) == 0x68) {
            for (k = d.first; k < gsVertCount; k++) {
                gsVerts[k].r = (uint8_t)(gsVerts[k].r * gsGlowPercent / 100 > 255 ? 255 : gsVerts[k].r * gsGlowPercent / 100);
                gsVerts[k].g = (uint8_t)(gsVerts[k].g * gsGlowPercent / 100 > 255 ? 255 : gsVerts[k].g * gsGlowPercent / 100);
                gsVerts[k].b = (uint8_t)(gsVerts[k].b * gsGlowPercent / 100 > 255 ? 255 : gsVerts[k].b * gsGlowPercent / 100);
            }
        }
    }
    if (Port_IsWide() && !gPortMenuMode && !d.tex_is_target && (type == 6 || fst) && gsTargets[d.target].fbp == (uint32_t)GS_FBP_QUADRO) {
        /* Widescreen. The 3D scene is projected for a 16:9 picture by the game itself; 2D art is laid out for
           4:3 and would come out a third too wide. Each 2D piece is narrowed to 3/4 about a fixed point:
             (not in the menus: their pages are shown whole, stretched to the width; see Port_WideFactor)
             - in a fight, the left edge, the right edge or the middle for the HUD's left panel, right panel and
               centre parts (markers from the HUD code), so the panels sit at the screen's edges;
             - any other 2D piece, the middle of the screen (see below).
           Only what is drawn into the picture itself: the work buffers of the effects and of the shadow are
           filled with 2D rectangles too, and narrowing those left unfilled bands and stripes in them.
           Left alone: full-screen fills and fades (untextured, full height), and in a fight sprites as wide as
           the screen (flashes, speed lines) and 2D triangles outside the HUD. */
        float x0 = gsVerts[d.first].x, x1 = x0, y0 = gsVerts[d.first].y, y1 = y0, pivot = -1.0f;
        uint32_t k;
        for (k = d.first; k < gsVertCount; k++) {
            if (gsVerts[k].x < x0) { x0 = gsVerts[k].x; }
            if (gsVerts[k].x > x1) { x1 = gsVerts[k].x; }
            if (gsVerts[k].y < y0) { y0 = gsVerts[k].y; }
            if (gsVerts[k].y > y1) { y1 = gsVerts[k].y; }
        }
        if (type == 6 && !d.mode[0] && y1 - y0 >= 400.0f) {
            pivot = -1.0f;
        } else if (x1 - x0 >= 480.0f) {
            /* as wide as the screen (the bands and streaks of READY / FIGHT, flashes, speed lines): meant to
               reach from edge to edge, so it keeps the full width */
            pivot = -1.0f;
        } else if (gsAnchor != 0) {
            pivot = gsAnchor == 1 ? 0.0f : gsAnchor == 2 ? 512.0f : 256.0f;
            if (gsAnchor == 4) { /* a part without a fixed side (captions: technique names): the side it is on */
                float cx = (x0 + x1) * 0.5f;
                pivot = x1 <= 300.0f || cx < 180.0f ? 0.0f : x0 >= 212.0f || cx > 332.0f ? 512.0f : 256.0f;
            }
        } else {
            /* 2D outside the HUD (the pause menu, messages): one page about the middle of the screen. (Narrowing
               each piece about its own middle pulled panels apart and spread the letters of the text.) A marker
               the game places over a fighter is pulled a little towards the middle by this: known, not handled.
               Sprites as wide as the screen (flashes, speed lines) are left alone. */
            pivot = 256.0f;
        }
        if (pivot >= 0.0f) {
            float narrow = 1333.333f / (float)Port_AspectMilli(); /* 3/4 at 16:9, 9/16 at 21:9 */
            for (k = d.first; k < gsVertCount; k++) {
                gsVerts[k].x = pivot + (gsVerts[k].x - pivot) * narrow;
            }
        }
    }
    d.count = gsVertCount - d.first;
    gsTargets[d.target].draws++;
    gsTargets[d.target].gen = gGsPageGen[gsTargets[d.target].fbp & 511];
    last = gsDrawCount ? &gsDraws[gsDrawCount - 1] : NULL;
    if (last != NULL && same_state(last, &d)) {
        last->count += d.count; /* same state as the previous primitive: one draw call */
    } else {
        gsDraws[gsDrawCount++] = d;
    }
}

#ifdef BT3_GS_ESPERAS
/* (F5, etapa 8, gravação, seção 15 da nota) O fecho dos três GsGpu_DrawVu*: juntar ao desenho anterior ou abrir outro. É o
   mesmo de antes, com o bloco uniforme montado direto na vaga seguinte do gsVuUni (os 304 bytes não são mais copiados da
   pilha para a lista a cada desenho novo; num desenho que junta, a vaga fica para o próximo, e nada lê o gsVuUni além da
   contagem) e as comparações feitas palavra a palavra em linha no lugar das chamadas ao memcmp (o resultado é o mesmo:
   memcmp igual a 0 é todas as palavras iguais). Num quadro de luta metade dos desenhos junta, e cada junção compara o
   bloco inteiro. */
#define VU_U (*pu)
static int iguais32(const void *a, const void *b, unsigned n) {
    const uint32_t *x = (const uint32_t *)a, *y = (const uint32_t *)b;
    unsigned i;
    for (i = 0; i < n; i++) {
        if (x[i] != y[i]) {
            return 0;
        }
    }
    return 1;
}
static int same_state_vu(const GsDraw *a, const GsDraw *b) {
    return !a->native && a->vu == b->vu && a->target == b->target && a->tex == b->tex && a->sampler == b->sampler &&
           a->pipeline == b->pipeline && iguais32(a->mode, b->mode, 4) && a->misc[0] == b->misc[0] && a->misc[1] == b->misc[1] &&
           a->misc[2] == b->misc[2] && a->misc[3] == b->misc[3] && a->blendc == b->blendc && iguais32(a->rect, b->rect, 4) &&
           iguais32(a->orig, b->orig, 4) && iguais32(a->scissor, b->scissor, 4);
}
static void vu_fecho(GsDraw *d, const Vu0Uniform *pu) {
    GsDraw *last;
    gsTargets[d->target].draws++;
    gsTargets[d->target].gen = gGsPageGen[gsTargets[d->target].fbp & 511];
    last = gsDrawCount ? &gsDraws[gsDrawCount - 1] : NULL;
#if defined(BT3_GPU_FALSO)
    EST_CONTA_FECHO(last, d, pu);
#endif
    if (last != NULL && same_state_vu(last, d) && iguais32(&gsVuUni[last->uniform], pu, sizeof(Vu0Uniform) / 4)) {
        vu_faixa(gsDrawCount - 1, d->first, d->count, 1);
        last->count += d->count;
#ifdef PORT_X360
        GS_VU_MARCA(GS_FAM_V_FECHO);
#endif
        return;
    }
    d->uniform = (int)gsVuUniCount++; /* (pu é a vaga gsVuUni[gsVuUniCount]) */
    vu_faixa(gsDrawCount, d->first, d->count, 0);
    gsDraws[gsDrawCount++] = *d;
#ifdef PORT_X360
    GS_VU_MARCA(GS_FAM_V_FECHO);
#endif
}
#else
#define VU_U u
#endif

/* Vertex program 6 as a shader: one strip of the ground under a fighter, textured with the shadow page (see
   shaders/vu6.vert). Vertices: four quadwords each (position with an integer "no draw" flag in w, normal, colour
   as floats, a slot the original fills with s, t). A flagged vertex means: the triangle that ends here is not
   drawn (it only joins two real triangles of the strip). */
void GsGpu_DrawVu6(int ctx, const float *vertices, uint32_t count, const float *consts) {
    if (gPortResim) { /* a frame that is only being re-run: nothing is drawn */
        return;
    }
#ifdef BT3_GS_ESPERAS
    Vu0Uniform *pu; /* (seção 15 da nota da gravação) o bloco uniforme, montado na vaga seguinte do gsVuUni (vu_fecho) */
    GsDraw d;
#else
    Vu0Uniform u;
    GsDraw d, *last;
#endif
    float us, vs;
#ifdef BT3_GS_ESPERAS
    uint32_t i, flag; /* (sem o k: os índices saem três a três, seção 16 da nota da gravação) */
#else
    uint32_t i, k, flag;
#endif

#ifdef PORT_X360
    GS_VU_MARCA(GS_FAM_V_PROG);
#endif
    if (count < 3 || gsVuVertCount + count > CAP_VU_VERTS || gsVuIdxCount + (count - 2) * 3 > CAP_VU_IDX || gsDrawCount == CAP_DRAWS ||
        gsVuUniCount == CAP_VU_UNIFORMS) {
        return;
    }
    if (!DRAW_STATE_VU(ctx, 3, &d, &us, &vs)) {
#ifdef PORT_X360
        GS_VU_MARCA(GS_FAM_V_ESTADO);
#endif
        return;
    }
    date_snapshot(&d);
#ifdef PORT_X360
    GS_VU_MARCA(GS_FAM_V_ESTADO);
#endif
#ifdef BT3_GS_ESPERAS
    pu = &gsVuUni[gsVuUniCount];
#endif
    memset(&VU_U, 0, sizeof(VU_U));
    memcpy(VU_U.boneA, &consts[0x0C * 4], 64);  /* world -> shadow camera */
    VU_U.pivotA[0] = consts[0x10 * 4];          /* texture scale */
    VU_U.pivotB[0] = us;
    VU_U.pivotB[1] = vs;
    memcpy(VU_U.screen, &consts[0], 64);
    VU_U.misc[0] = (float)(gGs.xyoffset[ctx] & 0xFFFF) / 16.0f;
    VU_U.misc[1] = (float)((gGs.xyoffset[ctx] >> 32) & 0xFFFF) / 16.0f;
    VU_U.misc[2] = ((gGs.zbuf[ctx] >> 24) & 15) == 0 ? 4294967295.0f : ((gGs.zbuf[ctx] >> 24) & 15) == 1 ? 16777215.0f : 65535.0f;
#ifdef PORT_X360
    GS_VU_MARCA(GS_FAM_V_CONST);
#endif
    d.vu = 3;
    d.first = gsVuIdxCount;
#ifdef BT3_GS_ESPERAS
    /* (F5, etapa 8, gravação, seção 16 da nota) A posição e a base dos índices em locais, gravadas de volta uma vez: com o
       contador global (gsVuIdxCount) escrito por um uint32_t * do mesmo tipo, o cl o relia e regravava a cada índice. Os mesmos
       índices, na mesma ordem. */
    {
        uint32_t *const idx = gsVuIdx;
        const uint32_t base = gsVuVertCount;
        uint32_t n = gsVuIdxCount;
        for (i = 0; i + 2 < count; i++) { /* strip -> triangles, without the flagged ones */
            memcpy(&flag, &vertices[(i + 2) * 16 + 3], 4);
            if (flag & 0xFFFF) {
                continue;
            }
            idx[n] = base + i;
            idx[n + 1] = base + i + 1;
            idx[n + 2] = base + i + 2;
            n += 3;
        }
        gsVuIdxCount = n;
    }
#else
    for (i = 0; i + 2 < count; i++) { /* strip -> triangles, without the flagged ones */
        memcpy(&flag, &vertices[(i + 2) * 16 + 3], 4);
        if (flag & 0xFFFF) {
            continue;
        }
        for (k = 0; k < 3; k++) {
            gsVuIdx[gsVuIdxCount++] = gsVuVertCount + i + k;
        }
    }
#endif
    d.count = gsVuIdxCount - d.first;
#ifdef PORT_X360
    GS_VU_MARCA(GS_FAM_V_IDX);
#endif
    if (d.count == 0) {
        return;
    }
    for (i = 0; i < count; i++) {
        float *o = &gsVuVerts[gsVuVertCount++ * 12];
        memcpy(o, &vertices[i * 16], 16);          /* position */
        memcpy(o + 4, &vertices[i * 16 + 8], 16);  /* colour */
        memset(o + 8, 0, 16);
    }
#ifdef PORT_X360
    if (gGsVuMede) {
        int c_ = 0;
        GS_VU_MARCA(GS_FAM_V_COPIA + c_);
        gGsFamVezes[GS_FAM_V_COPIA + c_]++;
        gGsFamVuVet[c_] += count;
    }
#endif
#ifdef BT3_GS_ESPERAS
    vu_fecho(&d, pu);
#else
    gsTargets[d.target].draws++;
    gsTargets[d.target].gen = gGsPageGen[gsTargets[d.target].fbp & 511];
    last = gsDrawCount ? &gsDraws[gsDrawCount - 1] : NULL;
    if (last != NULL && same_state(last, &d) && memcmp(&gsVuUni[last->uniform], &u, sizeof(u)) == 0) {
#ifdef BT3_GS_ESPERAS
        vu_faixa(gsDrawCount - 1, d.first, d.count, 1);
#endif
        last->count += d.count;
#ifdef PORT_X360
        GS_VU_MARCA(GS_FAM_V_FECHO);
#endif
        return;
    }
    d.uniform = (int)gsVuUniCount;
    gsVuUni[gsVuUniCount++] = u;
#ifdef BT3_GS_ESPERAS
    vu_faixa(gsDrawCount, d.first, d.count, 0);
#endif
    gsDraws[gsDrawCount++] = d;
#ifdef PORT_X360
    GS_VU_MARCA(GS_FAM_V_FECHO);
#endif
#endif
}

/* Vertex program 4 as a shader: one strip of stage geometry (see shaders/vu4.vert). Vertices: position, colour
   (0..255 floats), texture coordinates, 48 bytes; the screen matrix is VU memory 0..3. */
void GsGpu_DrawVu4(int ctx, const float *vertices, uint32_t count, const float *consts) {
    if (gPortResim) { /* a frame that is only being re-run: nothing is drawn */
        return;
    }
#ifdef BT3_GS_ESPERAS
    Vu0Uniform *pu; /* (seção 15 da nota da gravação) o bloco uniforme, montado na vaga seguinte do gsVuUni (vu_fecho) */
    GsDraw d;
#else
    Vu0Uniform u;
    GsDraw d, *last;
#endif
    float us, vs;
#ifdef BT3_GS_ESPERAS
    uint32_t i; /* (sem o k: os índices saem três a três, seção 16 da nota da gravação) */
#else
    uint32_t i, k;
#endif

#ifdef PORT_X360
    GS_VU_MARCA(GS_FAM_V_PROG);
#endif
    if (count < 3 || gsVuVertCount + count > CAP_VU_VERTS || gsVuIdxCount + (count - 2) * 3 > CAP_VU_IDX || gsDrawCount == CAP_DRAWS ||
        gsVuUniCount == CAP_VU_UNIFORMS) {
        return;
    }
    if (!DRAW_STATE_VU(ctx, 2, &d, &us, &vs)) {
#ifdef PORT_X360
        GS_VU_MARCA(GS_FAM_V_ESTADO);
#endif
        return;
    }
    date_snapshot(&d);
#ifdef PORT_X360
    GS_VU_MARCA(GS_FAM_V_ESTADO);
#endif
#ifdef BT3_GS_ESPERAS
    pu = &gsVuUni[gsVuUniCount];
#endif
    memset(&VU_U, 0, sizeof(VU_U));
    memcpy(VU_U.screen, &consts[0], 64);
    VU_U.misc[0] = (float)(gGs.xyoffset[ctx] & 0xFFFF) / 16.0f;
    VU_U.misc[1] = (float)((gGs.xyoffset[ctx] >> 32) & 0xFFFF) / 16.0f;
    VU_U.misc[2] = ((gGs.zbuf[ctx] >> 24) & 15) == 0 ? 4294967295.0f : ((gGs.zbuf[ctx] >> 24) & 15) == 1 ? 16777215.0f : 65535.0f;
#ifdef PORT_X360
    GS_VU_MARCA(GS_FAM_V_CONST);
#endif
    d.vu = 2;
    d.first = gsVuIdxCount;
#ifdef BT3_GS_ESPERAS
    /* (F5, etapa 8, gravação, seção 16 da nota) A posição e a base dos índices em locais, gravadas de volta uma vez: com o
       contador global (gsVuIdxCount) escrito por um uint32_t * do mesmo tipo, o cl o relia e regravava a cada índice. Os mesmos
       índices, na mesma ordem. */
    {
        uint32_t *const idx = gsVuIdx;
        const uint32_t base = gsVuVertCount;
        uint32_t n = gsVuIdxCount;
        for (i = 0; i + 2 < count; i++) { /* strip -> triangles */
            idx[n] = base + i;
            idx[n + 1] = base + i + 1;
            idx[n + 2] = base + i + 2;
            n += 3;
        }
        gsVuIdxCount = n;
    }
#else
    for (i = 0; i + 2 < count; i++) { /* strip -> triangles */
        for (k = 0; k < 3; k++) {
            gsVuIdx[gsVuIdxCount++] = gsVuVertCount + i + k;
        }
    }
#endif
    d.count = gsVuIdxCount - d.first;
#ifdef PORT_X360
    GS_VU_MARCA(GS_FAM_V_IDX);
#endif
    memcpy(&gsVuVerts[gsVuVertCount * 12], vertices, count * 48);
    gsVuVertCount += count;
#ifdef PORT_X360
    if (gGsVuMede) {
        int c_ = 0;
        GS_VU_MARCA(GS_FAM_V_COPIA + c_);
        gGsFamVezes[GS_FAM_V_COPIA + c_]++;
        gGsFamVuVet[c_] += count;
    }
#endif
#ifdef BT3_GS_ESPERAS
    vu_fecho(&d, pu);
#else
    gsTargets[d.target].draws++;
    gsTargets[d.target].gen = gGsPageGen[gsTargets[d.target].fbp & 511];
    last = gsDrawCount ? &gsDraws[gsDrawCount - 1] : NULL;
    if (last != NULL && same_state(last, &d) && memcmp(&gsVuUni[last->uniform], &u, sizeof(u)) == 0) {
#ifdef BT3_GS_ESPERAS
        vu_faixa(gsDrawCount - 1, d.first, d.count, 1);
#endif
        last->count += d.count;
#ifdef PORT_X360
        GS_VU_MARCA(GS_FAM_V_FECHO);
#endif
        return;
    }
    d.uniform = (int)gsVuUniCount;
    gsVuUni[gsVuUniCount++] = u;
#ifdef BT3_GS_ESPERAS
    vu_faixa(gsDrawCount, d.first, d.count, 0);
#endif
    gsDraws[gsDrawCount++] = d;
#ifdef PORT_X360
    GS_VU_MARCA(GS_FAM_V_FECHO);
#endif
#endif
}

/* Vertex program 0 as a shader: one strip of the fighters' models (see shaders/vu0.vert and gs_vu1.c). The strip
   becomes a triangle list of the program's own 48-byte vertices; the constants go into a uniform block. */
void GsGpu_DrawVu0(int layer, int ctx, const float *vertices, uint32_t count, const float *consts) {
    if (gPortResim) { /* a frame that is only being re-run: nothing is drawn */
        return;
    }
#ifdef BT3_GS_ESPERAS
    Vu0Uniform *pu; /* (seção 15 da nota da gravação) o bloco uniforme, montado na vaga seguinte do gsVuUni (vu_fecho) */
    GsDraw d;
#else
    Vu0Uniform u;
    GsDraw d, *last;
#endif
    float us, vs;
#ifdef BT3_GS_ESPERAS
    uint32_t i; /* (sem o k: os índices saem três a três, seção 16 da nota da gravação) */
#else
    uint32_t i, k;
#endif

#ifdef PORT_X360
    GS_VU_MARCA(GS_FAM_V_PROG);
#endif
    if (count < 3 || gsVuVertCount + count > CAP_VU_VERTS || gsVuIdxCount + (count - 2) * 3 > CAP_VU_IDX || gsDrawCount == CAP_DRAWS ||
        gsVuUniCount == CAP_VU_UNIFORMS) {
        return;
    }
    if (!DRAW_STATE_VU(ctx, 1, &d, &us, &vs)) {
#ifdef PORT_X360
        GS_VU_MARCA(GS_FAM_V_ESTADO);
#endif
        return;
    }
    date_snapshot(&d);
#ifdef PORT_X360
    GS_VU_MARCA(GS_FAM_V_ESTADO);
#endif
#ifdef BT3_GS_ESPERAS
    pu = &gsVuUni[gsVuUniCount];
#endif
    memcpy(VU_U.boneA, &consts[0], 64);
    memcpy(VU_U.boneB, &consts[16], 64);
    memcpy(VU_U.pivotA, &consts[32], 16);
    memcpy(VU_U.pivotB, &consts[36], 16);
    memcpy(VU_U.screen, &consts[56], 64);
    VU_U.light[0] = consts[40]; VU_U.light[1] = consts[44]; VU_U.light[2] = consts[48]; VU_U.light[3] = consts[52];
    memcpy(VU_U.color0, &consts[88], 16);
    memcpy(VU_U.color1, &consts[92], 16);
    VU_U.misc[0] = (float)(gGs.xyoffset[ctx] & 0xFFFF) / 16.0f;
    VU_U.misc[1] = (float)((gGs.xyoffset[ctx] >> 32) & 0xFFFF) / 16.0f;
    VU_U.misc[2] = ((gGs.zbuf[ctx] >> 24) & 15) == 0 ? 4294967295.0f : ((gGs.zbuf[ctx] >> 24) & 15) == 1 ? 16777215.0f : 65535.0f;
    VU_U.misc[3] = (float)layer;
    VU_U.light2[0] = consts[41]; VU_U.light2[1] = consts[45]; VU_U.light2[2] = consts[49]; VU_U.light2[3] = consts[53];
#ifdef PORT_X360
    GS_VU_MARCA(GS_FAM_V_CONST);
#endif
    d.vu = 1;
    d.first = gsVuIdxCount;
#ifdef BT3_GS_ESPERAS
    /* (F5, etapa 8, gravação, seção 16 da nota) A posição e a base dos índices em locais, gravadas de volta uma vez: com o
       contador global (gsVuIdxCount) escrito por um uint32_t * do mesmo tipo, o cl o relia e regravava a cada índice. Os mesmos
       índices, na mesma ordem. */
    {
        uint32_t *const idx = gsVuIdx;
        const uint32_t base = gsVuVertCount;
        uint32_t n = gsVuIdxCount;
        for (i = 0; i + 2 < count; i++) { /* strip -> triangles */
            if (layer == 4) { /* debris: a flagged vertex does not complete a triangle */
                uint32_t flag;
                memcpy(&flag, &vertices[(i + 2) * 12 + 3], 4);
                if (flag & 0xFFFF) {
                    continue;
                }
            }
            idx[n] = base + i;
            idx[n + 1] = base + i + 1;
            idx[n + 2] = base + i + 2;
            n += 3;
        }
        gsVuIdxCount = n;
    }
#else
    for (i = 0; i + 2 < count; i++) { /* strip -> triangles */
        if (layer == 4) { /* debris: a flagged vertex does not complete a triangle */
            uint32_t flag;
            memcpy(&flag, &vertices[(i + 2) * 12 + 3], 4);
            if (flag & 0xFFFF) {
                continue;
            }
        }
        for (k = 0; k < 3; k++) {
            gsVuIdx[gsVuIdxCount++] = gsVuVertCount + i + k;
        }
    }
#endif
    d.count = gsVuIdxCount - d.first;
#ifdef PORT_X360
    GS_VU_MARCA(GS_FAM_V_IDX);
#endif
    memcpy(&gsVuVerts[gsVuVertCount * 12], vertices, count * 48);
    gsVuVertCount += count;
#ifdef PORT_X360
    if (gGsVuMede) {
        int c_ = layer == 1 || layer == 5;
        GS_VU_MARCA(GS_FAM_V_COPIA + c_);
        gGsFamVezes[GS_FAM_V_COPIA + c_]++;
        gGsFamVuVet[c_] += count;
    }
#endif
#ifdef BT3_GS_ESPERAS
    vu_fecho(&d, pu);
#else
    gsTargets[d.target].draws++;
    gsTargets[d.target].gen = gGsPageGen[gsTargets[d.target].fbp & 511];
    last = gsDrawCount ? &gsDraws[gsDrawCount - 1] : NULL;
    if (last != NULL && same_state(last, &d) && memcmp(&gsVuUni[last->uniform], &u, sizeof(u)) == 0) {
#ifdef BT3_GS_ESPERAS
        vu_faixa(gsDrawCount - 1, d.first, d.count, 1);
#endif
        last->count += d.count;
#ifdef PORT_X360
        GS_VU_MARCA(GS_FAM_V_FECHO);
#endif
        return;
    }
    d.uniform = (int)gsVuUniCount;
    gsVuUni[gsVuUniCount++] = u;
#ifdef BT3_GS_ESPERAS
    vu_faixa(gsDrawCount, d.first, d.count, 0);
#endif
    gsDraws[gsDrawCount++] = d;
#ifdef PORT_X360
    GS_VU_MARCA(GS_FAM_V_FECHO);
#endif
#endif
}

/* The game uploaded pixels straight into a display buffer (a movie frame). Remembers the place in the frame's
   draw order; the pixels are taken from GS memory at the end of the frame (frame_end). */
void GsGpu_FbUpload(int second) {
    if (gPortResim) { /* a frame that is only being re-run: nothing is drawn */
        return;
    }
    GsDraw d;

    if (gsDrawCount == CAP_DRAWS) {
        return;
    }
    memset(&d, 0, sizeof(d));
    d.native = 100 + (second != 0);
    d.target = GsDraw_TargetGet(second ? 0x70 : 0, 1);
    if (d.target >= 0) {
        gsDraws[gsDrawCount++] = d;
    }
}

void GsGpu_Native(int effect) {
    if (gPortResim) { /* a frame that is only being re-run: nothing is drawn */
        return;
    }
    uint64_t sc = gGs.scissor[0];
    GsDraw d;

    /* 1 = outline. 2 (the see-through tint) is drawn by the generic table pass now (depth_clut), from the game's
       own table. */
    if (effect >= 0x10 && effect <= 0x14) {
        gsAnchor = effect == 0x13 ? 0 : effect == 0x14 ? 4 : effect - 0x0F; /* 1 left, 2 right, 3 centre, 4 by position */
        return;
    }
    if (effect != 1 || gsDrawCount == CAP_DRAWS || (gsFxOff & 1)) {
        return;
    }
    gsNative++;
    memset(&d, 0, sizeof(d));
    d.native = effect;
    d.target = GsDraw_TargetGet((uint32_t)(gGs.frame[0] & 0x1FF), 0);
    if (d.target < 0) {
        return;
    }
    d.scissor[0] = (int)(sc & 0x7FF) * SCALE;
    d.scissor[1] = (int)((sc >> 32) & 0x7FF) * SCALE;
    d.scissor[2] = ((int)((sc >> 16) & 0x7FF) + 1) * SCALE - d.scissor[0];
    d.scissor[3] = ((int)((sc >> 48) & 0x7FF) + 1) * SCALE - d.scissor[1];
    gsDraws[gsDrawCount++] = d;
}

/* A new resolution multiplier: every render target is dropped and made again at the new size when the game next
   draws to it. Buffers that carry something over from the previous frame start empty for one frame. */
static void scale_apply(void) {
    if (gsPendingScale == 0 || gsPendingScale == gsScale) {
        gsPendingScale = 0;
        return;
    }
    gsScale = gsPendingScale; /* the back end's attachments and copies are rebuilt to this size */
    gsPendingScale = 0;
    sBackend->scaleChanged();
    gsTargetCount = 0;
#ifdef BT3_GS_ESPERAS
    sAlvosMuda++;
#endif
    fprintf(stderr, "bt3: internal resolution %dx (%d x %d)\n", gsScale, 512 * gsScale, 448 * gsScale);
}

/* ------------------------------------------------------------------------------------------- the one window */
/* Created (with SDL's video subsystem) before the device / the GL context: everything the user can choose
   about it (BT3_WINDOW, BT3_DISPLAY, BT3_FULLSCREEN, the saved settings, the aspect ratio) is decided here,
   so both back ends show the same window. `opengl` requests a window a GL context can be made current on. */
SDL_Window *GsDraw_WindowCreate(int opengl) {
    extern int Port_AspectMilli(void);
    float want = (float)Port_AspectMilli() / 1000.0f;
    int h = 896, w = (int)((float)h * want + 0.5f), count = 0, pick = 0;
    SDL_DisplayID *displays;
    SDL_PropertiesID props;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "bt3: SDL_Init: %s\n", SDL_GetError());
        return NULL;
    }
    displays = SDL_GetDisplays(&count);
    props = SDL_CreateProperties();
    /* The window has the picture's shape: 896 lines high, 4:3 or 16:9 wide (1195 x 896 or 1593 x 896), and
       keeps that shape when it is resized. BT3_WINDOW=WxH gives another start size. */
    if (getenv("BT3_WINDOW") != NULL) {
        sscanf(getenv("BT3_WINDOW"), "%dx%d", &w, &h);
    }
    /* BT3_DISPLAY=n: the n-th display (1 = first) for the window or the full screen; without it the desktop
       places the window. Some desktops (Wayland) ignore a window's position but honour the display of a
       full-screen window. BT3_FULLSCREEN=1 starts in borderless full screen; F11 switches. */
    for (pick = 0; pick < count; pick++) {
        SDL_Rect r;
        SDL_GetDisplayBounds(displays[pick], &r);
        fprintf(stderr, "bt3: display %d: %s, %d x %d\n", pick + 1, SDL_GetDisplayName(displays[pick]), r.w, r.h);
    }
    sDisplaySetting = Port_Setting("display", 0);
    pick = getenv("BT3_DISPLAY") != NULL ? atoi(getenv("BT3_DISPLAY")) : sDisplaySetting;
    sFullscreen = (getenv("BT3_FULLSCREEN") != NULL ? atoi(getenv("BT3_FULLSCREEN")) : Port_Setting("fullscreen", 0)) != 0;
    sWantAspect = want;
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Tenkaichi3Decomp");
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, w);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, h);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
    if (opengl) {
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
    }
    if (pick >= 1 && pick <= count) {
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(displays[pick - 1]));
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(displays[pick - 1]));
    } else if (pick != 0) {
        fprintf(stderr, "bt3: BT3_DISPLAY=%d: there are %d displays\n", pick, count);
    }
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, sFullscreen);
    sWindow = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    SDL_free(displays);
    if (sWindow != NULL && !sFullscreen) {
        SDL_SetWindowSize(sWindow, (int)((float)896 * want + 0.5f), 896);
    }
    fprintf(stderr, "bt3: internal resolution %dx (%d x %d)\n", gsScale, 512 * gsScale, 448 * gsScale);
    return sWindow;
}

SDL_Window *GsDraw_Window(void) { return sWindow; }

void GsDraw_WindowShape(void) {
    float want = (float)Port_AspectMilli() / 1000.0f;
    int w, h;
    sWantAspect = want;
    if (!sFullscreen && sWindow != NULL) {
        SDL_GetWindowSize(sWindow, &w, &h);
        SDL_SetWindowSize(sWindow, (int)((float)h * want + 0.5f), h);
    }
}

void GsDraw_FullscreenSet(int on) {
    if (sWindow == NULL) {
        return;
    }
    sFullscreen = on;
    SDL_SetWindowFullscreen(sWindow, on);
}

void GsDraw_FullscreenToggle(void) {
    GsDraw_FullscreenSet(!sFullscreen);
    Port_SettingSave("fullscreen", sFullscreen);
    Port_SettingsWrite();
}

/* ------------------------------------------------------------------------------------------- the settings window
   F1 opens it (ui.cpp, Dear ImGui). What the user changes there reaches the renderer through these two: the
   scale, the effects switched off and the glow are shared state; the shape, the full screen and the display
   are the window's. */
void GsGpu_GetSettings(PortVideo *v) {
    v->scale = gsPendingScale ? gsPendingScale : gsScale;
    v->aspectMilli = Port_AspectMilli();
    v->fullscreen = sFullscreen;
    v->fxOff = (int)gsFxOff;
    v->glow = gsGlowPercent;
    v->music = gPortMusicPercent;
    v->effects = gPortSePercent;
    v->display = sDisplaySetting;
    v->texPack = sTexPackOn;
    v->texPackCount = TexPack_Count();
}

/* Applies what differs from the current state, all of it at once, and keeps it for the next run. */
void GsGpu_SetSettings(const PortVideo *v) {
    PortVideo now;
    GsGpu_GetSettings(&now);
    if (v->scale != now.scale) {
        gsPendingScale = v->scale < 1 ? 1 : v->scale > 8 ? 8 : v->scale; /* applied between two frames */
    }
    if (v->aspectMilli != now.aspectMilli) {
        Port_SetAspectMilli(v->aspectMilli);
        GsDraw_WindowShape();
    }
    if (v->fullscreen != now.fullscreen) {
        GsDraw_FullscreenSet(v->fullscreen);
    }
    gsFxOff = (unsigned)v->fxOff & 31;
    gsGlowPercent = v->glow;
    gPortSePercent = v->effects;
    if (v->music != now.music) {
        gPortMusicPercent = v->music;
        Port_AudioRefresh();
    }
    sDisplaySetting = v->display;
    sTexPackOn = v->texPack != 0;
    Port_SettingSave("texture_pack", sTexPackOn);
    Port_SettingSave("scale", gsPendingScale ? gsPendingScale : gsScale);
    Port_SettingSave("aspect_milli", Port_AspectMilli());
    Port_SettingSave("fullscreen", sFullscreen);
    Port_SettingSave("fx_off", (int)gsFxOff);
    Port_SettingSave("glow", gsGlowPercent);
    Port_SettingSave("music", gPortMusicPercent);
    Port_SettingSave("effects", gPortSePercent);
    Port_SettingSave("display", sDisplaySetting);
    Port_SettingsWrite();
}

int GsGpu_Init(void) {
    const char *api = getenv("BT3_GPU_API");
    GsBackend *sb;

    gsScale = getenv("BT3_SCALE") != NULL ? atoi(getenv("BT3_SCALE")) : Port_Setting("scale", 2);
    gsScale = gsScale < 1 ? 1 : gsScale > 8 ? 8 : gsScale;
    gsPendingScale = 0;
#ifdef BT3_GS_ESPERAS
#ifdef BT3_GPU_FALSO
    sCapDiv = getenv("BT3_GS_CAP_DIV") != NULL && atoi(getenv("BT3_GS_CAP_DIV")) > 1 ? (uint32_t)atoi(getenv("BT3_GS_CAP_DIV")) : 1u;
#endif
    jogo_aloca(&sJogos[0]); /* (o segundo, só com a divisão: divisao_decide) */
    jogo_usa(0);
#else
    gsVerts = malloc(MAX_VERTS * sizeof(Vtx));
    gsVuVerts = malloc(MAX_VU_VERTS * 48);
    gsVuIdx = malloc(MAX_VU_IDX * sizeof(uint32_t));
    gsVuUni = malloc(MAX_VU_UNIFORMS * sizeof(Vu0Uniform));
    gsDraws = malloc(MAX_DRAWS * sizeof(GsDraw));
#endif
#ifdef PORT_X360
    /* The Xbox 360 has one back end, Direct3D 9 on its own device (port/x360/src/gs/gs_d3d.cpp). */
    (void)api;
    {
        extern GsBackend sD3dBackend;
        sb = &sD3dBackend;
        sBackend = sb;
#ifdef BT3_GS_ESPERAS
        /* (degrau 2, etapa 4) a divisão antes do init: com ela, o init roda na thread de desenho, que fica dona do
           dispositivo; a gravação (esta thread) espera, e as alocações acima ficam com ela */
        divisao_decide();
        if (GsDraw_Dividido() ? !desenho_init() : !sb->init()) {
            if (gGsDesenhoParar != NULL) {
                gGsDesenhoParar();
                gGsDesenhoParar = NULL;
            }
            sBackend = NULL;
            return 0;
        }
#else
        if (!sb->init()) {
            sBackend = NULL;
            return 0;
        }
#endif
    }
#else
    /* BT3_GPU_API=vulkan|gl|auto picks the back end. Both are complete renderers of the same list, so a
       picture from either should be the same (see the frame-by-frame compare in the port's docs). */
    if (api == NULL || strcmp(api, "auto") == 0) {
        /* the settings window's choice (Renderer; Vulkan unless OpenGL was picked there) */
        sb = Port_Setting("gpu_api", 0) == 1 ? &sGlBackend : &sVulkanBackend;
        api = NULL;
    } else if (strcmp(api, "vk") == 0 || strcmp(api, "vulkan") == 0) {
        sb = &sVulkanBackend;
    } else if (strcmp(api, "gl") == 0 || strcmp(api, "opengl") == 0) {
        sb = &sGlBackend;
    } else {
        fprintf(stderr, "bt3: unknown BT3_GPU_API=%s (try vulkan, gl or auto)\n", api);
        return 0;
    }
    sBackend = sb;
    if (!sb->init()) {
        /* The chosen renderer cannot start (no Vulkan driver, an old graphics card): the other one is tried,
           unless one was asked for by name. */
        GsBackend *other = sb == &sVulkanBackend ? &sGlBackend : &sVulkanBackend;
        sBackend = NULL;
        if (api != NULL) {
            return 0;
        }
        fprintf(stderr, "bt3: the %s renderer could not start; trying %s\n", sb == &sVulkanBackend ? "Vulkan" : "OpenGL",
                other == &sVulkanBackend ? "Vulkan" : "OpenGL");
        sb = other;
        sBackend = sb;
        if (!sb->init()) {
            sBackend = NULL;
            return 0;
        }
    }
#endif
    gsWhite = sb->whiteTex();
    gsFxOff = (unsigned)(getenv("BT3_FX_OFF") != NULL ? atoi(getenv("BT3_FX_OFF")) : Port_Setting("fx_off", 0)) & 31;
    gsGlowPercent = getenv("BT3_GLOW") != NULL ? atoi(getenv("BT3_GLOW")) : Port_Setting("glow", GLOW_DEFAULT);
    sTexPackOn = Port_Setting("texture_pack", 1) != 0;
    gPortMusicPercent = Port_Setting("music", 100);
    gPortSePercent = Port_Setting("effects", 100);
    TexPack_Init();
#if defined(BT3_GS_ESPERAS) && !defined(PORT_X360)
    divisao_decide();
#endif
    return 1;
}

/* ---------------------------------------------------------------------------------- in-between pictures
 * The game computes 30 pictures a second (every loop waits for two vertical blanks). With BT3_INTERP=1 (setting
 * "interp") the renderer shows one more between each two: when the game has finished picture n, the list of n is
 * first replayed with the vertex programs' matrices (bones, pivots, the screen matrix: Vu0Uniform) half way from
 * those of n - 1, and the real n follows one vertical blank later (GsGpu_InterpFlush, from Port_VBlank). The
 * game itself is untouched: same simulation, same inputs, same online play. The cost is that picture n is seen
 * one vertical blank (16.7 ms) later than without.
 *
 * PROTOTYPE. Blended: what the vertex programs draw (fighters, stage, shadows, debris). Not blended: what the
 * game transforms itself (effects, HUD, 2D), which stays that of picture n. A draw of n is paired with the draw
 * of n - 1 at the same place in the list if it is the same kind of draw (signature below); a pair whose model
 * would jump far on screen (a camera cut, a teleport) is not blended, and when most are like that no in-between
 * picture is shown at all. */
int gsInterp = -1;
static uint32_t sInterpBlkView[MAX_VU_UNIFORMS], sInterpPrevBlkView[MAX_VU_UNIFORMS]; /* the view each block is drawn in (a number made of its draws' scissor rectangle) */
static float sInterpPrevRep[MAX_VU_UNIFORMS][3];
static uint8_t sInterpFollow[MAX_VU_UNIFORMS]; /* the block is scenery that goes with the camera (the far backdrop) */
static double sInterpCamNow[3], sInterpCamWas[3]; /* where the camera stands, this picture and the previous one */
static double sInterpSceneWas[16];                /* the scenery's screen matrix of the previous picture */
static int sInterpHaveCam;
static uint16_t sInterpKind[MAX_VU_UNIFORMS], sInterpPrevKind[MAX_VU_UNIFORMS]; /* which vertex program and layer each block is drawn with */
static uint16_t sInterpBucket2[4096], sInterpChain2[MAX_VU_UNIFORMS];
static float sInterpCamMove; /* GS pixels the camera alone moved the scenery in view this tick (the middle value) */
static float sInterpRep[MAX_VU_UNIFORMS][3]; /* the first vertex of each block's mesh (model space) */
/* The camera's change of this picture (interp_build): previous clip position = sInterpD * this clip position, for
   anything that stands still in the world; and the numbers that turn GS pixels into that clip space. */
static float sInterpD[16], sInterpOffX, sInterpOffY, sInterpZMax;
static int sInterpHaveD;
static Vu0Uniform *sInterpPrev, *sInterpBlend;
static uint32_t *sInterpPrevSig, *sInterpSig;
static uint32_t sInterpPrevCount;
static int sInterpNext;  /* the tick's next picture still to be shown (2..sInterpSteps), 0 = none */
static int sInterpSteps = 2;
static unsigned sInterpLastBlank;
static uint64_t sInterpTickNs = 33366700ull; /* how long the tick being shown lasts */
static unsigned gInterpTicks;
static uint64_t sInterpBlendShown, sInterpBlendTook; /* when the in-between picture was handed to the screen; how long replaying its list took */
static struct { uint32_t verts, vuVerts, vuIdx, vuUni, draws; int anchor; unsigned skipped, native; unsigned tdraws[MAX_TARGETS]; } sInterpSaved;
unsigned gInterpShown, gInterpSkipped; /* in-between pictures shown / left out since the start */
static unsigned gInterpCuts, gInterpStatic, gInterpNotMutual;
static unsigned gInterpFollowed, gInterpThrough; /* ticks left without in-between pictures because the camera's way crossed the scenery */
static unsigned gInterpPaired, gInterpUnpaired, gInterpCamera; /* blocks blended / without a partner / of those, moved by the camera's change (BT3_INTERP_LOG) */

#define INTERP_SCISSOR_ID(d) ((((uint32_t)(d)->scissor[0] * 73856093u) ^ ((uint32_t)(d)->scissor[1] * 19349663u) ^ ((uint32_t)(d)->scissor[2] * 83492791u) ^ ((uint32_t)(d)->scissor[3] * 2971215073u)) | 1u)
/* What a uniform block draws, as a number that is the same in the next picture: the kind of each of its draws and
   the first vertices of each (they are in model space: the same mesh has the same numbers every frame, wherever it
   stands in the list; the stage's pieces come and go with what the camera sees, so the place in the list is no
   identity). */
static void interp_signatures(uint32_t *sig) {
    uint32_t n, k, j;

    memset(sig, 0, gsVuUniCount * sizeof(uint32_t));
    for (n = 0; n < gsDrawCount; n++) {
        const GsDraw *d = &gsDraws[n];
        if (d->vu != 0 && d->uniform >= 0 && (uint32_t)d->uniform < gsVuUniCount) {
            uint32_t h = d->count * 2654435761u ^ (uint32_t)d->vu << 28; /* (not its texture or pipeline: a layer's texture can change every frame) */
            if (sig[d->uniform] == 0 && d->count != 0 && gsVuIdx[d->first] < gsVuVertCount) {
                memcpy(sInterpRep[d->uniform], &gsVuVerts[gsVuIdx[d->first] * 12], 3 * sizeof(float)); /* a point of the mesh */
            }
            for (k = 0; k < 4 && k < d->count; k++) {
                uint32_t v = gsVuIdx[d->first + k];
                if (v < gsVuVertCount) {
                    const uint32_t *w = (const uint32_t *)&gsVuVerts[v * 12];
                    for (j = 0; j < 12; j++) {
                        h = (h ^ w[j]) * 16777619u;
                    }
                }
            }
            sig[d->uniform] = (sig[d->uniform] * 31u + h) | 1u;
            sInterpKind[d->uniform] = (uint16_t)(d->vu * 64 + ((int)gsVuUni[d->uniform].misc[3] & 63));
            sInterpBlkView[d->uniform] = INTERP_SCISSOR_ID(d);
            if (d->vu == 3) {
                /* The ground under a fighter that carries its shadow (vertex program 6): its vertices are the piece
                   of ground the fighter stands over, new numbers every picture, so the vertices are no identity.
                   All of them count as the same kind; which fighter's it is, the nearest matrices decide. */
                sig[d->uniform] = 0x53484457u;
            }
        }
    }
}

/* Where the model's pivot lands on the GS screen (pixels), 0 if behind the eye. */
static int interp_screen_pos(const Vu0Uniform *u, float *x, float *y) {
    float p[4], s[4];
    int i;

    for (i = 0; i < 4; i++) { /* boneA * (0 - pivotA, 1) */
        p[i] = -(u->boneA[i] * u->pivotA[0] + u->boneA[4 + i] * u->pivotA[1] + u->boneA[8 + i] * u->pivotA[2]) + u->boneA[12 + i];
    }
    for (i = 0; i < 4; i++) {
        s[i] = u->screen[i] * p[0] + u->screen[4 + i] * p[1] + u->screen[8 + i] * p[2] + u->screen[12 + i];
    }
    if (!(s[3] > 0.0001f) && !(s[3] < -0.0001f)) {
        return 0;
    }
    *x = s[0] / s[3];
    *y = s[1] / s[3];
    return s[3] > 0.0f ? 1 : -1;
}

/* The camera's change, the part for the picture being built, applied to a screen matrix: in double precision. */
static void interp_camera_screen(const float *screen, float *o);

/* 4x4 matrices as the uniform block has them (columns). */
static void interp_mul(const float *a, const float *b, float *o) {
    int c, r;

    for (c = 0; c < 4; c++) {
        for (r = 0; r < 4; r++) {
            o[c * 4 + r] = a[r] * b[c * 4] + a[4 + r] * b[c * 4 + 1] + a[8 + r] * b[c * 4 + 2] + a[12 + r] * b[c * 4 + 3];
        }
    }
}

static int interp_inverse(const float *m, float *o) {
    double a[4][8];
    int i, j, k;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            a[i][j] = m[j * 4 + i];
            a[i][4 + j] = i == j;
        }
    }
    for (i = 0; i < 4; i++) {
        int piv = i;
        double d;
        for (k = i + 1; k < 4; k++) {
            if (fabs(a[k][i]) > fabs(a[piv][i])) { piv = k; }
        }
        if (fabs(a[piv][i]) < 1e-12) {
            return 0;
        }
        if (piv != i) {
            for (j = 0; j < 8; j++) { double x = a[i][j]; a[i][j] = a[piv][j]; a[piv][j] = x; }
        }
        d = a[i][i];
        for (j = 0; j < 8; j++) { a[i][j] /= d; }
        for (k = 0; k < 4; k++) {
            if (k != i) {
                d = a[k][i];
                for (j = 0; j < 8; j++) { a[k][j] -= d * a[i][j]; }
            }
        }
    }
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            o[j * 4 + i] = (float)a[i][4 + j];
        }
    }
    return 1;
}

/* Fills sInterpBlend for the frame just recorded; 0 = no in-between picture for this one.
   Each block of this picture is paired with the block of the previous picture that draws the same mesh; of several
   that do (a piece of scenery used more than once) the one whose matrices are nearest. */
static uint16_t sInterpBucket[4096];
static uint16_t sInterpChain[MAX_VU_UNIFORMS];
static uint8_t sInterpUsed[MAX_VU_UNIFORMS];
static int16_t sInterpPair[MAX_VU_UNIFORMS]; /* the previous picture's block each of this picture's was paired with, -1 none */

/* A point of the block's mesh (model space) in clip space: screen matrix * bone * (point - pivot). (A real point of
   the mesh, not the pivot or the origin: for scenery and for the sky's dome those can be beside or behind the eye,
   where a position on the screen means nothing.) */
static void interp_clip_pos(const Vu0Uniform *u, const float *v, float *s) {
    float p[4], m[3];
    int i;

    for (i = 0; i < 3; i++) {
        m[i] = v[i] - u->pivotA[i];
    }
    for (i = 0; i < 4; i++) {
        p[i] = u->boneA[i] * m[0] + u->boneA[4 + i] * m[1] + u->boneA[8 + i] * m[2] + u->boneA[12 + i];
    }
    for (i = 0; i < 4; i++) {
        s[i] = u->screen[i] * p[0] + u->screen[4 + i] * p[1] + u->screen[8 + i] * p[2] + u->screen[12 + i];
    }
}

/* The camera's change in parts: sInterpRoot[0] = its square root (half of the move), [1] = a quarter, [2] = an eighth.
   The picture a fraction of a tick before this one is then (the product of some of these) * this picture's screen
   matrix: the camera half way along its turn. (Half way between the two MATRICES is something else: a camera that
   turned by a good angle in one tick gives a picture that is too small half way, the average of two rotations
   being shorter than either. The scenery shrank in the in-between pictures of every fast turn; the sky came away
   from the edge of the screen and the cleared picture showed as a grey veil.) */
static double sInterpRoot[3][16];
static int sInterpRoots; /* they could be worked out */
static double sInterpDd[16], sInterpDtD[16]; /* the camera's change, and the part of it used for the picture being built, in double precision */
static float sInterpDt[16]; /* the camera's change over the part of the tick still to come at the picture being built */
static int sInterpHaveDt;

static int interp_inverse_d(const double *m, double *o) {
    double a[4][8];
    int i, j, k;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            a[i][j] = m[j * 4 + i];
            a[i][4 + j] = i == j;
        }
    }
    for (i = 0; i < 4; i++) {
        int piv = i;
        double d;
        for (k = i + 1; k < 4; k++) {
            if (fabs(a[k][i]) > fabs(a[piv][i])) { piv = k; }
        }
        if (fabs(a[piv][i]) < 1e-300) {
            return 0;
        }
        if (piv != i) {
            for (j = 0; j < 8; j++) { double x = a[i][j]; a[i][j] = a[piv][j]; a[piv][j] = x; }
        }
        d = a[i][i];
        for (j = 0; j < 8; j++) { a[i][j] /= d; }
        for (k = 0; k < 4; k++) {
            if (k != i) {
                d = a[k][i];
                for (j = 0; j < 8; j++) { a[k][j] -= d * a[i][j]; }
            }
        }
    }
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            o[j * 4 + i] = a[i][4 + j];
        }
    }
    return 1;
}

static void interp_mul_d(const double *a, const double *b, double *o) {
    int c, r;

    for (c = 0; c < 4; c++) {
        for (r = 0; r < 4; r++) {
            o[c * 4 + r] = a[r] * b[c * 4] + a[4 + r] * b[c * 4 + 1] + a[8 + r] * b[c * 4 + 2] + a[12 + r] * b[c * 4 + 3];
        }
    }
}

/* The square root of a matrix (Denman and Beavers' iteration): the matrix that, applied twice, is `a`. For the
   camera's change that is half of the camera's move. 0 = it did not settle (a turn of half a circle has none). */
static int interp_sqrt_d(const double *a, double *out) {
    double y[16], z[16], yi[16], zi[16], chk[16], err = 0.0, mag = 0.0;
    int i, it;

    memcpy(y, a, sizeof(y));
    for (i = 0; i < 16; i++) {
        z[i] = i % 5 == 0;
    }
    for (it = 0; it < 24; it++) {
        if (!interp_inverse_d(y, yi) || !interp_inverse_d(z, zi)) {
            return 0;
        }
        for (i = 0; i < 16; i++) {
            y[i] = 0.5 * (y[i] + zi[i]);
            z[i] = 0.5 * (z[i] + yi[i]);
        }
    }
    interp_mul_d(y, y, chk);
    for (i = 0; i < 16; i++) {
        err += fabs(chk[i] - a[i]);
        mag += fabs(a[i]);
        if (!(y[i] == y[i])) {
            return 0;
        }
    }
    if (!(err <= mag * 1e-6)) {
        return 0;
    }
    memcpy(out, y, sizeof(y));
    return 1;
}

static void interp_camera_screen(const float *screen, float *o) {
    double sc[16], r[16];
    int k;

    for (k = 0; k < 16; k++) {
        sc[k] = screen[k];
    }
    interp_mul_d(sInterpDtD, sc, r);
    for (k = 0; k < 16; k++) {
        o[k] = (float)r[k];
    }
}

/* Where the camera of a screen matrix stands in the world: the one point that it sends to "at the eye" (no x, no y,
   no w). 0 = it cannot be said. */
static int interp_camera_centre(const double *screen, double *c) {
    double inv[16];

    if (!interp_inverse_d(screen, inv) || fabs(inv[11]) < 1e-300) {
        return 0;
    }
    c[0] = inv[8] / inv[11];
    c[1] = inv[9] / inv[11];
    c[2] = inv[10] / inv[11];
    return c[0] == c[0] && c[1] == c[1] && c[2] == c[2];
}

/* A bone half way: the average of the two matrices, with each axis given back its length (the average of two
   directions is shorter than either), and, for a bone that holds the place as seen from the camera, the place
   moved around the camera and not through the chord. */
static void interp_bone(const float *p, const float *c, float t, int around, float *o) {
    int col, k;

    for (k = 0; k < 16; k++) {
        o[k] = p[k] + (c[k] - p[k]) * t;
    }
    for (col = 0; col < (around ? 4 : 3); col++) {
        const float *a = &p[col * 4], *b = &c[col * 4];
        float *m = &o[col * 4];
        float la = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]), lb = sqrtf(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
        float lm = sqrtf(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]), want = la + (lb - la) * t;
        if (lm > 1e-12f && la > 1e-12f && lb > 1e-12f && lm > 0.3f * want) {
            float f = want / lm;
            m[0] *= f;
            m[1] *= f;
            m[2] *= f;
        }
    }
}

/* (of the view being worked on: interp_build_view) */
static uint32_t jumped, nscreens;
static float D[16];
static const float *screens[16];
static int agreed, bonesInView;
static uint32_t sInterpView; /* the view being worked on: the scissor id of its draws */
static int sInterpWasCut;
#define INTERP_CUT 300.0f /* GS pixels the scenery may move in a tick by the camera alone; more is a cut */

static int interp_build_view(float t, int first) {
    uint32_t i, paired = 0;
    float inv[16], chk[16];

    if (!first) {
        goto blend; /* (who goes with whom and how the camera moved was worked out for the tick's first picture) */
    }
    jumped = 0;
    agreed = 0;
    memset(sInterpBucket, 0xFF, sizeof(sInterpBucket));
    memset(sInterpUsed, 0, sInterpPrevCount);
    for (i = sInterpPrevCount; i-- > 0;) { /* (backwards: each chain lists its blocks in the order of the list) */
        uint32_t b = sInterpPrevSig[i] * 2654435761u >> 20;
        sInterpChain[i] = sInterpBucket[b];
        sInterpBucket[b] = (uint16_t)i;
    }
    /* 1. Each block's partner: the block of the previous picture that draws the same mesh. */
    for (i = 0; i < gsVuUniCount; i++) {
        if (sInterpBlkView[i] != sInterpView) { continue; } /* (one view at a time) */
        const Vu0Uniform *c = &gsVuUni[i];
        const float *cf = (const float *)c;
        float bestDist = 0.0f, bestDist2 = 0.0f;
        int k, best = -1;
        uint16_t j;

        sInterpBlend[i] = *c;
        sInterpPair[i] = -1;
        if (sInterpSig[i] == 0) {
            continue;
        }
        for (j = sInterpBucket[sInterpSig[i] * 2654435761u >> 20]; j != 0xFFFF; j = sInterpChain[j]) {
            /* (a shadow's strips are several blocks with one fighter's matrices, and their number changes with
               the ground: each takes the nearest of the previous picture, used before or not) */
            if (sInterpPrevSig[j] == sInterpSig[i] && sInterpPrevBlkView[j] == sInterpView && (!sInterpUsed[j] || sInterpSig[i] == 0x53484457u)) {
                /* Nearest first by where the mesh stands (bones and pivots), and only among equals by the screen
                   matrix. One sum over both let the camera decide: the sky is the same slice of a dome many times
                   over, and while the camera turned, the slice NEXT to a slice had the nearer screen matrix. */
                const float *pf = (const float *)&sInterpPrev[j];
                float dist = 0.0f, dist2 = 0.0f;
                for (k = 0; k < 40; k++) { /* bones, pivots */
                    dist += fabsf(pf[k] - cf[k]);
                }
                for (k = 40; k < 56; k++) { /* screen matrix */
                    dist2 += fabsf(pf[k] - cf[k]);
                }
                if (best < 0 || dist < bestDist || (dist == bestDist && dist2 < bestDist2)) {
                    best = j;
                    bestDist = dist;
                    bestDist2 = dist2;
                }
            }
        }
        if (best >= 0) {
            sInterpUsed[best] = 1;
            sInterpPair[i] = (int16_t)best;
        }
    }
    /* 1b. A second chance for a block without a partner. What a piece of scenery draws changes with what the camera
       sees of it (its strips are culled one by one), and then its vertices do not name it. Where it stands does: the
       block of the previous picture with the same program, layer and the very same bones is the same piece, or one
       that stands exactly like it, which blends to the same thing. For what stands nearly as before (the dome of
       the sky follows the camera) the nearest such block. (Left without partners, the backdrop was placed by the
       camera's change, which does not fit what follows the camera: it slid aside and the cleared picture showed
       through as a grey veil.) */
    {
        memset(sInterpBucket2, 0xFF, sizeof(sInterpBucket2));
        for (i = sInterpPrevCount; i-- > 0;) {
            const uint32_t *w = (const uint32_t *)sInterpPrev[i].boneA;
            uint32_t h = sInterpPrevKind[i], k2;
            for (k2 = 0; k2 < 40; k2++) {
                h = (h ^ w[k2]) * 16777619u;
            }
            h = h * 2654435761u >> 20;
            sInterpChain2[i] = sInterpBucket2[h];
            sInterpBucket2[h] = (uint16_t)i;
        }
        for (i = 0; i < gsVuUniCount; i++) {
        if (sInterpBlkView[i] != sInterpView) { continue; } /* (one view at a time) */
            const Vu0Uniform *c = &gsVuUni[i];
            const uint32_t *w = (const uint32_t *)c->boneA;
            const float *cf = (const float *)c;
            uint32_t h = sInterpKind[i], k2, j2;
            uint16_t j;
            float bestDist = 0.0f, mag = 0.0f;
            int best = -1;

            /* (not the scenery's program: it has no bones, so "stands the same" says nothing there; see 1c) */
            if (sInterpPair[i] >= 0 || sInterpSig[i] == 0 || sInterpSig[i] == 0x53484457u || (sInterpKind[i] >> 6) == 2) {
                continue;
            }
            for (k2 = 0; k2 < 40; k2++) {
                h = (h ^ w[k2]) * 16777619u;
            }
            for (j = sInterpBucket2[h * 2654435761u >> 20]; j != 0xFFFF; j = sInterpChain2[j]) {
                if (sInterpPrevKind[j] == sInterpKind[i] && sInterpPrevBlkView[j] == sInterpView && memcmp(sInterpPrev[j].boneA, c->boneA, 40 * sizeof(float)) == 0) {
                    best = j;
                    break;
                }
            }
            if (best < 0) {
                for (k2 = 0; k2 < 40; k2++) {
                    mag += fabsf(cf[k2]);
                }
                for (j2 = 0; j2 < sInterpPrevCount; j2++) {
                    const float *pf = (const float *)&sInterpPrev[j2];
                    float dist = 0.0f;
                    if (sInterpUsed[j2] || sInterpPrevKind[j2] != sInterpKind[i] || sInterpPrevBlkView[j2] != sInterpView) {
                        continue;
                    }
                    for (k2 = 0; k2 < 40 && dist <= mag * 0.05f; k2++) {
                        dist += fabsf(pf[k2] - cf[k2]);
                    }
                    if (dist <= mag * 0.05f && (best < 0 || dist < bestDist)) {
                        best = (int)j2;
                        bestDist = dist;
                    }
                }
            }
            if (first && best < 0 && i < 14 && getenv("BT3_INTERP_JUMPS") != NULL && (int)gGsFrame == atoi(getenv("BT3_INTERP_JUMPS"))) {
                float nd = 1e30f; int nj = -1, same = 0;
                for (j2 = 0; j2 < sInterpPrevCount; j2++) {
                    const float *pf = (const float *)&sInterpPrev[j2]; float dist = 0.0f;
                    if (sInterpPrevKind[j2] != sInterpKind[i]) { continue; }
                    same++;
                    for (k2 = 0; k2 < 40; k2++) { dist += fabsf(pf[k2] - cf[k2]); }
                    if (dist < nd) { nd = dist; nj = (int)j2; }
                }
                fprintf(stderr, "no partner for block %u: kind %u, %d of that kind before, nearest %d at distance %.3f (size %.3f), used %d; bone row3 %.2f %.2f %.2f; before %.2f %.2f %.2f\n", i, sInterpKind[i], same, nj, nd, mag,
                        nj >= 0 ? sInterpUsed[nj] : -1, c->boneA[12], c->boneA[13], c->boneA[14], nj >= 0 ? sInterpPrev[nj].boneA[12] : 0.0f, nj >= 0 ? sInterpPrev[nj].boneA[13] : 0.0f, nj >= 0 ? sInterpPrev[nj].boneA[14] : 0.0f);
            }
            if (best >= 0) {
                sInterpUsed[best] = 1;
                sInterpPair[i] = (int16_t)best;
            }
        }
    }
    /* 2. The camera's change. For an object standing still, screen matrix = camera * model, so (previous screen) =
       D * (this screen) with one D for all of them: D = previous * inverse(this) of any still object (a pair whose
       bones did not change). Taken from one such pair and accepted when another agrees; a few are tried. */
    {
        int tries = 0;
        uint32_t a, b;

        for (a = 0; a < gsVuUniCount && !agreed && tries < 8; a++) {
        if (sInterpBlkView[a] != sInterpView) { continue; } /* (one view at a time) */
            const Vu0Uniform *c = &gsVuUni[a], *q;
            if (sInterpPair[a] < 0) {
                continue;
            }
            q = &sInterpPrev[sInterpPair[a]];
            if (memcmp(c->boneA, q->boneA, 40 * sizeof(float)) != 0 || !interp_inverse(c->screen, inv)) {
                continue;
            }
            interp_mul(q->screen, inv, D);
            tries++;
            for (b = a + 1; b < gsVuUniCount; b++) {
        if (sInterpBlkView[b] != sInterpView) { continue; } /* (one view at a time) */
                const Vu0Uniform *c2 = &gsVuUni[b], *q2;
                float err = 0.0f, mag = 0.0f;
                int k;
                if (sInterpPair[b] < 0) {
                    continue;
                }
                q2 = &sInterpPrev[sInterpPair[b]];
                if (memcmp(c2->boneA, q2->boneA, 40 * sizeof(float)) != 0) {
                    continue;
                }
                interp_mul(D, c2->screen, chk);
                for (k = 0; k < 16; k++) {
                    err += fabsf(chk[k] - q2->screen[k]);
                    mag += fabsf(q2->screen[k]);
                }
                if (err <= mag * 0.002f) {
                    agreed = 1;
                    {
                        /* The same once more in double precision, and that is the one used. A screen matrix is badly
                           conditioned (a projection), and its inverse in single precision is good to a part in a
                           thousand at best: enough to tell that two pieces share a camera, not enough to draw with.
                           (Scenery placed with it had corners near the eye on the wrong side of it: sheets over the
                           whole screen, a grey veil.) */
                        double sc[16], sp[16], si[16], dd[16];
                        int k3;
                        for (k3 = 0; k3 < 16; k3++) {
                            sc[k3] = c->screen[k3];
                            sp[k3] = q->screen[k3];
                        }
                        memcpy(sInterpSceneWas, sp, sizeof(sp));
                        sInterpHaveCam = interp_camera_centre(sc, sInterpCamNow) && interp_camera_centre(sp, sInterpCamWas);
                        if (interp_inverse_d(sc, si)) {
                            interp_mul_d(sp, si, dd);
                            for (k3 = 0; k3 < 16; k3++) {
                                sInterpDd[k3] = dd[k3];
                                D[k3] = (float)dd[k3];
                            }
                        } else {
                            for (k3 = 0; k3 < 16; k3++) {
                                sInterpDd[k3] = D[k3];
                            }
                        }
                    }
                    memcpy(sInterpD, D, sizeof(D));
                    sInterpOffX = c->misc[0];
                    sInterpOffY = c->misc[1];
                    sInterpZMax = c->misc[2];
                }
                break; /* (one witness per candidate) */
            }
        }
        sInterpHaveD = agreed && sInterpZMax > 0.0f;
        sInterpRoots = 0;
        if (agreed) {
            sInterpRoots = interp_sqrt_d(sInterpDd, sInterpRoot[0]) && interp_sqrt_d(sInterpRoot[0], sInterpRoot[1]) && interp_sqrt_d(sInterpRoot[1], sInterpRoot[2]);
        }
    }
    /* 2b. Scenery that goes with the camera. The far backdrop is drawn with the scenery's own screen matrix, but its
       vertices are made anew in every picture, around where the camera stands: it has no partner by its vertices,
       and the camera's change, which is right for what stands still, moves it out from around the camera (the
       camera looked at it from outside: a dark sheet over the whole picture, seen whenever it was "placed by the
       camera"). It is known by this: the previous picture has a block of the same kind whose first vertex is this
       one's, less the way the camera went. */
    memset(sInterpFollow, 0, gsVuUniCount);
    if (agreed && sInterpHaveCam) {
        float d[3], dl;
        d[0] = (float)(sInterpCamNow[0] - sInterpCamWas[0]);
        d[1] = (float)(sInterpCamNow[1] - sInterpCamWas[1]);
        d[2] = (float)(sInterpCamNow[2] - sInterpCamWas[2]);
        dl = fabsf(d[0]) + fabsf(d[1]) + fabsf(d[2]);
        for (i = 0; i < gsVuUniCount && dl > 1e-4f; i++) {
        if (sInterpBlkView[i] != sInterpView) { continue; } /* (one view at a time) */
            uint32_t j2;
            const float *r = sInterpRep[i];
            if (sInterpPair[i] >= 0 || sInterpSig[i] == 0 || (sInterpKind[i] >> 6) != 2) {
                continue;
            }
            if (i < 30 && getenv("BT3_INTERP_JUMPS") != NULL && (int)gGsFrame == atoi(getenv("BT3_INTERP_JUMPS"))) {
                float be = 1e30f; int bj = -1, cand = 0;
                for (j2 = 0; j2 < sInterpPrevCount; j2++) {
                    const float *q = sInterpPrevRep[j2]; float e;
                    if (sInterpPrevKind[j2] != sInterpKind[i]) { continue; }
                    cand += !sInterpUsed[j2];
                    e = fabsf(r[0] - q[0] - d[0]) + fabsf(r[1] - q[1] - d[1]) + fabsf(r[2] - q[2] - d[2]);
                    if (e < be) { be = e; bj = (int)j2; }
                }
                fprintf(stderr, "follow? block %u first vertex %.3f %.3f %.3f; camera went %.4f %.4f %.4f; %d free blocks of the kind before; nearest (any) %d off by %.4f, its vertex %.3f %.3f %.3f, used %d\n", i, r[0], r[1], r[2], d[0], d[1], d[2], cand, bj, be,
                        bj >= 0 ? sInterpPrevRep[bj][0] : 0.0f, bj >= 0 ? sInterpPrevRep[bj][1] : 0.0f, bj >= 0 ? sInterpPrevRep[bj][2] : 0.0f, bj >= 0 ? sInterpUsed[bj] : -1);
            }
            for (j2 = 0; j2 < sInterpPrevCount; j2++) {
                const float *q = sInterpPrevRep[j2];
                float e;
                if (sInterpUsed[j2] || sInterpPrevKind[j2] != sInterpKind[i] || sInterpPrevBlkView[j2] != sInterpView) {
                    continue;
                }
                e = fabsf(r[0] - q[0] - d[0]) + fabsf(r[1] - q[1] - d[1]) + fabsf(r[2] - q[2] - d[2]);
                if (e < 0.02f * dl + 1e-3f * (fabsf(r[0]) + fabsf(r[1]) + fabsf(r[2])) && e < 0.5f * dl) {
                    sInterpFollow[i] = 1;
                    break;
                }
            }
        }
    }
    /* 3. What moved too far to be the same thing a tick later. With the camera's change known, "far" is measured
       after taking the camera out: where the block's pivot would have been if only the camera had moved, against
       where its partner really was. (Measured on the screen as it is, a fast turn of the camera made everything
       "jump": nothing was blended while spinning, which showed as hitches and as things out of step.) A camera that
       itself moved the scenery by most of the screen is a cut: no in-between pictures for this tick. */
    {
        uint32_t still = 0, far = 0, nmove = 0, movN = 0, movBig = 0;
        static float move[256];
        float cam = 0.0f;

        /* how far the camera alone moved the scenery on the screen, typically: the middle of what the still blocks
           in view moved */
        for (i = 0; agreed && i < gsVuUniCount && nmove < 256; i++) {
        if (sInterpBlkView[i] != sInterpView) { continue; } /* (one view at a time) */
            const Vu0Uniform *c = &gsVuUni[i], *q;
            float sc[4], sp[4];
            if (sInterpPair[i] < 0) {
                continue;
            }
            q = &sInterpPrev[sInterpPair[i]];
            if (memcmp(c->boneA, q->boneA, 40 * sizeof(float)) != 0) {
                continue;
            }
            interp_clip_pos(c, sInterpRep[i], sc);
            interp_clip_pos(q, sInterpRep[i], sp);
            if (sc[3] > 1.0f && sp[3] > 1.0f && fabsf(sc[0] / sc[3]) < 4096.0f && fabsf(sc[1] / sc[3]) < 4096.0f) {
                float dx = fabsf(sc[0] / sc[3] - sp[0] / sp[3]), dy = fabsf(sc[1] / sc[3] - sp[1] / sp[3]);
                move[nmove++] = dx > dy ? dx : dy;
            }
        }
        /* The same from the scenery's own vertices: a few hundred of them, those in view now, and where the camera's
           change says they were. (The first vertex of each piece, as above, is seldom in view: with nothing to go
           by, the fighters were asked whether a tick is a cut, and one that dashed past the camera said yes.) */
        if (agreed) {
            uint32_t n2, total = 0, stepV;
            for (n2 = 0; n2 < gsDrawCount; n2++) {
                total += gsDraws[n2].vu == 2 && gsDraws[n2].uniform >= 0 && (uint32_t)gsDraws[n2].uniform < gsVuUniCount && sInterpBlkView[gsDraws[n2].uniform] == sInterpView ? gsDraws[n2].count : 0;
            }
            stepV = total / 400 ? total / 400 : 1;
            nmove = 0;
            for (n2 = 0; n2 < gsDrawCount; n2++) {
                const GsDraw *d = &gsDraws[n2];
                const Vu0Uniform *c;
                uint32_t k2;
                if (d->vu != 2 || d->uniform < 0 || (uint32_t)d->uniform >= gsVuUniCount || sInterpBlkView[d->uniform] != sInterpView) {
                    continue;
                }
                c = &gsVuUni[d->uniform];
                for (k2 = (n2 * 7) % stepV; k2 < d->count; k2 += stepV) {
                    const float *v = &gsVuVerts[gsVuIdx[d->first + k2] * 12];
                    float sc[4], sp[4], x, y;
                    int k3;
                    for (k3 = 0; k3 < 4; k3++) {
                        sc[k3] = c->screen[k3] * v[0] + c->screen[4 + k3] * v[1] + c->screen[8 + k3] * v[2] + c->screen[12 + k3];
                    }
                    if (!(sc[3] > 1.0f)) {
                        continue;
                    }
                    x = sc[0] / sc[3] - c->misc[0];
                    y = sc[1] / sc[3] - c->misc[1];
                    if (x < 0.0f || x > 640.0f || y < 0.0f || y > 448.0f) {
                        continue;
                    }
                    for (k3 = 0; k3 < 4; k3++) {
                        sp[k3] = D[k3] * sc[0] + D[4 + k3] * sc[1] + D[8 + k3] * sc[2] + D[12 + k3] * sc[3];
                    }
                    still++;
                    if (!(sp[3] > 1.0f)) {
                        far++;
                    } else {
                        float dx = fabsf(sc[0] / sc[3] - sp[0] / sp[3]), dy = fabsf(sc[1] / sc[3] - sp[1] / sp[3]);
                        far += dx > INTERP_CUT || dy > INTERP_CUT;
                        if (nmove < 256) {
                            move[nmove++] = dx > dy ? dx : dy;
                        }
                    }
                }
            }
        }
        if (nmove != 0) {
            uint32_t a2, b2;
            for (a2 = 1; a2 < nmove; a2++) { /* (insertion sort: at most 256) */
                float v = move[a2];
                for (b2 = a2; b2 > 0 && move[b2 - 1] > v; b2--) { move[b2] = move[b2 - 1]; }
                move[b2] = v;
            }
            cam = move[nmove / 2];
        }
        sInterpCamMove = cam;

        for (i = 0; i < gsVuUniCount; i++) {
        if (sInterpBlkView[i] != sInterpView) { continue; } /* (one view at a time) */
            const Vu0Uniform *c = &gsVuUni[i], *q;
            float sc[4], sp[4], pr[4];
            int k, bad = 0;

            if (sInterpPair[i] < 0) {
                continue;
            }
            q = &sInterpPrev[sInterpPair[i]];
            interp_clip_pos(c, sInterpRep[i], sc);
            interp_clip_pos(q, sInterpRep[i], sp);
            /* (The camera's change D cannot be applied to a fighter's position: the fighters are drawn through
               another projection than the scenery, and D is in the scenery's. Tried: it put them far away.) What
               moves on its own is judged on the screen as it is, with room for what the camera does to the picture:
               a fighter the camera swings past moves as far as the scenery behind it. */
            memcpy(pr, sc, sizeof(pr));
            (void)k;
            if (memcmp(c->boneA, q->boneA, 40 * sizeof(float)) == 0) {
                /* It stands where it stood: it did not jump, whatever the numbers say (the origin of a piece of
                   scenery can be anywhere, also beside or behind the eye, where a position on the screen is a
                   quotient of two small numbers). Those in view now tell how far the camera moved the scenery: one
                   that was behind the eye a tick ago, or most of a screen away, speaks for a cut. */

            } else if (sInterpSig[i] != 0x53484457u && c->misc[3] <= 1.0f && sc[3] > 1.0f) {
                /* a part of a fighter: did it go far, or to quite another distance? */
                movN++;
                movBig += !(sp[3] > 1.0f) || fabsf(sc[0] / sc[3] - sp[0] / sp[3]) > 160.0f || fabsf(sc[1] / sc[3] - sp[1] / sp[3]) > 120.0f || sc[3] > sp[3] * 1.7f || sp[3] > sc[3] * 1.7f;
            }
            if (memcmp(c->boneA, q->boneA, 40 * sizeof(float)) == 0) {
            } else if (sp[3] > 1.0f && pr[3] > 1.0f && sc[3] > 1.0f && sInterpSig[i] != 0x53484457u) {
                /* (a point behind the eye in either picture is not judged: what is there is not seen) */
                /* Most of the screen in one tick. (It was a third of it: a fighter close to the camera that dashes
                   off moves further than that, was taken for a jump with all its parts, and, being half of all
                   blocks, made the whole tick go without in-between pictures: a hitch of everything at each such
                   moment. A short teleport is now blended, as one picture half way.) */
                bad = fabsf(sp[0] / sp[3] - pr[0] / pr[3]) > 350.0f + 1.5f * cam || fabsf(sp[1] / sp[3] - pr[1] / pr[3]) > 300.0f + 1.5f * cam;
            }
            if (bad && first && getenv("BT3_INTERP_JUMPS") != NULL && (int)gGsFrame == atoi(getenv("BT3_INTERP_JUMPS"))) {
                fprintf(stderr, "jump block %u layer %.0f now %.0f,%.0f w %.1f; was %.0f,%.0f w %.1f; by the camera alone %.0f,%.0f w %.1f; pivot %.1f %.1f %.1f point %.1f %.1f %.1f\n", i, c->misc[3],
                        sc[0] / sc[3], sc[1] / sc[3], sc[3], sp[0] / sp[3], sp[1] / sp[3], sp[3], pr[0] / pr[3], pr[1] / pr[3], pr[3], c->pivotA[0], c->pivotA[1], c->pivotA[2],
                        sInterpRep[i][0], sInterpRep[i][1], sInterpRep[i][2]);
            }
            if (bad) {
                sInterpPair[i] = -1;
                jumped++;
            }
        }
        /* A cut: the scenery in view says so; where there is none to ask (a transformation's or an ultimate's own
           scenes before a backdrop), the fighters do. (With scenery in view the fighters are not asked: one that
           dashes past the camera goes far and to another distance in one tick, and that is no cut.) */
        if (getenv("BT3_INTERP_CUTLOG") != NULL) {
            fprintf(stderr, "cut? frame %u: agreed %d, still in view %u, far %u, camera move %.0f; fighter parts %u, big %u; blocks %u, paired %u\n", gGsFrame, agreed, still, far, cam, movN, movBig, gsVuUniCount,
                    (unsigned)0);
        }
        /* (also: the scenery as a whole went by more than a third of the screen, which no turn of the camera does
           in a thirtieth of a second, the pieces in view or not; and nearly every part of the fighters went far
           while the scenery is no witness or moved a good deal too: the shots of a transformation or an ultimate
           with a little scenery in a corner) */
        if ((still >= 8 ? far * 2 > still || cam > 250.0f : movN >= 6 && movBig * 2 > movN) ||
            (movN >= 6 && movBig * 10 >= movN * 9 && cam > 120.0f)) {
            gInterpCuts++;
            sInterpWasCut = 1;
            return 0;
        }
    }
    /* Do the bones of what moves hold places as seen from the camera (the fighters' do: their screen matrix is the
       same in every picture, the camera is in their bones)? Then a place half way is taken around the camera. */
    bonesInView = 0;
    {
        uint32_t same = 0, other = 0;
        for (i = 0; agreed && i < gsVuUniCount; i++) {
        if (sInterpBlkView[i] != sInterpView) { continue; } /* (one view at a time) */
            const Vu0Uniform *c = &gsVuUni[i], *q;
            if (sInterpPair[i] < 0 || sInterpSig[i] == 0x53484457u || c->misc[3] > 1.0f) {
                continue;
            }
            q = &sInterpPrev[sInterpPair[i]];
            if (memcmp(c->boneA, q->boneA, 16 * sizeof(float)) != 0) {
                if (memcmp(c->screen, q->screen, 64) == 0) { same++; } else { other++; }
            }
        }
        bonesInView = same > 8 && same > other * 4 && sInterpCamMove > 0.5f;
    }
    /* The screen matrices the standing scenery is drawn with (a stage uses one or a few for all its pieces): what
       the camera's change may be applied to. */
    nscreens = 0;
    for (i = 0; agreed && i < gsVuUniCount && nscreens < 16; i++) {
        if (sInterpBlkView[i] != sInterpView) { continue; } /* (one view at a time) */
        const Vu0Uniform *c = &gsVuUni[i];
        uint32_t k2;
        if (sInterpPair[i] < 0 || memcmp(c->boneA, sInterpPrev[sInterpPair[i]].boneA, 40 * sizeof(float)) != 0) {
            continue;
        }
        for (k2 = 0; k2 < nscreens && memcmp(screens[k2], c->screen, 64) != 0; k2++) {
        }
        if (k2 == nscreens) {
            screens[nscreens++] = c->screen;
        }
    }
    /* 3b. Does the camera go THROUGH the scenery between the two pictures? Its two places are both good, but the
       straight way between them need not be: over a rise of the ground, or past the corner of a rock, the camera of
       an in-between picture stands inside the ground and looks at it from below, a dark sheet over the whole picture
       (seen in a low fly-by: the pictures a quarter and nine tenths of the way were right, those between were not).
       So: if the way from the previous place to this one crosses a triangle of the scenery, the tick gets no
       in-between pictures. */
    if (agreed && sInterpHaveCam && getenv("BT3_INTERP_THROUGH") == NULL) {
        float a0[3], d0[3], lo3[3], hi3[3], len;
        uint32_t n2;
        int k3, hit = 0;

        for (k3 = 0; k3 < 3; k3++) {
            a0[k3] = (float)sInterpCamWas[k3];
            d0[k3] = (float)(sInterpCamNow[k3] - sInterpCamWas[k3]);
            lo3[k3] = (a0[k3] < a0[k3] + d0[k3] ? a0[k3] : a0[k3] + d0[k3]) - 0.01f;
            hi3[k3] = (a0[k3] > a0[k3] + d0[k3] ? a0[k3] : a0[k3] + d0[k3]) + 0.01f;
        }
        len = fabsf(d0[0]) + fabsf(d0[1]) + fabsf(d0[2]);
        for (n2 = 0; len > 0.05f && n2 < gsDrawCount && !hit; n2++) {
            const GsDraw *d = &gsDraws[n2];
            uint32_t k2, tri;
            if (d->vu != 2 || d->uniform < 0 || (uint32_t)d->uniform >= gsVuUniCount || sInterpBlkView[d->uniform] != sInterpView) {
                continue;
            }
            for (k2 = 0; k2 < nscreens && memcmp(screens[k2], gsVuUni[d->uniform].screen, 64) != 0; k2++) {
            }
            if (k2 == nscreens) {
                continue;
            }
            for (tri = 0; tri + 3 <= d->count && !hit; tri += 3) {
                const float *v0 = &gsVuVerts[gsVuIdx[d->first + tri] * 12], *v1 = &gsVuVerts[gsVuIdx[d->first + tri + 1] * 12],
                            *v2 = &gsVuVerts[gsVuIdx[d->first + tri + 2] * 12];
                float e1[3], e2[3], pv[3], tv[3], qv[3], det, u, v, tt;
                int out = 0;
                for (k3 = 0; k3 < 3 && !out; k3++) { /* the triangle's box against the way's box */
                    out = (v0[k3] < lo3[k3] && v1[k3] < lo3[k3] && v2[k3] < lo3[k3]) || (v0[k3] > hi3[k3] && v1[k3] > hi3[k3] && v2[k3] > hi3[k3]);
                }
                if (out) {
                    continue;
                }
                for (k3 = 0; k3 < 3; k3++) {
                    e1[k3] = v1[k3] - v0[k3];
                    e2[k3] = v2[k3] - v0[k3];
                    tv[k3] = a0[k3] - v0[k3];
                }
                pv[0] = d0[1] * e2[2] - d0[2] * e2[1];
                pv[1] = d0[2] * e2[0] - d0[0] * e2[2];
                pv[2] = d0[0] * e2[1] - d0[1] * e2[0];
                det = e1[0] * pv[0] + e1[1] * pv[1] + e1[2] * pv[2];
                if (fabsf(det) < 1e-12f) {
                    continue;
                }
                u = (tv[0] * pv[0] + tv[1] * pv[1] + tv[2] * pv[2]) / det;
                if (u < 0.0f || u > 1.0f) {
                    continue;
                }
                qv[0] = tv[1] * e1[2] - tv[2] * e1[1];
                qv[1] = tv[2] * e1[0] - tv[0] * e1[2];
                qv[2] = tv[0] * e1[1] - tv[1] * e1[0];
                v = (d0[0] * qv[0] + d0[1] * qv[1] + d0[2] * qv[2]) / det;
                if (v < 0.0f || u + v > 1.0f) {
                    continue;
                }
                tt = (e2[0] * qv[0] + e2[1] * qv[1] + e2[2] * qv[2]) / det;
                hit = tt > 0.0f && tt < 1.0f;
            }
        }
        if (hit) {
            gInterpThrough++;
            sInterpWasCut = 1;
            return 0;
        }
    }
    /* 4. The blend. */
blend:
    sInterpHaveDt = 0;
    if (agreed && sInterpRoots && getenv("BT3_INTERP_LINEAR") == NULL) {
        /* the camera's change over the rest of the tick, 1 - t of it, from its halves, quarters and eighths */
        float left = (1.0f - t) * 8.0f;
        int m = (int)(left + 0.5f), bit;
        if (m >= 1 && m <= 7 && fabsf(left - (float)m) < 0.01f) {
            double acc[16], tmp[16];
            int k, have = 0;
            for (bit = 0; bit < 3; bit++) {
                if (m & (4 >> bit)) {
                    if (!have) {
                        memcpy(acc, sInterpRoot[bit], sizeof(acc));
                        have = 1;
                    } else {
                        interp_mul_d(acc, sInterpRoot[bit], tmp);
                        memcpy(acc, tmp, sizeof(acc));
                    }
                }
            }
            for (k = 0; k < 16; k++) {
                sInterpDt[k] = (float)acc[k];
                sInterpDtD[k] = acc[k];
            }
            sInterpHaveDt = 1;
        }
    }
    for (i = 0; i < gsVuUniCount; i++) {
        if (sInterpBlkView[i] != sInterpView) { continue; } /* (one view at a time) */
        const Vu0Uniform *c = &gsVuUni[i];
        Vu0Uniform *o = &sInterpBlend[i];
        int k, scenery = 0, part = 0;

        if (agreed) {
            uint32_t k2;
            for (k2 = 0; k2 < nscreens && memcmp(screens[k2], c->screen, 64) != 0; k2++) {
            }
            scenery = k2 < nscreens;
        }

        if (first && i >= 10 && i <= 42 && getenv("BT3_INTERP_JUMPS") != NULL && (int)gGsFrame == atoi(getenv("BT3_INTERP_JUMPS"))) {
            const Vu0Uniform *q = sInterpPair[i] >= 0 ? &sInterpPrev[sInterpPair[i]] : NULL;
            float sc[4], sp[4] = {0, 0, 0, 0};
            uint32_t n2, tris = 0; int pipe = -1, target = -1;
            for (n2 = 0; n2 < gsDrawCount; n2++) {
                if (gsDraws[n2].vu != 0 && (uint32_t)gsDraws[n2].uniform == i) { tris += gsDraws[n2].count / 3; pipe = gsDraws[n2].pipeline; target = gsDraws[n2].target; }
            }
            interp_clip_pos(c, sInterpRep[i], sc);
            if (q != NULL) { interp_clip_pos(q, sInterpRep[i], sp); }
            for (n2 = 0; n2 < gsDrawCount; n2++) {
                if (gsDraws[n2].vu != 0 && (uint32_t)gsDraws[n2].uniform == i) {
                    const float *v0 = &gsVuVerts[gsVuIdx[gsDraws[n2].first] * 12], *v1 = &gsVuVerts[gsVuIdx[gsDraws[n2].first + gsDraws[n2].count - 1] * 12];
                    fprintf(stderr, "  block %u draw %u: %u indices, tex %lx target-tex %d sampler %d mode %d %d %d %d misc %.1f %.1f %.1f %.1f blendc %.2f | v0 %.2f %.2f %.2f %.3f col %.0f %.0f %.0f %.0f | vlast %.2f %.2f %.2f %.3f\n", i, n2, gsDraws[n2].count,
                            (unsigned long)gsDraws[n2].tex, gsDraws[n2].tex_is_target, gsDraws[n2].sampler, gsDraws[n2].mode[0], gsDraws[n2].mode[1], gsDraws[n2].mode[2], gsDraws[n2].mode[3],
                            gsDraws[n2].misc[0], gsDraws[n2].misc[1], gsDraws[n2].misc[2], gsDraws[n2].misc[3], gsDraws[n2].blendc, v0[0], v0[1], v0[2], v0[3], v0[4], v0[5], v0[6], v0[7], v1[0], v1[1], v1[2], v1[3]);
                    break;
                }
            }
            fprintf(stderr, "block %u kind %u sig %08x pair %d scenery %d tris %u pipe %d target %d | now %.0f,%.0f w %.2f | was %.0f,%.0f w %.2f | colour %.0f %.0f %.0f %.0f\n", i, sInterpKind[i], sInterpSig[i], sInterpPair[i], scenery, tris, pipe, target,
                    sc[0] / sc[3], sc[1] / sc[3], sc[3], q ? sp[0] / sp[3] : 0.0f, q ? sp[1] / sp[3] : 0.0f, sp[3], c->color0[0], c->color0[1], c->color0[2], c->color0[3]);
        }
        if (getenv("BT3_INTERP_ONE") != NULL) { /* testing: leave the blocks from..to as picture n has them */
            int lo2 = 0, hi2 = -1;
            sscanf(getenv("BT3_INTERP_ONE"), "%d-%d", &lo2, &hi2);
            if ((int)i >= lo2 && (int)i <= hi2 && getenv("BT3_INTERP_PART") == NULL) {
                continue;
            }
            part = (int)i >= lo2 && (int)i <= hi2 ? atoi(getenv("BT3_INTERP_PART")) : 0;
        }
        if (getenv("BT3_INTERP_SKIP") != NULL) { /* testing: leave a kind of block as picture n has it */
            int what = atoi(getenv("BT3_INTERP_SKIP"));
            if ((what == 1 && sInterpSig[i] == 0x53484457u) || (what == 2 && c->misc[3] == 2.0f) || (what == 3 && c->misc[3] <= 1.0f && sInterpSig[i] != 0x53484457u) ||
                (what == 4 && c->misc[3] >= 3.0f)) {
                continue;
            }
            if ((what == 5 || what == 6) && sInterpPair[i] >= 0 && c->misc[3] <= 1.0f && sInterpSig[i] != 0x53484457u &&
                (what == 6) == (memcmp(c->boneA, sInterpPrev[sInterpPair[i]].boneA, 40 * sizeof(float)) == 0)) {
                continue;
            }
            if (what == 7 && sInterpPair[i] < 0) {
                continue;
            }
        }
        if (sInterpFollow[i]) {
            /* with the camera: the scenery's matrix of this in-between picture, and the piece moved to where that
               picture's camera stands */
            double m[16], ct[3], dd3[3], col[4];
            int ok;
            if (sInterpHaveDt) {
                double sc[16];
                for (k = 0; k < 16; k++) { sc[k] = c->screen[k]; }
                interp_mul_d(sInterpDtD, sc, m);
            } else {
                for (k = 0; k < 16; k++) { m[k] = sInterpSceneWas[k] + ((double)c->screen[k] - sInterpSceneWas[k]) * t; }
            }
            ok = interp_camera_centre(m, ct);
            if (ok) {
                dd3[0] = ct[0] - sInterpCamNow[0];
                dd3[1] = ct[1] - sInterpCamNow[1];
                dd3[2] = ct[2] - sInterpCamNow[2];
                for (k = 0; k < 4; k++) {
                    col[k] = m[k] * dd3[0] + m[4 + k] * dd3[1] + m[8 + k] * dd3[2] + m[12 + k];
                }
                for (k = 0; k < 12; k++) { o->screen[k] = (float)m[k]; }
                for (k = 0; k < 4; k++) { o->screen[12 + k] = (float)col[k]; }
                gInterpFollowed++;
            }
            continue;
        }
        if (sInterpPair[i] >= 0) {
            const Vu0Uniform *q = &sInterpPrev[sInterpPair[i]];
            const float *pf = (const float *)q, *cf = (const float *)c;
            float *of = (float *)o;
            /* boneA, boneB, pivotA, pivotB, screen: the first 56 floats; colours and misc stay picture n's. The four
               after them are a fighter's light direction, which turns with it, and for the scenery's program
               something else: how bright the piece is drawn. A piece of scenery paired with another one that stands
               like it (the second chance above) got the average of two brightnesses: the grey veil over a whole
               in-between picture, whenever the pieces on screen changed. */
            for (k = 0; k < ((sInterpKind[i] >> 6) == 1 && c->misc[3] <= 1.0f ? 60 : 56); k++) {
                of[k] = pf[k] + (cf[k] - pf[k]) * t;
            }
            if (memcmp(c->boneA, q->boneA, 40 * sizeof(float)) == 0) {
                /* it stands as it stood: its bones are this picture's to the last bit (pieces of scenery laid over
                   each other must come out at exactly the same depth) */
                memcpy(o->boneA, c->boneA, 40 * sizeof(float));
            } else if (sInterpSig[i] != 0x53484457u && c->misc[3] <= 1.0f) { /* (a model's two bones: not the shadow's and other programs' use of these slots) */
                int around = bonesInView && !scenery && memcmp(c->screen, q->screen, 64) == 0;
                interp_bone(q->boneA, c->boneA, t, around, o->boneA);
                interp_bone(q->boneB, c->boneB, t, around, o->boneB);
            }
            if (scenery && sInterpHaveDt) {
                interp_camera_screen(c->screen, o->screen); /* the camera part of the way, not the matrices' average */
            }
            paired++;
        } else if (agreed && sInterpSig[i] != 0) {
            /* No partner (a piece of scenery that came into view, a mesh whose strips changed, something that
               jumped): it would stand ahead of its blended neighbours, a seam. What moved for it is at least the
               camera. Only for what is drawn with the scenery's own screen matrix: the camera's change is in that
               projection, and a block in another one (a fighter's, an effect's) comes out anywhere. (Seen: a grey
               veil over a whole in-between picture as a fighter flew past the camera.) */
            if (!scenery) {
                continue;
            }
            if (first && getenv("BT3_INTERP_JUMPS") != NULL && (int)gGsFrame == atoi(getenv("BT3_INTERP_JUMPS"))) {
                uint32_t n2, tris = 0, draws = 0;
                int vu = 0, pipe = 0;
                for (n2 = 0; n2 < gsDrawCount; n2++) {
                    if (gsDraws[n2].vu != 0 && (uint32_t)gsDraws[n2].uniform == i) { tris += gsDraws[n2].count / 3; draws++; vu = gsDraws[n2].vu; pipe = gsDraws[n2].pipeline; }
                }
                fprintf(stderr, "camera-placed block %u: program kind %d, layer %.0f, %u draws, %u triangles, pipeline %d, colour %.0f %.0f %.0f %.0f, bone moves %d\n", i, vu, c->misc[3], draws, tris, pipe,
                        c->color0[0], c->color0[1], c->color0[2], c->color0[3], c->boneA[12] != 0.0f || c->boneA[13] != 0.0f);
            }
            if (getenv("BT3_INTERP_ONE") != NULL && (int)i == atoi(getenv("BT3_INTERP_ONE"))) {
                continue;
            }
            if (sInterpHaveDt) {
                interp_camera_screen(c->screen, o->screen);
            } else {
                interp_mul(D, c->screen, chk);
                for (k = 0; k < 16; k++) {
                    o->screen[k] = chk[k] + (c->screen[k] - chk[k]) * t;
                }
            }
            gInterpCamera++;
        }
        if (part == 1) { /* testing: only the screen matrix of these blocks is blended */
            memcpy(o->boneA, c->boneA, 40 * sizeof(float));
            memcpy(o->light, c->light, 16);
        } else if (part == 2) { /* only bones and pivots */
            memcpy(o->screen, c->screen, 64);
            memcpy(o->light, c->light, 16);
        } else if (part == 3) { /* only the four after the screen matrix */
            memcpy(o->boneA, c->boneA, 40 * sizeof(float));
            memcpy(o->screen, c->screen, 64);
        }
    }
    if (first && getenv("BT3_INTERP_FRAMES") != NULL) { /* testing: one line per picture */
        static FILE *fp;
        extern unsigned gPortVBlanks;
        if (fp == NULL) { fp = fopen(getenv("BT3_INTERP_FRAMES"), "w"); }
        if (fp != NULL) {
            fprintf(fp, "%u %u blocks %u paired %u jumped %u previous %u camera %d\n", gPortVBlanks, gGsFrame, gsVuUniCount, paired, jumped, sInterpPrevCount, agreed * 100 + (int)nscreens);
            { uint32_t f2 = 0, i2; for (i2 = 0; i2 < gsVuUniCount; i2++) { f2 += sInterpFollow[i2]; } fprintf(fp, "  follow %u\n", f2); }
        }
    }
    gInterpPaired += paired;
    gInterpUnpaired += gsVuUniCount - paired;
    /* (Whether the tick is a cut was decided from the still scenery above. Only where there is none, a fighter alone
       before a backdrop, do many jumps mean one.) */
    return paired != 0 && (agreed || jumped * 2 <= paired);
}

/* The views of the picture: split screen has two, each with its own camera, and the rendering of a fighter's shadow
   is one of its own. A view is told by the scissor rectangle its draws are made with. Everything above is done for
   one view at a time (sInterpView), with what it found about that view's camera kept from the tick's first picture
   to its later ones. (Done for the picture as a whole, the second view of a split screen was given the first
   view's camera: none of what depends on the camera worked in it.) */
#define INTERP_MAX_VIEWS 8
typedef struct InterpViewState {
    uint32_t id;
    int ok, cut, scenery;
    float D[16], offX, offY, zMax, Dt[16], camMove;
    double root[3][16], Dd[16], DtD[16], camNow[3], camWas[3], sceneWas[16];
    int haveD, roots, haveDt, haveCam, agreed, bonesInView;
    uint32_t jumped, nscreens;
    const float *screens[16];
} InterpViewState;
static InterpViewState sInterpViews[INTERP_MAX_VIEWS];
static int sInterpViewCount, sInterpViewLoaded = -1;

static void interp_view_save(InterpViewState *v) {
    memcpy(v->D, D, sizeof(D));
    v->offX = sInterpOffX; v->offY = sInterpOffY; v->zMax = sInterpZMax; v->camMove = sInterpCamMove;
    memcpy(v->Dt, sInterpDt, sizeof(v->Dt));
    memcpy(v->root, sInterpRoot, sizeof(v->root));
    memcpy(v->Dd, sInterpDd, sizeof(v->Dd));
    memcpy(v->DtD, sInterpDtD, sizeof(v->DtD));
    memcpy(v->camNow, sInterpCamNow, sizeof(v->camNow));
    memcpy(v->camWas, sInterpCamWas, sizeof(v->camWas));
    memcpy(v->sceneWas, sInterpSceneWas, sizeof(v->sceneWas));
    v->haveD = sInterpHaveD; v->roots = sInterpRoots; v->haveDt = sInterpHaveDt; v->haveCam = sInterpHaveCam;
    v->agreed = agreed; v->bonesInView = bonesInView; v->jumped = jumped; v->nscreens = nscreens;
    memcpy(v->screens, screens, sizeof(v->screens));
}

static void interp_view_load(const InterpViewState *v) {
    memcpy(D, v->D, sizeof(D));
    memcpy(sInterpD, v->D, sizeof(sInterpD));
    sInterpOffX = v->offX; sInterpOffY = v->offY; sInterpZMax = v->zMax; sInterpCamMove = v->camMove;
    memcpy(sInterpDt, v->Dt, sizeof(v->Dt));
    memcpy(sInterpRoot, v->root, sizeof(v->root));
    memcpy(sInterpDd, v->Dd, sizeof(v->Dd));
    memcpy(sInterpDtD, v->DtD, sizeof(v->DtD));
    memcpy(sInterpCamNow, v->camNow, sizeof(v->camNow));
    memcpy(sInterpCamWas, v->camWas, sizeof(v->camWas));
    memcpy(sInterpSceneWas, v->sceneWas, sizeof(v->sceneWas));
    sInterpHaveD = v->haveD; sInterpRoots = v->roots; sInterpHaveDt = v->haveDt; sInterpHaveCam = v->haveCam;
    agreed = v->agreed; bonesInView = v->bonesInView; jumped = v->jumped; nscreens = v->nscreens;
    memcpy(screens, v->screens, sizeof(v->screens));
}

/* For the pieces (interp_build_verts): what is known of the camera of view `id`. 0 = the view has no in-between
   picture this tick (a cut in it): its pieces stay this picture's. */
static int interp_view_select(uint32_t id) {
    int k;

    if (sInterpViewLoaded >= 0 && sInterpViews[sInterpViewLoaded].id == id) {
        return !sInterpViews[sInterpViewLoaded].cut;
    }
    for (k = 0; k < sInterpViewCount; k++) {
        if (sInterpViews[k].id == id) {
            interp_view_load(&sInterpViews[k]);
            sInterpViewLoaded = k;
            return !sInterpViews[k].cut;
        }
    }
    /* a view without any block of a vertex program (the HUD's rectangle): no camera */
    sInterpHaveD = sInterpHaveDt = sInterpRoots = sInterpHaveCam = 0;
    sInterpCamMove = 0.0f;
    sInterpViewLoaded = -1;
    return 1;
}

static int interp_build(float t, int first) {
    uint32_t i;
    int k, okAny = 0, cutAny = 0, okScenery = 0;

    if (gsVuUniCount == 0 || sInterpPrevCount == 0) {
        return 0;
    }
    memcpy(sInterpBlend, gsVuUni, gsVuUniCount * sizeof(Vu0Uniform));
    if (first) {
        sInterpViewCount = 0;
        for (i = 0; i < gsVuUniCount; i++) {
            if (sInterpSig[i] == 0) {
                continue;
            }
            for (k = 0; k < sInterpViewCount && sInterpViews[k].id != sInterpBlkView[i]; k++) {
            }
            if (k == sInterpViewCount) {
                if (sInterpViewCount == INTERP_MAX_VIEWS) {
                    sInterpBlkView[i] = sInterpViews[0].id; /* (more views than there is room for: with the first) */
                    k = 0;
                } else {
                    memset(&sInterpViews[k], 0, sizeof(sInterpViews[k]));
                    sInterpViews[k].id = sInterpBlkView[i];
                    sInterpViewCount++;
                }
            }
            sInterpViews[k].scenery |= (sInterpKind[i] >> 6) == 2;
        }
    }
    for (k = 0; k < sInterpViewCount; k++) {
        InterpViewState *v = &sInterpViews[k];
        sInterpView = v->id;
        if (first) {
            sInterpWasCut = 0;
            sInterpHaveD = sInterpHaveDt = sInterpRoots = sInterpHaveCam = 0;
            sInterpCamMove = 0.0f;
            v->ok = interp_build_view(t, 1);
            v->cut = sInterpWasCut;
            interp_view_save(v);
        } else if (v->ok) {
            interp_view_load(v);
            interp_build_view(t, 0);
            interp_view_save(v);
        }
        okAny |= v->ok;
        cutAny |= v->cut;
        okScenery |= v->ok && v->scenery;
    }
    sInterpViewLoaded = -1;
    /* a cut where no view with scenery goes on: no in-between picture at all (the shots of a transformation) */
    return okAny && !(cutAny && !okScenery);
}

/* ---- the primitives the game transformed itself (effects, beams, dust, the HUD): Vtx in GS pixels ----
 * They have no matrix to blend, so their vertices are: each triangle of this picture is paired with a triangle of
 * the previous picture that is drawn the same way (texture, pipeline, target) and lies nearest, looked for around
 * the same place in the run of such triangles (particles keep their order while others are born and die). A
 * triangle whose nearest partner is far away keeps this picture's position (a beam's tip, a new spark, a cut).
 * 2D art (sprites with whole-texel coordinates: HUD, text) must also show the same piece of its texture, or a
 * changing text would slide from the old letters' places. */
typedef struct InterpPrim {
    uint32_t sig, first; /* first: index of its first vertex */
    uint32_t kind;       /* sig without the number of triangles: how it is drawn */
    uint32_t view;       /* the view it is drawn in (INTERP_SCISSOR_ID) */
    uint8_t flat2d, tris; /* tris: how many triangles the piece has (a quad: 2) */
} InterpPrim;
#define INTERP_PIECE_MAX 16 /* triangles of one piece (a longer strip is cut into several) */
#define INTERP_MAX_PRIMS (MAX_VERTS / 3)
#define INTERP_WINDOW 48      /* triangles before and after the same place in the run */
#define INTERP_FAR 90.0f      /* GS pixels a vertex may move in a tick and still be blended */
#define INTERP_FAR_2D 5.0f    /* the same for 2D art (HUD, text) */
static Vtx *sInterpPrevVerts, *sInterpBlendVerts;
static InterpPrim *sInterpPrevPrims, *sInterpPrims;
static uint32_t sInterpPrevPrimCount, sInterpPrimCount, sInterpPrevVertCount;
static uint8_t *sInterpPrimUsed;
static unsigned gInterpTris, gInterpTrisBlended, gInterpTrisCamera;

/* Where a vertex the game transformed itself was one picture earlier if the thing it belongs to stood still: back to
   clip space (its q is 1 / w), through the camera's change, and to pixels again. 0 = it cannot be said. */
static int interp_reproject_with(const float *D, const Vtx *v, float *x, float *y, float *z) {
    float w, c[4], p[4];
    int i;

    if (!(v->q > 1e-20f) || !(v->q < 1e20f)) {
        return 0;
    }
    w = 1.0f / v->q;
    c[0] = (v->x + sInterpOffX) * w;
    c[1] = (v->y + sInterpOffY) * w;
    c[2] = v->z * sInterpZMax * w;
    c[3] = w;
    for (i = 0; i < 4; i++) {
        p[i] = D[i] * c[0] + D[4 + i] * c[1] + D[8 + i] * c[2] + D[12 + i] * c[3];
    }
    if (!(p[3] > w * 0.05f)) { /* behind the eye then, or nearly */
        return 0;
    }
    *x = p[0] / p[3] - sInterpOffX;
    *y = p[1] / p[3] - sInterpOffY;
    *z = p[2] / p[3] / sInterpZMax;
    return 1;
}

static int interp_reproject(const Vtx *v, float *x, float *y, float *z) {
    return interp_reproject_with(sInterpD, v, x, y, z);
}

static int sInterpTrust; /* the piece is of a kind known to stand still in the world: no further questions */
/* Moves a piece by the camera's change. 0 = not possible (it keeps this picture's place). */
static int interp_piece_camera(const InterpPrim *c, float t) {
    const Vtx *cv = &gsVerts[c->first];
    Vtx *ov = &sInterpBlendVerts[c->first];
    float x[INTERP_PIECE_MAX * 3], y[INTERP_PIECE_MAX * 3], z[INTERP_PIECE_MAX * 3];
    int k, nv = c->tris * 3;

    if (!sInterpTrust) {   /* Not what faces the screen flat (every corner at the same depth: a banner, a flash, a sprite of the HUD
           drawn with perspective numbers): that is laid onto the screen, not standing in the world. (The "Fight!"
           banner slid down with the camera.) */
        float q0 = cv[0].q, q1 = q0;
        for (k = 1; k < nv; k++) {
            if (cv[k].q < q0) { q0 = cv[k].q; }
            if (cv[k].q > q1) { q1 = cv[k].q; }
        }
        if (!(q1 - q0 > q1 * 1e-4f)) {
            return 0;
        }
    }
    for (k = 0; k < nv; k++) {
        if (!interp_reproject(&cv[k], &x[k], &y[k], &z[k]) || fabsf(x[k] - cv[k].x) > 40.0f + 3.0f * sInterpCamMove || fabsf(y[k] - cv[k].y) > 40.0f + 3.0f * sInterpCamMove) {
            return 0;
        }
    }
    if (!sInterpTrust) {
        /* Is that where such a thing was? The camera's change is in the scenery's projection; a piece drawn through
           another one (what belongs to a fighter: its dust, its aura, the dark sheet of a close-up) comes out
           anywhere, also over the whole screen (seen: a grey veil over one in-between picture). So the previous
           picture must have had a piece drawn the same way roughly where this one is said to have been. (Roughly:
           the sky's panels are cut off at the screen's edge differently in every picture.) */
        float x0 = x[0], x1 = x0, y0 = y[0], y1 = y0, area;
        uint32_t j;
        int found = 0;

        for (k = 1; k < nv; k++) {
            if (x[k] < x0) { x0 = x[k]; } if (x[k] > x1) { x1 = x[k]; }
            if (y[k] < y0) { y0 = y[k]; } if (y[k] > y1) { y1 = y[k]; }
        }
        area = (x1 - x0) * (y1 - y0);
        for (j = 0; j < sInterpPrevPrimCount && !found; j++) {
            const InterpPrim *q = &sInterpPrevPrims[j];
            const Vtx *pv;
            float a0, a1, b0, b1, ix, iy, parea;
            int n2, m;
            if (q->kind != c->kind) {
                continue;
            }
            pv = &sInterpPrevVerts[q->first];
            n2 = q->tris * 3;
            a0 = a1 = pv[0].x;
            b0 = b1 = pv[0].y;
            for (m = 1; m < n2; m++) {
                if (pv[m].x < a0) { a0 = pv[m].x; } if (pv[m].x > a1) { a1 = pv[m].x; }
                if (pv[m].y < b0) { b0 = pv[m].y; } if (pv[m].y > b1) { b1 = pv[m].y; }
            }
            ix = (x1 < a1 ? x1 : a1) - (x0 > a0 ? x0 : a0);
            iy = (y1 < b1 ? y1 : b1) - (y0 > b0 ? y0 : b0);
            parea = (a1 - a0) * (b1 - b0);
            /* the two cover much the same part of the screen (a sheet of dust near the camera overlaps anything) */
            found = ix > 0.0f && iy > 0.0f && ix * iy > 0.35f * (area > parea ? area : parea);
        }
        if (!found) {
            return 0;
        }
    }
    for (k = 0; k < nv; k++) {
        float xt, yt, zt;
        if (sInterpHaveDt && interp_reproject_with(sInterpDt, &cv[k], &xt, &yt, &zt)) {
            ov[k].x = xt; /* where the camera, part of its way, sees the corner */
            ov[k].y = yt;
            ov[k].z = zt;
        } else {
            ov[k].x = x[k] + (cv[k].x - x[k]) * t;
            ov[k].y = y[k] + (cv[k].y - y[k]) * t;
            ov[k].z = z[k] + (cv[k].z - z[k]) * t;
        }
    }
    return 1;
}
static unsigned gInterpWhy[5]; /* no like-drawn triangle before; all too far; 2D with another piece of texture; standing still; (spare) */

static int interp_prim_cmp(const void *a, const void *b) {
    const InterpPrim *x = a, *y = b;
    return x->sig != y->sig ? (x->sig < y->sig ? -1 : 1) : x->first < y->first ? -1 : x->first > y->first;
}

/* The triangles of the recorded frame's own-transformed draws, sorted by how they are drawn and then by order. */
static uint32_t interp_prims(InterpPrim *out) {
    uint32_t n, k, count = 0;

    for (n = 0; n < gsDrawCount; n++) {
        const GsDraw *d = &gsDraws[n];
        uint32_t role;

        /* Not the passes that draw one buffer into another (the glow's and the explosions' smaller copies, the
           blur): their rectangles are in the coordinates of whichever buffer they fill, and a rectangle of one pass
           blended with that of another is a square of garbage. They stay this picture's. */
        if (d->vu != 0 || d->native != 0 || d->count % 3 != 0 || d->tex_is_target) {
            continue;
        }
        /* Which buffer it draws into, in a way that is the same next picture: the game shows its two frame buffers
           in turn, so "the picture's own buffer" is one role; any other buffer is itself. */
        role = d->target >= 0 && d->target < gsTargetCount && GS_FBP_QUADRO >= 0 && gsTargets[d->target].fbp == (uint32_t)GS_FBP_QUADRO
                   ? 0u : 1u + (d->target >= 0 && d->target < gsTargetCount ? gsTargets[d->target].fbp : 0x3FFu);
        /* The unit that is paired and moved is a PIECE: triangles that follow each other and share an edge (the two
           halves of a sprite or of a puff of smoke, a strip). Paired one triangle at a time, the halves of a quad
           found different partners and the quad came apart into wedges (seen in every explosion). */
        for (k = 0; k + 3 <= d->count && count < INTERP_MAX_PRIMS;) {
            uint32_t tris = 1;
            while (k + (tris + 1) * 3 <= d->count && tris < INTERP_PIECE_MAX) {
                const Vtx *a = &gsVerts[d->first + k + (tris - 1) * 3], *b = a + 3;
                int shared = 0, x, y;
                for (x = 0; x < 3; x++) {
                    for (y = 0; y < 3; y++) {
                        if (a[x].x == b[y].x && a[x].y == b[y].y) {
                            shared++;
                            break;
                        }
                    }
                }
                if (shared < 2) {
                    break;
                }
                tris++;
            }
            out[count].view = INTERP_SCISSOR_ID(d);
            out[count].sig = ((uint32_t)d->tex * 2654435761u) ^ ((uint32_t)d->pipeline * 40503u) ^ (role * 0x9E3779B1u) ^ (tris * 0x85EBCA6Bu) ^ (out[count].view * 0xC2B2AE35u);
            out[count].kind = ((uint32_t)d->tex * 2654435761u) ^ ((uint32_t)d->pipeline * 40503u) ^ (role * 0x9E3779B1u) ^ (out[count].view * 0xC2B2AE35u);
            out[count].first = d->first + k;
            out[count].flat2d = d->misc[3] != 0.0f;
            out[count].tris = (uint8_t)tris;
            count++;
            k += tris * 3;
        }
    }
    qsort(out, count, sizeof(InterpPrim), interp_prim_cmp);
    return count;
}

/* Which kinds of piece stand still in the world, asked corner by corner and once a tick: a kind does if most of a
   sample of its corners, taken back by the camera's change, lie on corners of the same kind in the previous picture.
   (Asked piece by piece it missed what is cut into pieces differently in every picture: the sheets of a waterfall,
   long strips that are cut at the screen's edge. Their pieces were paired by nearness with other parts of the
   sheet and the fall was pinched or grew a wedge in the in-between pictures.) */
#define INTERP_MAX_KINDS 512
static struct { uint32_t kind, count, seen, tested, landed; uint8_t flat, isStatic; } sInterpKinds[INTERP_MAX_KINDS];
static uint32_t sInterpKindCount;
static uint32_t *sInterpPrevByKind;

static int interp_bykind_cmp(const void *a, const void *b) {
    uint32_t x = sInterpPrevPrims[*(const uint32_t *)a].kind, y = sInterpPrevPrims[*(const uint32_t *)b].kind;
    return x < y ? -1 : x > y;
}

static int interp_kind_find(uint32_t kind, int add) {
    uint32_t k;

    for (k = 0; k < sInterpKindCount; k++) {
        if (sInterpKinds[k].kind == kind) {
            return (int)k;
        }
    }
    if (!add || sInterpKindCount == INTERP_MAX_KINDS) {
        return -1;
    }
    memset(&sInterpKinds[sInterpKindCount], 0, sizeof(sInterpKinds[0]));
    sInterpKinds[sInterpKindCount].kind = kind;
    return (int)sInterpKindCount++;
}

static void interp_static_kinds(void) {
    uint32_t i, j;

    sInterpKindCount = 0;
    if (sInterpPrevPrimCount == 0) {
        return;
    }
    if (sInterpPrevByKind == NULL) {
        sInterpPrevByKind = malloc(INTERP_MAX_PRIMS * sizeof(uint32_t));
    }
    for (i = 0; i < sInterpPrevPrimCount; i++) {
        sInterpPrevByKind[i] = i;
    }
    qsort(sInterpPrevByKind, sInterpPrevPrimCount, sizeof(uint32_t), interp_bykind_cmp);
    for (i = 0; i < sInterpPrimCount; i++) {
        int e = interp_kind_find(sInterpPrims[i].kind, 1);
        if (e >= 0) {
            sInterpKinds[e].count++;
            sInterpKinds[e].flat |= sInterpPrims[i].flat2d;
        }
    }
    for (i = 0; i < sInterpPrimCount; i++) {
        const InterpPrim *c = &sInterpPrims[i];
        int e = interp_kind_find(c->kind, 0), k, nv = c->tris * 3;
        uint32_t stride, a, b;
        if (e < 0 || sInterpKinds[e].flat) {
            continue;
        }
        if (!interp_view_select(c->view) || !sInterpHaveD) { /* (its view's camera; none, or a cut there: not asked) */
            continue;
        }
        stride = sInterpKinds[e].count / 16 + 1;
        if (sInterpKinds[e].seen++ % stride != 0) {
            continue;
        }
        /* the previous picture's pieces of the kind: a range of the list sorted by kind */
        a = 0;
        b = sInterpPrevPrimCount;
        while (a < b) {
            uint32_t m = (a + b) / 2;
            if (sInterpPrevPrims[sInterpPrevByKind[m]].kind < c->kind) { a = m + 1; } else { b = m; }
        }
        for (k = 0; k < nv; k += 2) { /* (every other corner) */
            const Vtx *cv = &gsVerts[c->first + k];
            float px, py, pz;
            int found = 0;
            if (!interp_reproject(cv, &px, &py, &pz)) {
                continue;
            }
            sInterpKinds[e].tested++;
            for (j = a; j < sInterpPrevPrimCount && !found; j++) {
                const InterpPrim *q = &sInterpPrevPrims[sInterpPrevByKind[j]];
                const Vtx *pv = &sInterpPrevVerts[q->first];
                int m, n2 = q->tris * 3;
                if (q->kind != c->kind) {
                    break;
                }
                for (m = 0; m < n2; m++) {
                    if (fabsf(pv[m].x - px) <= 1.0f && fabsf(pv[m].y - py) <= 1.0f) {
                        found = 1;
                        break;
                    }
                }
            }
            sInterpKinds[e].landed += found;
        }
    }
    for (i = 0; i < sInterpKindCount; i++) {
        sInterpKinds[i].isStatic = sInterpKinds[i].tested >= 2 && sInterpKinds[i].landed * 10 >= sInterpKinds[i].tested * 6;
    }
}

static void interp_build_verts(float t) {
    uint32_t i, lo = 0;
    int staticKind = 0;
    static unsigned forFrame = ~0u;

    if (forFrame != gGsFrame) {
        forFrame = gGsFrame;
        interp_static_kinds();
    }

    memcpy(sInterpBlendVerts, gsVerts, gsVertCount * sizeof(Vtx));
    memset(sInterpPrimUsed, 0, sInterpPrevPrimCount);
    for (i = 0; i < sInterpPrimCount;) {
        uint32_t sig = sInterpPrims[i].sig, end = i, pEnd, r;

        while (end < sInterpPrimCount && sInterpPrims[end].sig == sig) { end++; }
        while (lo < sInterpPrevPrimCount && sInterpPrevPrims[lo].sig < sig) { lo++; }
        pEnd = lo;
        while (pEnd < sInterpPrevPrimCount && sInterpPrevPrims[pEnd].sig == sig) { pEnd++; }
        if (!interp_view_select(sInterpPrims[i].view)) { /* a cut in this piece's view: it stays this picture's */
            i = end;
            lo = pEnd;
            continue;
        }
        gInterpTris += end - i;
        /* Is this kind of piece something that stands still in the world (the translucent wall at the edge of the
           map, drawn as a grid of like tiles; marks on the ground)? Then every piece of it goes by the camera's
           change, exactly. (Paired by nearness, a tile of the wall found its neighbour, and as the tiles shown
           change with where the fighters are, parts of the wall stood a tile off in the in-between pictures: a break
           across it that came and went.) It is known by this: taken back by the camera's change, the pieces land
           on pieces of the previous picture, corner for corner. Only asked while the camera moves: when it stands,
           everything would pass. */
        staticKind = 0;
        if (pEnd > lo && !sInterpPrims[i].flat2d && sInterpHaveD) { /* (also while the camera stands: what lands then did not move) */
            uint32_t stepS = (end - i) / 24 ? (end - i) / 24 : 1, tested = 0, okS = 0, rs, j;
            for (rs = 0; rs < end - i; rs += stepS) {
                const InterpPrim *c = &sInterpPrims[i + rs];
                const Vtx *cv = &gsVerts[c->first];
                float px[INTERP_PIECE_MAX * 3], py[INTERP_PIECE_MAX * 3], pz;
                int k, nv = c->tris * 3, can = 1, found = 0;
                for (k = 0; k < nv && can; k++) {
                    can = interp_reproject(&cv[k], &px[k], &py[k], &pz);
                }
                if (!can) {
                    continue;
                }
                tested++;
                for (j = lo; j < pEnd && !found; j++) {
                    const Vtx *pv = &sInterpPrevVerts[sInterpPrevPrims[j].first];
                    for (k = 0; k < nv; k++) {
                        if (fabsf(pv[k].x - px[k]) > 1.0f || fabsf(pv[k].y - py[k]) > 1.0f) {
                            break;
                        }
                    }
                    found = k == nv;
                }
                okS += found;
            }
            /* (half of them is enough: the tiles of the wall that are shown change from picture to picture, and
               the new ones have nothing to land on) */
            staticKind = okS >= 1 && okS * 2 >= tested;
            if (getenv("BT3_INTERP_DUMP") != NULL && (int)gGsFrame == atoi(getenv("BT3_INTERP_DUMP"))) {
                const InterpPrim *c = &sInterpPrims[i];
                const Vtx *cv = &gsVerts[c->first];
                float bx = 0.0f, by = 0.0f, bz = 0.0f, bd = 1e9f;
                int can = interp_reproject(&cv[0], &bx, &by, &bz);
                for (j = lo; j < pEnd; j++) {
                    const Vtx *pv = &sInterpPrevVerts[sInterpPrevPrims[j].first];
                    float dd = fabsf(pv[0].x - bx) + fabsf(pv[0].y - by);
                    if (dd < bd) { bd = dd; }
                }
                fprintf(stderr, "kind %08x tris %d: %u now, %u before; tested %u, landed %u; first corner %.1f,%.1f q %.5f -> by the camera %.1f,%.1f (%d), nearest first corner before %.1f away\n", c->kind, c->tris, end - i, pEnd - lo, tested, okS,
                        cv[0].x, cv[0].y, cv[0].q, bx, by, can, bd);
            }
        }
        {
            int e = interp_kind_find(sInterpPrims[i].kind, 0);
            int byCorners = e >= 0 && sInterpKinds[e].isStatic;
            if (getenv("BT3_INTERP_DUMP") != NULL && (int)gGsFrame == atoi(getenv("BT3_INTERP_DUMP")) && e >= 0) {
                fprintf(stderr, "  kind %08x by corners: %u pieces, %u corners tested, %u landed -> %d (by pieces %d)\n", sInterpPrims[i].kind, sInterpKinds[e].count, sInterpKinds[e].tested, sInterpKinds[e].landed, byCorners, staticKind);
            }
            if (byCorners && !staticKind) {
                /* all of them, also those with nothing like them before */
                uint32_t r3;
                sInterpTrust = 1;
                for (r3 = 0; r3 < end - i; r3++) {
                    if (interp_piece_camera(&sInterpPrims[i + r3], t)) {
                        gInterpTrisCamera++;
                        gInterpStatic++;
                    }
                }
                sInterpTrust = 0;
                i = end;
                lo = pEnd;
                continue;
            }
        }
        if (pEnd == lo) {
            gInterpWhy[0] += end - i;
            for (r = 0; sInterpHaveD && r < end - i; r++) {
                if (!sInterpPrims[i + r].flat2d && interp_piece_camera(&sInterpPrims[i + r], t)) {
                    gInterpTrisCamera++;
                }
            }
        }
        for (r = 0; pEnd > lo && r < end - i; r++) {
            const InterpPrim *c = &sInterpPrims[i + r];
            const Vtx *cv = &gsVerts[c->first];
            /* the same place in the previous run, counted from the nearer end (births and deaths happen at one) */
            uint32_t n = end - i, m = pEnd - lo;
            uint32_t at = r < n / 2 || n > m + r ? (r < m ? r : m - 1) : m - (n - r);
            uint32_t from = at > INTERP_WINDOW ? at - INTERP_WINDOW : 0, to = at + INTERP_WINDOW + 1 < m ? at + INTERP_WINDOW + 1 : m, j;
            int best = -1, k, sawFar = 0, sawUv = 0, nv = c->tris * 3, big = 0;
            float bestD = 0.0f, ex[INTERP_PIECE_MAX * 3], ey[INTERP_PIECE_MAX * 3];

            if (staticKind) {
                sInterpTrust = 1;
                if (interp_piece_camera(c, t)) {
                    gInterpTrisCamera++;
                    gInterpStatic++;
                }
                sInterpTrust = 0;
                continue;
            }

            /* Big pieces and pieces cut off at the edge of the screen are scenery the game transforms itself (the
               sky's panels): cut differently every picture, so "the nearest like piece" is another panel or another
               cut of it, and panels blended towards different partners leave streaks between them. They stand still
               in the world: the camera's change says exactly where each of their corners was. */
            if (!c->flat2d && sInterpHaveD) {
                float x0 = cv[0].x, x1 = x0, y0 = cv[0].y, y1 = y0;
                for (k = 1; k < nv; k++) {
                    if (cv[k].x < x0) { x0 = cv[k].x; } if (cv[k].x > x1) { x1 = cv[k].x; }
                    if (cv[k].y < y0) { y0 = cv[k].y; } if (cv[k].y > y1) { y1 = cv[k].y; }
                }
                if (x0 < -8.0f || y0 < -8.0f || x1 > 520.0f || y1 > 456.0f || (x1 - x0) * (y1 - y0) > 6000.0f) {
                    int okc = interp_piece_camera(c, t);
                    if (getenv("BT3_INTERP_DUMP") != NULL && (int)gGsFrame == atoi(getenv("BT3_INTERP_DUMP"))) {
                        fprintf(stderr, "big piece kind %08x tris %d at %.0f,%.0f..%.0f,%.0f q %.5f..%.5f: by the camera %d\n", c->kind, c->tris, x0, y0, x1, y1, cv[0].q, cv[nv - 1].q, okc);
                    }
                    if (okc) {
                        gInterpTrisCamera++;
                        continue;
                    }
                }
            }

            /* How far a partner may be: with room for what the camera does to the picture (while it turns fast,
               everything is far on the screen, and nothing was blended). Not by taking the camera's change out of
               each corner: effects that belong to a fighter are in the fighters' projection, which that change does
               not fit. */
            {
                float bx0 = cv[0].x, bx1 = bx0, by0 = cv[0].y, by1 = by0;
                for (k = 0; k < nv; k++) {
                    ex[k] = cv[k].x;
                    ey[k] = cv[k].y;
                    if (cv[k].x < bx0) { bx0 = cv[k].x; } if (cv[k].x > bx1) { bx1 = cv[k].x; }
                    if (cv[k].y < by0) { by0 = cv[k].y; } if (cv[k].y > by1) { by1 = cv[k].y; }
                }
                big = (bx1 - bx0) * (by1 - by0) > 25000.0f;
            }
            for (j = from; j < to; j++) {
                const Vtx *pv;
                float d = 0.0f, far = 0.0f;
                if (sInterpPrimUsed[lo + j]) {
                    continue;
                }
                pv = &sInterpPrevVerts[sInterpPrevPrims[lo + j].first];
                if (!big) { /* (or the partner is such a sheet) */
                    float bx0 = pv[0].x, bx1 = bx0, by0 = pv[0].y, by1 = by0;
                    for (k = 1; k < nv; k++) {
                        if (pv[k].x < bx0) { bx0 = pv[k].x; } if (pv[k].x > bx1) { bx1 = pv[k].x; }
                        if (pv[k].y < by0) { by0 = pv[k].y; } if (pv[k].y > by1) { by1 = pv[k].y; }
                    }
                    if ((bx1 - bx0) * (by1 - by0) > 25000.0f) {
                        continue;
                    }
                }
                for (k = 0; k < nv; k++) {
                    float dx = fabsf(pv[k].x - ex[k]), dy = fabsf(pv[k].y - ey[k]);
                    d += dx + dy;
                    if (dx > far) { far = dx; }
                    if (dy > far) { far = dy; }
                }
                /* 2D art moves only a little or not at all: the digits of a number that changes are the same few
                   glyphs in new places, and the nearest "same glyph" is a different digit of the old number (the
                   damage counter lost digits in the in-between pictures) */
                /* A piece that covers a large part of the screen (a sheet of dust or a flash near the camera) is
                   blended only if it hardly moved: half way between two such sheets is a veil over everything. */
                if (far > (c->flat2d ? INTERP_FAR_2D : big ? 12.0f : INTERP_FAR + 1.5f * sInterpCamMove)) {
                    sawFar = 1;
                    continue;
                }
                if (c->flat2d && (fabsf(pv[0].s - cv[0].s) > 0.01f || fabsf(pv[0].t - cv[0].t) > 0.01f || fabsf(pv[2].s - cv[2].s) > 0.01f ||
                                  fabsf(pv[2].t - cv[2].t) > 0.01f)) {
                    sawUv = 1;
                    continue;
                }
                if (best < 0 || d < bestD) {
                    best = (int)j;
                    bestD = d;

                }
            }
            if (best >= 0 && bestD > 4.0f * (float)nv) {
                /* The partner must want this piece too: if another piece of this picture lies clearly nearer to it,
                   it is that one's, and this piece has none (a tile of the sea that came into view, or one cut off
                   at the screen's edge into another number of triangles: paired with a tile some way off, it stood
                   half way to it for one picture, a blue square on the beach). */
                const Vtx *pv = &sInterpPrevVerts[sInterpPrevPrims[lo + best].first];
                uint32_t n = end - i, r0 = r > INTERP_WINDOW ? r - INTERP_WINDOW : 0, r1 = r + INTERP_WINDOW + 1 < n ? r + INTERP_WINDOW + 1 : n, r2;
                for (r2 = r0; r2 < r1; r2++) {
                    const Vtx *ov2 = &gsVerts[sInterpPrims[i + r2].first];
                    float d2 = 0.0f;
                    if (r2 == r) {
                        continue;
                    }
                    for (k = 0; k < nv && d2 < bestD * 0.6f; k++) {
                        d2 += fabsf(pv[k].x - ov2[k].x) + fabsf(pv[k].y - ov2[k].y);
                    }
                    if (d2 < bestD * 0.6f) {
                        best = -1;
                        gInterpNotMutual++;
                        break;
                    }
                }
            }
            if (best >= 0) {
                const Vtx *pv = &sInterpPrevVerts[sInterpPrevPrims[lo + best].first];
                Vtx *ov = &sInterpBlendVerts[c->first];
                sInterpPrimUsed[lo + best] = 1;
                if (bestD != 0.0f && getenv("BT3_INTERP_DUMP") != NULL && (int)gGsFrame == atoi(getenv("BT3_INTERP_DUMP"))) {
                    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f, px0 = 1e9f, px1 = -1e9f, py0 = 1e9f, py1 = -1e9f;
                    for (k = 0; k < nv; k++) {
                        if (cv[k].x < x0) { x0 = cv[k].x; } if (cv[k].x > x1) { x1 = cv[k].x; }
                        if (cv[k].y < y0) { y0 = cv[k].y; } if (cv[k].y > y1) { y1 = cv[k].y; }
                        if (pv[k].x < px0) { px0 = pv[k].x; } if (pv[k].x > px1) { px1 = pv[k].x; }
                        if (pv[k].y < py0) { py0 = pv[k].y; } if (pv[k].y > py1) { py1 = pv[k].y; }
                    }
                    fprintf(stderr, "piece sig %08x tris %d flat %d rank %u/%u<-%u/%u now %.0f,%.0f..%.0f,%.0f before %.0f,%.0f..%.0f,%.0f d %.0f\n", c->sig, c->tris, c->flat2d,
                            r, end - i, (unsigned)best, pEnd - lo, x0, y0, x1, y1, px0, py0, px1, py1, bestD);
                }
                bestD = 0.0f;
                for (k = 0; k < nv; k++) {
                    bestD += fabsf(pv[k].x - cv[k].x) + fabsf(pv[k].y - cv[k].y);
                }
                if (bestD != 0.0f) {
                    for (k = 0; k < nv; k++) {
                        ov[k].x = pv[k].x + (cv[k].x - pv[k].x) * t;
                        ov[k].y = pv[k].y + (cv[k].y - pv[k].y) * t;
                        ov[k].z = pv[k].z + (cv[k].z - pv[k].z) * t;
                        /* (the texture coordinates stay this picture's: a puff of an explosion shows another cell of
                           its animated texture every picture, and half way between two cells is a square of both) */
                    }
                    gInterpTrisBlended++;
                } else {
                    gInterpWhy[3]++;
                }
            } else {
                gInterpWhy[sawUv ? 2 : sawFar ? 1 : 4]++;
                /* no partner (new, or too fast): at least the camera's change is known */
                if (!c->flat2d && sInterpHaveD && interp_piece_camera(c, t)) {
                    gInterpTrisCamera++;
                }
            }
        }
        i = end;
        lo = pEnd;
    }
}

static void interp_remember(void) {
    {
        InterpPrim *swap = sInterpPrevPrims;
        sInterpPrevPrims = sInterpPrims;
        sInterpPrims = swap;
        sInterpPrevPrimCount = sInterpPrimCount;
        memcpy(sInterpPrevVerts, gsVerts, gsVertCount * sizeof(Vtx));
        sInterpPrevVertCount = gsVertCount;
    }
    memcpy(sInterpPrev, gsVuUni, gsVuUniCount * sizeof(Vu0Uniform));
    memcpy(sInterpPrevSig, sInterpSig, gsVuUniCount * sizeof(uint32_t));
    memcpy(sInterpPrevKind, sInterpKind, gsVuUniCount * sizeof(uint16_t));
    memcpy(sInterpPrevBlkView, sInterpBlkView, gsVuUniCount * sizeof(uint32_t));
    memcpy(sInterpPrevRep, sInterpRep, gsVuUniCount * sizeof(sInterpRep[0]));
    sInterpPrevCount = gsVuUniCount;
}

/* BT3_PRESENT_LOG=<file>: one line per picture shown: time in microseconds, kind (B in-between, R the real one
   after it, S a picture shown alone), the vertical blank count. For measuring how evenly the pictures come. */
static void interp_present_log(char kind) {
    static FILE *fp;
    static int tried;
    extern unsigned gPortVBlanks;

    if (!tried) {
        tried = 1;
        fp = getenv("BT3_PRESENT_LOG") != NULL ? fopen(getenv("BT3_PRESENT_LOG"), "w") : NULL;
    }
    if (fp != NULL) {
        fprintf(fp, "%llu %c %u\n", (unsigned long long)(gpu_now() / 1000ull), kind, gPortVBlanks);
    }
}

/* How many pictures a tick is shown as: 1 = the game's own (30 a second), 2 = one in-between picture (60), 4 = three
   (120), 8 = seven (240). The setting "interp" is 0..3. */
static int interp_steps(void) {
    return gsInterp <= 0 ? 1 : gsInterp == 1 ? 2 : gsInterp == 2 ? 4 : 8;
}

static void interp_counts_restore(void) {
    int i;

    gsVertCount = sInterpSaved.verts;
    gsVuVertCount = sInterpSaved.vuVerts;
    gsVuIdxCount = sInterpSaved.vuIdx;
    gsVuUniCount = sInterpSaved.vuUni;
    gsDrawCount = sInterpSaved.draws;
    gsAnchor = sInterpSaved.anchor;
    gsSkipped = sInterpSaved.skipped;
    gsNative = sInterpSaved.native;
    for (i = 0; i < gsTargetCount && i < MAX_TARGETS; i++) {
        gsTargets[i].draws = sInterpSaved.tdraws[i];
    }
}

/* Shows picture `step` of the tick's sInterpSteps: the list replayed `step / steps` of the way from the previous
   tick; the last one is the tick itself. The recorded list has to be in place (interp_counts_restore). */
static void interp_show(int step) {
    if (step < sInterpSteps) {
        Vu0Uniform *real = gsVuUni;
        Vtx *realVerts = gsVerts;
        float t = getenv("BT3_INTERP_T") != NULL ? (float)atof(getenv("BT3_INTERP_T")) : (float)step / (float)sInterpSteps;

        if (step > 1) {
            interp_build(t, 0); /* (the first was built to decide whether the tick can be blended at all) */
        }
        if (getenv("BT3_INTERP_STEPLOG") != NULL) {
            fprintf(stderr, "step %d of %d, t %.3f, frame %u\n", step, sInterpSteps, t, gGsFrame);
        }
        if (getenv("BT3_INTERP_3D") == NULL) { /* (BT3_INTERP_3D=1: the vertex programs' draws only, as the first prototype) */
            interp_build_verts(t);
            gsVerts = sInterpBlendVerts;
        }
        gsVuUni = sInterpBlend;
        BACKEND_FRAME_END();
        gsVuUni = real;
        gsVerts = realVerts;
        interp_present_log('B');
        gInterpShown++;
    } else {
        interp_remember(); /* (only now: the in-between pictures were built from the previous tick's copy) */
        BACKEND_FRAME_END();
        interp_present_log('R');
        scale_apply();
    }
}

/* The tick's pictures after the first. They are due at even distances over the tick (33.4 ms / steps), counted from
   when the first was shown: that is when the game had finished computing the tick, a millisecond or two after a
   vertical blank, so "at the next blank" came too early (measured with one in-between picture: 15 ms after it, then
   18 ms to the next). Called from Port_VBlank before it waits for the blank (`ahead` = what may be waited for here:
   the pictures that fall before the blank) and after (those at it).
   all = 1: the game is back with a new tick: what is left is dropped, except the tick's real picture. */
void GsGpu_InterpPump(int ahead_ms, int all) {
    if (sInterpNext == 0 || sBackend == NULL) {
        return;
    }
    if (gsDrawCount != 0 || gsVertCount != 0 || gsVuUniCount != 0) {
        /* the game drew before the tick's pictures were shown: its new draws would be mixed into the kept list */
        static int said;
        if (!said) {
            said = 1;
            fprintf(stderr, "bt3: interp: draws recorded before the tick's pictures were shown; they are dropped\n");
        }
        sInterpNext = 0;
        return;
    }
    while (sInterpNext != 0) {
        uint64_t due = sInterpBlendShown + (uint64_t)(sInterpNext - 1) * (sInterpTickNs / (uint64_t)sInterpSteps), now = gpu_now();

        due = due > sInterpBlendTook ? due - sInterpBlendTook : 0; /* (replaying the list takes that long again) */
        if (all) {
            sInterpNext = sInterpSteps;
        } else if (getenv("BT3_UNCAPPED") == NULL) {
            if (ahead_ms < 0) {
                /* before a vertical blank: only a picture that is shown and done with before the blank is due,
                   so that the blank (the game's clock: input, sound) is never made late */
                extern int Port_VBlankUsLeft(void);
                uint64_t left = (uint64_t)Port_VBlankUsLeft() * 1000ull, wait = now < due ? due - now : 0;
                if (wait + sInterpBlendTook + 700000ull > left) {
                    return;
                }
            } else if (now < due && due - now > (uint64_t)ahead_ms * 1000000ull) {
                return; /* not yet: at the next blank */
            }
            if (now < due) {
                SDL_DelayPrecise(due - now);
            }
        }
        if (sInterpNext == sInterpSteps && getenv("BT3_INTERP_ONLY") != NULL) {
            sInterpNext = 0; /* testing: only in-between pictures are shown (so a screenshot holds one) */
            interp_counts_restore();
            interp_remember();
            gsVertCount = gsVuVertCount = gsVuIdxCount = gsVuUniCount = gsDrawCount = 0;
            return;
        }
        interp_counts_restore();
        {
            int step = sInterpNext;
            sInterpNext = step == sInterpSteps ? 0 : step + 1;
            interp_show(step);
        }
    }
}

void GsGpu_InterpFlush(void) { /* (the name Port_VBlank knew it by) */
    GsGpu_InterpPump(6, 0);
}

#ifdef BT3_GS_ESPERAS
/* (os acentos das mensagens vão por escape octal: o c89.py conta as posições do AST em caracteres e o clang em bytes, e
   um literal com acento desloca toda reescrita que vem depois no mesmo fonte; guia, seção 6) */
/* ------------------------------------------------------------------ (F5, etapa 8, degrau 2, etapa 3) a divisão
   BT3_GS_DESENHO=1 (só com BT3_GS_THREAD=1; recusada com BT3_INTERP): o frameEnd do backend sai
   da thread de gravação para uma terceira thread. Na entrega (desenho_entrega, no lugar do frameEnd), a gravação espera o
   desenho largar o retrato anterior, passa ao desenho o jogo de listas em que gravou o quadro, sem cópia, copia os campos
   pequenos do quadro para o retrato, liga o seu cleared, zera os contadores e segue com o quadro seguinte no outro jogo
   (sJogos, mais acima). Um quadro em voo só: o jogo para onde a gravação passa é o do retrato anterior, que o desenho
   acabou de largar. Os pedidos de soltar (texturas, a vaga de um alvo) vão numa fila que viaja com o retrato
   do quadro em que foram feitos: o desenho solta depois de ter desenhado esse quadro (o risco 4 do inventário,
   08-degrau2.md). O sPipes do backend não se copia: só cresce, e a entrega é a barreira.
   No 360 (etapa 4) a thread de desenho é a dona do dispositivo: o init do backend roda nela (a gravação espera), ociosa ela
   atende o pedido de memória da gravação (GsD3d_Ocioso) e, sem retrato há 50 ms, repete o último quadro (GsD3d_Repete, a
   Guide), e ao sair solta o dispositivo (GsD3d_Soltar). Enquanto a gravação monta o retrato (sEntregando) o desenho não
   repete: a entrega mexe no estado do desenho do backend, com ele parado. */
#ifdef PORT_X360
extern double gGsVu1Ms; /* gs_vu1.c (só no 360) */
#define VU1_MS gGsVu1Ms
#else
static double sVu1MsZero; /* (fora do 360 o gs_vu1.c não mede) */
#define VU1_MS sVu1MsZero
#endif
static GsQuadro sQuadroVivo;
const GsQuadro *gsQuadro = &sQuadroVivo;
static int sDividido;
static GsQuadro sRetrato;
static GsTarget sRetAlvos[MAX_TARGETS];
static GsSolta *sSoltas[2]; /* uma lista enche na gravação, a outra viaja com o retrato */
static uint32_t sSoltaN[2], sSoltaCap[2];
static int sSoltaPend;
static pthread_t sDesenho;
static pthread_mutex_t sDesLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sDesAcorda = PTHREAD_COND_INITIALIZER, sDesFeito = PTHREAD_COND_INITIALIZER;
static int sDesTem, sDesPara; /* sDesTem: 1 um retrato; no 360 também 2 repetindo o último quadro, 3 o init do backend */
#ifdef PORT_X360
static int sEntregando, sDesInitRes;
static volatile int sDesParaJa, sDesParado; /* a parada sem esperar (uma queda); a thread de desenho já saiu */
volatile int gGsDesenhoEstado; /* (o vigia) 0 sem a thread de desenho, 1 ociosa, 2 desenhando, 3 repetindo o último quadro, 4 no init */
extern void Port_GsDesenhoPartida(void); /* main360.cpp: processador e identidade da thread */
extern int GsD3d_Repete(void);           /* gs_d3d.cpp */
extern void GsD3d_Ocioso(void);          /* gs_d3d.cpp: o pedido de memória da gravação */
extern void GsD3d_Soltar(void);          /* gs_d3d.cpp: o dispositivo volta para a main */
extern int Port_X360CondEsperaMs(pthread_cond_t *c, pthread_mutex_t *m, unsigned ms); /* posix360.c: 1 se o tempo acabou */
#endif
static unsigned sEntregas, sSoltasPedidas;
static uint32_t sMaior[5]; /* desenhos, vértices de CPU, da VU, índices, constantes: o maior quadro entregue */
static uint64_t sCopiaNs, sCopiaPior;
#ifdef BT3_GPU_FALSO
static int sDesAtrasoUs = -1; /* BT3_GS_DESENHO_ATRASO_US: o desenho demora de propósito (teste, só nas builds --gpu) */
#endif

static void divisao_log(const char *msg) {
#ifdef PORT_X360
    extern void Diario_Escrever(const char *formato, ...);
    Diario_Escrever("gs: %s", msg);
#else
    fprintf(stderr, "gs: %s\n", msg);
#endif
}

/* sem a divisão: o quadro que o frameEnd lê são as listas vivas, com as contagens de agora */
static void backend_frame_end(void) {
    GsQuadro *q = &sQuadroVivo;
    q->draws = gsDraws;
    q->drawCount = gsDrawCount;
    q->verts = gsVerts;
    q->vertCount = gsVertCount;
    q->vuVerts = gsVuVerts;
    q->vuVertCount = gsVuVertCount;
    q->vuIdx = gsVuIdx;
    q->vuIdxCount = gsVuIdxCount;
    q->vuFaixa = sVuFaixa;
    q->vuUni = gsVuUni;
    q->vuUniCount = gsVuUniCount;
    q->targets = gsTargets;
    q->targetCount = gsTargetCount;
    q->quadro = gGsFrame;
    q->frame[0] = gGs.frame[0];
    q->frame[1] = gGs.frame[1];
    q->fbp = GS_FBP_QUADRO;
    q->anchor = gsAnchor;
    q->texCount = gsTexCount;
    q->skipped = gsSkipped;
    q->native = gsNative;
    q->vu1Ms = VU1_MS;
    q->soltas = NULL;
    q->soltaCount = 0;
    q->vivo = 1;
    q->dividido = 0;
    gsQuadro = q;
    sBackend->frameEnd();
}

static void solta_pede(GsTex tex, int alvo) {
    GsSolta *s;
    if (!sDividido) {
        if (alvo < 0) {
            sBackend->texDestroy(tex);
        } else {
            sBackend->targetDrop(alvo);
        }
        return;
    }
    if (alvo >= 0) {
        tex = sBackend->targetColor(alvo);
    }
    sBackend->soltaPede(tex, alvo);
    sSoltasPedidas++;
    if (sSoltaN[sSoltaPend] == sSoltaCap[sSoltaPend]) {
        uint32_t cap = sSoltaCap[sSoltaPend] != 0 ? sSoltaCap[sSoltaPend] * 2 : 1024;
        GsSolta *n = (GsSolta *)realloc(sSoltas[sSoltaPend], cap * sizeof(GsSolta));
        if (n == NULL) { /* sem memória para a fila: o desenho acaba o retrato em andamento (o único que pode usar o que
                            se solta: o quadro em gravação nunca pede o que ele mesmo usa) e solta-se já */
            GsDraw_DesenhoEspera();
            sBackend->solta(tex, alvo);
            return;
        }
        sSoltas[sSoltaPend] = n;
        sSoltaCap[sSoltaPend] = cap;
    }
    s = &sSoltas[sSoltaPend][sSoltaN[sSoltaPend]++];
    s->tex = tex;
    s->alvo = alvo;
    s->quadro = gGsFrame;
}

static void *desenho_thread(void *arg) {
    uint32_t i;
    (void)arg;
#ifdef PORT_X360
    Port_GsDesenhoPartida();
#endif
    pthread_mutex_lock(&sDesLock);
    for (;;) {
#ifdef PORT_X360
        while (!sDesTem && !sDesPara && !sDesParaJa) {
            int repete;
            gGsDesenhoEstado = 1;
            Port_X360CondEsperaMs(&sDesAcorda, &sDesLock, 50);
            if (sDesTem || sDesPara || sDesParaJa) {
                continue;
            }
            repete = !sEntregando; /* (a entrega mexe no estado do desenho; o pedido de memória não) */
            if (repete) {
                sDesTem = 2;
            }
            pthread_mutex_unlock(&sDesLock);
            GsD3d_Ocioso();
            if (repete) {
                gGsDesenhoEstado = 3;
                GsD3d_Repete(); /* (só se passou de 50 ms sem apresentar) */
            }
            pthread_mutex_lock(&sDesLock);
            if (repete) {
                sDesTem = 0;
                pthread_cond_broadcast(&sDesFeito);
            }
        }
        if (sDesParaJa) {
            break;
        }
#else
        while (!sDesTem && !sDesPara) {
            pthread_cond_wait(&sDesAcorda, &sDesLock);
        }
#endif
        if (!sDesTem) {
            break;
        }
#ifdef PORT_X360
        if (sDesTem == 3) {
            gGsDesenhoEstado = 4;
            pthread_mutex_unlock(&sDesLock);
            sDesInitRes = sBackend->init();
            pthread_mutex_lock(&sDesLock);
            sDesTem = 0;
            pthread_cond_broadcast(&sDesFeito);
            continue;
        }
        gGsDesenhoEstado = 2;
#endif
        pthread_mutex_unlock(&sDesLock);
#ifdef BT3_GPU_FALSO
        if (sDesAtrasoUs > 0) { /* antes de desenhar: a gravação do quadro seguinte corre junto */
            struct timespec ts;
            ts.tv_sec = sDesAtrasoUs / 1000000;
            ts.tv_nsec = (long)(sDesAtrasoUs % 1000000) * 1000L;
            nanosleep(&ts, NULL);
        }
#endif
        gsQuadro = &sRetrato;
        sBackend->frameEnd();
        for (i = 0; i < sRetrato.soltaCount; i++) {
            sBackend->solta(sRetrato.soltas[i].tex, sRetrato.soltas[i].alvo);
        }
#ifdef PORT_X360
        GsD3d_Ocioso(); /* (o que a gravação espera pode ter saído com este quadro) */
#endif
        pthread_mutex_lock(&sDesLock);
        sDesTem = 0;
        pthread_cond_broadcast(&sDesFeito);
    }
#ifdef PORT_X360
    sDesTem = 0; /* (na parada sem esperar, o retrato pendente fica sem desenho) */
    sDesParado = 1;
    gGsDesenhoEstado = 0;
    pthread_cond_broadcast(&sDesFeito);
    pthread_mutex_unlock(&sDesLock);
    GsD3d_Soltar(); /* o dispositivo volta para quem for usá-lo (a main) */
    return NULL;
#else
    pthread_mutex_unlock(&sDesLock);
    return NULL;
#endif
}

void GsDraw_DesenhoEspera(void) {
    if (!sDividido) {
        return;
    }
    pthread_mutex_lock(&sDesLock);
#ifdef PORT_X360
    while (sDesTem && !sDesParado) {
#else
    while (sDesTem) {
#endif
        pthread_cond_wait(&sDesFeito, &sDesLock);
    }
    pthread_mutex_unlock(&sDesLock);
}

#ifdef PORT_X360
/* (degrau 2, etapa 4) para o gs_d3d.cpp e a main360.cpp */
int GsDraw_Dividido(void) {
    return sDividido;
}

/* acorda o desenho ocioso (o pedido de memória da gravação) */
void GsDraw_DesenhoAcorda(void) {
    pthread_cond_signal(&sDesAcorda);
}

/* uma queda: o desenho sai sem esperar (sem desenhar o retrato pendente) e solta o dispositivo */
void GsDraw_DesenhoPararJa(void) {
    sDesParaJa = 1;
    pthread_cond_signal(&sDesAcorda);
}

/* a própria thread de desenho caiu (o AoMorrer dela): quem espera por ela segue, e o que vier depois se perde */
void GsDraw_DesenhoCaiu(void) {
    sDesParado = 1;
    sDesTem = 0;
    gGsDesenhoEstado = 0;
    pthread_cond_broadcast(&sDesFeito);
}

/* o init do backend na thread de desenho, a dona do dispositivo; a gravação espera */
static int desenho_init(void) {
    pthread_mutex_lock(&sDesLock);
    while (sDesTem && !sDesParado) { /* (o desenho ocioso pode estar no meio de uma volta: sDesTem 2) */
        pthread_cond_wait(&sDesFeito, &sDesLock);
    }
    sDesTem = 3;
    pthread_cond_signal(&sDesAcorda);
    while (sDesTem && !sDesParado) {
        pthread_cond_wait(&sDesFeito, &sDesLock);
    }
    pthread_mutex_unlock(&sDesLock);
    return sDesParado ? 0 : sDesInitRes;
}

/* a thread de desenho já saiu: o quadro se perde (a gravação não é dona do dispositivo) */
static void desenho_descarta(void) {
    int k;
    for (k = 0; k < gsTargetCount; k++) {
        gsTargets[k].draws = 0;
    }
    gsVertCount = 0;
    gsVuVertCount = 0;
    gsVuIdxCount = 0;
    gsVuUniCount = 0;
    gsDrawCount = 0;
    gsAnchor = 0;
    gsSkipped = 0;
    gsNative = 0;
}
#endif

/* (o falso o chama no resumo: o tamanho do maior quadro, quantos não couberam, o tempo da cópia) */
void GsDraw_DesenhoResumo(void) {
    if (!sDividido) {
        return;
    }
#ifdef PORT_X360
    {
        extern void Diario_Escrever(const char *formato, ...);
        Diario_Escrever("gs: divis\303\243o: %u retratos (os dois jogos de listas, sem c\303\263pia), %u pedidos de soltar; maior quadro %u "
                        "desenhos, %u v\303\251rtices, %u da VU, %u \303\255ndices, %u constantes; entrega m\303\251dia %.3f ms, pior %.3f ms",
                        sEntregas, sSoltasPedidas, sMaior[0], sMaior[1], sMaior[2], sMaior[3], sMaior[4],
                        sEntregas ? (double)sCopiaNs / sEntregas / 1e6 : 0.0, (double)sCopiaPior / 1e6);
        sCopiaPior = 0; /* (o pior é do trecho) */
        return;
    }
#endif
    fprintf(stderr, "gs: divis\303\243o: %u retratos (os dois jogos de listas, sem c\303\263pia), %u pedidos de soltar; maior quadro %u desenhos, %u "
                    "v\303\251rtices, %u da VU, %u \303\255ndices, %u constantes; entrega m\303\251dia %.3f ms, pior %.3f ms\n",
            sEntregas, sSoltasPedidas, sMaior[0], sMaior[1], sMaior[2], sMaior[3], sMaior[4],
            sEntregas ? (double)sCopiaNs / sEntregas / 1e6 : 0.0, (double)sCopiaPior / 1e6);
}

static void desenho_parar(void) {
    GsDraw_DesenhoEspera();
    pthread_mutex_lock(&sDesLock);
    sDesPara = 1;
    pthread_cond_signal(&sDesAcorda);
    pthread_mutex_unlock(&sDesLock);
    pthread_join(sDesenho, NULL);
#ifdef PORT_X360
    sDesParado = 1; /* (a gravação não volta a ser dona do dispositivo: o que vier depois se perde) */
#else
    sDividido = 0;
#endif
}

static void divisao_decide(void) {
    const char *e = getenv("BT3_GS_DESENHO");
    int interp, r;
    if (e == NULL || strcmp(e, "0") == 0) {
        return;
    }
    if (!Port_GsComThread()) {
        divisao_log("BT3_GS_DESENHO=1 sem BT3_GS_THREAD=1: ignorado, tudo na thread de grava\303\247\303\243o");
        return;
    }
    interp = getenv("BT3_INTERP") != NULL ? atoi(getenv("BT3_INTERP")) : Port_Setting("interp", 0);
    if (interp > 0) {
        divisao_log("BT3_GS_DESENHO=1 recusado com BT3_INTERP (o frameEnd roda v\303\241rias vezes por quadro trocando as listas)");
        return;
    }
    if (sBackend->soltaPede == NULL || sBackend->solta == NULL) {
        divisao_log("BT3_GS_DESENHO=1 recusado: o backend n\303\243o tem a fila de libera\303\247\303\243o");
        return;
    }
    if (sJogos[0].draws == NULL || !jogo_aloca(&sJogos[1])) {
        divisao_log("BT3_GS_DESENHO=1 recusado: sem mem\303\263ria para o segundo jogo de listas");
        return;
    }
#ifdef BT3_GPU_FALSO
    sDesAtrasoUs = getenv("BT3_GS_DESENHO_ATRASO_US") != NULL ? atoi(getenv("BT3_GS_DESENHO_ATRASO_US")) : 0;
#endif
#ifdef PORT_X360
    {
        pthread_attr_t at;
        pthread_attr_init(&at);
        pthread_attr_setstacksize(&at, 4u * 1024u * 1024u); /* (o frame_end do 360 rodava na de gravação, com 4 MB) */
        r = pthread_create(&sDesenho, &at, desenho_thread, NULL);
    }
#else
    r = pthread_create(&sDesenho, NULL, desenho_thread, NULL);
#endif
    if (r != 0) {
        divisao_log("BT3_GS_DESENHO=1 recusado: a thread de desenho n\303\243o partiu");
        free(sJogos[1].verts);
        free(sJogos[1].vuVerts);
        free(sJogos[1].vuIdx);
        free(sJogos[1].vuUni);
        free(sJogos[1].draws);
        free(sJogos[1].vuFaixa);
        memset(&sJogos[1], 0, sizeof(sJogos[1]));
        return;
    }
    sDividido = 1;
    gsQuadro = &sRetrato;
    gGsDesenhoParar = desenho_parar;
#ifdef PORT_X360
    {
        char m[160];
        sprintf(m, "BT3_GS_DESENHO=1: o desenho numa terceira thread, dois jogos de listas de %u KB", JOGO_BYTES >> 10);
        divisao_log(m);
    }
#else
    fprintf(stderr, "gs: BT3_GS_DESENHO=1: o desenho numa terceira thread, dois jogos de listas de %u KB\n", JOGO_BYTES >> 10);
#endif
}

/* (F5, etapa 8, gravação) A cada 60 fins de quadro, na thread de gravação: o tempo por família desde o trecho anterior
   (gs_internal.h), em ms por quadro, e quantas vezes cada uma rodou. Sai também dentro da janela do passos (o log de 60
   quadros do gs_d3d fica mudo nela, e o diário do 360 grava fora da thread); "N na janela" diz quantos dos quadros do
   trecho a janela cobriu. A parede é o período da gravação; "fora delas" é a parede menos a soma (o hash de dentro da VU1 só uma vez):
   andar a cadeia e o VIF, o GIF, os desenhos de CPU e, sem a divisão, o desenho. Fora do 360, a CPU da própria thread. */
int Passos_Medindo(void); /* port/x360/headless/passos.c */
static void Gs_FamTrecho(void) {
    static uint64_t desde;
    static unsigned quadros, naJanela;
#ifndef PORT_X360
    static uint64_t cpuDesde;
#else
    static uint64_t tbDesde; /* o mftb no começo do trecho: a calibração do relógio das famílias */
#endif
    double ms[GS_FAM_N], soma, parede, cpu = -1.0;
    uint64_t agora;
    int k;
    char linha[700];

    if (quadros == 0 && desde == 0) { /* o primeiro fim de quadro: liga e começa o trecho */
        const char *e = getenv("BT3_TEMPOS");
#ifdef PORT_X360
        gGsFam = e == NULL || atoi(e) != 0;
#else
        gGsFam = e != NULL && atoi(e) != 0; /* (fora do 360 só com BT3_TEMPOS=1: gs_internal.h) */
#endif
        if (!gGsFam) {
            divisao_log("grava\303\247\303\243o por fam\303\255lia: desligada (BT3_TEMPOS)");
        }
        desde = gpu_now();
#ifdef PORT_X360
        tbDesde = GS_FAM_TIQUE();
#else
        {
            struct timespec ts;
            clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
            cpuDesde = (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
        }
#endif
        return;
    }
    if (!gGsFam) {
        return;
    }
    naJanela += Passos_Medindo() != 0;
    if (++quadros < 60) {
        return;
    }
    agora = gpu_now();
    parede = (double)(agora - desde) / 1e6 / quadros;
#ifdef PORT_X360
    {
        uint64_t tb = GS_FAM_TIQUE();
        if (agora > desde) {
            gGsFamTbMs = (double)(tb - tbDesde) / ((double)(agora - desde) / 1e6);
        }
        tbDesde = tb;
    }
#endif
    for (k = 0; k < GS_FAM_N; k++) {
        ms[k] = Gs_FamMs(gGsFamTiques[k]) / quadros;
    }
    soma = ms[GS_FAM_ENVIO] + ms[GS_FAM_HASH] - ms[GS_FAM_HASH_VU1] + ms[GS_FAM_VU1] + ms[GS_FAM_UNPACK] + ms[GS_FAM_ESPERA] +
           ms[GS_FAM_COPIA] + ms[GS_FAM_BACKEND];
#ifndef PORT_X360
    {
        struct timespec ts;
        uint64_t c;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
        c = (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
        cpu = (double)(c - cpuDesde) / 1e6 / quadros;
        cpuDesde = c;
    }
#endif
    snprintf(linha, sizeof(linha),
             "grava\303\247\303\243o por fam\303\255lia, quadro %u (%u quadros, %u na janela): parede %.2f ms por quadro; envio %.2f "
             "(%.0f vezes, %.0f KB), hash %.2f (%.0f rec\303\241lculos; %.2f dentro da VU1), VU1 %.2f (%.0f lotes), Unpack %.2f (%.0f), "
             "entrega: espera %.2f, c\303\263pia %.2f, backend %.2f; soma %.2f, fora delas %.2f; CPU da thread %.2f",
             gGsFrame, quadros, naJanela, parede, ms[GS_FAM_ENVIO], (double)gGsFamVezes[GS_FAM_ENVIO] / quadros,
             (double)gGsFamBytes / 1024.0 / quadros, ms[GS_FAM_HASH], (double)gGsFamVezes[GS_FAM_HASH] / quadros,
             ms[GS_FAM_HASH_VU1], ms[GS_FAM_VU1], (double)gGsFamVezes[GS_FAM_VU1] / quadros, ms[GS_FAM_UNPACK],
             (double)gGsFamVezes[GS_FAM_UNPACK] / quadros, ms[GS_FAM_ESPERA], ms[GS_FAM_COPIA], ms[GS_FAM_BACKEND], soma,
             parede - soma, cpu);
    divisao_log(linha);
    /* a linha irmã: o gancho do backend por dentro (o resto é o gancho menos as três partes: o grav_junta, o last do alvo
       mostrado, o sincroniza_alvos e a fila das profundidades) e o Unpack por caminho, com as vezes e os vetores */
    snprintf(linha, sizeof(linha),
             "grava\303\247\303\243o por fam\303\255lia, por dentro, quadro %u: backend %.2f: plano da VU1 %.2f, alvos e "
             "profundidades %.2f, mem\303\263ria %.2f, o resto %.2f; Unpack %.2f: c\303\263pia V4-32 %.2f (%.0f vezes, %.0f "
             "vetores), r\303\241pido %.2f (%.0f vezes, %.0f vetores), geral %.2f (%.0f vezes, %.0f vetores)",
             gGsFrame, ms[GS_FAM_BACKEND], ms[GS_FAM_E_PLANO], ms[GS_FAM_E_ALVOS], ms[GS_FAM_E_MEM],
             ms[GS_FAM_BACKEND] - ms[GS_FAM_E_PLANO] - ms[GS_FAM_E_ALVOS] - ms[GS_FAM_E_MEM], ms[GS_FAM_UNPACK],
             ms[GS_FAM_UNP_COPIA], (double)gGsFamVezes[GS_FAM_UNP_COPIA] / quadros, (double)gGsFamUnpVet[0] / quadros,
             ms[GS_FAM_UNP_RAPIDO], (double)gGsFamVezes[GS_FAM_UNP_RAPIDO] / quadros, (double)gGsFamUnpVet[1] / quadros,
             ms[GS_FAM_UNP_GERAL], (double)gGsFamVezes[GS_FAM_UNP_GERAL] / quadros, (double)gGsFamUnpVet[2] / quadros);
    divisao_log(linha);
#ifdef PORT_X360
    /* (a VU1 por dentro, gs_internal.h, só no 360) as partes medidas nos lotes da amostra, levadas à VU1 inteira pela
       proporção (a soma das partes é o tempo dos lotes da amostra); as vezes e os vértices das cópias, também levados ao
       total de lotes; e quanto custou, em média, um lote da amostra */
    {
        double sv = 0.0, f, fl;
        for (k = GS_FAM_V_PROG; k <= GS_FAM_V_INTERP; k++) {
            sv += ms[k];
        }
        f = sv > 0.0 ? ms[GS_FAM_VU1] / sv : 0.0;
        fl = gGsFamVezes[GS_FAM_V_PROG] ? (double)gGsFamVezes[GS_FAM_VU1] / gGsFamVezes[GS_FAM_V_PROG] : 0.0;
        snprintf(linha, sizeof(linha),
                 "grava\303\247\303\243o por fam\303\255lia, VU1 por dentro, quadro %u: VU1 %.2f (%.0f lotes; amostra de %.0f, "
                 "%.2f us por lote): programa %.2f, estado %.2f, constantes %.2f, \303\255ndices %.2f, c\303\263pia %.2f (%.0f vezes, "
                 "%.0f v\303\251rtices), c\303\263pia da segunda camada %.2f (%.0f vezes, %.0f v\303\251rtices), fecho %.2f, "
                 "interpretador %.2f",
                 gGsFrame, ms[GS_FAM_VU1], (double)gGsFamVezes[GS_FAM_VU1] / quadros, (double)gGsFamVezes[GS_FAM_V_PROG] / quadros,
                 gGsFamVezes[GS_FAM_V_PROG] ? sv * 1000.0 * quadros / gGsFamVezes[GS_FAM_V_PROG] : 0.0, ms[GS_FAM_V_PROG] * f,
                 ms[GS_FAM_V_ESTADO] * f, ms[GS_FAM_V_CONST] * f, ms[GS_FAM_V_IDX] * f, ms[GS_FAM_V_COPIA] * f,
                 (double)gGsFamVezes[GS_FAM_V_COPIA] * fl / quadros, (double)gGsFamVuVet[0] * fl / quadros, ms[GS_FAM_V_COPIA2] * f,
                 (double)gGsFamVezes[GS_FAM_V_COPIA2] * fl / quadros, (double)gGsFamVuVet[1] * fl / quadros, ms[GS_FAM_V_FECHO] * f,
                 ms[GS_FAM_V_INTERP] * f);
        divisao_log(linha);
        memset(gGsFamVuVet, 0, sizeof(gGsFamVuVet));
    }
#endif
    memset(gGsFamTiques, 0, sizeof(gGsFamTiques));
    memset(gGsFamVezes, 0, sizeof(gGsFamVezes));
    memset(gGsFamUnpVet, 0, sizeof(gGsFamUnpVet));
    gGsFamBytes = 0;
    quadros = 0;
    naJanela = 0;
    desde = agora;
}

/* A entrega, na thread de gravação, no lugar do frameEnd. */
static void desenho_entrega(void) {
    unsigned char limpa[MAX_TARGETS];
    uint32_t i, c[5];
    int k;
    uint64_t t0, dt, f0, f1;
#ifdef PORT_X360
    uint64_t pEsp; /* (picos: a espera pelo desenho, gs_internal.h) */
#endif

    GS_FAM_INI(f0); /* (F5, etapa 8, gravação: a espera pelo desenho, a cópia e o gancho do backend, gs_internal.h) */
#ifdef PORT_X360
    pEsp = gPicos != 0 ? __mftb() : 0;
    pthread_mutex_lock(&sDesLock); /* o desenho largou o retrato anterior e não repete o último quadro até a entrega acabar */
    while (sDesTem && !sDesParado) {
        pthread_cond_wait(&sDesFeito, &sDesLock);
    }
    sEntregando = 1;
    pthread_mutex_unlock(&sDesLock);
    if (pEsp != 0) {
        pEsp = __mftb() - pEsp;
    }
#else
    GsDraw_DesenhoEspera(); /* o desenho largou o retrato anterior e soltou o que viajava com ele */
#endif
    GS_FAM_FIM(GS_FAM_ESPERA, f0);
    GS_FAM_INI(f1);
    t0 = gpu_now();
    c[0] = gsDrawCount;
    c[1] = gsVertCount;
    c[2] = gsVuVertCount;
    c[3] = gsVuIdxCount;
    c[4] = gsVuUniCount;
    for (k = 0; k < 5; k++) {
        if (c[k] > sMaior[k]) {
            sMaior[k] = c[k];
        }
    }
    /* o jogo em que a gravação gravou vai como está; ela passa para o outro no fim desta função */
    sRetrato.draws = gsDraws;
    sRetrato.verts = gsVerts;
    sRetrato.vuVerts = gsVuVerts;
    sRetrato.vuIdx = gsVuIdx;
    sRetrato.vuFaixa = sVuFaixa;
    sRetrato.vuUni = gsVuUni;
    sRetrato.drawCount = c[0];
    sRetrato.vertCount = c[1];
    sRetrato.vuVertCount = c[2];
    sRetrato.vuIdxCount = c[3];
    sRetrato.vuUniCount = c[4];
    memcpy(sRetAlvos, gsTargets, sizeof(sRetAlvos));
    GS_FAM_FIM(GS_FAM_COPIA, f1);
    sRetrato.targets = sRetAlvos;
    sRetrato.targetCount = gsTargetCount;
    sRetrato.quadro = gGsFrame;
    sRetrato.frame[0] = gGs.frame[0];
    sRetrato.frame[1] = gGs.frame[1];
    sRetrato.fbp = GS_FBP_QUADRO;
    sRetrato.anchor = gsAnchor;
    sRetrato.texCount = gsTexCount;
    sRetrato.skipped = gsSkipped;
    sRetrato.native = gsNative;
    sRetrato.vu1Ms = VU1_MS;
    VU1_MS = 0.0;
    sRetrato.soltas = sSoltas[sSoltaPend];
    sRetrato.soltaCount = sSoltaN[sSoltaPend];
    sSoltaPend ^= 1;
    sSoltaN[sSoltaPend] = 0;
    sRetrato.vivo = 0;
    sRetrato.dividido = 1;
    /* o cleared da gravação (o risco 1): ligado aqui para todo alvo com desenho não nativo, mais o que a regra do backend
       pedir; o do desenho fica na cópia da tabela que vai no retrato */
    memset(limpa, 0, sizeof(limpa));
    for (i = 0; i < c[0]; i++) {
        if (gsDraws[i].native == 0 && gsDraws[i].target >= 0 && gsDraws[i].target < MAX_TARGETS) {
            limpa[gsDraws[i].target] = 1;
        }
    }
    if (sBackend->entrega != NULL) {
        GS_FAM_INI(f1);
        sBackend->entrega(&sRetrato, limpa);
        GS_FAM_FIM(GS_FAM_BACKEND, f1);
    }
    for (k = 0; k < gsTargetCount; k++) {
        if (limpa[k]) {
            gsTargets[k].cleared = 1;
        }
        gsTargets[k].draws = 0;
    }
    gsVertCount = 0;
    gsVuVertCount = 0;
    gsVuIdxCount = 0;
    gsVuUniCount = 0;
    gsDrawCount = 0;
    gsAnchor = 0;
    gsSkipped = 0;
    gsNative = 0;
    jogo_usa(sJogo ^ 1); /* o do retrato anterior: o desenho já o largou (a espera do começo desta função) */
    dt = gpu_now() - t0;
    sCopiaNs += dt;
    if (dt > sCopiaPior) {
        sCopiaPior = dt;
    }
    sEntregas++;
#ifdef PORT_X360
    if (gPicos != 0) { /* (antes de acordar o desenho: ele escreve a linha do quadro de dois atrás) */
        Picos_Gravou(sRetrato.quadro, pEsp, c);
    }
#endif
    pthread_mutex_lock(&sDesLock);
#ifdef PORT_X360
    sEntregando = 0;
#endif
    sDesTem = 1;
    pthread_cond_signal(&sDesAcorda);
    pthread_mutex_unlock(&sDesLock);
    if (gsPendingScale) { /* (o backend muda de tamanho com o desenho parado) */
        GsDraw_DesenhoEspera();
        scale_apply();
    }
}
#endif

#if defined(BT3_GS_ESPERAS) && defined(BT3_GPU_FALSO)
/* (F5, etapa 8, gravação) BT3_VU_PLANO_CONFERE=1, só de teste nas builds --gpu: no fim de cada quadro, a faixa guardada de cada
   desenho da VU1 (vu_faixa) contra o menor e o maior índice relidos como o vu_plano do gs_d3d.cpp fazia. O vu_plano só depende
   dessa faixa, da contagem, do vuVertCount e da capacidade, então faixas iguais dão o mesmo plano. Uma linha a cada 1000
   quadros com o acumulado, e uma por desenho diferente (até 20). */
static void vu_faixa_confere(void) {
    static int liga = -1;
    static unsigned quadros, conferidos, diferentes;
    uint32_t n, k;
    if (liga < 0) {
        liga = getenv("BT3_VU_PLANO_CONFERE") != NULL && atoi(getenv("BT3_VU_PLANO_CONFERE")) != 0;
    }
    if (!liga) {
        return;
    }
    for (n = 0; n < gsDrawCount; n++) {
        const GsDraw *d = &gsDraws[n];
        uint32_t lo = 0xFFFFFFFFu, hi = 0;
        if (d->vu < 1 || d->vu > 3 || d->native != 0 || d->count == 0) {
            continue;
        }
        for (k = 0; k < d->count; k++) {
            uint32_t i = gsVuIdx[d->first + k];
            if (i < lo) lo = i;
            if (i > hi) hi = i;
        }
        conferidos++;
        if (lo != sVuFaixa[2 * n] || hi != sVuFaixa[2 * n + 1]) {
            if (diferentes++ < 20) {
                fprintf(stderr, "gs: BT3_VU_PLANO_CONFERE: quadro %u, desenho %u (vu %d, %u índices): relido %u a %u, guardado %u a %u\n",
                        gGsFrame, n, d->vu, d->count, lo, hi, sVuFaixa[2 * n], sVuFaixa[2 * n + 1]);
            }
        }
    }
    if (++quadros % 1000 == 0) {
        fprintf(stderr, "gs: BT3_VU_PLANO_CONFERE: %u quadros, %u desenhos da VU1 conferidos, %u diferentes\n", quadros, conferidos,
                diferentes);
    }
}
#endif

void GsGpu_FrameEnd(void) {
    if (gPortResim) { /* a frame that is only being re-run: nothing was recorded, nothing is shown */
        return;
    }
#ifdef BT3_GS_ESPERAS
    Gs_FamTrecho();
#endif
#if defined(BT3_GS_ESPERAS) && defined(BT3_GPU_FALSO)
    vu_faixa_confere();
    est_confere_linha();
#endif
    uint64_t t0 = gpu_now();
    if (sBackend != NULL) {
        int i;

        if (sInterpNext != 0) { /* (pictures still waiting: the game did not reach its vertical blanks in between) */
            uint32_t verts = gsVertCount, vuVerts = gsVuVertCount, vuIdx = gsVuIdxCount, vuUni = gsVuUniCount, draws = gsDrawCount;
            /* cannot happen in the order the game works in (the list of the new tick is complete here); if it does,
               the old tick's pictures are gone with its list */
            if (verts != 0 || vuVerts != 0 || vuIdx != 0 || vuUni != 0 || draws != 0) {
                sInterpNext = 0;
            }
        }
        if (gsInterp < 0) {
            gsInterp = getenv("BT3_INTERP") != NULL ? atoi(getenv("BT3_INTERP")) : Port_Setting("interp", 0);
            gsInterp = gsInterp < 0 ? 0 : gsInterp > 3 ? 3 : gsInterp;
            sInterpPrev = malloc(MAX_VU_UNIFORMS * sizeof(Vu0Uniform));
            sInterpBlend = malloc(MAX_VU_UNIFORMS * sizeof(Vu0Uniform));
            sInterpPrevSig = malloc(MAX_VU_UNIFORMS * sizeof(uint32_t));
            sInterpSig = malloc(MAX_VU_UNIFORMS * sizeof(uint32_t));
            sInterpPrevVerts = malloc(MAX_VERTS * sizeof(Vtx));
            sInterpBlendVerts = malloc(MAX_VERTS * sizeof(Vtx));
            sInterpPrevPrims = malloc(INTERP_MAX_PRIMS * sizeof(InterpPrim));
            sInterpPrims = malloc(INTERP_MAX_PRIMS * sizeof(InterpPrim));
            sInterpPrimUsed = malloc(INTERP_MAX_PRIMS);
        }
#ifdef BT3_GS_ESPERAS
        if (sDividido) { /* (degrau 2) o retrato vai para a thread de desenho; a gravação segue */
#ifdef PORT_X360
            if (sDesParado) {
                desenho_descarta();
                gGpuEndNs += gpu_now() - t0;
                return;
            }
#endif
            desenho_entrega();
            gGpuEndNs += gpu_now() - t0;
            return;
        }
#endif
        {
            /* A scene the game shows at 60 a second (one vertical blank per picture: menus, the character select) has
               half the time per tick and needs half the pictures: none at "60", one at "120". How many blanks this
               tick will last is taken from the last one. */
            extern unsigned gPortVBlanks;
            unsigned blanks = gPortVBlanks - sInterpLastBlank;
            sInterpLastBlank = gPortVBlanks;
            blanks = blanks < 1 ? 1 : blanks > 2 ? 2 : blanks;
            sInterpSteps = interp_steps() * (int)blanks / 2;
            sInterpTickNs = (uint64_t)blanks * 16683350ull;
        }
        if (gsInterp > 0) {
            interp_signatures(sInterpSig);
            sInterpPrimCount = interp_prims(sInterpPrims);
        }
        if (gsInterp > 0 && sInterpSteps > 1 && interp_build(getenv("BT3_INTERP_T") != NULL ? (float)atof(getenv("BT3_INTERP_T")) : 1.0f / (float)sInterpSteps, 1)) {
            uint64_t before;

            sInterpSaved.verts = gsVertCount;
            sInterpSaved.vuVerts = gsVuVertCount;
            sInterpSaved.vuIdx = gsVuIdxCount;
            sInterpSaved.vuUni = gsVuUniCount;
            sInterpSaved.draws = gsDrawCount;
            sInterpSaved.anchor = gsAnchor;
            sInterpSaved.skipped = gsSkipped;
            sInterpSaved.native = gsNative;
            for (i = 0; i < gsTargetCount && i < MAX_TARGETS; i++) {
                sInterpSaved.tdraws[i] = gsTargets[i].draws;
            }
            before = gpu_now();
            interp_show(1); /* the first in-between picture; the others and the real one follow (GsGpu_InterpPump) */
            sInterpBlendShown = gpu_now();
            sInterpBlendTook = sInterpBlendShown - before;
            if (sInterpBlendTook > 4000000ull) {
                sInterpBlendTook = 4000000ull;
            }
            sInterpNext = 2;
            if (getenv("BT3_INTERP_LOG") != NULL && (gInterpTicks++ % 300) == 299) {
                fprintf(stderr, "interp: %u in-between pictures shown, %u ticks without; blocks: %u blended, %u not\n", gInterpShown, gInterpSkipped, gInterpPaired, gInterpUnpaired);
                fprintf(stderr, "interp:   of those without a partner, %u moved with the camera\n", gInterpCamera);
                fprintf(stderr, "interp:   effects and 2D: %u triangles, %u moved, %u still; not paired: %u new kind, %u too far, %u other texture piece, %u none free\n",
                        gInterpTris, gInterpTrisBlended, gInterpWhy[3], gInterpWhy[0], gInterpWhy[1], gInterpWhy[2], gInterpWhy[4]);
                fprintf(stderr, "interp:   pieces moved by the camera's change: %u, of them of kinds that stand still: %u\n", gInterpTrisCamera, gInterpStatic);
                gInterpStatic = 0;
                fprintf(stderr, "interp:   ticks where the camera's way went through the scenery: %u; cuts: %u\n", gInterpThrough, gInterpCuts);
                gInterpTrisCamera = 0;
                memset(gInterpWhy, 0, sizeof(gInterpWhy));
                gInterpPaired = gInterpUnpaired = gInterpCamera = gInterpTris = gInterpTrisBlended = 0;
            }
        } else {
            if (gsInterp > 0) {
                interp_remember();
                gInterpSkipped++;
            }
#ifdef PORT_X360
            if (gPicos != 0) { /* (sem a divisão: a gravação e o desenho na mesma thread, sem espera entre eles) */
                uint32_t pc[5];
                pc[0] = gsDrawCount;
                pc[1] = gsVertCount;
                pc[2] = gsVuVertCount;
                pc[3] = gsVuIdxCount;
                pc[4] = gsVuUniCount;
                Picos_Gravou(gGsFrame, 0, pc);
            }
#endif
            BACKEND_FRAME_END();
            interp_present_log('S');
            scale_apply();
        }
    }
    gGpuEndNs += gpu_now() - t0;
}
