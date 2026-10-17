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
 *     novo alvo (um retângulo desenhado). A profundidade (F5, etapa 7) faz o mesmo caminho: o que os desenhos com
 *     escrita de z tocaram volta para uma textura D24S8 do alvo, e a volta para a EDRAM a restaura (como cor na mesma
 *     base, ou pelo oDepth; o autoteste do boot escolhe). Ver prof_* abaixo e docs/x360/plano/F5/07.md.
 *   - Ler a cor de um alvo como textura (o jogo faz isso nos efeitos) é ler a textura do alvo, sempre em dia: um
 *     alvo nunca é lido enquanto é o desenhado (gs_draw.c recusa), então ele já saiu da EDRAM.
 *   - Sem o segundo alvo com o byte de alfa (outAux do gs.frag): o teste de alfa do destino (DATE) lê o alfa da
 *     própria cor, que guarda min(A / 0x80, 1) e chega a 1 exatamente com o bit 7 ligado. Os efeitos nativos que
 *     precisam do byte (contorno, tabela de profundidade, cópia de ids) ficam para a F5, e as imagens copiadas direto
 *     para a memória de vídeo (filmes) para a F6: são contados e não desenhados.
 *   - Os programas de vértice (VU1, F5, etapa 6): as três famílias, vu0 (os lutadores), vu4 (o estágio) e vu6 (a
 *     sombra no chão) (vs_vu0, vs_vu4 e vs_vu6 em gs_d3d.hlsl), são desenhadas com os vértices e os índices do quadro
 *     copiados para um par de buffers da GPU (dois pares, alternados por quadro). BT3_VU_VERTS=65536 vértices por par
 *     (os índices são o triplo), para as três juntas; o que não couber é contado no log, nunca some.
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
#include "zcor_ps.h"
#include "zprof_ps.h"
#include "vu0_vs.h"
#include "vu4_vs.h"
#include "vu6_vs.h"

#define TAM GS_W            /* lado de um alvo, em pixels (SCALE = 1) */
/* (F5, etapa 7) a largura da profundidade vista como cor: os 13 tiles de 80 pixels que o alvo de 1024 ocupa na EDRAM.
   O restauro como cor troca as metades de 40 colunas de cada tile, então a profundidade das colunas 984 a 999 mora
   nas colunas 1024 a 1039 da cor, que uma superfície de 1024 não alcança. */
#define TAMZ ((TAM + 79) / 80 * 80)
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
/* F5, etapa 6: as três famílias da VU1 (GsDraw.vu 1 vu0, os lutadores; 2 vu4, o estágio; 3 vu6, a sombra no chão): os
   shaders, a declaração comum (três FLOAT4, 48 bytes) e os pares de buffers da GPU com os vértices e os índices de um
   quadro, alternados (o do quadro anterior ainda pode estar em uso) */
#define VU_PARES 2
#define VU_C0 16 /* o primeiro registrador das constantes dos vs_vu* (Vu0Uniform, 19 float4, o mesmo nas três) */
static D3DVertexShader *sVsVu[3]; /* vs_vu0, vs_vu4, vs_vu6 (GsDraw.vu - 1) */
static D3DVertexDeclaration *sDeclVu;
static D3DVertexBuffer *sVuVb[VU_PARES];
static D3DIndexBuffer *sVuIb[VU_PARES];
static unsigned sVuCapV, sVuCapI; /* vértices e índices de cada par */
static int sVuPar;
static uint32_t sVuIni[MAX_DRAWS]; /* o primeiro índice de cada desenho da VU1 no buffer do quadro (~0: fora) */
static int sCurUni = -1;            /* o Vu0Uniform posto nas constantes dos vs_vu* */
static D3DSurface *sEdramCor, *sEdramProf, *sTela;
static D3DSurface *sEdramProfCor; /* (F5, etapa 7) a profundidade da EDRAM vista como cor: mesma base, mesmos tiles */
static D3DPixelShader *sPsZCor, *sPsZProf;
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

/* F5, etapa 7: a profundidade de cada alvo fora da EDRAM. A textura só existe para os alvos em que algum desenho
   escreve z (criada no começo do fim do quadro, antes de refazer a lista); `conteudo` tem x em múltiplos de 80 (a
   largura de um tile da EDRAM, por causa da troca de metades do restauro como cor). */
typedef struct Prof {
    D3DTexture *tex; /* D24S8 tiled, o destino do Resolve da profundidade (NULL: sem) */
    D3DTexture cor;  /* cabeçalho: a mesma memória lida como A8B8G8R8 (o restauro como cor) */
    Ret conteudo;    /* onde ela vale (fora dali, zero, o valor da limpeza) */
    unsigned falhou; /* o quadro em que a criação falhou (tenta de novo 60 quadros depois) */
} Prof;
static Prof sProf[MAX_TARGETS];
static int sModoZ;       /* 0 a profundidade volta zerada (o de antes), 1 restauro como cor, 2 pelo oDepth */
static int sZDono = -1;  /* o alvo cuja profundidade está na EDRAM (-1: nenhum; ver prof_entrega) */
static Ret sSujoZ;       /* o que os desenhos com escrita de z do sZDono tocaram desde que ela entrou na EDRAM */
static double sMsZRes, sMsZRest, sMsCorRes, sMsCorRest; /* autoteste: GPU por alvo inteiro (1024 x 1024), ms */

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

enum { CL_TEX, CL_ALVO, CL_PROF, CL_N }; /* CL_PROF: as profundidades dos alvos (F5, etapa 7) */
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
    /* F5, etapa 6, por família da VU1 (0 vu0, 1 vu4, 2 vu6): desenhos da lista feitos pela GPU, pulados (e deles os
       por falta de espaço nos buffers), triângulos feitos, o maior quadro em vértices e índices; o maior quadro das
       três juntas (vuMaxT: vértices, índices), o que o par tem de comportar; as esperas pelo par;
       o tempo da VU1 na gravação (gs_vu1.c) e o do desenho dela aqui (cópia e chamadas), por quadro */
    unsigned vuFeitos[3], vuPulados[3], vuSemEspaco[3], vuTri[3], vuMaxV[3], vuMaxI[3], vuMaxT[2], vuEsperas;
    double msVuGrav, msVuGravPior, msVuDes, msVuDesPior;
    double msFim, msFimPior, msQuadro, msQuadroPior, msPresent, msPresentPior;
    unsigned acima16, acima33, capturas, maiorDesenho;
    double msCaptura;
    /* F5, etapa 7: profundidade: resolves e restauros (e os pixels de cada), visitas que usaram z, visitas que
       precisavam e não tinham textura; restauros e resolves de cor (pixels), para a estimativa da GPU; o tempo de CPU
       das chamadas da profundidade (o da GPU sai do autoteste) */
    unsigned zRes, zRest, zVisitas, zMantida, zSemTex;
    double corResPx, corRestPx, zResPx, zRestPx, msZ, msZPior, msZQuadro;
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

/* o relógio para o gs_vu1.c (C, sem o xtl.h): o tempo da VU1 na gravação do quadro */
extern "C" uint64_t GsD3d_Tique(void) {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (uint64_t)t.QuadPart;
}
extern "C" double GsD3d_Ms(uint64_t tiques) {
    return (double)tiques * 1000.0 / (double)sFreq.QuadPart;
}
extern "C" {
extern unsigned gGsVu1Lotes[4][3], gGsVu1Desconhecido[8][2], gGsVu1InstrDesc; /* gs_vu1.c */
extern double gGsVu1Ms;
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
                        "pendentes, alvos e profundidades %u KB, livres %u MB (piso %u): %s",
                        sMem.pressoesTot, gGsFrame, pedido >> 10, modo == MODO_ALVO ? "alvo" : "textura", w, h, n, bytes >> 10,
                        idade, emUso, sMem.esperas - esperas, sMem.vivos[CL_TEX] >> 10, sMem.pend[CL_TEX] >> 10,
                        (sMem.vivos[CL_ALVO] + sMem.pend[CL_ALVO] + sMem.vivos[CL_PROF] + sMem.pend[CL_PROF]) >> 10, livres >> 20, sMem.pisoMb,
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
    /* Triagem da etapa 6 (docs/x360/plano/F5/06.md): uma textura com todos os texels de RGB zero é contada e as 32
       primeiras vão para o log, com o tamanho (256 x 1 é paleta) e o quadro. Só diagnóstico: o cenário saía com texturas
       pretas e nada diz se o jogo as mandou assim ou se a decodificação (paleta, formato) falhou. Para na primeira cor
       não preta, então quase não custa. */
    {
        static unsigned sPretas;
        const uint32_t *p32 = (const uint32_t *)px;
        uint32_t n = w * h, i = 0;
        while (i < n && (p32[i] & 0x00FFFFFFu) == 0) {
            i++;
        }
        if (n > 1 && i == n) {
            sPretas++;
            if (sPretas <= 32) {
                Diario_Escrever("gs_d3d: textura preta (RGB zero em todos os texels) %ux%u no quadro %u, a %u.a", (unsigned)w,
                                (unsigned)h, gGsFrame, sPretas);
            }
        }
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

static void prof_solta(int i);
static void target_ensure(int i) {
    sAlvos[i].tex = alvo_com_espaco();
    sAlvos[i].conteudo.x0 = sAlvos[i].conteudo.x1 = 0;
    sProf[i].conteudo.x0 = sProf[i].conteudo.x1 = 0; /* (a textura de profundidade, se houver, fica para o novo dono) */
    if (sZDono == i) {
        sZDono = -1;
        sSujoZ.x0 = sSujoZ.x1 = 0;
    }
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
    prof_solta(i);
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
    sCurUni = -1;
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

/* Um retângulo do alvo atual (W x H) com a textura t pelo pixel shader ps (vs_copia); os estados de mistura,
   profundidade e escrita são do chamador. */
static void quad(D3DTexture *t, D3DPixelShader *ps, int W, int H, float x0, float y0, float x1, float y1, float u0,
                 float v0, float u1, float v1, int linear) {
    struct { float x, y, u, v; } q[4];
    float cx0 = x0 / W * 2.0f - 1.0f, cx1 = x1 / W * 2.0f - 1.0f, cy0 = 1.0f - y0 / H * 2.0f, cy1 = 1.0f - y1 / H * 2.0f;
    q[0].x = cx0; q[0].y = cy0; q[0].u = u0; q[0].v = v0;
    q[1].x = cx1; q[1].y = cy0; q[1].u = u1; q[1].v = v0;
    q[2].x = cx0; q[2].y = cy1; q[2].u = u0; q[2].v = v1;
    q[3].x = cx1; q[3].y = cy1; q[3].u = u1; q[3].v = v1;
    sDev->SetVertexDeclaration(sDeclCopia);
    sDev->SetVertexShader(sVsCopia);
    sDev->SetPixelShader(ps);
    sDev->SetTexture(0, t);
    aplica_sampler(0, (linear ? 1 : 0) | 2 | 4);
    sDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    sDev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    sDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(q[0]));
    esquece_estado();
}

/* Uma textura copiada para um retângulo do alvo atual (W x H): sem mistura, sem profundidade, sem tesoura. */
static void copia(D3DTexture *t, int W, int H, float x0, float y0, float x1, float y1, float u0, float v0, float u1,
                  float v1, int linear) {
    sDev->SetRenderState(D3DRS_ZENABLE, FALSE);
    sDev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    sDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);
    quad(t, sPsCopia, W, H, x0, y0, x1, y1, u0, v0, u1, v1, linear);
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

/* (profundidade) x em múltiplos de 80, a largura de um tile da EDRAM com 32 bits por pixel: o restauro como cor lê o
   pixel x + 40 ou x - 40 do mesmo tile, que assim fica sempre dentro do que foi resolvido */
static void alinha80(Ret *r) {
    alinha8(r);
    r->x0 -= r->x0 % 80;
    r->x1 = (r->x1 + 79) / 80 * 80;
    if (r->x1 > TAM) r->x1 = TAM;
}

/* ---- profundidade por alvo (F5, etapa 7) --------------------------------------------------------------------- */

/* A profundidade de r (alinhado) da textura z volta para a EDRAM, que já tem o alvo posto (sEdramCor e sEdramProf).
   Modo 1: escrita como cor na superfície que tem a mesma base da profundidade (o exemplo FastDepthRestore do XDK).
   Modo 2: pelo oDepth, com a cor desligada. */
static void prof_poe(Prof *z, const Ret *r, int modo) {
    D3DVIEWPORT9 vp = {0, 0, TAM, TAM, 0.0f, 1.0f}, vpz = {0, 0, TAMZ, TAM, 0.0f, 1.0f};
    float x0 = (float)r->x0, y0 = (float)r->y0, x1 = (float)r->x1, y1 = (float)r->y1;
    if (modo == 1) {
        if (r->x1 == TAM) {
            x1 = (float)TAMZ; /* (o último tile inteiro: ver TAMZ) */
        }
        sDev->SetDepthStencilSurface(NULL);
        sDev->SetRenderTarget(0, sEdramProfCor);
        sDev->SetViewport(&vpz);
        sDev->SetRenderState(D3DRS_ZENABLE, FALSE);
        sDev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        sDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);
        quad(&z->cor, sPsZCor, TAMZ, TAM, x0, y0, x1, y1, x0 / TAM, y0 / TAM, x1 / TAM, y1 / TAM, 0);
        sDev->SetRenderTarget(0, sEdramCor);
        sDev->SetDepthStencilSurface(sEdramProf);
        sDev->SetViewport(&vp);
    } else {
        sDev->SetRenderState(D3DRS_ZENABLE, TRUE);
        sDev->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
        sDev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        sDev->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
        quad(z->tex, sPsZProf, TAM, TAM, x0, y0, x1, y1, x0 / TAM, y0 / TAM, x1 / TAM, y1 / TAM, 0);
    }
    /* (a textura não fica presa no estágio: ela é destino do próximo Resolve deste alvo) */
    sDev->SetTexture(0, NULL);
    sCurTex = (GsTex)-1;
}

/* A profundidade de r da EDRAM para a textura z (o depth-stencil posto é o sEdramProf). */
static void prof_tira(Prof *z, const Ret *r) {
    D3DRECT fonte;
    D3DPOINT destino;
    fonte.x1 = r->x0; fonte.y1 = r->y0; fonte.x2 = r->x1; fonte.y2 = r->y1;
    destino.x = r->x0; destino.y = r->y0;
    sDev->Resolve(D3DRESOLVE_DEPTHSTENCIL, &fonte, z->tex, &destino, 0, 0, NULL, 0.0f, 0, NULL);
}

/* a textura de profundidade de um alvo e o cabeçalho que a lê como cor; 0: não coube */
static int prof_cria(int i, int abre) {
    Prof *z = &sProf[i];
    D3DTexture *t = NULL;
    XGTEXTURE_DESC desc;
    UINT base = 0;
    if (abre) {
        const char *antes = (const char *)gGsD3dEtapa;
        gGsD3dEtapa = "criando a textura de profundidade de um alvo";
        GsDraw_X360Abre(TAM, TAM, MODO_ALVO); /* (mesmo tamanho de um alvo: 4 MB) */
        gGsD3dEtapa = antes;
    }
    if (mem_livres(tam_alvo()) < sMem.pisoMb * MB + tam_alvo() ||
        FAILED(sDev->CreateTexture(TAM, TAM, 1, 0, D3DFMT_D24S8, D3DPOOL_DEFAULT, &t, NULL)) || t == NULL) {
        return 0;
    }
    mem_cria(CL_PROF, tam_alvo());
    XGGetTextureDesc(t, 0, &desc);
    XGGetTextureLayout(t, &base, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL, 0);
    XGSetTextureHeader(TAM, TAM, 1, 0, D3DFMT_A8B8G8R8, (D3DPOOL)0, base, 0, desc.RowPitch, &z->cor, NULL, NULL);
    z->tex = t;
    z->conteudo.x0 = z->conteudo.x1 = 0;
    return 1;
}

static void prof_solta(int i) {
    Prof *z = &sProf[i];
    if (sZDono == i) {
        sZDono = -1;
        sSujoZ.x0 = sSujoZ.x1 = 0;
    }
    if (z->tex != NULL) {
        /* o cabeçalho que lê como cor tem a cerca dele: a GPU pode ainda estar lendo a memória por ele */
        if (z->cor.IsBusy()) {
            z->cor.BlockUntilNotBusy();
        }
        cova(z->tex, CL_PROF, tam_alvo());
    }
    z->tex = NULL;
    z->conteudo.x0 = z->conteudo.x1 = 0;
    z->falhou = 0;
}

/* No começo do fim do quadro, antes de refazer a lista (nada dela está na GPU ainda e as texturas do quadro estão
   protegidas do descarte): a textura de profundidade de cada alvo em que algum desenho escreve z. */
static void prof_garante(void) {
    static unsigned char quer[MAX_TARGETS];
    static unsigned falas;
    uint32_t n;
    int i;
    if (sModoZ == 0) {
        return;
    }
    memset(quer, 0, sizeof(quer));
    for (n = 0; n < gsDrawCount; n++) {
        const GsDraw *d = &gsDraws[n];
        if (d->native == 0 && d->count != 0 && d->target >= 0 && d->target < gsTargetCount &&
            sPipes[d->pipeline].zwrite && sPipes[d->pipeline].ztst != 0) {
            quer[d->target] = 1;
        }
    }
    for (i = 0; i < gsTargetCount; i++) {
        if (!quer[i] || sProf[i].tex != NULL || (sProf[i].falhou != 0 && gGsFrame < sProf[i].falhou + 60)) {
            continue;
        }
        if (prof_cria(i, 1)) {
            sProf[i].falhou = 0;
            Diario_Escrever("gs_d3d: profundidade: o alvo %d ganhou a textura de profundidade no quadro %u (%u ao todo, %u KB)",
                            i, gGsFrame, sMem.nVivos[CL_PROF], sMem.vivos[CL_PROF] >> 10);
        } else {
            sProf[i].falhou = gGsFrame | 1;
            if (falas++ < 20) {
                Diario_Escrever("gs_d3d: ERRO: profundidade: o alvo %d ficou sem textura de profundidade no quadro %u: livres %u MB "
                                "(piso %u); a profundidade dele volta zerada, tenta de novo em 60 quadros", i, gGsFrame,
                                mb_livres(), sMem.pisoMb);
            }
        }
    }
}

/* A profundidade que está na EDRAM (a do alvo sZDono) volta para a textura dele: só o que os desenhos com escrita de z
   tocaram desde que ela entrou. Chamada quando outro alvo que usa z vai entrar, no fim do quadro e quando o dono
   some. Entre uma coisa e outra, as visitas de alvos que não usam z não limpam a profundidade da EDRAM: quando o
   dono volta, ela ainda está lá, sem resolve nem restauro (o caso das passadas de tela no meio do cenário). */
static void prof_entrega(void) {
    if (sZDono >= 0 && sSujoZ.x1 > sSujoZ.x0 && sProf[sZDono].tex != NULL) {
        Prof *z = &sProf[sZDono];
        LARGE_INTEGER t;
        Ret rz = sSujoZ;
        QueryPerformanceCounter(&t);
        sDev->SetTexture(0, NULL);
        sDev->SetTexture(1, NULL);
        sCurTex = (GsTex)-1;
        sCurDate = NULL;
        alinha80(&rz);
        prof_tira(z, &rz);
        ret_une(&z->conteudo, rz.x0, rz.y0, rz.x1, rz.y1);
        sConta.zRes++;
        sConta.zResPx += (double)(rz.x1 - rz.x0) * (rz.y1 - rz.y0);
        sConta.msZQuadro += ms_desde(&t);
    }
    sZDono = -1;
    sSujoZ.x0 = sSujoZ.x1 = 0;
}

/* O alvo da EDRAM volta para a textura dele (só o que foi desenhado desde que entrou). A profundidade não: ela fica
   na EDRAM até outro alvo que usa z entrar (prof_entrega). */
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
        sConta.corResPx += (double)(r.x1 - r.x0) * (r.y1 - r.y0);
    }
    sResidente = -1;
    sSujo.x0 = sSujo.x1 = 0;
}

/* Põe o alvo i na EDRAM: limpa a cor (0) e, se ele já tinha conteúdo, desenha a textura dele de volta por cima.
   A profundidade (usaZ: algum desenho desta visita testa ou escreve z): se a da EDRAM já é a dele, fica; senão a do
   dono anterior volta para a textura dele, a EDRAM é limpa (0, como o gs_gl.c limpa um alvo novo) e a dele volta da
   textura. Uma visita sem z não toca na profundidade. Com BT3_Z=0 (sModoZ 0), o de antes: tudo limpo a cada entrada. */
static void entra_na_edram(int i, int usaZ) {
    Alvo *a = &sAlvos[i];
    D3DVIEWPORT9 vp = {0, 0, TAM, TAM, 0.0f, 1.0f};
    DWORD limpa = D3DCLEAR_TARGET;
    int poeZ = 0;
    sai_da_edram();
    if (!gsTargets[i].cleared && sZDono == i) {
        sZDono = -1; /* (novo para o jogo: o que a EDRAM tem dele não vale) */
        sSujoZ.x0 = sSujoZ.x1 = 0;
    }
    if (sModoZ == 0) {
        limpa |= D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL;
    } else if (usaZ) {
        sConta.zVisitas++;
        sConta.zSemTex += sProf[i].tex == NULL;
        if (sZDono == i) {
            sConta.zMantida++;
        } else {
            prof_entrega();
            limpa |= D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL;
            poeZ = 1;
        }
    }
    sDev->SetRenderTarget(0, sEdramCor);
    sDev->SetDepthStencilSurface(sEdramProf);
    sDev->SetViewport(&vp);
    sDev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    sDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);
    sDev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    sDev->Clear(0, NULL, limpa, 0x00000000, 0.0f, 0);
    if (!gsTargets[i].cleared) {
        a->conteudo.x0 = a->conteudo.x1 = 0; /* novo para o jogo: começa limpo */
        sProf[i].conteudo.x0 = sProf[i].conteudo.x1 = 0;
    } else {
        if (a->conteudo.x1 > a->conteudo.x0 && a->tex != NULL) {
            Ret r = a->conteudo;
            copia(a->tex, TAM, TAM, (float)r.x0, (float)r.y0, (float)r.x1, (float)r.y1, (float)r.x0 / TAM, (float)r.y0 / TAM,
                  (float)r.x1 / TAM, (float)r.y1 / TAM, 0);
            sConta.restauros++;
            sConta.corRestPx += (double)(r.x1 - r.x0) * (r.y1 - r.y0);
        }
        if (poeZ && sProf[i].tex != NULL && sProf[i].conteudo.x1 > sProf[i].conteudo.x0) {
            LARGE_INTEGER t;
            Ret r = sProf[i].conteudo;
            QueryPerformanceCounter(&t);
            prof_poe(&sProf[i], &r, sModoZ);
            sConta.zRest++;
            sConta.zRestPx += (double)(r.x1 - r.x0) * (r.y1 - r.y0);
            sConta.msZQuadro += ms_desde(&t);
        }
    }
    if (poeZ) {
        sZDono = i;
    }
    esquece_estado();
    sResidente = i;
    sSujo.x0 = sSujo.x1 = 0;
    sConta.trocas++;
}

/* ---- profundidade: o autoteste do boot (F5, etapa 7) -------------------------------------------------------- */

/* Quantas palavras de a e b são iguais: inteiras, sem o byte baixo e sem o alto (qual dos dois é o stencil na memória
   não importa aqui: o modo 2 não restaura o stencil); nz: quantas de a não são zero (o padrão foi mesmo escrito). */
static void prof_compara(D3DTexture *a, D3DTexture *b, unsigned c[5]) {
    D3DLOCKED_RECT ra, rb;
    unsigned n, k;
    c[0] = c[1] = c[2] = c[3] = c[4] = 0;
    if (FAILED(a->LockRect(0, &ra, NULL, D3DLOCK_READONLY))) {
        return;
    }
    if (FAILED(b->LockRect(0, &rb, NULL, D3DLOCK_READONLY))) {
        a->UnlockRect(0);
        return;
    }
    n = (unsigned)ra.Pitch / 4 * TAM;
    c[4] = n;
    for (k = 0; k < n; k++) {
        unsigned x = ((const unsigned *)ra.pBits)[k], y = ((const unsigned *)rb.pBits)[k];
        c[0] += x == y;
        c[1] += (x & 0xFFFFFF00u) == (y & 0xFFFFFF00u);
        c[2] += (x & 0x00FFFFFFu) == (y & 0x00FFFFFFu);
        c[3] += x != 0;
    }
    b->UnlockRect(0);
    a->UnlockRect(0);
}

/* o tempo da GPU de `vezes` chamadas de f (com a GPU parada antes e depois), em ms por chamada */
static double prof_cronometra(void (*f)(void *), void *ctx, int vezes) {
    LARGE_INTEGER t;
    int k;
    sDev->BlockUntilIdle();
    QueryPerformanceCounter(&t);
    for (k = 0; k < vezes; k++) {
        f(ctx);
    }
    sDev->BlockUntilIdle();
    return ms_desde(&t) / vezes;
}
static const Ret kInteiro = {0, 0, TAM, TAM};
static void cr_zres(void *z) { prof_tira((Prof *)z, &kInteiro); }
static void cr_zcor(void *z) { prof_poe((Prof *)z, &kInteiro, 1); }
static void cr_zprof(void *z) { prof_poe((Prof *)z, &kInteiro, 2); }
static void cr_corres(void *t) {
    D3DRECT fonte = {0, 0, TAM, TAM};
    D3DPOINT destino = {0, 0};
    sDev->Resolve(D3DRESOLVE_RENDERTARGET0, &fonte, (D3DTexture *)t, &destino, 0, 0, NULL, 0.0f, 0, NULL);
}
static void cr_correst(void *t) { copia((D3DTexture *)t, TAM, TAM, 0.0f, 0.0f, (float)TAM, (float)TAM, 0.0f, 0.0f, 1.0f, 1.0f, 0); }

/* Antes do primeiro quadro: um padrão de bits qualquer (pseudoaleatório, sem período de 40 nem de 80 colunas) é
   escrito como cor na profundidade da EDRAM e resolvido (A); a EDRAM é limpa, A volta por um modo e é resolvida de
   novo (B); A e B têm de ser iguais. O controle (B sem restauro, tudo zero) tem de dar diferente: prova que a
   comparação acusa. Mede também a GPU do restauro e do resolve de um alvo inteiro, de cor e de profundidade. Escolhe
   o modo: BT3_Z=0 desliga (a profundidade volta zerada, como antes), 1 ou 2 força; sem ele, o 1 se for exato em todos
   os bits, senão o 2 se for exato na profundidade, senão desligado com ERRO. */
static void prof_autoteste(void) {
    D3DVIEWPORT9 vp = {0, 0, TAM, TAM, 0.0f, 1.0f}, vpz = {0, 0, TAMZ, TAM, 0.0f, 1.0f};
    const char *pedido = getenv("BT3_Z");
    int forcado = pedido != NULL && *pedido != '\0' ? atoi(pedido) : -1, modo, exato[3] = {0, 0, 0};
    unsigned c[3][5], ctl[5], total, x = 12345u, k;
    D3DTexture *pad = NULL;
    D3DLOCKED_RECT r;
    Prof za, zb;
    double ms[3] = {0, 0, 0};

    memset(&za, 0, sizeof(za));
    memset(&zb, 0, sizeof(zb));
    memset(c, 0, sizeof(c));
    sModoZ = 0;
    if (forcado == 0) {
        Diario_Escrever("gs_d3d: profundidade: desligada (BT3_Z=0): cada alvo volta para a EDRAM com a profundidade zerada");
        return;
    }
    if (TAM != 1024 || sEdramProfCor == NULL || sPsZCor == NULL || sPsZProf == NULL) {
        Diario_Escrever("gs_d3d: ERRO: profundidade: sem a superfície ou os shaders do restauro; desligada");
        return;
    }
    gGsD3dEtapa = "autoteste da profundidade";
    Diario_Escrever("gs_d3d: profundidade: autoteste (2 texturas D24S8 de %u KB e o padrão de 256 KB, soltas no fim)",
                    tam_alvo() >> 10);
    if (FAILED(sDev->CreateTexture(256, 256, 1, 0, D3DFMT_LIN_A8B8G8R8, D3DPOOL_DEFAULT, &pad, NULL)) || pad == NULL ||
        !prof_cria(MAX_TARGETS - 1, 0)) {
        Diario_Escrever("gs_d3d: ERRO: profundidade: as texturas do autoteste não foram criadas; desligada");
        if (pad != NULL) pad->Release();
        return;
    }
    za = sProf[MAX_TARGETS - 1];
    sProf[MAX_TARGETS - 1].tex = NULL;
    if (!prof_cria(MAX_TARGETS - 1, 0)) {
        Diario_Escrever("gs_d3d: ERRO: profundidade: as texturas do autoteste não foram criadas; desligada");
        pad->Release();
        za.tex->Release();
        sMem.vivos[CL_PROF] -= tam_alvo();
        sMem.nVivos[CL_PROF]--;
        return;
    }
    zb = sProf[MAX_TARGETS - 1];
    memset(&sProf[MAX_TARGETS - 1], 0, sizeof(Prof));
    if (SUCCEEDED(pad->LockRect(0, &r, NULL, 0))) {
        for (k = 0; k < 256u * (unsigned)r.Pitch / 4; k++) {
            x = x * 1664525u + 1013904223u;
            ((unsigned *)r.pBits)[k] = x ^ (x >> 16);
        }
        pad->UnlockRect(0);
    }
    for (modo = 0; modo <= 2; modo++) { /* (o 0 é o controle: sem restauro) */
        /* o padrão como cor na profundidade da EDRAM (uns 4 x 4 pixels por texel, os 13 tiles) e resolvido em A */
        sDev->SetDepthStencilSurface(NULL);
        sDev->SetRenderTarget(0, sEdramProfCor);
        sDev->SetViewport(&vpz);
        copia(pad, TAMZ, TAM, 0.0f, 0.0f, (float)TAMZ, (float)TAM, 0.0f, 0.0f, 1.0f, 1.0f, 0);
        sDev->SetTexture(0, NULL);
        sDev->SetRenderTarget(0, sEdramCor);
        sDev->SetDepthStencilSurface(sEdramProf);
        sDev->SetViewport(&vp);
        prof_tira(&za, &kInteiro);
        sDev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        sDev->Clear(0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 0.0f, 0);
        if (modo != 0) {
            prof_poe(&za, &kInteiro, modo);
        }
        prof_tira(&zb, &kInteiro);
        sDev->BlockUntilIdle();
        prof_compara(za.tex, zb.tex, modo == 0 ? ctl : c[modo]);
    }
    total = TAM * TAM;
    exato[1] = c[1][0] == total && c[1][4] == total;
    exato[2] = c[2][1] == total || c[2][2] == total;
    Diario_Escrever("gs_d3d: profundidade: autoteste com %u pixels, %u do padrão não nulos; controle sem restauro: %u iguais "
                    "(tem de ser perto de 0); modo 1 (como cor): %u iguais, %u sem o byte baixo, %u sem o alto; modo 2 "
                    "(oDepth): %u iguais, %u sem o byte baixo, %u sem o alto",
                    total, ctl[3], ctl[0], c[1][0], c[1][1], c[1][2], c[2][0], c[2][1], c[2][2]);
    /* o tempo da GPU, um alvo inteiro (1024 x 1024) por chamada, 8 vezes */
    sMsZRes = prof_cronometra(cr_zres, &za, 8);
    ms[1] = prof_cronometra(cr_zcor, &za, 8);
    ms[2] = prof_cronometra(cr_zprof, &za, 8);
    /* a cor, com a EDRAM e o sDate zerados (o sDate continua zerado, como foi criado) */
    sDev->SetTexture(0, NULL);
    sDev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    sDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);
    sDev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 0.0f, 0);
    sMsCorRes = prof_cronometra(cr_corres, sDate, 8);
    sMsCorRest = prof_cronometra(cr_correst, sDate, 8);
    sDev->SetTexture(0, NULL);
    sDev->BlockUntilIdle();
    if (forcado == 1 || forcado == 2) {
        modo = forcado;
    } else {
        modo = exato[1] && ctl[0] < total / 2 ? 1 : exato[2] && ctl[0] < total / 2 ? 2 : 0;
    }
    sModoZ = modo;
    sMsZRest = modo != 0 ? ms[modo] : 0.0;
    Diario_Escrever("gs_d3d: profundidade: GPU por alvo inteiro (1024 x 1024): resolve %.3f ms, restauro como cor %.3f ms, "
                    "pelo oDepth %.3f ms; a cor: resolve %.3f ms, restauro %.3f ms", sMsZRes, ms[1], ms[2], sMsCorRes, sMsCorRest);
    Diario_Escrever("gs_d3d: profundidade: %s modo %d (%s)%s; EDRAM sem tile a mais (a cor vista na base da profundidade); "
                    "RAM: %u KB por alvo que escreve z, criada só para esses", forcado > 0 ? "forçado o" : "escolhido o", modo,
                    modo == 1 ? "restauro como cor" : modo == 2 ? "restauro pelo oDepth" : "desligada",
                    modo == 0 && forcado != 0 ? ": ERRO: nenhum modo exato, a profundidade volta zerada como antes" : "",
                    tam_alvo() >> 10);
    za.cor.BlockUntilNotBusy();
    zb.cor.BlockUntilNotBusy();
    za.tex->Release();
    zb.tex->Release();
    pad->Release();
    sMem.vivos[CL_PROF] -= 2 * tam_alvo();
    sMem.nVivos[CL_PROF] -= 2;
    sDev->SetRenderTarget(0, sTela);
    sDev->SetDepthStencilSurface(NULL);
    esquece_estado();
    gGsD3dEtapa = "antes do primeiro quadro";
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

/* F5, etapa 6: os pares de buffers da VU1. Sem eles, os desenhos dela são contados como sem espaço. */
static void vu_init(void) {
    unsigned v = (unsigned)env_num("BT3_VU_VERTS", 65536);
    int i, ok = 1;
    v = v < 1024 ? 1024 : v > (1u << 20) ? (1u << 20) : v;
    for (i = 0; i < VU_PARES && ok; i++) {
        ok = SUCCEEDED(sDev->CreateVertexBuffer(v * 48, 0, 0, D3DPOOL_DEFAULT, &sVuVb[i], NULL)) &&
             SUCCEEDED(sDev->CreateIndexBuffer(v * 3 * 4, 0, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &sVuIb[i], NULL));
    }
    if (!ok) {
        for (i = 0; i < VU_PARES; i++) {
            if (sVuVb[i] != NULL) sVuVb[i]->Release();
            if (sVuIb[i] != NULL) sVuIb[i]->Release();
            sVuVb[i] = NULL;
            sVuIb[i] = NULL;
        }
        Diario_Escrever("gs_d3d: ERRO: os buffers da VU1 (%u vértices) não foram criados; a VU1 fica de fora", v);
        return;
    }
    sVuCapV = v;
    sVuCapI = v * 3;
    sMem.buffers += VU_PARES * (sVuCapV * 48u + sVuCapI * 4u);
    Diario_Escrever("gs_d3d: VU1: %d pares de buffers das famílias vu0, vu4 e vu6, %u vértices e %u índices cada (%u KB "
                    "ao todo; BT3_VU_VERTS)", VU_PARES, sVuCapV, sVuCapI,
                    VU_PARES * (sVuCapV * 48u + sVuCapI * 4u) >> 10);
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
    static const D3DVERTEXELEMENT9 elemVu[] = { /* os 48 bytes de um vértice dos programas (gsVuVerts) */
        {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 16, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
        {0, 32, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
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
        FAILED(sDev->CreatePixelShader((const DWORD *)kZCorPs, &sPsZCor)) ||
        FAILED(sDev->CreatePixelShader((const DWORD *)kZProfPs, &sPsZProf)) ||
        FAILED(sDev->CreateVertexDeclaration(elemCopia, &sDeclCopia)) ||
        FAILED(sDev->CreateVertexShader((const DWORD *)kVu0Vs, &sVsVu[0])) ||
        FAILED(sDev->CreateVertexShader((const DWORD *)kVu4Vs, &sVsVu[1])) ||
        FAILED(sDev->CreateVertexShader((const DWORD *)kVu6Vs, &sVsVu[2])) || FAILED(sDev->CreateVertexDeclaration(elemVu, &sDeclVu))) {
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
    /* (F5, etapa 7) a mesma EDRAM da profundidade vista como cor, para o restauro como cor (exemplo FastDepthRestore) */
    if (XGSurfaceSize(TAMZ, TAM, D3DFMT_A8B8G8R8, D3DMULTISAMPLE_NONE) != tilesProf ||
        FAILED(sDev->CreateRenderTarget(TAMZ, TAM, D3DFMT_A8B8G8R8, D3DMULTISAMPLE_NONE, 0, FALSE, &sEdramProfCor, &sp))) {
        Diario_Escrever("gs_d3d: ERRO na profundidade da EDRAM vista como cor (%ux%u, %u tiles contra %u)", TAMZ, TAM,
                        XGSurfaceSize(TAMZ, TAM, D3DFMT_A8B8G8R8, D3DMULTISAMPLE_NONE), tilesProf);
        sEdramProfCor = NULL;
    }
    if (FAILED(sDev->CreateTexture(1, 1, 1, 0, D3DFMT_LIN_A8B8G8R8, D3DPOOL_DEFAULT, &sBranco, NULL)) ||
        FAILED(sBranco->LockRect(0, &r, NULL, 0))) {
        Diario_Escrever("gs_d3d: ERRO na textura branca");
        return 0;
    }
    *(DWORD *)r.pBits = 0xFFFFFFFFu;
    sBranco->UnlockRect(0);
    mem_init();
    vu_init();
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
    if (tam_real(TAM, TAM, D3DFMT_D24S8) != tam_alvo()) {
        Diario_Escrever("gs_d3d: ERRO: a textura D24S8 tem %u KB e a contagem usa %u KB", tam_real(TAM, TAM, D3DFMT_D24S8) >> 10,
                        tam_alvo() >> 10);
    }
    prof_autoteste();
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

/* O estado comum a um desenho de primitivas do GS (estado 1: vs_gs, vértices pelo DrawPrimitiveUP) e a um da VU1
   (estado 1 + GsDraw.vu: 2 vs_vu0, 3 vs_vu4, 4 vs_vu6, o par de buffers do quadro): shaders, pipeline, textura,
   amostragem, constantes do pixel shader, cor da mistura e tesoura. 0: nada a desenhar (tesoura vazia). */
static int prepara(const GsDraw *d, int estado, Ret *area) {
    float c[16];
    int x0 = d->scissor[0], y0 = d->scissor[1], x1 = d->scissor[0] + d->scissor[2], y1 = d->scissor[1] + d->scissor[3];

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > TAM) x1 = TAM;
    if (y1 > TAM) y1 = TAM;
    if (x1 <= x0 || y1 <= y0 || d->count == 0) {
        return 0;
    }
    if (sEstadoGs != estado) {
        sDev->SetVertexDeclaration(estado >= 2 ? sDeclVu : sDeclGs);
        sDev->SetVertexShader(estado >= 2 ? sVsVu[estado - 2] : sVsGs);
        sDev->SetPixelShader(sPsGs);
        sDev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
        if (estado >= 2) {
            /* (o DrawPrimitiveUP desfaz o fluxo 0: posto de novo a cada volta para cá) */
            sDev->SetStreamSource(0, sVuVb[sVuPar], 0, 48);
            sDev->SetIndices(sVuIb[sVuPar]);
        }
        sEstadoGs = estado;
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
    area->x0 = x0; area->y0 = y0; area->x1 = x1; area->y1 = y1;
    return 1;
}

static void desenha(const GsDraw *d) {
    const Pipe *p = &sPipes[d->pipeline];
    D3DPRIMITIVETYPE tipo = p->topo == 0 ? D3DPT_TRIANGLELIST : p->topo == 1 ? D3DPT_LINELIST : D3DPT_POINTLIST;
    int porPrim = p->topo == 0 ? 3 : p->topo == 1 ? 2 : 1;
    uint32_t feitos = 0;
    Ret a;

    if (!prepara(d, 1, &a)) {
        return;
    }
    while (feitos + porPrim <= d->count) {
        uint32_t n = d->count - feitos > MAX_UP ? MAX_UP : d->count - feitos;
        n -= n % porPrim;
        sDev->DrawPrimitiveUP(tipo, n / porPrim, &gsVerts[d->first + feitos], sizeof(Vtx));
        feitos += n;
    }
    ret_une(&sSujo, a.x0, a.y0, a.x1, a.y1);
    if (p->zwrite && p->ztst != 0) {
        ret_une(&sSujoZ, a.x0, a.y0, a.x1, a.y1);
    }
    sConta.desenhos++;
    sConta.vertices += d->count;
    if (d->count > sConta.maiorDesenho) sConta.maiorDesenho = d->count;
}

/* F5, etapa 6: um desenho da VU1 (d->vu 1 a 3; d->first e d->count são índices de gsVuIdx), já copiado para o par
   de buffers do quadro a partir do índice ini (vu_copia). As três famílias usam o mesmo Vu0Uniform (gs_draw.c). */
static void desenha_vu(const GsDraw *d, uint32_t ini) {
    Ret a;

    if (!prepara(d, 1 + d->vu, &a)) {
        return;
    }
    if (d->uniform != sCurUni) {
        sDev->SetVertexShaderConstantF(VU_C0, (const float *)&gsVuUni[d->uniform], sizeof(Vu0Uniform) / 16);
        sCurUni = d->uniform;
    }
    sDev->DrawIndexedVertices(D3DPT_TRIANGLELIST, 0, ini, d->count);
    ret_une(&sSujo, a.x0, a.y0, a.x1, a.y1);
    if (sPipes[d->pipeline].zwrite && sPipes[d->pipeline].ztst != 0) {
        ret_une(&sSujoZ, a.x0, a.y0, a.x1, a.y1);
    }
    sConta.desenhos++;
    sConta.vertices += d->count;
    sConta.vuFeitos[d->vu - 1]++;
    sConta.vuTri[d->vu - 1] += d->count / 3;
}

/* F5, etapa 6: os vértices e os índices dos desenhos da VU1 do quadro (as três famílias) vão para um par de buffers da GPU, o
   que não foi usado no quadro anterior. Cada desenho leva só a faixa de vértices que os índices dele tocam, e os
   índices saem já com a posição nova (o desenho começa no índice sVuIni[n]); o que não cabe fica com ~0 e é contado.
   A espera pelo par (a GPU ainda lendo o quadro de dois atrás) é contada; não deve acontecer com o Present no meio. */
static void vu_copia(void) {
    uint32_t n, k, f, nv = 0, ni = 0, tem = 0, pedV[3] = {0, 0, 0}, pedI[3] = {0, 0, 0};
    float *vb = NULL;
    uint32_t *ib = NULL;

    for (n = 0; n < gsDrawCount; n++) {
        sVuIni[n] = ~0u;
        tem += gsDraws[n].vu >= 1 && gsDraws[n].vu <= 3 && gsDraws[n].native == 0;
    }
    if (tem == 0 || sVuCapV == 0) {
        return;
    }
    sVuPar = (sVuPar + 1) % VU_PARES;
    if (sVuVb[sVuPar]->IsBusy() || sVuIb[sVuPar]->IsBusy()) {
        const char *antes = (const char *)gGsD3dEtapa;
        gGsD3dEtapa = "esperando a GPU soltar o par de buffers da VU1";
        sVuVb[sVuPar]->BlockUntilNotBusy();
        sVuIb[sVuPar]->BlockUntilNotBusy();
        gGsD3dEtapa = antes;
        sConta.vuEsperas++;
    }
    if (FAILED(sVuVb[sVuPar]->Lock(0, 0, (void **)&vb, 0))) {
        return;
    }
    if (FAILED(sVuIb[sVuPar]->Lock(0, 0, (void **)&ib, 0))) {
        sVuVb[sVuPar]->Unlock();
        return;
    }
    for (n = 0; n < gsDrawCount; n++) {
        const GsDraw *d = &gsDraws[n];
        uint32_t lo = 0xFFFFFFFFu, hi = 0;
        if (d->vu < 1 || d->vu > 3 || d->native != 0 || d->count == 0) {
            continue;
        }
        f = (uint32_t)d->vu - 1;
        for (k = 0; k < d->count; k++) {
            uint32_t i = gsVuIdx[d->first + k];
            if (i < lo) lo = i;
            if (i > hi) hi = i;
        }
        pedV[f] += hi >= lo ? hi - lo + 1 : 0;
        pedI[f] += d->count;
        if (hi >= gsVuVertCount || hi - lo + 1 > sVuCapV - nv || d->count > sVuCapI - ni) {
            continue; /* sem espaço (ou índice fora do quadro): pulado e contado no frame_end */
        }
        memcpy(vb + nv * 12, gsVuVerts + lo * 12, (hi - lo + 1) * 48);
        for (k = 0; k < d->count; k++) {
            ib[ni + k] = gsVuIdx[d->first + k] - lo + nv;
        }
        sVuIni[n] = ni;
        nv += hi - lo + 1;
        ni += d->count;
    }
    sVuIb[sVuPar]->Unlock();
    sVuVb[sVuPar]->Unlock();
    for (f = 0; f < 3; f++) { /* o pedido, mesmo o que não coube */
        if (pedV[f] > sConta.vuMaxV[f]) sConta.vuMaxV[f] = pedV[f];
        if (pedI[f] > sConta.vuMaxI[f]) sConta.vuMaxI[f] = pedI[f];
    }
    if (pedV[0] + pedV[1] + pedV[2] > sConta.vuMaxT[0]) sConta.vuMaxT[0] = pedV[0] + pedV[1] + pedV[2];
    if (pedI[0] + pedI[1] + pedI[2] > sConta.vuMaxT[1]) sConta.vuMaxT[1] = pedI[0] + pedI[1] + pedI[2];
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
    unsigned nosso = sMem.vivos[CL_TEX] + sMem.vivos[CL_ALVO] + sMem.pend[CL_TEX] + sMem.pend[CL_ALVO] + sMem.buffers +
                     sMem.vivos[CL_PROF] + sMem.pend[CL_PROF];
    unsigned usado = sMem.total > sMem.livres ? sMem.total - sMem.livres : 0;
    unsigned brancas = 0;
    int i;
    for (i = 1; i < BR_N; i++) {
        brancas += sMem.branca[i];
    }
    Diario_Escrever("gs_d3d:   memória: texturas %u (%u KB, teto %u MB), alvos %u (%u KB), profundidades %u (%u KB), pendentes "
                    "%u + %u + %u (%u KB + %u KB + %u KB), buffers %u KB; resto do jogo e do sistema %d MB; livres %u MB, mínimo "
                    "visto %u MB (piso %u)",
                    sMem.nVivos[CL_TEX], sMem.vivos[CL_TEX] >> 10, sMem.texTetoMb, sMem.nVivos[CL_ALVO],
                    sMem.vivos[CL_ALVO] >> 10, sMem.nVivos[CL_PROF], sMem.vivos[CL_PROF] >> 10, sMem.nPend[CL_TEX],
                    sMem.nPend[CL_ALVO], sMem.nPend[CL_PROF], sMem.pend[CL_TEX] >> 10, sMem.pend[CL_ALVO] >> 10,
                    sMem.pend[CL_PROF] >> 10, sMem.buffers >> 10, ((int)usado - (int)nosso) / (int)MB, sMem.livres >> 20,
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

/* F5, etapa 6: a VU1 no trecho de 60 quadros, por família de shader: desenhos da lista feitos pela GPU e pulados
   (e deles os sem espaço), triângulos, o maior quadro de cada uma e das três juntas, os lotes de cada uma pelo
   caminho que tomaram em gs_vu1.c (shader, interpretador, não executados), os programas não reconhecidos com o tamanho, as instruções desconhecidas e os tempos por quadro. */
static void log_vu1(unsigned t) {
    char desc[160];
    int k, n = 0;

    desc[0] = '\0';
    for (k = 0; k < 8 && gGsVu1Desconhecido[k][1] != 0; k++) {
        n += _snprintf(desc + n, sizeof(desc) - n - 1, "%s%u instruções x%u", k ? ", " : "", gGsVu1Desconhecido[k][0],
                       gGsVu1Desconhecido[k][1]);
        if (n < 0 || n >= (int)sizeof(desc) - 1) {
            desc[sizeof(desc) - 1] = '\0';
            break;
        }
    }
    static const char *const nome[3] = {"vu0 (lutadores)", "vu4 (estágio)", "vu6 (sombra no chão)"};
    for (k = 0; k < 3; k++) {
        Diario_Escrever("gs_d3d:   VU1, desenhos: %s %u pela GPU (%u triângulos), %u pulados (%u sem espaço); no maior "
                        "quadro %u vértices e %u índices", nome[k], sConta.vuFeitos[k], sConta.vuTri[k], sConta.vuPulados[k],
                        sConta.vuSemEspaco[k], sConta.vuMaxV[k], sConta.vuMaxI[k]);
    }
    Diario_Escrever("gs_d3d:   VU1, buffers: as três no maior quadro %u de %u vértices e %u de %u índices, %u esperas pelo par",
                    sConta.vuMaxT[0], sVuCapV, sConta.vuMaxT[1], sVuCapI, sConta.vuEsperas);
    Diario_Escrever("gs_d3d:   VU1, lotes (shader/interpretador/não executados): vu0 %u/%u/%u, vu4 %u/%u/%u, vu6 %u/%u/%u; "
                    "não reconhecidos %u/%u (%s); instruções desconhecidas %u; tempo por quadro: gravação %.2f ms (pior "
                    "%.2f), desenho %.2f ms (pior %.2f)", gGsVu1Lotes[0][0], gGsVu1Lotes[0][1], gGsVu1Lotes[0][2],
                    gGsVu1Lotes[1][0], gGsVu1Lotes[1][1], gGsVu1Lotes[1][2], gGsVu1Lotes[2][0], gGsVu1Lotes[2][1],
                    gGsVu1Lotes[2][2], gGsVu1Lotes[3][1], gGsVu1Lotes[3][2], desc[0] ? desc : "nenhum", gGsVu1InstrDesc,
                    sConta.msVuGrav / t, sConta.msVuGravPior, sConta.msVuDes / t, sConta.msVuDesPior);
}

static void log_vu1_zera(void) {
    memset(gGsVu1Lotes, 0, sizeof(gGsVu1Lotes));
    memset(gGsVu1Desconhecido, 0, sizeof(gGsVu1Desconhecido));
    gGsVu1InstrDesc = 0;
}

/* F5, etapa 7: a profundidade no trecho, por quadro: resolves e restauros com os pixels, as visitas que usaram z,
   a CPU das chamadas e a GPU estimada pelo autoteste (ms por pixel de um alvo inteiro), com a da cor ao lado */
static void log_prof(unsigned t) {
    double px = (double)TAM * TAM, gz = (sConta.zResPx * sMsZRes + sConta.zRestPx * sMsZRest) / px / t,
           gc = (sConta.corResPx * sMsCorRes + sConta.corRestPx * sMsCorRest) / px / t;
    if (sModoZ == 0) {
        return;
    }
    Diario_Escrever("gs_d3d:   profundidade (modo %d), por quadro: %.1f resolves (%.0f mil pixels), %.1f restauros (%.0f mil "
                    "pixels), %.1f visitas com z de %.1f trocas (%.1f com a profundidade ainda na EDRAM), %u sem textura no trecho; CPU %.3f ms (pior %.3f); GPU "
                    "estimada %.2f ms (a cor: %.2f ms); %u alvos com profundidade (%u KB)",
                    sModoZ, (double)sConta.zRes / t, sConta.zResPx / t / 1000.0, (double)sConta.zRest / t,
                    sConta.zRestPx / t / 1000.0, (double)sConta.zVisitas / t, (double)sConta.trocas / t, (double)sConta.zMantida / t,
                    sConta.zSemTex,
                    sConta.msZ / t, sConta.msZPior, gz, gc, sMem.nVivos[CL_PROF], sMem.vivos[CL_PROF] >> 10);
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
        log_vu1_zera();
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
    log_vu1(t);
    log_vu1_zera();
    log_prof(t);
    log_memoria();
    memset(&sConta, 0, sizeof(sConta));
}

/* (profundidade) se algum desenho da visita que começa em n, no alvo t, testa ou escreve z: só então a profundidade
   do alvo volta para a EDRAM (as trocas de alvo das passadas de tela, que não usam z, não pagam o restauro) */
static int usa_z(uint32_t n, int t) {
    if (sModoZ == 0) {
        return 0;
    }
    for (; n < gsDrawCount; n++) {
        const GsDraw *d = &gsDraws[n];
        if (d->native != 0 || (d->vu && sVuIni[n] == ~0u)) {
            continue; /* (os pulados não mudam de alvo) */
        }
        if (d->target != t) {
            return 0;
        }
        if (sPipes[d->pipeline].zwrite || sPipes[d->pipeline].ztst >= 2) {
            return 1;
        }
    }
    return 0;
}

static void frame_end(void) {
    LARGE_INTEGER t0, tp, tv;
    double msVu;
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
    gGsD3dEtapa = "fim do quadro: texturas de profundidade";
    prof_garante();
    gGsD3dEtapa = "fim do quadro: refazendo a lista";
    sConta.msZQuadro = 0.0;
    {
        /* F5, etapa 6: o tempo da VU1 na gravação do quadro (gs_vu1.c) e a cópia das três famílias para a GPU */
        double g = gGsVu1Ms;
        gGsVu1Ms = 0.0;
        sConta.msVuGrav += g;
        if (g > sConta.msVuGravPior) sConta.msVuGravPior = g;
        QueryPerformanceCounter(&tv);
        gGsD3dEtapa = "fim do quadro: cópia da VU1";
        vu_copia();
        msVu = ms_desde(&tv);
        gGsD3dEtapa = "fim do quadro: refazendo a lista";
    }
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
            /* F5, etapa 6: as três famílias (1 vu0, 2 vu4, 3 vu6); o que vu_copia não pôs no par é contado */
            if (sVuIni[n] == ~0u) {
                int f = d->vu >= 1 && d->vu <= 3 ? d->vu - 1 : 0;
                sConta.semVu++;
                sConta.vuPulados[f]++;
                sConta.vuSemEspaco[f] += d->vu >= 1 && d->vu <= 3;
                continue;
            }
        }
        if (cur != d->target || sResidente != d->target) {
            entra_na_edram(d->target, usa_z(n, d->target));
            gsTargets[d->target].cleared = 1;
            cur = d->target;
        }
        if (d->vu) {
            QueryPerformanceCounter(&tv);
            desenha_vu(d, sVuIni[n]);
            msVu += ms_desde(&tv);
        } else {
            desenha(d);
        }
    }
    sConta.msVuDes += msVu;
    if (msVu > sConta.msVuDesPior) sConta.msVuDesPior = msVu;
    gGsD3dEtapa = "fim do quadro: o último alvo volta para a textura";
    sai_da_edram();
    prof_entrega(); /* (nada fica na EDRAM entre quadros: a Guide e o present podem usá-la) */
    sConta.msZ += sConta.msZQuadro;
    if (sConta.msZQuadro > sConta.msZPior) sConta.msZPior = sConta.msZQuadro;
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
    /* (F5, etapa 6) o par de buffers da VU1 também: preso no dispositivo, ficaria ocupado para sempre (IsBusy) */
    sDev->SetStreamSource(0, NULL, 0, 0);
    sDev->SetIndices(NULL);
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
