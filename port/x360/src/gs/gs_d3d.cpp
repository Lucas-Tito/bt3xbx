/*
 * F3, etapa 7: o backend D3D9 do Xbox 360 (GsBackend, port/src/gs/gs_draw.h), no lugar do gs_gl.c e do gs_gpu.c do
 * PC. A gravação do quadro é a do port (gs_draw.c: a lista de desenhos, os vértices, as texturas decodificadas da
 * memória do GS); este arquivo é dono dos objetos do D3D, refaz a lista no fim do quadro e apresenta.
 *
 * O que muda em relação ao PC, por causa do Xenos:
 *   - Os alvos de desenho moram na EDRAM (10 MB), não em texturas. Há UM par de superfícies na EDRAM, 1024 x 1024
 *     (o tamanho de um alvo do GS com SCALE = 1): cor em A8R8G8B8 na base 0 e profundidade D24S8 logo depois, cerca
 *     de 8 MB. Cada alvo do GS tem uma textura (tiled, A8R8G8B8) com o conteúdo dele. Desenhar num alvo: o que está
 *     na EDRAM volta para a textura do dono (Resolve, só o retângulo sujo), a EDRAM é limpa e recebe a textura do
 *     novo alvo (um retângulo desenhado). A profundidade não volta: um alvo que sai da EDRAM perde a dele (basta
 *     para o 2D dos menus; a luta, F5, vai precisar resolver a profundidade também).
 *   - Ler a cor de um alvo como textura (o jogo faz isso nos efeitos) é ler a textura do alvo, sempre em dia: um
 *     alvo nunca é lido enquanto é o desenhado (gs_draw.c recusa), então ele já saiu da EDRAM.
 *   - Sem o segundo alvo com o byte de alfa (outAux do gs.frag): o teste de alfa do destino (DATE) lê o alfa da
 *     própria cor, que guarda min(A / 0x80, 1) e chega a 1 exatamente com o bit 7 ligado. Os efeitos nativos que
 *     precisam do byte (contorno, tabela de profundidade, cópia de ids) e os programas de vértice (VU1) ficam para
 *     a F5, e as imagens copiadas direto para a memória de vídeo (filmes) para a F6: são contados e não desenhados.
 *   - Sem janela, painel de configurações nem pacote de texturas.
 *
 * O dispositivo é criado pela main (port/x360/src/f2/main360.cpp) e passa a ser desta thread (a do jogo) no init;
 * GsD3d_Soltar o devolve quando o jogo termina. O desenho e o present são na thread do jogo, um por quadro do GS.
 *
 * Saídas para conferência (port/x360/tools/compara_quadros.py, no WSL):
 *   BT3_SHOT=N        a imagem mostrada a cada N quadros do GS em game:\shots\gpu_NNNNN.ppm, no formato do
 *                     rasterizador de software (640 x 448, as colunas além da largura do quadro em preto), com o
 *                     número do quadro do GS dele; BT3_SHOT_FROM / BT3_SHOT_TO limitam a faixa;
 *   BT3_SHOT_LISTA=G,G...  só esses quadros do GS (até 32), com ou sem o BT3_SHOT: os das imagens da etapa 6 da F4
 *                     (a linha "imagem: ... gs G" da build sem janela), para medir os menus contra elas;
 *   o log (bt3xbx.log) a cada 60 quadros: tempo por quadro, desenhos, texturas novas, o que não foi desenhado, e a
 *                     memória por classe (F5, etapa 1).
 *
 * Memória (F5, etapa 1; o contrato de posse está em docs/x360/plano/F5/01.md): os bytes reais de cada classe
 * (texturas decodificadas, alvos, buffers fixos, o que espera a GPU na cova), um piso de memória física livre e um
 * teto para as texturas. Quando uma textura ou um alvo novo não cabe, o backend diz quanto falta e o frontend
 * (gs_draw.c, GsDraw_X360Abre) descarta as texturas do cache usadas há mais tempo. Ajustes no ambiente.txt:
 *   BT3_MEM_PISO_MB=32   memória física que fica sempre livre (o piso; abaixo dele, descarte e, sem o que
 *                        descartar, recusa com diagnóstico);
 *   BT3_MEM_TEX_MB=128   teto das texturas decodificadas, vivas e pendentes (acima dele, descarte; sem o que
 *                        descartar, passa do teto em vez de sair branca, enquanto o piso deixar);
 *   BT3_COR_FUNDO=0xFF010101  a cor da limpeza da tela fora da imagem 4:3 (as barras laterais). Quase preto de
 *                        propósito: com o preto exato (0xFF000000) a limpeza não acontece no console e as barras
 *                        ficam com o azul da fase da main360 (rodada da F5, etapa 1; 01.md).
 */
#include <xtl.h>
#include <xgraphics.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "diario.h"
extern "C" {
#include "gs_internal.h"
#include "gs_draw.h"
#include "gs_texpack.h"
extern D3DDevice *gX360Dev; /* main360.cpp */
int GsDraw_X360Abre(uint32_t w, uint32_t h, int modo); /* gs_draw.c: descarta texturas do cache até caber */
extern volatile unsigned gX360Luta; /* estado360.c: o vblank em que a luta começou (0: fora da luta) */
/* Para o vigia da main (main360.cpp): onde o fim do quadro está e quantos quadros já foram apresentados. */
volatile const char *gGsD3dEtapa = "antes do primeiro quadro";
volatile unsigned gGsD3dQuadros;
int Port_AspectMilli(void);  /* plat_stub.c */
}
#include "gs_vs.h" /* gerados pelo fxc no build (gs_d3d.hlsl): kGsVs, kGsPs, kCopiaVs, kCopiaPs */
#include "gs_ps.h"
#include "copia_vs.h"
#include "copia_ps.h"

#define TAM GS_W            /* lado de um alvo, em pixels (SCALE = 1) */
#define TELA_W 1280
#define TELA_H 720
#define EDRAM_TILES 2048    /* 10 MB em tiles de 5.120 bytes */
/* Vértices por DrawPrimitiveUP. Ele copia para o ring buffer secundário, e um bloco tem de caber em um quarto dele
   (documentação do BeginVertices: 512 KB com o padrão de 2 MB), além de ter menos de 65.535 vértices. A primeira
   versão usava 65.532 (1,8 MB com 28 bytes por vértice): os menus nunca passaram de 512 KB num desenho, e o console
   congelou no começo da luta. 16.380 vértices são 448 KB, múltiplo de 2, 3 e 6. */
#define MAX_UP 16380
#define MAX_COVA 4096
#define MB (1u << 20)

static D3DDevice *sDev;
static D3DVertexShader *sVsGs, *sVsCopia;
static D3DPixelShader *sPsGs, *sPsCopia;
static D3DVertexDeclaration *sDeclGs, *sDeclCopia;
static D3DSurface *sEdramCor, *sEdramProf, *sTela;
static D3DTexture *sBranco, *sDate;
static D3DTexture *sDateFonte; /* o que os desenhos com DATE leem: sDate, ou a textura de um alvo fora da EDRAM */

typedef struct Ret { int x0, y0, x1, y1; } Ret; /* vazio quando x1 <= x0 */
typedef struct Alvo {
    D3DTexture *tex;
    Ret conteudo; /* onde ele já foi desenhado (fora dali, zero) */
} Alvo;
static Alvo sAlvos[MAX_TARGETS];
static int sResidente = -1; /* o alvo que está na EDRAM */
static Ret sSujo;           /* o que foi desenhado nele desde que entrou */

typedef struct Pipe { uint32_t key, bkey, wmask; int ztst, zwrite, topo, vu, fx; } Pipe;
static Pipe sPipes[1024];
static int sPipeCount;

/* recursos soltos que a GPU ainda pode estar lendo: Release só quando IsBusy diz que não (senão bloqueia) */
static D3DResource *sCova[MAX_COVA];
static unsigned sCovaBytes[MAX_COVA];
static unsigned char sCovaClasse[MAX_COVA];
static int sCovaN;

static void esquece_estado(void);
static int sCurPipe = -1, sCurSamp = -1, sEstadoGs;
static GsTex sCurTex = (GsTex)-1;
static D3DTexture *sCurDate;

/* ---- memória (F5, etapa 1) ----------------------------------------------------------------------------------- */

enum { CL_TEX, CL_ALVO, CL_N };
enum { MODO_TEX, MODO_ALVO, MODO_FALHOU }; /* o pedido: textura nova, alvo novo, textura depois de o D3D recusar */
enum { BR_ESPACO = 1, BR_CACHE, BR_D3D, BR_FORMATO, BR_ALVO, BR_N }; /* por que uma textura saiu branca */
static const char *const kBranca[BR_N] = {"", "sem espaço e nada a descartar", "cache de 8.192 cheio no quadro",
                                          "a criação falhou mesmo depois do descarte", "formato", "alvo sem textura"};
static struct {
    unsigned pisoMb, texTetoMb, folga; /* os ajustes (um só lugar: mem_init) */
    unsigned vivos[CL_N], nVivos[CL_N], pend[CL_N], nPend[CL_N]; /* bytes reais (alinhados) e quantos */
    unsigned buffers;                  /* os vetores fixos do gs_draw.c */
    unsigned livres, livresMin, criadoDesde, total; /* dwAvailPhys medido, o menor visto, criado desde a medida */
    unsigned pressoes, descN, descBytes, esperas, branca[BR_N];          /* no trecho de 60 quadros */
    unsigned acimaTeto;                /* no trecho: texturas criadas acima do teto (nada descartável) */
    unsigned pressoesTot, descNTot, esperasTot, brancaTot[BR_N], falasBranca[BR_N], falasPressao;
    double descBytesTot;
    unsigned maxVerts, maxVu, maxIdx, maxUni, maxDraws;                  /* buffers: o máximo de um quadro no trecho */
    unsigned emUso;                    /* entradas protegidas na última pressão (usadas nos dois últimos quadros) */
    int seguro;                        /* 0: BT3_INTERP ou BT3_GS_THREAD ligados, sem descarte */
    unsigned semAlvo;                  /* alvos sem textura agora */
} sMem;

/* bytes reais de uma textura de 1 nível: o que o XG calcula com o alinhamento do formato, em páginas de 4 KB */
static unsigned tam_real(UINT w, UINT h, D3DFORMAT f) {
    D3DTexture cab;
    UINT t = XGSetTextureHeader(w, h, 1, 0, f, (D3DPOOL)0, 0, 0, 0, &cab, NULL, NULL);
    return (t + 4095u) & ~4095u;
}
static unsigned tam_tex(UINT w, UINT h) { return tam_real(w, h, D3DFMT_LIN_A8B8G8R8); }
static unsigned tam_alvo(void) {
    static unsigned t;
    if (t == 0) {
        t = tam_real(TAM, TAM, D3DFMT_A8R8G8B8);
    }
    return t;
}

/* a memória física livre agora, em bytes (e o menor valor visto) */
static unsigned mem_mede(void) {
    MEMORYSTATUS m;
    GlobalMemoryStatus(&m);
    sMem.livres = (unsigned)m.dwAvailPhys;
    sMem.total = (unsigned)m.dwTotalPhys;
    sMem.criadoDesde = 0;
    if (sMem.livresMin == 0 || sMem.livres < sMem.livresMin) {
        sMem.livresMin = sMem.livres;
    }
    return sMem.livres;
}

/* memória física livre, em MB */
static unsigned mb_livres(void) { return mem_mede() >> 20; }

/* a estimativa sem chamar o sistema (o que foi criado desde a última medida sai dela); perto do piso, medida */
static unsigned mem_livres(unsigned pedido) {
    unsigned est = sMem.livres > sMem.criadoDesde ? sMem.livres - sMem.criadoDesde : 0;
    if (est < sMem.pisoMb * MB + pedido + 32u * MB) {
        est = mem_mede();
    }
    return est;
}

static void mem_cria(int cl, unsigned bytes) {
    sMem.vivos[cl] += bytes;
    sMem.nVivos[cl]++;
    sMem.criadoDesde += bytes;
}

/* o que o quadro teve e o que ficou de fora, para o log */
static struct {
    unsigned quadros, desenhos, vertices, texNovas, trocas, restauros, semVu, semNativo[8], semFb, semTextura;
    double msFim, msFimPior, msQuadro, msQuadroPior, msPresent, msPresentPior;
    unsigned acima16, acima33, capturas, maiorDesenho;
    double msCaptura;
} sConta;
static LARGE_INTEGER sFreq, sUltimoFim;
static unsigned sAvisado; /* bits: o que já foi dito uma vez no log */

/* (F5, etapa 3) o tempo por passo e os descartes da janela medida (port/x360/headless/passos.c): cada Present vai para
   lá com o quadro do GS mostrado e o tempo dentro dele, e os descartes acumulados desde o começo saem de
   Passos_Descartes (VU1, os nativos 1 a 4, direto no quadro, texturas recusadas, primitivas só do PS2). Com uma janela
   aberta, o resumo de 60 quadros (log_quadros) não se escreve: a medida não paga a escrita do log */
extern "C" void Passos_Apresentou(unsigned quadroGs, unsigned presentUs);
extern "C" int Passos_Medindo(void);
static unsigned sDescAntes[8]; /* o que os sConta já zerados tinham */
static unsigned sPs2Total;     /* gsSkipped somado de cada quadro */
extern "C" int Passos_Descartes(unsigned *v, int n) {
    unsigned agora[8];
    int i;
    agora[0] = sConta.semVu;
    for (i = 1; i <= 4; i++) {
        agora[i] = sConta.semNativo[i];
    }
    agora[5] = sConta.semFb;
    agora[6] = sConta.semTextura;
    agora[7] = sPs2Total + gsSkipped;
    for (i = 0; i < n && i < 8; i++) {
        v[i] = sDescAntes[i] + agora[i];
    }
    return 8;
}

static double ms_desde(const LARGE_INTEGER *a) {
    LARGE_INTEGER b;
    QueryPerformanceCounter(&b);
    return (double)(b.QuadPart - a->QuadPart) * 1000.0 / (double)sFreq.QuadPart;
}

static void ret_une(Ret *r, int x0, int y0, int x1, int y1) {
    if (x1 <= x0 || y1 <= y0) {
        return;
    }
    if (r->x1 <= r->x0) {
        r->x0 = x0; r->y0 = y0; r->x1 = x1; r->y1 = y1;
        return;
    }
    if (x0 < r->x0) r->x0 = x0;
    if (y0 < r->y0) r->y0 = y0;
    if (x1 > r->x1) r->x1 = x1;
    if (y1 > r->y1) r->y1 = y1;
}

static void cova_solta(int i) {
    sCova[i]->Release();
    sMem.pend[sCovaClasse[i]] -= sCovaBytes[i];
    sMem.nPend[sCovaClasse[i]]--;
}

/* a mais antiga da cova, esperando a GPU (nada do quadro fica preso no dispositivo fora do frame_end; mesmo assim os
   estágios são soltos antes, porque uma textura presa fica ocupada para sempre) */
static void cova_espera_primeira(void) {
    const char *antes = (const char *)gGsD3dEtapa;
    gGsD3dEtapa = "esperando a GPU soltar um recurso descartado";
    sDev->SetTexture(0, NULL);
    sDev->SetTexture(1, NULL);
    esquece_estado();
    sCova[0]->BlockUntilNotBusy();
    cova_solta(0);
    memmove(sCova, sCova + 1, (sCovaN - 1) * sizeof(sCova[0]));
    memmove(sCovaBytes, sCovaBytes + 1, (sCovaN - 1) * sizeof(sCovaBytes[0]));
    memmove(sCovaClasse, sCovaClasse + 1, (sCovaN - 1) * sizeof(sCovaClasse[0]));
    sCovaN--;
    sMem.esperas++;
    gGsD3dEtapa = antes;
}

/* um recurso solto, com os bytes dele, que saem dos vivos e passam a pendentes */
static void cova(D3DResource *r, int cl, unsigned bytes) {
    if (r == NULL) {
        return;
    }
    sMem.vivos[cl] -= sMem.vivos[cl] >= bytes ? bytes : sMem.vivos[cl];
    sMem.nVivos[cl]--;
    if (sCovaN == MAX_COVA) {
        cova_espera_primeira();
    }
    sCova[sCovaN] = r;
    sCovaBytes[sCovaN] = bytes;
    sCovaClasse[sCovaN] = (unsigned char)cl;
    sCovaN++;
    sMem.pend[cl] += bytes;
    sMem.nPend[cl]++;
}

/* o que a GPU já soltou sai (no fim do quadro, depois de soltar as texturas do dispositivo e do InsertFence, e a cada
   pressão de memória) */
static void cova_varre(void) {
    int i, n = 0;
    for (i = 0; i < sCovaN; i++) {
        if (sCova[i]->IsBusy()) {
            sCova[n] = sCova[i];
            sCovaBytes[n] = sCovaBytes[i];
            sCovaClasse[n] = sCovaClasse[i];
            n++;
        } else {
            cova_solta(i);
        }
    }
    sCovaN = n;
}

/* ---- a pressão de memória, pedida pelo frontend (gs_draw.c, GsDraw_X360Abre) ---------------------------------- */

static unsigned pedido_bytes(uint32_t w, uint32_t h, int modo) {
    return modo == MODO_ALVO ? tam_alvo() : tam_tex(w, h);
}

/* Quantos bytes o frontend deve descartar antes de criar o pedido (0: cabe). Textura: o piso de memória livre e o
   teto da classe (vivas + pendentes); alvo: só o piso; depois de o D3D recusar: pelo menos o dobro do pedido. Com
   falta, pede também a folga, para a próxima pressão não vir na textura seguinte. */
extern "C" uint32_t GsD3d_Falta(uint32_t w, uint32_t h, int modo) {
    unsigned pedido = pedido_bytes(w, h, modo), livres, falta = 0, piso = sMem.pisoMb * MB;
    if (!sMem.seguro) {
        return 0;
    }
    cova_varre();
    livres = mem_livres(pedido);
    if (livres < piso + pedido) {
        falta = piso + pedido - livres;
    }
    if (modo != MODO_ALVO) {
        unsigned tex = sMem.vivos[CL_TEX] + sMem.pend[CL_TEX] + pedido, teto = sMem.texTetoMb * MB;
        if (tex > teto && tex - teto > falta) {
            falta = tex - teto;
        }
    }
    if (modo == MODO_FALHOU && falta < 2 * pedido) {
        falta = 2 * pedido;
    }
    return falta == 0 ? 0 : falta + sMem.folga;
}

/* os bytes reais de uma textura do cache */
extern "C" uint32_t GsD3d_TexBytes(GsTex tex) {
    D3DSURFACE_DESC d;
    if (tex == 0 || FAILED(((D3DTexture *)tex)->GetLevelDesc(0, &d))) {
        return 0;
    }
    return tam_tex(d.Width, d.Height);
}

/* O frontend descartou `n` texturas (`bytes`, a mais recente sem uso há `idade` quadros; `emUso` protegidas). O que a
   GPU já soltou sai da cova; se o piso ainda não deixa, espera a GPU soltar o mais antigo da cova, um por vez. 1: o
   pedido cabe no piso (o teto das texturas é macio: passar dele é melhor que a branca). */
extern "C" int GsD3d_Descartou(uint32_t w, uint32_t h, int modo, unsigned n, uint32_t bytes, unsigned idade, unsigned emUso) {
    unsigned pedido = pedido_bytes(w, h, modo), piso = sMem.pisoMb * MB, livres, esperas = sMem.esperas;
    int cabe;
    sMem.pressoes++;
    sMem.pressoesTot++;
    sMem.descN += n;
    sMem.descNTot += n;
    sMem.descBytes += bytes;
    sMem.descBytesTot += bytes;
    sMem.emUso = emUso;
    cova_varre();
    livres = mem_mede();
    while (livres < piso + pedido && sCovaN > 0) {
        cova_espera_primeira();
        livres = mem_mede();
    }
    sMem.esperasTot += sMem.esperas - esperas;
    cabe = livres >= piso + pedido;
    if (n == 0 && cabe) {
        sMem.acimaTeto++; /* (só o teto das texturas, sem nada a descartar: passa dele, sem linha) */
        return 1;
    }
    if (sMem.falasPressao < 300 || sMem.pressoesTot % 50 == 0 || !cabe) {
        sMem.falasPressao++;
        Diario_Escrever("gs_d3d: pressão %u, quadro %u: pedido %u KB (%s %ux%u); descartadas %u texturas (%u KB), a mais "
                        "recente sem uso há %u quadros, %u protegidas; %u esperas pela GPU; agora texturas %u KB + %u KB "
                        "pendentes, alvos %u KB, livres %u MB (piso %u): %s",
                        sMem.pressoesTot, gGsFrame, pedido >> 10, modo == MODO_ALVO ? "alvo" : "textura", w, h, n, bytes >> 10,
                        idade, emUso, sMem.esperas - esperas, sMem.vivos[CL_TEX] >> 10, sMem.pend[CL_TEX] >> 10,
                        (sMem.vivos[CL_ALVO] + sMem.pend[CL_ALVO]) >> 10, livres >> 20, sMem.pisoMb,
                        cabe ? "cabe" : "NÃO CABE");
    }
    return cabe;
}

/* uma textura que saiu branca, e por quê (as primeiras de cada motivo com o retrato da memória) */
static void branca(int motivo, uint32_t w, uint32_t h) {
    sMem.branca[motivo]++;
    sMem.brancaTot[motivo]++;
    if (sMem.falasBranca[motivo] < 20) {
        sMem.falasBranca[motivo]++;
        Diario_Escrever("gs_d3d: BRANCA (%s): %ux%u no quadro %u; livres %u MB (piso %u), texturas %u KB + %u KB "
                        "pendentes (teto %u MB), alvos %u, %d no cache, %u protegidas na última pressão",
                        kBranca[motivo], w, h, gGsFrame, mb_livres(), sMem.pisoMb, sMem.vivos[CL_TEX] >> 10,
                        sMem.pend[CL_TEX] >> 10, sMem.texTetoMb, sMem.nVivos[CL_ALVO], gsTexCount, sMem.emUso);
    }
}
extern "C" void GsD3d_Branca(int motivo, uint32_t w, uint32_t h) {
    branca(motivo > 0 && motivo < BR_N ? motivo : BR_ESPACO, w, h);
}

/* ---- texturas -------------------------------------------------------------------------------------------- */

/* As texturas decodificadas: D3DFMT_LIN_A8B8G8R8, que recebe o u32 nativo R | G<<8 | B<<16 | A<<24 do gs_draw.c
   sem troca (medido na etapa 6). Só RGBA8: o 360 não tem pacote de texturas.
   O espaço já foi aberto pelo frontend (GsDraw_X360Abre); aqui só o piso é conferido de novo (as paletas vêm sem
   passar por lá, 1 KB cada). */
static GsTex tex_create(uint32_t w, uint32_t h, int format, int levels) {
    D3DTexture *t = NULL;
    unsigned bytes = tam_tex(w, h);
    (void)levels;
    if (format != GS_TEXFMT_RGBA8) {
        sConta.semTextura++;
        branca(BR_FORMATO, w, h);
        return 0;
    }
    if (mem_livres(bytes) < sMem.pisoMb * MB + bytes) {
        sConta.semTextura++;
        return 0; /* (quem chamou conta o motivo) */
    }
    if (FAILED(sDev->CreateTexture(w, h, 1, 0, D3DFMT_LIN_A8B8G8R8, D3DPOOL_DEFAULT, &t, NULL)) || t == NULL) {
        sConta.semTextura++;
        return 0;
    }
    mem_cria(CL_TEX, bytes);
    sConta.texNovas++;
    return (GsTex)t;
}

static int tex_upload(GsTex handle, int level, uint32_t w, uint32_t h, int format, const void *px, uint32_t bytes) {
    D3DTexture *t = (D3DTexture *)handle;
    D3DLOCKED_RECT r;
    uint32_t y;
    (void)format;
    (void)bytes;
    if (t == NULL || level != 0 || FAILED(t->LockRect(0, &r, NULL, 0))) {
        free((void *)px);
        return 0;
    }
    for (y = 0; y < h; y++) {
        memcpy((BYTE *)r.pBits + y * r.Pitch, (const uint32_t *)px + y * w, w * 4);
    }
    t->UnlockRect(0);
    free((void *)px);
    return 1;
}

static void tex_destroy(GsTex tex) {
    D3DTexture *t = (D3DTexture *)tex;
    D3DSURFACE_DESC d;
    if (t == NULL) {
        return;
    }
    cova(t, CL_TEX, SUCCEEDED(t->GetLevelDesc(0, &d)) ? tam_tex(d.Width, d.Height) : 0);
}

/* só as paletas (clut_texture) perguntam: o texture_get do 360 abre espaço por GsDraw_X360Abre */
static int tex_slots_left(void) { return mem_livres(4096) >= sMem.pisoMb * MB + 4096 ? 1 << 20 : 0; }
static GsTex white_tex(void) { return (GsTex)sBranco; }

/* textura tiled (destino de Resolve), zerada: o conteúdo de um alvo novo é o da limpeza */
static D3DTexture *textura_alvo(void) {
    D3DTexture *t = NULL;
    D3DLOCKED_RECT r;
    if (mem_livres(tam_alvo()) < sMem.pisoMb * MB + tam_alvo() ||
        FAILED(sDev->CreateTexture(TAM, TAM, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &t, NULL)) || t == NULL) {
        return NULL;
    }
    mem_cria(CL_ALVO, tam_alvo());
    if (SUCCEEDED(t->LockRect(0, &r, NULL, 0))) {
        memset(r.pBits, 0, (size_t)r.Pitch * TAM);
        t->UnlockRect(0);
    }
    return t;
}

/* ---- alvos --------------------------------------------------------------------------------------------------- */

/* A textura de um alvo, abrindo espaço no cache de texturas se o piso não deixa; se o D3D recusa, descarta mais e
   tenta de novo. Sem textura, o alvo ainda desenha na EDRAM, mas perde o conteúdo ao sair e quem o lê recebe a
   branca: o log diz, e o fim do quadro tenta de novo. */
static D3DTexture *alvo_com_espaco(void) {
    D3DTexture *t;
    const char *antes = (const char *)gGsD3dEtapa;
    gGsD3dEtapa = "criando a textura de um alvo";
    GsDraw_X360Abre(TAM, TAM, MODO_ALVO);
    t = textura_alvo();
    if (t == NULL && GsDraw_X360Abre(TAM, TAM, MODO_FALHOU)) {
        t = textura_alvo();
    }
    gGsD3dEtapa = antes;
    return t;
}

static int sAlvoSemTex[MAX_TARGETS]; /* 1: a vaga está sem textura (o fim do quadro tenta de novo) */

static void target_ensure(int i) {
    sAlvos[i].tex = alvo_com_espaco();
    sAlvos[i].conteudo.x0 = sAlvos[i].conteudo.x1 = 0;
    if (sResidente == i) {
        sResidente = -1; /* (a vaga mudou de dono: o que estava na EDRAM era do antigo) */
    }
    sAlvoSemTex[i] = sAlvos[i].tex == NULL;
    if (sAlvos[i].tex == NULL) {
        Diario_Escrever("gs_d3d: ERRO: o alvo %d (de %d) ficou sem textura no quadro %u: livres %u MB (piso %u), texturas "
                        "%u KB + %u KB pendentes; o fim do quadro tenta de novo", i, gsTargetCount, gGsFrame, mb_livres(),
                        sMem.pisoMb, sMem.vivos[CL_TEX] >> 10, sMem.pend[CL_TEX] >> 10);
    }
}

/* no fim do quadro, com os estágios soltos: as vagas que ficaram sem textura tentam de novo (o conteúdo delas já se
   perdeu; volta a valer a partir do próximo desenho) */
static void alvos_sem_textura(void) {
    int i;
    sMem.semAlvo = 0;
    for (i = 0; i < gsTargetCount; i++) {
        if (sAlvos[i].tex != NULL || !sAlvoSemTex[i]) {
            continue;
        }
        sAlvos[i].tex = alvo_com_espaco();
        sAlvos[i].conteudo.x0 = sAlvos[i].conteudo.x1 = 0;
        if (sAlvos[i].tex != NULL) {
            sAlvoSemTex[i] = 0;
            gsTargets[i].cleared = 0;
            Diario_Escrever("gs_d3d: o alvo %d ganhou a textura no quadro %u", i, gGsFrame);
        } else {
            sMem.semAlvo++;
        }
    }
}

static void target_drop(int i) {
    if (sResidente == i) {
        sResidente = -1;
    }
    if (sDateFonte == sAlvos[i].tex) {
        sDateFonte = NULL;
    }
    if (sAlvos[i].tex != NULL) {
        cova(sAlvos[i].tex, CL_ALVO, tam_alvo());
    }
    sAlvos[i].tex = NULL;
    sAlvoSemTex[i] = 0;
}

static GsTex target_color(int i) {
    if (sAlvos[i].tex == NULL) {
        branca(BR_ALVO, TAM, TAM);
    }
    return (GsTex)sAlvos[i].tex;
}
static GsTex target_none(int i) { (void)i; return 0; }

static void scale_changed(void) {
    gsScale = 1; /* a EDRAM só comporta SCALE = 1 sem tiling predicado */
}

/* ---- pipelines (a chave do gs_draw.c, como no gs_gl.c) ------------------------------------------------------- */

static int pipe_get(uint32_t key) {
    int i;
    Pipe *p;
    for (i = 0; i < sPipeCount; i++) {
        if (sPipes[i].key == key) {
            return i;
        }
    }
    if (sPipeCount == 1024) {
        return 0;
    }
    p = &sPipes[sPipeCount];
    p->key = key;
    p->bkey = key & 0x1FF;
    p->ztst = (int)((key >> 10) & 3);
    p->zwrite = (int)((key >> 12) & 1);
    p->topo = (int)((key >> 13) & 3);
    p->vu = (int)((key >> 20) & 3);
    p->fx = (int)((key >> 22) & 1);
    p->wmask = (key >> 16) & 15;
    return sPipeCount++;
}

/* o estado do dispositivo que já foi posto, para não repetir (sCurPipe, sCurSamp, sEstadoGs, sCurTex e sCurDate estão
   antes da memória) */
static float sCurC[16];
static int sTemC, sTemScissor, sScissor[4];
static float sCurBlendc = -1.0f;
static float sCurTexW = 1.0f, sCurTexH = 1.0f;
static unsigned sLutaVista, sPassoAPasso;

static void esquece_estado(void) {
    sCurPipe = -1;
    sCurSamp = -1;
    sCurTex = (GsTex)-1;
    sCurDate = NULL;
    sTemC = 0;
    sTemScissor = 0;
    sCurBlendc = -1.0f;
    sEstadoGs = 0;
}

/* ((A - B) * C >> 7) + D: k 0 nada, 1 um, 2 C, 3 -C, 4 1 - C, 5 1 + C (o 1 + C sai como um, como no gs_gl.c) */
static void aplica_blend(uint32_t bkey) {
    int k[2] = {(int)((bkey >> 1) & 7), (int)((bkey >> 4) & 7)}, C = (int)((bkey >> 7) & 3), i;
    DWORD fc = C == 0 ? D3DBLEND_SRCALPHA : C == 1 ? D3DBLEND_DESTALPHA : D3DBLEND_BLENDFACTOR;
    DWORD fi = C == 0 ? D3DBLEND_INVSRCALPHA : C == 1 ? D3DBLEND_INVDESTALPHA : D3DBLEND_INVBLENDFACTOR;
    DWORD f[2];
    if (!(bkey & 1)) {
        sDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        return;
    }
    for (i = 0; i < 2; i++) {
        f[i] = k[i] == 0 ? D3DBLEND_ZERO : k[i] == 1 || k[i] == 5 ? D3DBLEND_ONE : k[i] == 4 ? fi : fc;
    }
    sDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    sDev->SetRenderState(D3DRS_SRCBLEND, f[0]);
    sDev->SetRenderState(D3DRS_DESTBLEND, f[1]);
    sDev->SetRenderState(D3DRS_BLENDOP, k[0] == 3 ? D3DBLENDOP_REVSUBTRACT : k[1] == 3 ? D3DBLENDOP_SUBTRACT : D3DBLENDOP_ADD);
    /* o GS não mistura o alfa: o gravado é o da fonte */
    sDev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
    sDev->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
    sDev->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_ZERO);
    sDev->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
}

static void aplica_pipe(int pipe) {
    Pipe *p = &sPipes[pipe];
    if (pipe == sCurPipe) {
        return;
    }
    sCurPipe = pipe;
    aplica_blend(p->bkey);
    sDev->SetRenderState(D3DRS_COLORWRITEENABLE, ((p->wmask & GS_CC_R) ? D3DCOLORWRITEENABLE_RED : 0) |
                                                     ((p->wmask & GS_CC_G) ? D3DCOLORWRITEENABLE_GREEN : 0) |
                                                     ((p->wmask & GS_CC_B) ? D3DCOLORWRITEENABLE_BLUE : 0) |
                                                     ((p->wmask & GS_CC_A) ? D3DCOLORWRITEENABLE_ALPHA : 0));
    sDev->SetRenderState(D3DRS_ZENABLE, TRUE);
    sDev->SetRenderState(D3DRS_ZWRITEENABLE, p->zwrite ? TRUE : FALSE);
    sDev->SetRenderState(D3DRS_ZFUNC, p->ztst == 0 ? D3DCMP_NEVER : p->ztst == 1 ? D3DCMP_ALWAYS :
                                      p->ztst == 2 ? D3DCMP_GREATEREQUAL : D3DCMP_GREATER);
}

/* bit 0 linear, bit 1 U preso na borda, bit 2 V (bit 3, os níveis menores de um pacote, não existe aqui) */
static void aplica_sampler(DWORD estagio, int s) {
    sDev->SetSamplerState(estagio, D3DSAMP_MINFILTER, (s & 1) ? D3DTEXF_LINEAR : D3DTEXF_POINT);
    sDev->SetSamplerState(estagio, D3DSAMP_MAGFILTER, (s & 1) ? D3DTEXF_LINEAR : D3DTEXF_POINT);
    sDev->SetSamplerState(estagio, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    sDev->SetSamplerState(estagio, D3DSAMP_ADDRESSU, (s & 2) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP);
    sDev->SetSamplerState(estagio, D3DSAMP_ADDRESSV, (s & 4) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP);
}

/* Uma textura copiada para um retângulo do alvo atual (W x H): sem mistura, sem profundidade, sem tesoura. */
static void copia(D3DTexture *t, int W, int H, float x0, float y0, float x1, float y1, float u0, float v0, float u1,
                  float v1, int linear) {
    struct { float x, y, u, v; } q[4];
    float cx0 = x0 / W * 2.0f - 1.0f, cx1 = x1 / W * 2.0f - 1.0f, cy0 = 1.0f - y0 / H * 2.0f, cy1 = 1.0f - y1 / H * 2.0f;
    q[0].x = cx0; q[0].y = cy0; q[0].u = u0; q[0].v = v0;
    q[1].x = cx1; q[1].y = cy0; q[1].u = u1; q[1].v = v0;
    q[2].x = cx0; q[2].y = cy1; q[2].u = u0; q[2].v = v1;
    q[3].x = cx1; q[3].y = cy1; q[3].u = u1; q[3].v = v1;
    sDev->SetVertexDeclaration(sDeclCopia);
    sDev->SetVertexShader(sVsCopia);
    sDev->SetPixelShader(sPsCopia);
    sDev->SetTexture(0, t);
    aplica_sampler(0, (linear ? 1 : 0) | 2 | 4);
    sDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    sDev->SetRenderState(D3DRS_ZENABLE, FALSE);
    sDev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    sDev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    sDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);
    sDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(q[0]));
    esquece_estado();
}

/* o retângulo em múltiplos de 8 (exigência do Resolve), dentro do alvo */
static void alinha8(Ret *r) {
    r->x0 &= ~7;
    r->y0 &= ~7;
    r->x1 = (r->x1 + 7) & ~7;
    r->y1 = (r->y1 + 7) & ~7;
    if (r->x1 > TAM) r->x1 = TAM;
    if (r->y1 > TAM) r->y1 = TAM;
}

/* O alvo da EDRAM volta para a textura dele (só o que foi desenhado desde que entrou). */
static void sai_da_edram(void) {
    Ret r = sSujo;
    D3DRECT fonte;
    D3DPOINT destino;
    if (sResidente < 0) {
        return;
    }
    if (r.x1 > r.x0 && sAlvos[sResidente].tex != NULL) {
        /* o destino do Resolve não fica preso como textura (a cópia de volta deixou a dele no estágio 0) */
        sDev->SetTexture(0, NULL);
        sDev->SetTexture(1, NULL);
        sCurTex = (GsTex)-1;
        sCurDate = NULL;
        alinha8(&r);
        fonte.x1 = r.x0; fonte.y1 = r.y0; fonte.x2 = r.x1; fonte.y2 = r.y1;
        destino.x = r.x0; destino.y = r.y0;
        sDev->Resolve(D3DRESOLVE_RENDERTARGET0, &fonte, sAlvos[sResidente].tex, &destino, 0, 0, NULL, 0.0f, 0, NULL);
        ret_une(&sAlvos[sResidente].conteudo, r.x0, r.y0, r.x1, r.y1);
    }
    sResidente = -1;
    sSujo.x0 = sSujo.x1 = 0;
}

/* Põe o alvo i na EDRAM: limpa (cor 0, profundidade 0, como o gs_gl.c limpa um alvo novo) e, se ele já tinha
   conteúdo, desenha a textura dele de volta por cima. */
static void entra_na_edram(int i) {
    Alvo *a = &sAlvos[i];
    D3DVIEWPORT9 vp = {0, 0, TAM, TAM, 0.0f, 1.0f};
    sai_da_edram();
    sDev->SetRenderTarget(0, sEdramCor);
    sDev->SetDepthStencilSurface(sEdramProf);
    sDev->SetViewport(&vp);
    sDev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    sDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);
    sDev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    sDev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0x00000000, 0.0f, 0);
    if (!gsTargets[i].cleared) {
        a->conteudo.x0 = a->conteudo.x1 = 0; /* novo para o jogo: começa limpo */
    } else if (a->conteudo.x1 > a->conteudo.x0 && a->tex != NULL) {
        Ret r = a->conteudo;
        copia(a->tex, TAM, TAM, (float)r.x0, (float)r.y0, (float)r.x1, (float)r.y1, (float)r.x0 / TAM, (float)r.y0 / TAM,
              (float)r.x1 / TAM, (float)r.y1 / TAM, 0);
        sConta.restauros++;
    }
    esquece_estado();
    sResidente = i;
    sSujo.x0 = sSujo.x1 = 0;
    sConta.trocas++;
}

/* ---- init ---------------------------------------------------------------------------------------------------- */

static DWORD sCorFundo = 0xFF010101; /* BT3_COR_FUNDO: a limpeza da tela fora da imagem (as barras do 4:3) */

static unsigned env_num(const char *nome, unsigned padrao) {
    const char *v = getenv(nome);
    return v != NULL && *v != '\0' ? (unsigned)strtoul(v, NULL, 0) : padrao;
}

/* Os ajustes da memória (o único lugar com os valores), os buffers fixos do gs_draw.c e uma calibração: o que o
   dwAvailPhys mede ao criar texturas pequenas e médias contra o que tam_tex conta (o alinhamento real do D3D). */
static void mem_init(void) {
    static const UINT lado[2] = {8, 256};
    static const int quantas[2] = {64, 16};
    static D3DTexture *teste[64];
    int k, i;
    sMem.pisoMb = env_num("BT3_MEM_PISO_MB", 32);
    sMem.texTetoMb = env_num("BT3_MEM_TEX_MB", 128);
    sMem.folga = sMem.pisoMb * MB / 2;
    sMem.seguro = (getenv("BT3_INTERP") == NULL || atoi(getenv("BT3_INTERP")) == 0) && getenv("BT3_GS_THREAD") == NULL;
    sMem.buffers = MAX_VERTS * (unsigned)sizeof(Vtx) + MAX_VU_VERTS * 48u + MAX_VU_IDX * 4u +
                   MAX_VU_UNIFORMS * (unsigned)sizeof(Vu0Uniform) + MAX_DRAWS * (unsigned)sizeof(GsDraw);
    sCorFundo = (DWORD)env_num("BT3_COR_FUNDO", 0xFF010101u);
    mem_mede();
    Diario_Escrever("gs_d3d: memória: piso %u MB livres (BT3_MEM_PISO_MB), teto das texturas %u MB (BT3_MEM_TEX_MB), "
                    "folga do descarte %u MB, descarte %s; buffers do gs_draw.c %u KB; alvo %u KB; %u MB livres de %u; "
                    "cor de fundo 0x%08X (BT3_COR_FUNDO)", sMem.pisoMb, sMem.texTetoMb, sMem.folga >> 20,
                    sMem.seguro ? "ligado" : "DESLIGADO (BT3_INTERP ou BT3_GS_THREAD)", sMem.buffers >> 10,
                    tam_alvo() >> 10, sMem.livres >> 20, sMem.total >> 20, (unsigned)sCorFundo);
    for (k = 0; k < 2; k++) {
        unsigned antes = mem_mede(), depois, feitas = 0;
        for (i = 0; i < quantas[k]; i++) {
            teste[i] = NULL;
            if (SUCCEEDED(sDev->CreateTexture(lado[k], lado[k], 1, 0, D3DFMT_LIN_A8B8G8R8, D3DPOOL_DEFAULT, &teste[i], NULL))) {
                feitas++;
            }
        }
        depois = mem_mede();
        for (i = 0; i < quantas[k]; i++) {
            if (teste[i] != NULL) {
                teste[i]->Release(); /* (nunca usadas pela GPU) */
            }
        }
        Diario_Escrever("gs_d3d: calibração: %u texturas de %ux%u: contadas %u KB, medidas %d KB pelo dwAvailPhys",
                        feitas, lado[k], lado[k], feitas * (tam_tex(lado[k], lado[k]) >> 10), ((int)antes - (int)depois) / 1024);
    }
    sMem.livresMin = 0;
    mem_mede();
}

static int d3d_init(void) {
    static const D3DVERTEXELEMENT9 elemGs[] = {
        {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 12, D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
        {0, 16, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()};
    static const D3DVERTEXELEMENT9 elemCopia[] = {
        {0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()};
    D3DSURFACE_PARAMETERS sp;
    UINT tilesCor, tilesProf;
    D3DLOCKED_RECT r;

    QueryPerformanceFrequency(&sFreq);
    if (gX360Dev == NULL) {
        Diario_Escrever("gs_d3d: sem dispositivo (a main não o criou)");
        return 0;
    }
    sDev = gX360Dev;
    sDev->AcquireThreadOwnership(); /* a main o soltou antes de criar a thread do jogo */
    if (gsScale != 1) {
        Diario_Escrever("gs_d3d: SCALE %d pedido; o 360 usa 1 (a EDRAM)", gsScale);
    }
    gsScale = 1;
    if (sizeof(Vtx) != 28 || sizeof(GsDraw) != 128) { /* o mesmo layout dos dois lados (cl/stdint.h) */
        Diario_Escrever("gs_d3d: ERRO: Vtx %u bytes, GsDraw %u bytes", (unsigned)sizeof(Vtx), (unsigned)sizeof(GsDraw));
        return 0;
    }
    if (FAILED(sDev->CreateVertexShader((const DWORD *)kGsVs, &sVsGs)) || FAILED(sDev->CreatePixelShader((const DWORD *)kGsPs, &sPsGs)) ||
        FAILED(sDev->CreateVertexShader((const DWORD *)kCopiaVs, &sVsCopia)) ||
        FAILED(sDev->CreatePixelShader((const DWORD *)kCopiaPs, &sPsCopia)) || FAILED(sDev->CreateVertexDeclaration(elemGs, &sDeclGs)) ||
        FAILED(sDev->CreateVertexDeclaration(elemCopia, &sDeclCopia))) {
        Diario_Escrever("gs_d3d: ERRO nos shaders ou nas declarações");
        return 0;
    }
    sDev->GetRenderTarget(0, &sTela); /* o back buffer do dispositivo (1280 x 720) */
    /* a EDRAM: cor na base 0, profundidade logo depois; o back buffer também está na base 0, e os dois nunca são
       usados ao mesmo tempo (o alvo sai da EDRAM antes do present) */
    tilesCor = XGSurfaceSize(TAM, TAM, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE);
    tilesProf = XGSurfaceSize(TAM, TAM, D3DFMT_D24S8, D3DMULTISAMPLE_NONE);
    memset(&sp, 0, sizeof(sp));
    sp.Base = 0;
    if (FAILED(sDev->CreateRenderTarget(TAM, TAM, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &sEdramCor, &sp))) {
        Diario_Escrever("gs_d3d: ERRO na cor da EDRAM");
        return 0;
    }
    sp.Base = tilesCor;
    sp.HierarchicalZBase = 0;
    if (FAILED(sDev->CreateDepthStencilSurface(TAM, TAM, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &sEdramProf, &sp))) {
        Diario_Escrever("gs_d3d: ERRO na profundidade da EDRAM");
        return 0;
    }
    if (FAILED(sDev->CreateTexture(1, 1, 1, 0, D3DFMT_LIN_A8B8G8R8, D3DPOOL_DEFAULT, &sBranco, NULL)) ||
        FAILED(sBranco->LockRect(0, &r, NULL, 0))) {
        Diario_Escrever("gs_d3d: ERRO na textura branca");
        return 0;
    }
    *(DWORD *)r.pBits = 0xFFFFFFFFu;
    sBranco->UnlockRect(0);
    mem_init();
    sDate = textura_alvo();
    if (sDate == NULL) {
        Diario_Escrever("gs_d3d: ERRO na textura do DATE (%u MB livres)", mb_livres());
        return 0;
    }
    /* fixos para todos os desenhos */
    sDev->SetRenderState(D3DRS_HALFPIXELOFFSET, TRUE); /* centros em .5, como GL e Vulkan (etapa 6) */
    sDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    sDev->SetRenderState(D3DRS_HIZENABLE, D3DHIZ_DISABLE); /* testes GEQUAL/GREATER e profundidade que volta limpa */
    sDev->SetRenderState(D3DRS_HIZWRITEENABLE, D3DHIZ_DISABLE);
    aplica_sampler(1, 2 | 4);
    esquece_estado();
    Diario_Escrever("gs_d3d: pronto; alvo %dx%d, EDRAM: cor %u + profundidade %u de %u tiles; %u MB livres", TAM, TAM,
                    tilesCor, tilesProf, (unsigned)EDRAM_TILES, mb_livres());
    if (tilesCor + tilesProf > EDRAM_TILES) {
        Diario_Escrever("gs_d3d: ERRO: o alvo não cabe na EDRAM");
        return 0;
    }
    QueryPerformanceCounter(&sUltimoFim);
    return 1;
}

/* ---- captura (BT3_SHOT) -------------------------------------------------------------------------------------- */

/* BT3_SHOT_LISTA: os quadros do GS a capturar, lidos uma vez (-1 enquanto não lidos) */
static int sListaN = -1;
static unsigned sLista[32];

static int na_lista(unsigned quadro) {
    int i;

    if (sListaN < 0) {
        const char *p = getenv("BT3_SHOT_LISTA");

        sListaN = 0;
        while (p != NULL && *p != '\0' && sListaN < 32) {
            if (*p >= '0' && *p <= '9') {
                sLista[sListaN++] = (unsigned)strtoul(p, (char **)&p, 10);
            } else {
                p++;
            }
        }
        if (sListaN > 0) {
            Diario_Escrever("gs_d3d: BT3_SHOT_LISTA com %d quadros do GS", sListaN);
        }
    }
    for (i = 0; i < sListaN; i++) {
        if (sLista[i] == quadro) {
            return 1;
        }
    }
    return 0;
}

/* A cor do alvo mostrado, já resolvida na textura dele, no formato do rasterizador de software (gs_core.c,
   screenshot): P6 640 x 448, as colunas além da largura do quadro (FRAME.FBW * 64) em preto. A textura sai do tiling
   de uma vez (XGUntileSurface) e o arquivo vai num WriteFile só: pixel a pixel, com uma escrita por linha, cada
   captura levava 260 ms (a primeira rodada no console). */
static void captura(int alvo) {
    static int criou;
    static DWORD linear[640 * 448];
    static unsigned char ppm[16 + 640 * 448 * 3];
    static const char cab[] = "P6\n640 448\n255\n";
    char nome[64];
    D3DLOCKED_RECT r;
    HANDLE h;
    DWORD n;
    RECT ret;
    uint64_t fr = (gGs.frame[0] & 0x1FF) == gsTargets[alvo].fbp ? gGs.frame[0] :
                  (gGs.frame[1] & 0x1FF) == gsTargets[alvo].fbp ? gGs.frame[1] : (8ull << 16);
    int fbw = (int)((fr >> 16) & 0x3F), x, y;
    unsigned char *o;
    LARGE_INTEGER t0;

    QueryPerformanceCounter(&t0);
    if (!criou) {
        CreateDirectory("game:\\shots", NULL);
        criou = 1;
    }
    fbw = (fbw ? fbw : 10) * 64;
    if (fbw > 640) fbw = 640;
    /* textura presa no dispositivo fica ocupada para sempre (IsBusy): o LockRect esperaria sem fim */
    sDev->SetTexture(0, NULL);
    sDev->SetTexture(1, NULL);
    esquece_estado();
    sDev->BlockUntilIdle();
    if (FAILED(sAlvos[alvo].tex->LockRect(0, &r, NULL, D3DLOCK_READONLY))) {
        return;
    }
    ret.left = 0; ret.top = 0; ret.right = fbw; ret.bottom = 448;
    XGUntileSurface(linear, 640 * 4, NULL, r.pBits, TAM, TAM, &ret, 4);
    sAlvos[alvo].tex->UnlockRect(0);
    memcpy(ppm, cab, sizeof(cab) - 1);
    o = ppm + sizeof(cab) - 1;
    for (y = 0; y < 448; y++) {
        for (x = 0; x < 640; x++, o += 3) {
            DWORD c = x < fbw ? linear[y * 640 + x] : 0; /* A8R8G8B8 como u32 nativo */
            o[0] = (unsigned char)(c >> 16);
            o[1] = (unsigned char)(c >> 8);
            o[2] = (unsigned char)c;
        }
    }
    _snprintf(nome, sizeof(nome), "game:\\shots\\gpu_%05u.ppm", gGsFrame);
    h = CreateFile(nome, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        WriteFile(h, ppm, (DWORD)(o - ppm), &n, NULL);
        CloseHandle(h);
    }
    sConta.capturas++;
    sConta.msCaptura += ms_desde(&t0);
}

/* ---- o quadro ------------------------------------------------------------------------------------------------ */

static void desenha(const GsDraw *d) {
    const Pipe *p = &sPipes[d->pipeline];
    float c[16];
    D3DPRIMITIVETYPE tipo = p->topo == 0 ? D3DPT_TRIANGLELIST : p->topo == 1 ? D3DPT_LINELIST : D3DPT_POINTLIST;
    int porPrim = p->topo == 0 ? 3 : p->topo == 1 ? 2 : 1;
    uint32_t feitos = 0;
    int x0 = d->scissor[0], y0 = d->scissor[1], x1 = d->scissor[0] + d->scissor[2], y1 = d->scissor[1] + d->scissor[3];

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > TAM) x1 = TAM;
    if (y1 > TAM) y1 = TAM;
    if (x1 <= x0 || y1 <= y0 || d->count == 0) {
        return;
    }
    if (!sEstadoGs) {
        sDev->SetVertexDeclaration(sDeclGs);
        sDev->SetVertexShader(sVsGs);
        sDev->SetPixelShader(sPsGs);
        sDev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
        sEstadoGs = 1;
    }
    aplica_pipe(d->pipeline);
    if (d->tex != sCurTex) {
        D3DTexture *t = (D3DTexture *)(d->tex != 0 ? d->tex : (GsTex)sBranco);
        D3DSURFACE_DESC desc;
        sDev->SetTexture(0, t);
        sCurTex = d->tex;
        /* o tamanho, para a arte 2D lida no ponto (gs_d3d.hlsl) */
        if (SUCCEEDED(t->GetLevelDesc(0, &desc))) {
            sCurTexW = (float)desc.Width;
            sCurTexH = (float)desc.Height;
        }
    }
    if (d->sampler != sCurSamp) {
        aplica_sampler(0, d->sampler);
        sCurSamp = d->sampler;
    }
    if (d->misc[1] != 0.0f) {
        /* a cópia para o DATE: a do marcador nativo 5, ou a própria textura de um alvo fora da EDRAM */
        D3DTexture *f = sDateFonte != NULL ? sDateFonte : sDate;
        if (sCurDate != f) {
            sDev->SetTexture(1, f);
            sCurDate = f;
        }
    }
    c[0] = (float)d->mode[0]; c[1] = (float)d->mode[1]; c[2] = (float)d->mode[2]; c[3] = (float)d->mode[3];
    memcpy(c + 4, d->misc, sizeof(d->misc));
    memcpy(c + 8, d->rect, sizeof(d->rect));
    c[12] = 1.0f / TAM; c[13] = 1.0f / TAM; c[14] = sCurTexW; c[15] = sCurTexH;
    if (!sTemC || memcmp(c, sCurC, sizeof(c)) != 0) {
        sDev->SetPixelShaderConstantF(0, c, 4);
        memcpy(sCurC, c, sizeof(c));
        sTemC = 1;
    }
    if (d->blendc != sCurBlendc) {
        int b = (int)(d->blendc * 255.0f + 0.5f);
        b = b < 0 ? 0 : b > 255 ? 255 : b;
        sDev->SetRenderState(D3DRS_BLENDFACTOR, D3DCOLOR_ARGB(b, b, b, b));
        sCurBlendc = d->blendc;
    }
    if (!sTemScissor || sScissor[0] != x0 || sScissor[1] != y0 || sScissor[2] != x1 || sScissor[3] != y1) {
        RECT sc;
        sc.left = x0; sc.top = y0; sc.right = x1; sc.bottom = y1;
        sDev->SetScissorRect(&sc);
        sScissor[0] = x0; sScissor[1] = y0; sScissor[2] = x1; sScissor[3] = y1;
        sTemScissor = 1;
    }
    while (feitos + porPrim <= d->count) {
        uint32_t n = d->count - feitos > MAX_UP ? MAX_UP : d->count - feitos;
        n -= n % porPrim;
        sDev->DrawPrimitiveUP(tipo, n / porPrim, &gsVerts[d->first + feitos], sizeof(Vtx));
        feitos += n;
    }
    ret_une(&sSujo, x0, y0, x1, y1);
    sConta.desenhos++;
    sConta.vertices += d->count;
    if (d->count > sConta.maiorDesenho) sConta.maiorDesenho = d->count;
}

/* o máximo de um quadro em cada buffer fixo do gs_draw.c (no começo do frame_end, com a lista completa) */
static void mem_buffers(void) {
    if (gsVertCount > sMem.maxVerts) sMem.maxVerts = gsVertCount;
    if (gsVuVertCount > sMem.maxVu) sMem.maxVu = gsVuVertCount;
    if (gsVuIdxCount > sMem.maxIdx) sMem.maxIdx = gsVuIdxCount;
    if (gsVuUniCount > sMem.maxUni) sMem.maxUni = gsVuUniCount;
    if (gsDrawCount > sMem.maxDraws) sMem.maxDraws = gsDrawCount;
}

static void log_memoria_zera(void) {
    sMem.pressoes = sMem.descN = sMem.descBytes = sMem.esperas = sMem.acimaTeto = 0;
    memset(sMem.branca, 0, sizeof(sMem.branca));
    sMem.maxVerts = sMem.maxVu = sMem.maxIdx = sMem.maxUni = sMem.maxDraws = 0;
}

/* A memória no trecho de 60 quadros: bytes vivos e pendentes por classe, o resto do sistema, o mínimo livre visto,
   os descartes, as esperas pela GPU, as brancas por motivo e o máximo de cada buffer (CHEIO quando chegou ao fim). */
static void log_memoria(void) {
    unsigned nosso = sMem.vivos[CL_TEX] + sMem.vivos[CL_ALVO] + sMem.pend[CL_TEX] + sMem.pend[CL_ALVO] + sMem.buffers;
    unsigned usado = sMem.total > sMem.livres ? sMem.total - sMem.livres : 0;
    unsigned brancas = 0;
    int i;
    for (i = 1; i < BR_N; i++) {
        brancas += sMem.branca[i];
    }
    Diario_Escrever("gs_d3d:   memória: texturas %u (%u KB, teto %u MB), alvos %u (%u KB), pendentes %u + %u (%u KB + %u KB), "
                    "buffers %u KB; resto do jogo e do sistema %d MB; livres %u MB, mínimo visto %u MB (piso %u)",
                    sMem.nVivos[CL_TEX], sMem.vivos[CL_TEX] >> 10, sMem.texTetoMb, sMem.nVivos[CL_ALVO],
                    sMem.vivos[CL_ALVO] >> 10, sMem.nPend[CL_TEX], sMem.nPend[CL_ALVO], sMem.pend[CL_TEX] >> 10,
                    sMem.pend[CL_ALVO] >> 10, sMem.buffers >> 10, ((int)usado - (int)nosso) / (int)MB, sMem.livres >> 20,
                    sMem.livresMin >> 20, sMem.pisoMb);
    Diario_Escrever("gs_d3d:   descarte: %u pressões, %u texturas (%u KB), %u esperas pela GPU, %u acima do teto no trecho; desde o "
                    "começo %u pressões, %u texturas (%.0f MB), %u esperas; brancas no trecho %u (sem espaço %u, cache cheio %u, criação "
                    "%u, formato %u, alvo sem textura %u), alvos sem textura agora %u",
                    sMem.pressoes, sMem.descN, sMem.descBytes >> 10, sMem.esperas, sMem.acimaTeto, sMem.pressoesTot, sMem.descNTot,
                    sMem.descBytesTot / MB, sMem.esperasTot, brancas, sMem.branca[BR_ESPACO], sMem.branca[BR_CACHE],
                    sMem.branca[BR_D3D], sMem.branca[BR_FORMATO], sMem.branca[BR_ALVO], sMem.semAlvo);
    Diario_Escrever("gs_d3d:   buffers, o maior quadro do trecho: vértices %u de %u%s, VU %u de %u%s, índices %u de %u%s, "
                    "constantes %u de %u%s, desenhos %u de %u%s",
                    sMem.maxVerts, (unsigned)MAX_VERTS, sMem.maxVerts >= MAX_VERTS ? " CHEIO" : "", sMem.maxVu,
                    (unsigned)MAX_VU_VERTS, sMem.maxVu >= MAX_VU_VERTS ? " CHEIO" : "", sMem.maxIdx, (unsigned)MAX_VU_IDX,
                    sMem.maxIdx >= MAX_VU_IDX ? " CHEIO" : "", sMem.maxUni, (unsigned)MAX_VU_UNIFORMS,
                    sMem.maxUni >= MAX_VU_UNIFORMS ? " CHEIO" : "", sMem.maxDraws, (unsigned)MAX_DRAWS,
                    sMem.maxDraws >= MAX_DRAWS ? " CHEIO" : "");
    log_memoria_zera();
}

static void log_quadros(void) {
    /* os tempos sem os quadros que tiveram captura (cada uma pesa centenas de ms e encobria o resto) */
    unsigned q = sConta.quadros, t = sConta.quadros + sConta.capturas;
    int i;
    /* (os descartes acumulados, antes de zerar os do trecho) */
    sDescAntes[0] += sConta.semVu;
    for (i = 1; i <= 4; i++) {
        sDescAntes[i] += sConta.semNativo[i];
    }
    sDescAntes[5] += sConta.semFb;
    sDescAntes[6] += sConta.semTextura;
    if (Passos_Medindo()) {
        memset(&sConta, 0, sizeof(sConta));
        log_memoria_zera();
        return;
    }
    if (q == 0) {
        q = 1;
    }
    Diario_Escrever("gs_d3d: quadro %u: %u quadros sem captura: quadro inteiro %.2f ms (pior %.2f), dos quais fim do quadro "
                    "%.2f (pior %.2f) e nele o Present %.2f (pior %.2f); jogo e gravação %.2f; %u acima de 16,7 e %u acima "
                    "de 33,4 ms; %u capturas, %.1f ms cada", gGsFrame, sConta.quadros, sConta.msQuadro / q, sConta.msQuadroPior,
                    sConta.msFim / q, sConta.msFimPior, sConta.msPresent / q, sConta.msPresentPior,
                    (sConta.msQuadro - sConta.msFim) / q, sConta.acima16, sConta.acima33, sConta.capturas,
                    sConta.capturas ? sConta.msCaptura / sConta.capturas : 0.0);
    Diario_Escrever("gs_d3d:   por quadro: %.1f desenhos, %.0f vértices, %.1f trocas de alvo; maior desenho %u vértices",
                    (double)sConta.desenhos / t, (double)sConta.vertices / t, (double)sConta.trocas / t, sConta.maiorDesenho);
    Diario_Escrever("gs_d3d:   %u texturas novas (%u KB de textura viva, %d no cache), %d alvos, %d pipelines; fora: %u VU1, "
                    "nativos %u/%u/%u/%u (contorno, chave, tabela, ids), %u imagens direto no quadro, %u texturas recusadas; "
                    "%u MB livres", sConta.texNovas, sMem.vivos[CL_TEX] >> 10, gsTexCount, gsTargetCount, sPipeCount, sConta.semVu,
                    sConta.semNativo[1], sConta.semNativo[2], sConta.semNativo[3], sConta.semNativo[4], sConta.semFb,
                    sConta.semTextura, mb_livres());
    log_memoria();
    memset(&sConta, 0, sizeof(sConta));
}

static void frame_end(void) {
    LARGE_INTEGER t0, tp;
    double msFim, msPresent;
    unsigned capturas = sConta.capturas;
    int cur = -1, mostrado, i;
    uint32_t n;
    double ms;
    static int every = -1;

    QueryPerformanceCounter(&t0);
    mem_buffers();
    gGsD3dEtapa = "fim do quadro: refazendo a lista";
    if (gX360Luta != sLutaVista) { /* os primeiros quadros da luta, passo a passo (a primeira rodada travou ali) */
        sLutaVista = gX360Luta;
        sPassoAPasso = gX360Luta != 0 ? 8 : 0;
    }
    if (sPassoAPasso > 0) {
        Diario_Escrever("gs_d3d: quadro %u (luta): %u desenhos, %u vértices, %u da VU1, %d alvos, %d texturas", gGsFrame,
                        gsDrawCount, gsVertCount, gsVuVertCount, gsTargetCount, gsTexCount);
    }
    sDateFonte = NULL;
    for (n = 0; n < gsDrawCount; n++) {
        const GsDraw *d = &gsDraws[n];
        if (d->native >= 100) {
            sConta.semFb++;
            continue;
        }
        if (d->native == 5) {
            /* o começo de uma sequência de desenhos com DATE: a cópia da cor do alvo agora */
            if (!gsTargets[d->target].cleared) {
                continue;
            }
            sDateFonte = sAlvos[d->target].tex; /* fora da EDRAM, a textura do alvo já é a imagem dele agora */
            if (d->target == sResidente) {
                Ret r = sSujo;
                ret_une(&r, sAlvos[d->target].conteudo.x0, sAlvos[d->target].conteudo.y0, sAlvos[d->target].conteudo.x1,
                        sAlvos[d->target].conteudo.y1);
                if (r.x1 > r.x0 && sDate != NULL) {
                    D3DRECT fonte;
                    D3DPOINT destino;
                    alinha8(&r);
                    fonte.x1 = r.x0; fonte.y1 = r.y0; fonte.x2 = r.x1; fonte.y2 = r.y1;
                    destino.x = r.x0; destino.y = r.y0;
                    sDev->SetTexture(1, NULL); /* (o destino do Resolve não pode estar em uso) */
                    sCurDate = NULL;
                    sDev->Resolve(D3DRESOLVE_RENDERTARGET0, &fonte, sDate, &destino, 0, 0, NULL, 0.0f, 0, NULL);
                }
                sDateFonte = sDate;
            }
            continue;
        }
        if (d->native != 0) {
            sConta.semNativo[d->native & 7]++;
            continue;
        }
        if (d->vu) {
            sConta.semVu++;
            continue;
        }
        if (cur != d->target || sResidente != d->target) {
            entra_na_edram(d->target);
            gsTargets[d->target].cleared = 1;
            cur = d->target;
        }
        desenha(d);
    }
    gGsD3dEtapa = "fim do quadro: o último alvo volta para a textura";
    sai_da_edram();
    if (sPassoAPasso > 0) {
        Diario_Escrever("gs_d3d: quadro %u (luta): lista refeita (maior desenho até aqui %u vértices)", gGsFrame,
                        sConta.maiorDesenho);
    }

    /* a imagem mostrada: a que o sceGsSwapDBuff montou para este quadro, senão o primeiro alvo (como o gs_gl.c) */
    mostrado = gGsMainFbp >= 0 ? GsDraw_TargetGet((uint32_t)gGsMainFbp, 0) : -1;
    for (i = 0; mostrado < 0 && i < gsTargetCount; i++) {
        mostrado = i;
    }
    if (every < 0) {
        every = getenv("BT3_SHOT") != NULL ? atoi(getenv("BT3_SHOT")) : 0;
    }
    if (((every > 0 && gGsFrame % (unsigned)every == 0 &&
          (getenv("BT3_SHOT_FROM") == NULL || (int)gGsFrame >= atoi(getenv("BT3_SHOT_FROM"))) &&
          (getenv("BT3_SHOT_TO") == NULL || (int)gGsFrame <= atoi(getenv("BT3_SHOT_TO")))) ||
         na_lista(gGsFrame)) &&
        mostrado >= 0 && gsTargets[mostrado].cleared && sAlvos[mostrado].tex != NULL) {
        gGsD3dEtapa = "fim do quadro: captura";
        captura(mostrado);
    }
    gGsD3dEtapa = "fim do quadro: cópia para a tela";
    {
        D3DVIEWPORT9 vp = {0, 0, TELA_W, TELA_H, 0.0f, 1.0f};
        sDev->SetRenderTarget(0, sTela);
        sDev->SetDepthStencilSurface(NULL);
        sDev->SetViewport(&vp);
        sDev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        sDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);
        sDev->Clear(0, NULL, D3DCLEAR_TARGET, sCorFundo, 1.0f, 0);
        if (mostrado >= 0 && gsTargets[mostrado].cleared && sAlvos[mostrado].tex != NULL) {
            /* 512 x 448 do GS na forma da imagem (4:3: 960 x 720 no meio da tela de 1280 x 720) */
            int w = (int)((float)TELA_H * (float)Port_AspectMilli() / 1000.0f + 0.5f), x;
            w = w > TELA_W ? TELA_W : w;
            x = (TELA_W - w) / 2;
            copia(sAlvos[mostrado].tex, TELA_W, TELA_H, (float)x, 0.0f, (float)(x + w), (float)TELA_H, 0.0f, 0.0f,
                  512.0f / TAM, 448.0f / TAM, 1);
        }
    }
    QueryPerformanceCounter(&tp);
    gGsD3dEtapa = "fim do quadro: Present";
    sDev->Present(NULL, NULL, NULL, NULL);
    msPresent = ms_desde(&tp);
    Passos_Apresentou(gGsFrame, (unsigned)(msPresent * 1000.0));
    gGsD3dEtapa = "fim do quadro: texturas soltas";
    /* nada do quadro fica preso no dispositivo: as texturas soltas podem ir quando a GPU terminar com elas */
    sDev->SetTexture(0, NULL);
    sDev->SetTexture(1, NULL);
    esquece_estado();
    sDev->InsertFence();
    cova_varre();
    gGsD3dEtapa = "fim do quadro: memória";
    mem_mede(); /* (uma medida por quadro: o mínimo livre visto e a base da estimativa) */
    alvos_sem_textura();

    msFim = ms_desde(&t0);
    ms = ms_desde(&sUltimoFim);
    QueryPerformanceCounter(&sUltimoFim);
    if (capturas == sConta.capturas) {
        sConta.msFim += msFim;
        if (msFim > sConta.msFimPior) sConta.msFimPior = msFim;
        sConta.msPresent += msPresent;
        if (msPresent > sConta.msPresentPior) sConta.msPresentPior = msPresent;
        sConta.msQuadro += ms;
        if (ms > sConta.msQuadroPior) sConta.msQuadroPior = ms;
        sConta.acima16 += ms > 16.7;
        sConta.acima33 += ms > 33.4;
        sConta.quadros++;
    }
    if (sConta.quadros + sConta.capturas >= 60) {
        log_quadros();
    }
    if (sPassoAPasso > 0) {
        Diario_Escrever("gs_d3d: quadro %u (luta): apresentado em %.2f ms (Present %.2f)", gGsFrame, msFim, msPresent);
        sPassoAPasso--;
    }
    gGsD3dQuadros++;
    gGsD3dEtapa = "jogo e gravação do quadro";

    for (i = 0; i < gsTargetCount; i++) {
        gsTargets[i].draws = 0;
    }
    gsVertCount = 0;
    gsVuVertCount = 0;
    gsVuIdxCount = 0;
    gsVuUniCount = 0;
    gsDrawCount = 0;
    gsAnchor = 0;
    sPs2Total += gsSkipped;
    gsSkipped = 0;
    gsNative = 0;
}

/* O jogo terminou nesta thread (Port_Sair ou queda): o dispositivo volta para quem for usá-lo (a main). */
extern "C" void GsD3d_Soltar(void) {
    if (sDev != NULL && sDev->QueryThreadOwnership() == GetCurrentThreadId()) {
        sDev->ReleaseThreadOwnership();
    }
}

extern "C" {
GsBackend sD3dBackend = {
    "d3d9",
    d3d_init,
    frame_end,
    scale_changed,
    white_tex,
    tex_create,
    tex_upload,
    tex_destroy,
    tex_slots_left,
    target_ensure,
    target_drop,
    target_color,
    target_none,
    target_none,
    pipe_get,
};
}

/* gs_texpack.c: o 360 não tem pacote de texturas (o leitor do PC é feito sobre a SDL) */
extern "C" {
void TexPack_Init(void) {}
int TexPack_Count(void) { return 0; }
const char *TexPack_Lookup(uint32_t tbp, uint32_t tbw, uint32_t psm, uint32_t tw, uint32_t th, uint32_t tcc, uint32_t cbp,
                           uint32_t cpsm, uint32_t *maxAlpha) {
    (void)tbp; (void)tbw; (void)psm; (void)tw; (void)th; (void)tcc; (void)cbp; (void)cpsm; (void)maxAlpha;
    return NULL;
}
int TexPack_Load(const char *path, TexPackImage *img) { (void)path; (void)img; return 0; }
void TexPack_Free(TexPackImage *img) { (void)img; }
}
