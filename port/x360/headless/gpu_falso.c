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
static void fake_tex_destroy(GsTex tex) { mix(0xDE57u); mix((uint32_t)tex); }
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
/* o hash do quadro em seções (cada uma com a sua semente), para dizer onde duas builds divergem */
enum { S_TEX, S_CONTAGENS, S_DESENHOS, S_VERTICES, S_VUVERT, S_VUIDX, S_VUUNI, S_N };
static uint64_t sSecao[S_N];
static void fecha(int k) {
    sSecao[k] = sHash;
    sHash = 1469598103934665603ull;
}

static void fake_frame_end(void) {
    uint32_t i;
    uint64_t quadro = 1469598103934665603ull;

    fecha(S_TEX); /* o que os backends receberam desde o quadro anterior (texturas, alvos): já no sHash */
    mix(gGsFrame);
    mix(gsDrawCount);
    mix(gsVertCount);
    mix(gsVuVertCount);
    mix(gsVuIdxCount);
    mix(gsVuUniCount);
    mix((uint32_t)gsAnchor);
    mix(gsSkipped);
    mix(gsNative);
    mix((uint32_t)gsTargetCount);
    for (i = 0; i < (uint32_t)gsTargetCount; i++) {
        mix(gsTargets[i].fbp);
        mix((uint32_t)gsTargets[i].cleared);
        mix(gsTargets[i].draws);
        mix(gsTargets[i].gen);
        mix((uint32_t)gsTargets[i].stale);
    }
    for (i = 0; i < (uint32_t)sPipeCount; i++) {
        mix(sPipeKey[i]);
    }
    fecha(S_CONTAGENS);
    mix_words(gsDraws, gsDrawCount * (uint32_t)(sizeof(GsDraw) / 4));
    fecha(S_DESENHOS);
    for (i = 0; i < gsVertCount; i++) { /* o Vtx por campo: os quatro bytes de cor não são uma palavra do host */
        const Vtx *v = &gsVerts[i];
        mix_words(&v->x, 3);
        mix((uint32_t)v->r | (uint32_t)v->g << 8 | (uint32_t)v->b << 16 | (uint32_t)v->a << 24);
        mix_words(&v->s, 3);
    }
    if (getenv("BT3_GPU_DUMP") != NULL && (unsigned)atoi(getenv("BT3_GPU_DUMP")) == gGsFrame) { /* os vértices de um quadro, para ver onde duas builds divergem */
        for (i = 0; i < gsVertCount; i++) {
            const Vtx *v = &gsVerts[i];
            uint32_t w[6];
            memcpy(w, &v->x, 12);
            memcpy(w + 3, &v->s, 12);
            fprintf(stderr, "gpu_dump: %u %08x %08x %08x %02x%02x%02x%02x %08x %08x %08x\n", i, w[0], w[1], w[2], v->r, v->g, v->b, v->a, w[3], w[4], w[5]);
        }
    }
    fecha(S_VERTICES);
    mix_words(gsVuVerts, gsVuVertCount * 12);
    fecha(S_VUVERT);
    mix_words(gsVuIdx, gsVuIdxCount);
    fecha(S_VUIDX);
    mix_words(gsVuUni, gsVuUniCount * (uint32_t)(sizeof(Vu0Uniform) / 4));
    fecha(S_VUUNI);
    for (i = 0; i < S_N; i++) {
        quadro = (quadro ^ sSecao[i]) * 1099511628211ull;
    }
    sTotal = (sTotal ^ quadro) * 1099511628211ull;
    if (getenv("BT3_GPU_LISTA") != NULL) {
        fprintf(stderr, "gpu_falso: quadro %u lista %016llx desenhos %u vértices %u vu %u/%u/%u texturas %u secoes", gGsFrame,
                (unsigned long long)quadro, gsDrawCount, gsVertCount, gsVuVertCount, gsVuIdxCount, gsVuUniCount, sTexNovas);
        for (i = 0; i < S_N; i++) {
            fprintf(stderr, " %08x", (unsigned)(sSecao[i] >> 32 ^ sSecao[i]));
        }
        fprintf(stderr, "\n");
    }
    if (getenv("BT3_GPU_BYTES") != NULL) { /* (F5, degrau 1) o que a cadeia leva por quadro: gs_core.c, bytes_walk */
        fprintf(stderr, "gpu_falso: bytes quadro %u tags %u lista %u externa %u ref %u refunico %u imagem %u vu %u/%u/%u\n", gGsFrame,
                gGsBytes[0], gGsBytes[1], gGsBytes[2], gGsBytes[3], gGsBytes[4], gGsBytes[5], gsVuVertCount, gsVuIdxCount, gsVuUniCount);
    }
    if (getenv("BT3_GPU_NORMAL") != NULL) { /* (F5, degrau 1) o quadro sem o que depende de quantas texturas se decodificaram */
        uint64_t n = 1469598103934665603ull, d = 1469598103934665603ull;
        uint32_t k;
        n = (n ^ sSecao[S_CONTAGENS]) * 1099511628211ull;
        for (i = 0; i < gsDrawCount; i++) {
            GsDraw g = gsDraws[i];
            const uint32_t *w;
            if (!g.tex_is_target && g.tex >= 16 && g.tex - 16 < sConteudoN) {
                g.tex = (GsTex)(sConteudo[g.tex - 16] ^ sConteudo[g.tex - 16] >> 32); /* o conteúdo no lugar do número */
            }
            w = (const uint32_t *)&g;
            for (k = 0; k < (uint32_t)(sizeof(GsDraw) / 4); k++) {
                d = (d ^ w[k]) * 1099511628211ull;
            }
        }
        n = (n ^ d) * 1099511628211ull;
        for (i = S_VERTICES; i < S_N; i++) {
            n = (n ^ sSecao[i]) * 1099511628211ull;
        }
        fprintf(stderr, "gpu_falso: normal quadro %u %016llx\n", gGsFrame, (unsigned long long)n);
    }
    sFrames++;
    /* (F5, etapa 8, degrau 2, etapa 3) o cleared ligado como o gs_gpu.c o liga ao refazer a lista: todo alvo em que o quadro
       desenha primitivas (native 0, a VU1 inclusive) ou recebe uma imagem enviada (native 100 e 101); as nativas 1 a 5 não.
       Sem isto o caminho do alvo como textura (gs_draw.c, draw_state: gsTargets[src].cleared) nunca aparecia no WSL */
    for (i = 0; i < gsDrawCount; i++) {
        if (gsDraws[i].native == 0 || gsDraws[i].native >= 100) {
            gsTargets[gsDraws[i].target].cleared = 1;
        }
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

GsBackend sVulkanBackend = {"falso", fake_init, fake_frame_end, fake_scale, fake_white, fake_tex_create, fake_tex_upload,
                            fake_tex_destroy, fake_slots, fake_target_ensure, fake_target_drop, fake_target_color,
                            fake_target_aux, fake_target_depth, fake_pipe_get};
GsBackend sGlBackend = {"falso", fake_init, fake_frame_end, fake_scale, fake_white, fake_tex_create, fake_tex_upload,
                        fake_tex_destroy, fake_slots, fake_target_ensure, fake_target_drop, fake_target_color,
                        fake_target_aux, fake_target_depth, fake_pipe_get};

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

static void relatorio(void) {
    int f;
    const char *saida = getenv("BT3_PERFIL_SAIDA");
    FILE *fp = saida != NULL ? fopen(saida, "w") : stderr;
    uint32_t i;

    if (Port_GsEspera != NULL) {
        Port_GsEspera(); /* o último quadro entregue acaba de ser gravado antes do resumo (o atexit do backend roda antes do do gs_core) */
        /* depois da espera os dois contadores são o mesmo número: o do jogo avançou na entrega, o do desenho no fim da gravação */
        fprintf(stderr, "gpu_falso: contador de quadros do jogo %u, do desenho %u\n", gGsFrameJogo, gGsFrame);
    }
    fprintf(stderr, "gpu_falso: %u quadros, hash do jogo %016llx, %u texturas decodificadas (%u KB), %d pipelines\n", sFrames,
            (unsigned long long)sTotal, sTexNovas, sTexBytes / 1024, sPipeCount);
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
