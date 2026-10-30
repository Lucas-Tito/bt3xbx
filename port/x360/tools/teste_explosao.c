/*
 * F8, item B do docs/x360/plano/F8/11-cpu-vu0.md: o EftBurst_DrawModel com a tabela por vértice (src/battle/eft_d_b.c,
 * ramo PORT) contra o original (o ramo #else do mesmo arquivo), bit a bit. Montado por port/x360/tools/teste_explosao.py:
 * o arquivo do jogo entra duas vezes (EftBurst_DrawModel_Velho e EftBurst_DrawModel_Novo), com a VU0 emulada do port
 * (src/port/vu0_a.c, vu0_b.c) e o modelo de float da build (PS2 no PC, a FPU em truncamento no 360).
 *
 * Para cada cena (modelo sintético, quadro, distância da luz, matriz): o mesmo estado de partida, as duas versões, e a
 * comparação de tudo o que sai: a lista de desenho inteira (RGBAQ, ST, XYZ2 dos dois passes), as texturas pedidas, o
 * estado da VU0 emulada depois (vf, ACC, Q, flags, memória) e as posições de textura escritas no modelo. Conta também
 * as chamadas da biblioteca vetorial feitas pelo código do jogo (--wrap), para a estimativa do ganho.
 *
 *   teste_explosao             as cenas; sai com 1 se alguma difere
 *   teste_explosao identidade  também 1.0 * q == q para todo padrão de q que a divisão da build pode dar (2^32 casos)
 */
#include "common.h"
#include "battle/eft_d.h"
#include "sys/gfx.h"
#include "port/vu0_a.h"
#include <stdarg.h>

#ifdef TESTE_LIBC
#include <stdio.h>
#include <string.h>
#define say(s) fputs(s, stdout)
#else
extern s32 write(s32 fd, const void *p, u32 n);
extern void *memset(void *d, s32 c, u32 n);
extern void *memcpy(void *d, const void *s, u32 n);
extern s32 memcmp(const void *a, const void *b, u32 n);
extern u32 strlen(const char *s);
static void say(const char *s) { write(1, s, strlen(s)); }
#endif

/* ---- saída sem printf (o binário da x86 não tem libc) ---- */
static char sLinha[512];
static u32 sPos;
static void put(const char *s) {
    while (*s && sPos < sizeof(sLinha) - 2) {
        sLinha[sPos++] = *s++;
    }
}
static void putu(u32 v) {
    char b[12];
    s32 i = 11;
    b[i] = 0;
    do {
        b[--i] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    put(b + i);
}
static void putx(u32 v) {
    char b[9];
    s32 i;
    for (i = 7; i >= 0; i--, v >>= 4) {
        b[i] = "0123456789abcdef"[v & 15];
    }
    b[8] = 0;
    put(b);
}
static void fim(void) {
    sLinha[sPos++] = '\n';
    sLinha[sPos] = 0;
    say(sLinha);
    sPos = 0;
}

/* o printf que o ramo PORT usa no PC para o aviso de índice fora da tabela */
s32 printf(const char *f, ...) {
    va_list a;

    va_start(a, f);
    put("  (aviso do jogo) ");
    for (; *f && *f != '\n'; f++) {
        if (f[0] == '%' && (f[1] == 'u' || f[1] == 'd')) {
            s32 v = va_arg(a, s32);
            if (f[1] == 'd' && v < 0) {
                put("-");
                v = -v;
            }
            putu((u32)v);
            f++;
        } else {
            char c[2] = {*f, 0};
            put(c);
        }
    }
    va_end(a);
    fim();
    return 0;
}

/* ---- o que o EftBurst_DrawModel lê e chama fora da biblioteca vetorial ---- */
typedef struct TesteView {
    u8 unk0[0x140];
    Mtx44 proj;
} TesteView;

static TesteView sView __attribute__((aligned(16)));
TesteView *gBtlCamView = &sView;
static EftBurstWork sWork;
EftBurstWork *gEftBurst = &sWork;
EftBurstRes gEftBurstRes;
Gfx gGfx; /* o quadro par / ímpar que o PutModelEnvEnd lê */
static EftBurstTexPack sPack;
static EftBurstTex sPackTex[8];

#define DMA_QW (1 << 20)
static u64 sDma[DMA_QW * 2] __attribute__((aligned(16)));
static u64 *sDmaCur;
static u32 sLog[4096];
static u32 sLogN;

static void logw(u32 w) {
    if (sLogN < 4096) {
        sLog[sLogN] = w;
    }
    sLogN++;
}

u64 *Dma_BeginDirect(void) {
    logw(0xB0000000u | (u32)(sDmaCur - sDma));
    return sDmaCur;
}
void Dma_EndDirect(u64 *end) {
    logw(0xE0000000u | (u32)(end - sDma));
    sDmaCur = end;
}
/* o EftBurst_SetTexture do arquivo (o gcc o põe dentro do desenho) sobe a textura por aqui */
void Tex_Upload(EftBurstTex *tex, s32 x, s32 y) {
    logw(0x70000000u | (u32)((u8 *)tex - (u8 *)0) & 0xFFFFF);
    logw((u32)x);
    logw((u32)y);
}
void EftBurst_ClearAlphaPlane(void) {
    logw(0xC0000000u);
}
u64 *EftBurst_PutModelEnv(u64 *p) {
    *p++ = 0x1111111111111111ull;
    return p;
}
u64 *EftBurst_PutModelEnvEnd(u64 *p) {
    *p++ = 0x2222222222222222ull;
    return p;
}

/* O que o resto do arquivo do jogo cita (as partículas): o gc do ld guarda algumas dessas funções em uma das variantes
   sem que o desenho as chame. Chamadas de fato, param o teste. */
extern void _exit(s32 c);
static void falta(const char *n) {
    put("chamada inesperada: ");
    put(n);
    fim();
    _exit(3);
}
f32 gEftBurstStreakAngle;
f32 Port_sinf(f32 x) {
    falta("sinf");
    return x;
}
f32 Port_cosf(f32 x) {
    falta("cosf");
    return x;
}
s32 Port_Rand(void) {
    falta("rand");
    return 0;
}

/* ---- contagem das chamadas do código do jogo à biblioteca vetorial (-Wl,--wrap) ---- */
enum { C_PROJ, C_TOF12, C_TOF, C_SCALE, C_SUB, C_ADD, C_CLAMP, C_TOINT, C_DOT, C_SQRT, C_COPY, C_SET, C_N };
static const char *const kNomes[C_N] = {"ProjectPointStq", "ToFloat12", "ToFloat", "Scale", "Sub", "Add", "Clamp", "ToInt",
                                        "Dot", "sqrtf", "Copy", "Set"};
static u32 sConta[C_N];

extern s32 __real_Mtx_ProjectPointStq(s32 *scr, f32 *st, Mtx44 *m, f32 *pos, f32 *nrm);
extern void __real_IVec4_ToFloat12(f32 *out, f32 *in);
extern void __real_IVec4_ToFloat(f32 *out, s32 *in);
extern void __real_Vec4_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern void __real_Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void __real_Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void __real_Vec4_Clamp(f32 *out, f32 *in, f32 lo, f32 hi);
extern void __real_Vec4_ToInt(s32 *dst, Vec4 *src);
extern f32 __real_Vec3_Dot(Vec4 *a, Vec4 *b);
extern f32 __real_Port_sqrtf(f32 x);
extern void __real_Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void __real_IVec4_Set(f32 *out, s32 a, s32 b, s32 c, s32 d);

s32 __wrap_Mtx_ProjectPointStq(s32 *scr, f32 *st, Mtx44 *m, f32 *pos, f32 *nrm) {
    sConta[C_PROJ]++;
    return __real_Mtx_ProjectPointStq(scr, st, m, pos, nrm);
}
void __wrap_IVec4_ToFloat12(f32 *out, f32 *in) { sConta[C_TOF12]++; __real_IVec4_ToFloat12(out, in); }
void __wrap_IVec4_ToFloat(f32 *out, s32 *in) { sConta[C_TOF]++; __real_IVec4_ToFloat(out, in); }
void __wrap_Vec4_Scale(Vec4 *dst, Vec4 *src, f32 s) { sConta[C_SCALE]++; __real_Vec4_Scale(dst, src, s); }
void __wrap_Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b) { sConta[C_SUB]++; __real_Vec4_Sub(dst, a, b); }
void __wrap_Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b) { sConta[C_ADD]++; __real_Vec4_Add(dst, a, b); }
void __wrap_Vec4_Clamp(f32 *out, f32 *in, f32 lo, f32 hi) { sConta[C_CLAMP]++; __real_Vec4_Clamp(out, in, lo, hi); }
void __wrap_Vec4_ToInt(s32 *dst, Vec4 *src) { sConta[C_TOINT]++; __real_Vec4_ToInt(dst, src); }
f32 __wrap_Vec3_Dot(Vec4 *a, Vec4 *b) { sConta[C_DOT]++; return __real_Vec3_Dot(a, b); }
f32 __wrap_Port_sqrtf(f32 x) { sConta[C_SQRT]++; return __real_Port_sqrtf(x); }
void __wrap_Vec4_Copy(Vec4 *dst, Vec4 *src) { sConta[C_COPY]++; __real_Vec4_Copy(dst, src); }
void __wrap_IVec4_Set(f32 *out, s32 a, s32 b, s32 c, s32 d) { sConta[C_SET]++; __real_IVec4_Set(out, a, b, c, d); }

extern void EftBurst_DrawModel_Velho(EftBurstModel *model, s32 unused);
extern void EftBurst_DrawModel_Novo(EftBurstModel *model, s32 unused);
extern u32 gEftBurstVtxFora;

/* ---- modelos sintéticos ---- */
static u32 sSemente;
static u32 rnd(void) {
    sSemente = sSemente * 1664525u + 1013904223u;
    return sSemente >> 8;
}
static s32 entre(s32 a, s32 b) { /* [a, b] */
    return a + (s32)(rnd() % (u32)(b - a + 1));
}
static f32 fbits(u32 u) {
    f32 f;
    memcpy(&f, &u, 4);
    return f;
}

#define MAX_GRUPOS 6
#define MAX_VERTS 4000
#define MAX_TRIS 6000
static s16 sVerts[MAX_GRUPOS][(MAX_VERTS + 1000) * 3];
typedef struct Tri {
    s16 vtx[3];
    s16 nrm[3][2];
    u16 col[3][2];
} Tri; /* o EftBurstTri do eft_d_b.c: 0x1E bytes */
static Tri sTris[MAX_GRUPOS][MAX_TRIS];
static EftBurstGroup sGrupos[MAX_GRUPOS];
static EftBurstTex sTex[4];
static EftBurstModel sModelo;

/* As floats das cenas por padrão de bits: nenhuma conta de float no teste fora do código medido. */
#define F_30 0x41F00000u   /* 30.0 */
#define F_12 0x41400000u   /* 12.0 */
#define F_45 0x42340000u   /* 45.0 */
#define F_1 0x3F800000u
#define F_M1 0xBF800000u

static void matriz(u32 tipo) {
    f32 *m = &sView.proj.m[0][0];
    s32 i;

    for (i = 0; i < 16; i++) {
        m[i] = fbits(0);
    }
    /* colunas: o.c = r0.c x + r1.c y + r2.c z + r3.c w (mtx_apply); w de saída = -z, x / w = 500 x / -z + 2048 */
    m[0 * 4 + 0] = fbits(0x43FA0000u);  /* r0.x = 500 */
    m[1 * 4 + 1] = fbits(0x43FA0000u);  /* r1.y = 500 */
    m[2 * 4 + 0] = fbits(0xC5000000u);  /* r2.x = -2048 */
    m[2 * 4 + 1] = fbits(0xC5000000u);  /* r2.y = -2048 */
    m[2 * 4 + 2] = fbits(0xC1200000u);  /* r2.z = -10 */
    m[2 * 4 + 3] = fbits(F_M1);         /* r2.w = -1 */
    m[3 * 4 + 2] = fbits(0x47000000u);  /* r3.z = 32768 */
    if (tipo == 1) {                    /* w minúsculo: Q enorme, infinito ou o maior número (a divisão da build) */
        m[2 * 4 + 3] = fbits(0x80800000u | (rnd() & 0x7FFFFF)); /* -(1.2e-38 ..) */
        m[3 * 4 + 3] = fbits(0);
    } else if (tipo == 2) {             /* w enorme: Q minúsculo (denormal ou zero, conforme o modelo) */
        m[2 * 4 + 3] = fbits(0xFE000000u | (rnd() & 0x7FFFFF)); /* -4e37 .. */
    } else if (tipo == 3) {             /* matriz aleatória */
        for (i = 0; i < 16; i++) {
            m[i] = fbits(((rnd() & 1) << 31) | ((u32)entre(0x78, 0x88) << 23) | (rnd() & 0x7FFFFF));
        }
    }
}

typedef struct Cena {
    const char *nome;
    u32 semente;
    s32 grupos;
    s32 verts;     /* vértices por grupo */
    s32 tris;      /* triângulos por grupo */
    s32 idxMin;    /* faixa dos índices (negativos e acima do limite da tabela incluídos quando pedidos) */
    s32 idxMax;
    s32 compartilha; /* os grupos usam o mesmo vetor de vértices, com escalas diferentes */
    s32 extremos;  /* vértices com valores extremos / zero */
    u32 matriz;
} Cena;

static const Cena kCenas[] = {
    {"aleatorio", 11, 3, 300, 400, 0, 299, 0, 0, 0},
    {"repetidos", 22, 4, 20, 300, 0, 19, 1, 0, 0},
    {"planeta", 33, 2, 877, 1650, 0, 876, 0, 0, 0},
    {"acima_do_limite", 44, 2, 3000, 2500, -600, 2999, 0, 0, 0},
    {"extremos", 55, 3, 64, 200, 0, 63, 1, 1, 0},
    {"w_minusculo", 66, 2, 64, 200, 0, 63, 0, 1, 1},
    {"w_enorme", 77, 2, 64, 200, 0, 63, 0, 1, 2},
    {"matriz_aleatoria", 88, 3, 100, 300, 0, 99, 1, 1, 3},
};
static const s32 kQuadros[] = {5, 10, 11, 37, 90};
static const u32 kDist[] = {0x41C80000u /* 25 */, 0x42700000u /* 60 */, 0x43C80000u /* 400 */, 0 /* 0: len / 0 */};

static void monta(const Cena *c, s32 flagsMask) {
    s32 g, i, k;

    sSemente = c->semente;
    for (g = 0; g < c->grupos; g++) {
        s16 *v = sVerts[c->compartilha ? 0 : g];
        EftBurstGroup *grp = &sGrupos[g];

        if (!c->compartilha || g == 0) {
            for (i = 0; i < (c->verts + 1000) * 3; i += 3) {
                if (c->extremos && rnd() % 8 == 0) {
                    static const s16 ex[] = {0, 0, 0, 32767, -32768, 1, -1, 4096, -4096};
                    v[i] = ex[rnd() % 9];
                    v[i + 1] = ex[rnd() % 9];
                    v[i + 2] = ex[rnd() % 9];
                } else {
                    v[i] = (s16)entre(-8192, 8192);
                    v[i + 1] = (s16)entre(-8192, 8192);
                    v[i + 2] = (s16)entre(-14000, 1500); /* quase todos na frente da câmera (z < 0) */
                }
            }
        }
        for (i = 0; i < c->tris; i++) {
            Tri *t = &sTris[g][i];
            for (k = 0; k < 3; k++) {
                t->vtx[k] = (s16)entre(c->idxMin, c->idxMax);
                t->nrm[k][0] = (s16)entre(-4096, 4096);
                t->nrm[k][1] = (s16)entre(-4096, 4096);
                t->col[k][0] = (u16)rnd();
                t->col[k][1] = (u16)rnd();
            }
        }
        grp->flags = (s32)((rnd() & 0x7FF) | (g & 1 ? 0x800 : 0)) & flagsMask;
        grp->texIdx = g & 3;
        grp->verts = sVerts[c->compartilha ? 0 : g] + 1000 * 3; /* índices negativos até -1000 caem dentro do vetor */
        grp->tris = (s16 *)sTris[g];
        grp->scale = fbits(g == 0 ? F_30 : g == 1 ? F_12 : g == 2 ? F_45 : F_1);
        grp->triCount = (s16)c->tris;
    }
    for (i = 0; i < 4; i++) {
        memset(&sTex[i], 0, sizeof(sTex[i]));
        sTex[i].w = 64 << i;
        sTex[i].h = 32 << i;
        sTex[i].tex0 = 0x123456789ull * (u64)(i + 1);
    }
    memset(sPackTex, 0, sizeof(sPackTex));
    sPackTex[6].tex0 = 0xABCDEF0123ull;
    sModelo.groupCount = c->grupos;
    sModelo.groups = sGrupos;
    sModelo.tex = sTex;
    matriz(c->matriz);
}

/* ---- estado de partida e captura ---- */
static RefVu0State sVu0Partida;
static RefVu0State sVu0Saida[2];
static u32 sDmaN[2];
static u64 *sDmaCopia[2];
static u32 sLogCopia[2][4096];
static u32 sLogN2[2];
static EftBurstTex sTexSaida[2][4];
static EftBurstTex sEnvSaida[2];
static u32 sContaSaida[2][C_N];
static u64 sDmaGuardado[2][DMA_QW] __attribute__((aligned(16)));

static void roda(s32 lado) {
    gRefVu0 = sVu0Partida;
    memset(sDma, 0xA5, sizeof(u64) * DMA_QW);
    sDmaCur = sDma;
    sLogN = 0;
    memset(sConta, 0, sizeof(sConta));
    if (lado == 0) {
        EftBurst_DrawModel_Velho(&sModelo, 0);
    } else {
        EftBurst_DrawModel_Novo(&sModelo, 0);
    }
    sVu0Saida[lado] = gRefVu0;
    sDmaN[lado] = (u32)(sDmaCur - sDma);
    memcpy(sDmaGuardado[lado], sDma, sizeof(u64) * (sDmaN[lado] < DMA_QW ? sDmaN[lado] : DMA_QW));
    memcpy(sLogCopia[lado], sLog, sizeof(sLog));
    sLogN2[lado] = sLogN;
    memcpy(sTexSaida[lado], sTex, sizeof(sTex));
    sEnvSaida[lado] = sPackTex[6];
    memcpy(sContaSaida[lado], sConta, sizeof(sConta));
    (void)sDmaCopia;
}

static u32 sCenas, sFalhas;
static u32 sTotal[2][C_N];

static void compara(const char *nome, s32 quadro, u32 dist, s32 env) {
    const char *o = NULL;
    u32 i;

    if (sDmaN[0] != sDmaN[1]) {
        o = "tamanho da lista";
    } else if (memcmp(sDmaGuardado[0], sDmaGuardado[1], sizeof(u64) * sDmaN[0]) != 0) {
        o = "lista de desenho";
    } else if (sLogN2[0] != sLogN2[1] || memcmp(sLogCopia[0], sLogCopia[1], sizeof(sLogCopia[0])) != 0) {
        o = "texturas / DMA pedidos";
    } else if (memcmp(&sVu0Saida[0], &sVu0Saida[1], sizeof(RefVu0State)) != 0) {
        o = "estado da VU0 depois";
    } else if (memcmp(sTexSaida[0], sTexSaida[1], sizeof(sTexSaida[0])) != 0 ||
               memcmp(&sEnvSaida[0], &sEnvSaida[1], sizeof(EftBurstTex)) != 0) {
        o = "texturas do modelo";
    }
    sCenas++;
    for (i = 0; i < C_N; i++) {
        sTotal[0][i] += sContaSaida[0][i];
        sTotal[1][i] += sContaSaida[1][i];
    }
    if (o != NULL) {
        sFalhas++;
        put("DIFERE: ");
        put(nome);
        put(" quadro ");
        putu((u32)quadro);
        put(" dist ");
        putx(dist);
        put(env ? " com 0x800" : " sem 0x800");
        put(": ");
        put(o);
        if (o[0] == 'l') {
            for (i = 0; i < sDmaN[0] && sDmaGuardado[0][i] == sDmaGuardado[1][i]; i++) {
            }
            put(" (palavra ");
            putu(i);
            put(" de ");
            putu(sDmaN[0]);
            put(")");
        }
        fim();
    }
}

/* 1.0 * q == q pela multiplicação da biblioteca (Vec4_Scale), para os q que a divisão da build pode dar */
static u32 identidade(u32 passo) {
    f32 um[4] __attribute__((aligned(16)));
    f32 out[4] __attribute__((aligned(16)));
    u32 q, ruins = 0, vistos = 0;

    um[0] = um[1] = um[2] = um[3] = fbits(F_1);
    q = 0;
    do {
        u32 e = (q >> 23) & 0xFF;
#ifdef MODELO_360
        s32 pode = !(e == 0xFF && (q & 0x7FFFFF) != 0 && !(q & 0x400000)); /* todos, menos NaN sinalizador */
#else
        s32 pode = (e != 0 && e != 0xFF) || (q & 0x7FFFFFFF) == 0 || (q & 0x7FFFFFFF) == 0x7F7FFFFF;
#endif
        if (pode) {
            u32 r;
            __real_Vec4_Scale((Vec4 *)out, (Vec4 *)um, fbits(q));
            memcpy(&r, &out[0], 4);
            vistos++;
            if (r != q) {
                if (ruins++ < 5) {
                    put("1.0 * ");
                    putx(q);
                    put(" = ");
                    putx(r);
                    fim();
                }
            }
        }
        q += passo;
    } while (q != 0 && q >= passo);
    put("identidade 1.0 * q: ");
    putu(vistos);
    put(" padrões, ");
    putu(ruins);
    put(" diferentes");
    fim();
    return ruins;
}

s32 main(s32 argc, char **argv) {
    u32 c, q, d, e, i;
    s32 completa = argc > 1 && argv[1][0] == 'i';

#ifdef MODELO_360
    { /* a FPU do 360 em truncamento (_controlfp(_RC_CHOP)): SSE e x87 em direção a zero */
        u32 mx = 0x1F80 | 0x6000;
        u16 cw;
        __asm__ volatile("ldmxcsr %0" : : "m"(mx));
        __asm__ volatile("fnstcw %0" : "=m"(cw));
        cw |= 0x0C00;
        __asm__ volatile("fldcw %0" : : "m"(cw));
    }
#endif
    memset(&sVu0Partida, 0x5A, sizeof(sVu0Partida)); /* resto de registradores de antes: igual nos dois lados */
    sVu0Partida.vf[0].u[0] = sVu0Partida.vf[0].u[1] = sVu0Partida.vf[0].u[2] = 0;
    sVu0Partida.vf[0].u[3] = F_1;
    for (i = 1; i <= 3; i++) { /* vf1 = (0,0,1,0), vf2 = (0,1,0,0), vf3 = (1,0,0,0) (Vu0_Init) */
        sVu0Partida.vf[i].u[0] = sVu0Partida.vf[i].u[1] = sVu0Partida.vf[i].u[2] = sVu0Partida.vf[i].u[3] = 0;
        sVu0Partida.vf[i].u[3 - i] = F_1;
    }
    gEftBurstRes.texPack = &sPack;
    sPack.tex = sPackTex;

    for (c = 0; c < sizeof(kCenas) / sizeof(kCenas[0]); c++) {
        for (e = 0; e < 3; e++) { /* 0x800 em grupos alternados, em nenhum, em todos */
            s32 mask = e == 0 ? -1 : e == 1 ? ~0x800 : -1;
            for (q = 0; q < sizeof(kQuadros) / sizeof(kQuadros[0]); q++) {
                for (d = 0; d < sizeof(kDist) / sizeof(kDist[0]); d++) {
                    u32 fora0 = gEftBurstVtxFora;
                    s32 g;

                    monta(&kCenas[c], mask);
                    if (e == 2) {
                        for (g = 0; g < kCenas[c].grupos; g++) {
                            sGrupos[g].flags |= 0x800;
                        }
                    }
                    sWork.frame = kQuadros[q];
                    sWork.fadeDist = fbits(kDist[d]);
                    roda(0);
                    monta(&kCenas[c], mask); /* o mesmo modelo (o desenho escreve as posições de textura) */
                    if (e == 2) {
                        for (g = 0; g < kCenas[c].grupos; g++) {
                            sGrupos[g].flags |= 0x800;
                        }
                    }
                    roda(1);
                    compara(kCenas[c].nome, kQuadros[q], kDist[d], e != 1);
                    if (c == 2 && e == 2 && q == 3 && d == 1) { /* o tamanho do planeta, com a luz: um quadro */
                        put("um quadro do tamanho do planeta (2 grupos de 1650 tri. e 877 vért., 0x800, quadro 37): ");
                        for (i = 0; i < C_N; i++) {
                            if (sContaSaida[0][i] != sContaSaida[1][i]) {
                                put(kNomes[i]);
                                put(" ");
                                putu(sContaSaida[0][i]);
                                put("->");
                                putu(sContaSaida[1][i]);
                                put(" ");
                            }
                        }
                        fim();
                    }
                    if (kCenas[c].idxMin < 0 && gEftBurstVtxFora == fora0) {
                        put("DIFERE: ");
                        put(kCenas[c].nome);
                        put(": nenhum canto fora da tabela");
                        fim();
                        sFalhas++;
                    }
                }
            }
        }
        put("cena ");
        put(kCenas[c].nome);
        put(": feita");
        fim();
    }
    put("chamadas do jogo à biblioteca vetorial (velho -> novo), ");
    putu(sCenas);
    put(" cenas:");
    fim();
    for (i = 0; i < C_N; i++) {
        put("  ");
        put(kNomes[i]);
        put(": ");
        putu(sTotal[0][i]);
        put(" -> ");
        putu(sTotal[1][i]);
        fim();
    }
    put("cantos fora da tabela (gEftBurstVtxFora): ");
    putu(gEftBurstVtxFora);
    fim();
    if (identidade(completa ? 1 : 0x101)) {
        sFalhas++;
    }
    put(sFalhas ? "FALHOU: " : "igual: ");
    putu(sCenas);
    put(" cenas, ");
    putu(sFalhas);
    put(" diferentes");
    fim();
    return sFalhas != 0;
}
