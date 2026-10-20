/*
 * F5, etapa 8 (build sem janela com --gpu, headless.py): o GS em modo gpu com um backend que não desenha nada.
 *
 * Por que existe. As builds sem janela (x86 e PPC) rodam o GS em software (BT3_GS=1) ou nem o rodam (BT3_GS=none):
 * nos dois o GsGpu_Enabled() é 0, e os programas da VU1 vão pelo interpretador. No 360 é o contrário: o GsGpu_Enabled()
 * é 1, e o gs_vu1.c entrega cada lote já reconhecido (hle_program0 a hle_program6) ao gs_draw.c, que grava a lista do
 * quadro para o backend D3D9. Esse caminho, que é o "tempo da gravação da VU1" do log do console (gGsVu1Ms), nunca
 * rodava no WSL. Esta build o roda: o gs_draw.c de verdade (o mesmo que o 360 compila, com o pedaço de SDL de
 * port/x360/src/gs/sdl) grava a lista, e este arquivo faz de backend: aceita tudo, não desenha e, no fim de cada
 * quadro, resume a lista gravada num hash.
 *
 * O que dá. (1) A prova de que uma mudança no caminho da gravação não mudou a lista: o hash de cada quadro (e o do
 * jogo todo) sai igual antes e depois, na x86 e na PPC (o hash lê palavras de 32 bits pelo valor, então a ordem dos
 * bytes do host não entra). (2) A medida: BT3_PERFIL=1 conta, por família de programa, as chamadas e o tempo dentro do

 * GsVu1_Call (rdtsc, só na x86; a PPC no qemu não tem tempo que valha); BT3_PERFIL=2 amostra também o contador de
 * instruções (SIGPROF, só na x86) dentro dele, por endereço, para o perfil por função e por linha; =3 só amostra, sem o
 * relógio nem as contagens (as amostras sem o custo da medida)
 * (port/x360/tools/vu1_perfil.py). A PPC se mede por contagem de instruções do qemu (vu1_perfil.py ppc).
 *
 * Fora das builds com --gpu este arquivo não é compilado, e as builds de sempre não mudam.
 */
#define _GNU_SOURCE /* REG_EIP (ucontext.h) */
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#ifdef __i386__
#include <ucontext.h>
#endif

#include "../../src/gs/gs_draw.h"
#include "../../src/gs/gs_texpack.h"

extern unsigned gGsFrame;
extern uint32_t gGsBytes[8]; /* gs_core.c (BT3_GPU_FALSO): os bytes que a cadeia do quadro leva */
extern void __real_GsVu1_Call(int addr);
extern void __real_GsVu1_Unpack(uint32_t cmd, uint32_t num, uint32_t imm, const uint32_t *data);
struct GsVertex;
extern void __real_GsGpu_Draw(int type, int ctx, const struct GsVertex *v);
extern void __real_GsGpu_DrawVu0(int layer, int ctx, const float *v, uint32_t n, const float *c);
extern void __real_GsGpu_DrawVu4(int ctx, const float *v, uint32_t n, const float *c);
extern void __real_GsGpu_DrawVu6(int ctx, const float *v, uint32_t n, const float *c);
extern uint32_t __real_Gs_PageHash(uint32_t page);
extern uint32_t gGsPageGen[512];

/* ------------------------------------------------------------------------------------------------ texturas e
   alvos falsos: só números de identificação, em ordem de criação */
static uintptr_t sNextTex = 16;
static uint64_t sHash = 1469598103934665603ull; /* o hash do quadro em curso */
static uint64_t sTotal = 1469598103934665603ull; /* o do jogo todo */
static unsigned sTexNovas, sTexBytes, sFrames;

static void mix(uint32_t w) {
    sHash = (sHash ^ w) * 1099511628211ull;
}
static void mix_words(const void *p, uint32_t words) {
    const uint32_t *w = (const uint32_t *)p;
    uint32_t i;
    for (i = 0; i < words; i++) {
        mix(w[i]);
    }
}

/* (F5, etapa 8, degrau 1) O conteúdo de cada textura decodificada, pelo número que fake_tex_create deu: o hash "normal" do
   quadro (BT3_GPU_NORMAL=1) troca o número de cada textura do desenho pelo conteúdo dela, e assim duas rodadas que decodificam
   um número diferente de texturas (uma que não refaz a que já tinha) se comparam pelo que cada desenho de fato amostra. */
static uint64_t *sConteudo;
static uintptr_t sConteudoN, sConteudoCap;

static int fake_init(void) { return 1; }
static GsTex fake_white(void) { return 1; }
static GsTex fake_tex_create(uint32_t w, uint32_t h, int format, int levels) {
    (void)w; (void)h; (void)format; (void)levels;
    if (sNextTex - 16 >= sConteudoCap) {
        sConteudoCap = sConteudoCap != 0 ? sConteudoCap * 2 : 4096;
        sConteudo = (uint64_t *)realloc(sConteudo, sConteudoCap * sizeof(uint64_t));
    }
    sConteudo[sNextTex - 16] = 0;
    sConteudoN = sNextTex - 16 + 1;
    return sNextTex++;
}
static void conteudo_mistura(GsTex tex, int level, uint32_t w, uint32_t h, int format, const void *px, uint32_t bytes) {
    uint64_t c;
    const uint32_t *p = (const uint32_t *)px;
    uint32_t i, ch[5];
    if (tex < 16 || tex - 16 >= sConteudoN) {
        return;
    }
    c = sConteudo[tex - 16] * 1099511628211ull + 0x9E3779B97F4A7C15ull;
    ch[0] = (uint32_t)level; ch[1] = w; ch[2] = h; ch[3] = (uint32_t)format; ch[4] = bytes;
    for (i = 0; i < 5; i++) {
        c = (c ^ ch[i]) * 1099511628211ull;
    }
    for (i = 0; i < bytes / 4; i++) {
        c = (c ^ p[i]) * 1099511628211ull;
    }
    sConteudo[tex - 16] = c;
}
/* o conteúdo de cada textura decodificada entra no hash: a decodificação (texture_get) faz parte do que se prova */
static int fake_tex_upload(GsTex tex, int level, uint32_t w, uint32_t h, int format, const void *px, uint32_t bytes) {
    mix((uint32_t)tex);
    mix((uint32_t)level);
    mix(w);
    mix(h);
    mix((uint32_t)format);
    mix(bytes);
    mix_words(px, bytes / 4);
    conteudo_mistura(tex, level, w, h, format, px, bytes);
    sTexNovas++;
    sTexBytes += bytes;
    free((void *)px); /* a posse é do backend */
    return 1;
}
/* (F5, etapa 8, degrau 2, etapa 3) A textura solta fica "envenenada": todo desenho que ainda a use é contado e dito (o
   verificador da fila de liberação, o risco 4 do inventário). Os números das texturas nunca se repetem (sNextTex só cresce);
   um mapa de bits fixo, sem realloc, porque com a divisão quem solta e quem confere é a thread de desenho, e quem cria é a
   de gravação. */
#define VENENO_N (1u << 24)
static uint8_t sVeneno[VENENO_N / 8];
static unsigned sUsadasSoltas;
static void envenena(GsTex tex) {
    if (tex < VENENO_N) {
        sVeneno[tex >> 3] |= (uint8_t)(1u << (tex & 7));
    }
}
static int envenenada(GsTex tex) {
    return tex < VENENO_N && (sVeneno[tex >> 3] >> (tex & 7) & 1) != 0;
}
/* sem a divisão: o pedido e a liberação são a mesma chamada, na gravação */
static void fake_tex_destroy(GsTex tex) { mix(0xDE57u); mix((uint32_t)tex); envenena(tex); }
static int fake_slots(void) { return 1 << 20; }
static void fake_target_ensure(int i) { mix(0xE45Eu); mix((uint32_t)i); }
static void fake_target_drop(int i) { mix(0xD409u); mix((uint32_t)i); }
static GsTex fake_target_color(int i) { return 0x1000u + (uintptr_t)i; }
static GsTex fake_target_aux(int i) { return 0x2000u + (uintptr_t)i; }
static GsTex fake_target_depth(int i) { return 0x3000u + (uintptr_t)i; }
static void fake_scale(void) {}

/* os pipelines pela chave, em ordem de criação, com a busca linear do gs_d3d.cpp (pipe_get) */
static uint32_t sPipeKey[1024];
static int sPipeCount;
static int fake_pipe_get(uint32_t key) {
    int i;
    for (i = 0; i < sPipeCount; i++) {
        if (sPipeKey[i] == key) {
            return i;
        }
    }
    if (sPipeCount == 1024) {
        return 0;
    }
    sPipeKey[sPipeCount] = key;
    return sPipeCount++;
}

/* ------------------------------------------------------------------------------------------------ o fim do quadro */
/* o hash do quadro em seções (cada uma com a sua semente), para dizer onde duas builds divergem. (degrau 2) As seções
   depois da S_TEX se fazem num hash próprio (sH), do lado do desenho: com a divisão, o sHash continua recebendo, na
   gravação, as texturas do quadro seguinte. */
enum { S_TEX, S_CONTAGENS, S_DESENHOS, S_VERTICES, S_VUVERT, S_VUIDX, S_VUUNI, S_N };
static uint64_t sSecao[S_N];
static uint64_t sH = 1469598103934665603ull;
static void hmix(uint32_t w) {
    sH = (sH ^ w) * 1099511628211ull;
}
static void hmix_words(const void *p, uint32_t words) {
    const uint32_t *w = (const uint32_t *)p;
    uint32_t i;
    for (i = 0; i < words; i++) {
        hmix(w[i]);
    }
}
static void fecha(int k) {
    sSecao[k] = sH;
    sH = 1469598103934665603ull;
}

/* o hash "normal" dos desenhos (BT3_GPU_NORMAL): cada textura vale pelo conteúdo. Lê o sConteudo, que a gravação aumenta
   (realloc): com a divisão é feito na entrega, na gravação */
static uint64_t normal_desenhos(const GsDraw *draws, uint32_t n) {
    uint64_t d = 1469598103934665603ull;
    uint32_t i, k;
    for (i = 0; i < n; i++) {
        GsDraw g = draws[i];
        const uint32_t *w;
        if (!g.tex_is_target && g.tex >= 16 && g.tex - 16 < sConteudoN) {
            g.tex = (GsTex)(sConteudo[g.tex - 16] ^ sConteudo[g.tex - 16] >> 32); /* o conteúdo no lugar do número */
        }
        w = (const uint32_t *)&g;
        for (k = 0; k < (uint32_t)(sizeof(GsDraw) / 4); k++) {
            d = (d ^ w[k]) * 1099511628211ull;
        }
    }
    return d;
}

/* (degrau 2) o quadro que o fim do quadro lê: o gsQuadro nas builds com a divisão (as listas vivas sem ela), as listas
   vivas nas outras */
typedef struct Vista {
    const GsDraw *draws;
    uint32_t drawCount;
    const Vtx *verts;
    uint32_t vertCount;
    const float *vuVerts;
    uint32_t vuVertCount;
    const uint32_t *vuIdx;
    uint32_t vuIdxCount;
    const Vu0Uniform *vuUni;
    uint32_t vuUniCount;
    GsTarget *targets;
    int targetCount;
    unsigned quadro;
    int anchor;
    unsigned skipped, native;
    int dividido;
} Vista;
/* o que, com a divisão, a gravação manda na entrega (fake_entrega): uma entrega por vez, o desenho lê depois do sinal */
static struct {
    uint64_t tex, normal;
    unsigned novas;
    int pipes;
    uint32_t bytes[8];
} sEntregue;

static void fake_frame_end(void) {
    uint32_t i;
    uint64_t quadro = 1469598103934665603ull;
    Vista q;
    unsigned novas;
    int pipes;
    const uint32_t *bytes;
#ifdef BT3_GS_ESPERAS
    const GsQuadro *g = gsQuadro;
    q.draws = g->draws; q.drawCount = g->drawCount; q.verts = g->verts; q.vertCount = g->vertCount;
    q.vuVerts = g->vuVerts; q.vuVertCount = g->vuVertCount; q.vuIdx = g->vuIdx; q.vuIdxCount = g->vuIdxCount;
    q.vuUni = g->vuUni; q.vuUniCount = g->vuUniCount; q.targets = g->targets; q.targetCount = g->targetCount;
    q.quadro = g->quadro; q.anchor = g->anchor; q.skipped = g->skipped; q.native = g->native; q.dividido = g->dividido;
#else
    q.draws = gsDraws; q.drawCount = gsDrawCount; q.verts = gsVerts; q.vertCount = gsVertCount;
    q.vuVerts = gsVuVerts; q.vuVertCount = gsVuVertCount; q.vuIdx = gsVuIdx; q.vuIdxCount = gsVuIdxCount;
    q.vuUni = gsVuUni; q.vuUniCount = gsVuUniCount; q.targets = gsTargets; q.targetCount = gsTargetCount;
    q.quadro = gGsFrame; q.anchor = gsAnchor; q.skipped = gsSkipped; q.native = gsNative; q.dividido = 0;
#endif

    if (q.dividido) { /* a S_TEX e as contagens da gravação vieram na entrega */
        sSecao[S_TEX] = sEntregue.tex;
        novas = sEntregue.novas;
        pipes = sEntregue.pipes;
        bytes = sEntregue.bytes;
    } else {
        sSecao[S_TEX] = sHash; /* o que os backends receberam desde o quadro anterior (texturas, alvos): já no sHash */
        sHash = 1469598103934665603ull;
        novas = sTexNovas;
        pipes = sPipeCount;
        bytes = gGsBytes;
    }
    sH = 1469598103934665603ull;
    hmix(q.quadro);
    hmix(q.drawCount);
    hmix(q.vertCount);
    hmix(q.vuVertCount);
    hmix(q.vuIdxCount);
    hmix(q.vuUniCount);
    hmix((uint32_t)q.anchor);
    hmix(q.skipped);
    hmix(q.native);
    hmix((uint32_t)q.targetCount);
    for (i = 0; i < (uint32_t)q.targetCount; i++) {
        hmix(q.targets[i].fbp);
        hmix((uint32_t)q.targets[i].cleared);
        hmix(q.targets[i].draws);
        hmix(q.targets[i].gen);
        hmix((uint32_t)q.targets[i].stale);
    }
    for (i = 0; i < (uint32_t)pipes; i++) { /* (o sPipeKey só cresce: abaixo da contagem da entrega não muda) */
        hmix(sPipeKey[i]);
    }
    fecha(S_CONTAGENS);
    hmix_words(q.draws, q.drawCount * (uint32_t)(sizeof(GsDraw) / 4));
    fecha(S_DESENHOS);
    for (i = 0; i < q.vertCount; i++) { /* o Vtx por campo: os quatro bytes de cor não são uma palavra do host */
        const Vtx *v = &q.verts[i];
        hmix_words(&v->x, 3);
        hmix((uint32_t)v->r | (uint32_t)v->g << 8 | (uint32_t)v->b << 16 | (uint32_t)v->a << 24);
        hmix_words(&v->s, 3);
    }
    if (getenv("BT3_GPU_DUMP") != NULL && (unsigned)atoi(getenv("BT3_GPU_DUMP")) == q.quadro) { /* os vértices de um quadro, para ver onde duas builds divergem */
        for (i = 0; i < q.vertCount; i++) {
            const Vtx *v = &q.verts[i];
            uint32_t w[6];
            memcpy(w, &v->x, 12);
            memcpy(w + 3, &v->s, 12);
            fprintf(stderr, "gpu_dump: %u %08x %08x %08x %02x%02x%02x%02x %08x %08x %08x\n", i, w[0], w[1], w[2], v->r, v->g, v->b, v->a, w[3], w[4], w[5]);
        }
    }
    fecha(S_VERTICES);
    hmix_words(q.vuVerts, q.vuVertCount * 12);
    fecha(S_VUVERT);
    hmix_words(q.vuIdx, q.vuIdxCount);
    fecha(S_VUIDX);
    hmix_words(q.vuUni, q.vuUniCount * (uint32_t)(sizeof(Vu0Uniform) / 4));
    fecha(S_VUUNI);
    for (i = 0; i < S_N; i++) {
        quadro = (quadro ^ sSecao[i]) * 1099511628211ull;
    }
    sTotal = (sTotal ^ quadro) * 1099511628211ull;
    if (getenv("BT3_GPU_LISTA") != NULL) {
        fprintf(stderr, "gpu_falso: quadro %u lista %016llx desenhos %u vértices %u vu %u/%u/%u texturas %u secoes", q.quadro,
                (unsigned long long)quadro, q.drawCount, q.vertCount, q.vuVertCount, q.vuIdxCount, q.vuUniCount, novas);
        for (i = 0; i < S_N; i++) {
            fprintf(stderr, " %08x", (unsigned)(sSecao[i] >> 32 ^ sSecao[i]));
        }
        fprintf(stderr, "\n");
    }
    if (getenv("BT3_GPU_BYTES") != NULL) { /* (F5, degrau 1) o que a cadeia leva por quadro: gs_core.c, bytes_walk */
        fprintf(stderr, "gpu_falso: bytes quadro %u tags %u lista %u externa %u ref %u refunico %u imagem %u vu %u/%u/%u\n", q.quadro,
                bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], q.vuVertCount, q.vuIdxCount, q.vuUniCount);
    }
    if (getenv("BT3_GPU_NORMAL") != NULL) { /* (F5, degrau 1) o quadro sem o que depende de quantas texturas se decodificaram */
        uint64_t n = 1469598103934665603ull, d = q.dividido ? sEntregue.normal : normal_desenhos(q.draws, q.drawCount);
        n = (n ^ sSecao[S_CONTAGENS]) * 1099511628211ull;
        n = (n ^ d) * 1099511628211ull;
        for (i = S_VERTICES; i < S_N; i++) {
            n = (n ^ sSecao[i]) * 1099511628211ull;
        }
        fprintf(stderr, "gpu_falso: normal quadro %u %016llx\n", q.quadro, (unsigned long long)n);
    }
    for (i = 0; i < q.drawCount; i++) { /* (degrau 2) o verificador da fila de liberação */
        const GsDraw *d = &q.draws[i];
        if (!d->tex_is_target && d->tex >= 16 && envenenada(d->tex)) {
            if (sUsadasSoltas++ < 20) {
                fprintf(stderr, "gpu_falso: textura solta usada: quadro %u desenho %u textura %u\n", q.quadro, i, (unsigned)d->tex);
            }
        }
    }
    sFrames++;
    /* (F5, etapa 8, degrau 2, etapa 3) o cleared ligado como o gs_gpu.c o liga ao refazer a lista: todo alvo em que o quadro
       desenha primitivas (native 0, a VU1 inclusive) ou recebe uma imagem enviada (native 100 e 101); as nativas 1 a 5 não.
       Sem isto o caminho do alvo como textura (gs_draw.c, draw_state: gsTargets[src].cleared) nunca aparecia no WSL */
    /* (com a divisão, na cópia da tabela que veio no retrato: o cleared do desenho; o da gravação ela liga na entrega) */
    for (i = 0; i < q.drawCount; i++) {
        if (q.draws[i].native == 0 || q.draws[i].native >= 100) {
            q.targets[q.draws[i].target].cleared = 1;
        }
    }
    if (q.dividido) { /* os contadores, a gravação zerou na entrega */
        return;
    }
    /* o que o gs_gpu.c zera ao fim do quadro */
    for (i = 0; i < (uint32_t)gsTargetCount; i++) {
        gsTargets[i].draws = 0;
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

#ifdef BT3_GS_ESPERAS
/* (degrau 2, etapa 3) com a divisão, na gravação: o que o desenho do retrato precisa e que a gravação segue mudando (a S_TEX
   fechada no ponto em que o fim do quadro a fechava, as contagens, os bytes da cadeia, o hash normal dos desenhos); e a
   regra do gs_gpu.c para o cleared da gravação: também o alvo que recebe uma imagem enviada (native 100 e 101) */
static void fake_entrega(const GsQuadro *q, unsigned char *limpa) {
    uint32_t i;
    sEntregue.tex = sHash;
    sHash = 1469598103934665603ull;
    sEntregue.novas = sTexNovas;
    sEntregue.pipes = sPipeCount;
    memcpy(sEntregue.bytes, gGsBytes, sizeof(sEntregue.bytes));
    sEntregue.normal = getenv("BT3_GPU_NORMAL") != NULL ? normal_desenhos(q->draws, q->drawCount) : 0;
    for (i = 0; i < q->drawCount; i++) {
        if (q->draws[i].native >= 100 && q->draws[i].target >= 0 && q->draws[i].target < MAX_TARGETS) {
            limpa[q->draws[i].target] = 1;
        }
    }
}
/* o pedido, na gravação: entra no hash onde o texDestroy e o targetDrop entravam */
static void fake_solta_pede(GsTex tex, int alvo) {
    if (alvo < 0) {
        mix(0xDE57u);
        mix((uint32_t)tex);
    } else {
        mix(0xD409u);
        mix((uint32_t)alvo);
    }
}
/* a liberação, no desenho, depois do quadro do pedido (os alvos do falso não têm recurso a soltar) */
static void fake_solta(GsTex tex, int alvo) {
    if (alvo < 0) {
        envenena(tex);
    }
}
#define FAKE_DIVISAO , fake_entrega, fake_solta_pede, fake_solta
#else
#define FAKE_DIVISAO
#endif

GsBackend sVulkanBackend = {"falso", fake_init, fake_frame_end, fake_scale, fake_white, fake_tex_create, fake_tex_upload,
                            fake_tex_destroy, fake_slots, fake_target_ensure, fake_target_drop, fake_target_color,
                            fake_target_aux, fake_target_depth, fake_pipe_get FAKE_DIVISAO};
GsBackend sGlBackend = {"falso", fake_init, fake_frame_end, fake_scale, fake_white, fake_tex_create, fake_tex_upload,
                        fake_tex_destroy, fake_slots, fake_target_ensure, fake_target_drop, fake_target_color,
                        fake_target_aux, fake_target_depth, fake_pipe_get FAKE_DIVISAO};

/* (F5, etapa 3) os descartes da janela medida (passos.c): o backend falso não descarta nada */
int Passos_Descartes(unsigned *v, int n) { (void)v; (void)n; return 0; }

/* sem pacote de texturas (o 360 também não tem) */
void TexPack_Init(void) {}
int TexPack_Count(void) { return 0; }
const char *TexPack_Lookup(uint32_t tbp, uint32_t tbw, uint32_t psm, uint32_t tw, uint32_t th, uint32_t tcc, uint32_t cbp,
                           uint32_t cpsm, uint32_t *maxAlpha) {
    (void)tbp; (void)tbw; (void)psm; (void)tw; (void)th; (void)tcc; (void)cbp; (void)cpsm; (void)maxAlpha;
    return NULL;
}
int TexPack_Load(const char *path, TexPackImage *img) { (void)path; (void)img; return 0; }
void TexPack_Free(TexPackImage *img) { (void)img; }

/* ------------------------------------------------------------------------------------------------ a medida */
enum { F_P0, F_P1, F_P2, F_P8, F_P7, F_P4, F_P6, F_OUTROS, F_N };
static const char *const kFam[F_N] = {"p0 (lutador)", "p1 (lutador, 2a passada)", "p2a/2b (sombra do lutador)", "p8 (objeto animado)",
                                      "p7 (destroços)", "p4 (estágio)", "p6 (sombra no chão)", "sem desenho (MSCALF, cabeçalho, outros)"};
static struct { uint64_t chamadas, tiques, desenhos, tiques_desenho, vertices, indices; } sFam[F_N];
static uint64_t sUnpackTiques, sUnpackChamadas;
static uint64_t sHashChamadas, sHashRecalculos, sHashIguais; /* Gs_PageHash: pedidos e páginas de 8 KB de fato refeitas (espelha o cache dele) */
static int sPerfil = -1;
static int sFamAtual;   /* a família do último desenho da chamada em curso */
static int sDentro;     /* dentro do GsVu1_Call (as amostras só contam aí) */

#ifdef __i386__
static inline uint64_t tique(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
#else
static inline uint64_t tique(void) { return 0; }
#endif

#define AMOSTRAS_MAX 65536
static struct { uint32_t pc, n; } sAmostra[AMOSTRAS_MAX];
static unsigned sAmostraN, sAmostrasFora, sAmostrasTodas;
#ifdef __i386__
static void amostra(int sig, siginfo_t *si, void *uc) {
    uint32_t pc = (uint32_t)((ucontext_t *)uc)->uc_mcontext.gregs[REG_EIP], h, k;
    (void)sig; (void)si;
    sAmostrasTodas++;
    if (!sDentro) {
        return;
    }
    h = (pc * 2654435761u) % AMOSTRAS_MAX;
    for (k = 0; k < AMOSTRAS_MAX; k++, h = (h + 1) % AMOSTRAS_MAX) {
        if (sAmostra[h].n == 0) {
            sAmostra[h].pc = pc;
        }
        if (sAmostra[h].pc == pc) {
            sAmostra[h].n++;
            sAmostraN++;
            return;
        }
    }
    sAmostrasFora++;
}
#endif

/* (F5, etapa 8, degrau 1) só existe nas builds com a espera do jogo pela thread de gravação (gs_core.c) */
extern void Port_GsEspera(void) __attribute__((weak));
extern unsigned gGsFrameJogo __attribute__((weak)); /* o contador de quadros do lado do jogo (avança na entrega) */
extern void GsDraw_DesenhoEspera(void) __attribute__((weak)); /* (degrau 2) gs_draw.c, nas builds com a divisão */
extern void GsDraw_DesenhoResumo(void) __attribute__((weak));

static void relatorio(void) {
    int f;
    const char *saida = getenv("BT3_PERFIL_SAIDA");
    FILE *fp = saida != NULL ? fopen(saida, "w") : stderr;
    uint32_t i;

    if (Port_GsEspera != NULL) {
        Port_GsEspera(); /* o último quadro entregue acaba de ser gravado antes do resumo (o atexit do backend roda antes do do gs_core) */
        if (GsDraw_DesenhoEspera != NULL) {
            GsDraw_DesenhoEspera(); /* (degrau 2) e desenhado */
            GsDraw_DesenhoResumo();
        }
        /* depois da espera os dois contadores são o mesmo número: o do jogo avançou na entrega, o do desenho no fim da gravação */
        fprintf(stderr, "gpu_falso: contador de quadros do jogo %u, do desenho %u\n", gGsFrameJogo, gGsFrame);
    }
    fprintf(stderr, "gpu_falso: %u quadros, hash do jogo %016llx, %u texturas decodificadas (%u KB), %d pipelines\n", sFrames,
            (unsigned long long)sTotal, sTexNovas, sTexBytes / 1024, sPipeCount);
    fprintf(stderr, "gpu_falso: %u desenhos com textura já solta\n", sUsadasSoltas);
    if (sPerfil <= 0 || fp == NULL) {
        return;
    }
    for (f = 0; f < F_N; f++) {
        if (sFam[f].chamadas != 0) {
            fprintf(fp, "perfil: fam %d | %s | chamadas %llu | tiques %llu | desenhos %llu | tiques_desenho %llu | vertices %llu | indices %llu\n",
                    f, kFam[f], (unsigned long long)sFam[f].chamadas, (unsigned long long)sFam[f].tiques,
                    (unsigned long long)sFam[f].desenhos, (unsigned long long)sFam[f].tiques_desenho,
                    (unsigned long long)sFam[f].vertices, (unsigned long long)sFam[f].indices);
        }
    }
    fprintf(fp, "perfil: pagehash | pedidos %llu | recalculos %llu | recalculos de conteudo igual %llu\n", (unsigned long long)sHashChamadas, (unsigned long long)sHashRecalculos, (unsigned long long)sHashIguais);
    fprintf(fp, "perfil: unpack | chamadas %llu | tiques %llu\n", (unsigned long long)sUnpackChamadas, (unsigned long long)sUnpackTiques);
    fprintf(fp, "perfil: amostras %u dentro do GsVu1_Call de %u (fora da tabela %u)\n", sAmostraN, sAmostrasTodas, sAmostrasFora);
    for (i = 0; i < AMOSTRAS_MAX; i++) {
        if (sAmostra[i].n != 0) {
            fprintf(fp, "perfil: pc 0x%08x %u\n", sAmostra[i].pc, sAmostra[i].n);
        }
    }
    if (fp != stderr) {
        fclose(fp);
    }
}

static void perfil_inicia(void) {
    const char *e = getenv("BT3_PERFIL");
    sPerfil = e != NULL ? atoi(e) : 0;
    atexit(relatorio);
#ifdef __i386__
    if (sPerfil >= 2) {
        struct sigaction sa;
        struct sigevent ev;
        struct itimerspec it;
        timer_t tm;
        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = amostra;
        sa.sa_flags = SA_SIGINFO | SA_RESTART;
        sigaction(SIGPROF, &sa, NULL);
        /* o relógio monotônico (de alta resolução) em intervalos curtos: o de CPU e o setitimer só disparam de 4 em 4 ms neste kernel */
        memset(&ev, 0, sizeof(ev));
        ev.sigev_notify = SIGEV_SIGNAL;
        ev.sigev_signo = SIGPROF;
        if (timer_create(CLOCK_MONOTONIC, &ev, &tm) == 0) {
            it.it_interval.tv_sec = 0;
            it.it_interval.tv_nsec = (getenv("BT3_PERFIL_US") != NULL ? atoi(getenv("BT3_PERFIL_US")) : 100) * 1000L;
            it.it_value = it.it_interval;
            timer_settime(tm, 0, &it, NULL);
        }
    }
#endif
}

/* BT3_PERFIL_JANELA=A,B: só os quadros do GS de A a B-1 passam pela VU1 (antes de A a chamada é pulada: o jogo não
   depende dela, e a lista nem o log do qemu crescem; em B o programa termina; o Unpack e o GsGpu_Draw também são pulados antes de A, e o Draw sempre). Serve à contagem de instruções da PPC no
   qemu (vu1_perfil.sh), onde cada instrução vira uma linha de log. Fora disso não existe. Não vale para a prova do hash. */
static int sJanelaA = -1, sJanelaB;
static void janela_inicia(void) {
    const char *e = getenv("BT3_PERFIL_JANELA");
    sJanelaA = 0;
    sJanelaB = 0;
    if (e != NULL && sscanf(e, "%d,%d", &sJanelaA, &sJanelaB) != 2) {
        sJanelaA = sJanelaB = 0;
    }
}

/* Na janela, só a VU1 conta: os desenhos de CPU (GsGpu_Draw, que também passam por draw_state) são pulados. */
void __wrap_GsGpu_Draw(int type, int ctx, const struct GsVertex *v) {
    if (sJanelaA < 0) {
        janela_inicia();
    }
    if (sJanelaB == 0) {
        __real_GsGpu_Draw(type, ctx, v);
    }
}

void __wrap_GsVu1_Call(int addr) {
    uint64_t t0, t1;
    if (sJanelaA < 0) {
        janela_inicia();
    }
    if (sJanelaB != 0) {
        if ((int)gGsFrame < sJanelaA) {
            return;
        }
        if ((int)gGsFrame >= sJanelaB) {
            fprintf(stderr, "gpu_falso: fim da janela de %d a %d\n", sJanelaA, sJanelaB);
            exit(0);
        }
    }
    if (sPerfil < 0) {
        perfil_inicia();
    }
    if (sPerfil <= 0) {
        __real_GsVu1_Call(addr);
        return;
    }
    if (sPerfil == 3) { /* só a amostragem, sem o relógio: as amostras não pagam a medida */
        sDentro = 1;
        __real_GsVu1_Call(addr);
        sDentro = 0;
        return;
    }
    sFamAtual = F_OUTROS;
    sDentro = 1;
    t0 = tique();
    __real_GsVu1_Call(addr);
    t1 = tique();
    sDentro = 0;
    sFam[sFamAtual].chamadas++;
    sFam[sFamAtual].tiques += t1 - t0;
}

void __wrap_GsVu1_Unpack(uint32_t cmd, uint32_t num, uint32_t imm, const uint32_t *data) {
    uint64_t t0;
    if (sJanelaA < 0) {
        janela_inicia();
    }
    if (sJanelaB != 0 && (int)gGsFrame < sJanelaA) {
        return; /* (antes da janela as chamadas da VU1 são puladas, os dados que ela receberia não servem) */
    }
    if (sPerfil < 0) {
        perfil_inicia();
    }
    if (sPerfil <= 0 || sPerfil == 3) {
        __real_GsVu1_Unpack(cmd, num, imm, data);
        return;
    }
    t0 = tique();
    __real_GsVu1_Unpack(cmd, num, imm, data);
    sUnpackTiques += tique() - t0;
    sUnpackChamadas++;
}

static void conta(int fam, uint64_t t0, uint32_t n, uint32_t idx0) {
    sFam[fam].desenhos++;
    sFam[fam].tiques_desenho += tique() - t0;
    sFam[fam].vertices += n;
    sFam[fam].indices += gsVuIdxCount - idx0; /* (0 se o desenho foi recusado; a lista só cresce) */
    sFamAtual = fam;
}

void __wrap_GsGpu_DrawVu0(int layer, int ctx, const float *v, uint32_t n, const float *c) {
    uint64_t t0;
    uint32_t i0 = gsVuIdxCount;
    if (sPerfil <= 0 || sPerfil == 3) {
        __real_GsGpu_DrawVu0(layer, ctx, v, n, c);
        return;
    }
    t0 = tique();
    __real_GsGpu_DrawVu0(layer, ctx, v, n, c);
    conta(layer == 5 ? F_P1 : layer == 2 ? F_P2 : layer == 3 ? F_P8 : layer == 4 ? F_P7 : F_P0, t0, n, i0);
}
void __wrap_GsGpu_DrawVu4(int ctx, const float *v, uint32_t n, const float *c) {
    uint64_t t0;
    uint32_t i0 = gsVuIdxCount;
    if (sPerfil <= 0 || sPerfil == 3) {
        __real_GsGpu_DrawVu4(ctx, v, n, c);
        return;
    }
    t0 = tique();
    __real_GsGpu_DrawVu4(ctx, v, n, c);
    conta(F_P4, t0, n, i0);
}
void __wrap_GsGpu_DrawVu6(int ctx, const float *v, uint32_t n, const float *c) {
    uint64_t t0;
    uint32_t i0 = gsVuIdxCount;
    if (sPerfil <= 0 || sPerfil == 3) {
        __real_GsGpu_DrawVu6(ctx, v, n, c);
        return;
    }
    t0 = tique();
    __real_GsGpu_DrawVu6(ctx, v, n, c);
    conta(F_P6, t0, n, i0);
}

uint32_t __wrap_Gs_PageHash(uint32_t page) {
    static uint32_t seen[512], antes[512];
    static uint8_t valid[512];
    uint32_t h;
    if (sPerfil > 0 && sPerfil != 3) {
        sHashChamadas++;
        page &= 511;
        h = __real_Gs_PageHash(page);
        if (!(valid[page] && seen[page] == gGsPageGen[page])) {
            sHashRecalculos++;
            if (valid[page] && antes[page] == h) {
                sHashIguais++; /* o envio mexeu na página mas o conteúdo ficou o mesmo */
            }
            valid[page] = 1;
            seen[page] = gGsPageGen[page];
            antes[page] = h;
        }
        return h;
    }
    return __real_Gs_PageHash(page);
}
