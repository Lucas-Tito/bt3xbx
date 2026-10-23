/*
 * F3, etapa 7: o backend D3D9 do Xbox 360 (GsBackend, port/src/gs/gs_draw.h), no lugar do gs_gl.c e do gs_gpu.c do
 * PC. A gravação do quadro é a do port (gs_draw.c: a lista de desenhos, os vértices, as texturas decodificadas da
 * memória do GS); este arquivo é dono dos objetos do D3D, refaz a lista no fim do quadro e apresenta.
 *
 * O que muda em relação ao PC, por causa do Xenos:
 *   - Os alvos de desenho moram na EDRAM (10 MB), não em texturas. Há UM par de superfícies na EDRAM, 1024 x 1024
 *     (o tamanho de um alvo do GS com SCALE = 1; com BT3_ALVO_AREA, a área dada, e BT3_ESCALA, vezes 1,5 ou 2, ver sAlvoW,
 *     sAreaW e escala.h): cor em
 *     A8R8G8B8 na base 0 e profundidade D24S8 logo depois, cerca de 8 MB. Cada alvo do GS tem uma textura (tiled, A8R8G8B8) com o conteúdo dele. Desenhar num alvo: o que está
 *     na EDRAM volta para a textura do dono (Resolve, só o retângulo sujo), a EDRAM é limpa e recebe a textura do
 *     novo alvo (um retângulo desenhado). A profundidade (F5, etapa 7) faz o mesmo caminho: o que os desenhos com
 *     escrita de z tocaram volta para uma textura D24S8 do alvo, e a volta para a EDRAM a restaura (como cor na mesma
 *     base, ou pelo oDepth; o autoteste do boot escolhe). Ver prof_* abaixo e docs/x360/plano/F5/07.md.
 *   - Ler a cor de um alvo como textura (o jogo faz isso nos efeitos) é ler a textura do alvo, sempre em dia: um
 *     alvo nunca é lido enquanto é o desenhado (gs_draw.c recusa), então ele já saiu da EDRAM.
 *   - Sem o segundo alvo com o byte de alfa (outAux do gs.frag): o teste de alfa do destino (DATE) lê o alfa da
 *     própria cor, que guarda min(A / 0x80, 1) e chega a 1 exatamente com o bit 7 ligado. Os efeitos nativos (F5,
 *     etapa 7, segunda parte; nat_* abaixo e docs/x360/formatos/gs-nativos.md) leem os ids do mesmo alfa, exatos de 0
 *     a 0x80: a cópia dos ids é um Resolve da cor, a passada de tabela lê os ids ou a profundidade do alvo, o contorno
 *     lê os ids. As imagens copiadas direto para a memória de vídeo (filmes) são contadas, não desenhadas; com BT3_FILME=1
 *     (F6, etapa 6; filme_* abaixo e port/x360/src/f2/filme360.c) o filme vem pronto numa textura e é desenhado no lugar.
 *   - Os programas de vértice (VU1, F5, etapa 6): as três famílias, vu0 (os lutadores), vu4 (o estágio) e vu6 (a
 *     sombra no chão) (vs_vu0, vs_vu4 e vs_vu6 em gs_d3d.hlsl), são desenhadas com os vértices e os índices do quadro
 *     copiados para um par de buffers da GPU (dois pares, alternados por quadro). BT3_VU_VERTS=65536 vértices por par
 *     (os índices são o triplo), para as três juntas; o que não couber é contado no log, nunca some.
 *   - Sem janela, painel de configurações nem pacote de texturas.
 *
 * O dispositivo é criado pela main (port/x360/src/f2/main360.cpp) e passa a ser desta thread (a do jogo) no init;
 * GsD3d_Soltar o devolve quando o jogo termina. O desenho e o present são na thread do jogo, um por quadro do GS. Com
 * BT3_GS_THREAD=1 (F5, etapa 8, degrau 1) a gravação do quadro e este desenho são de uma thread só, a de gravação do
 * gs_core.c (que roda o init, é dona do dispositivo e o solta ao parar); o jogo só entrega a lista e espera nos pontos do
 * PS2. GsD3d_Repete mostra de novo o último quadro quando o jogo passa de 50 ms sem entregar um (a Guide).
 * Com BT3_GS_DESENHO=1 também (degrau 2, etapa 4; docs/x360/plano/F5/08-degrau2.md) o desenho é de uma terceira thread, a
 * dona do dispositivo (init, frame_end, Present, Repete, Soltar), e lê o quadro pelo retrato que a gravação entrega
 * (gsQuadro). Do lado da gravação ficam as entradas que criam (texturas, alvos e profundidades pelo XG), o cache e o
 * descarte; a cova, o conteúdo dos alvos e a EDRAM são do desenho. A entrega (entrega, mais abaixo) roda na gravação com o
 * desenho parado: é ali que um lado passa ao outro o que mudou (as texturas das vagas, os contadores do trecho).
 *
 * Saídas para conferência (port/x360/tools/compara_quadros.py, no WSL):
 *   BT3_SHOT=N        a imagem mostrada a cada N quadros do GS em game:\shots\gpu_NNNNN.ppm, no formato do
 *                     rasterizador de software (640 x 448, as colunas além da largura do quadro em preto), com o
 *                     número do quadro do GS dele; BT3_SHOT_FROM / BT3_SHOT_TO limitam a faixa;
 *   BT3_SHOT_LISTA=G,G...  só esses quadros do GS (até 32), com ou sem o BT3_SHOT: os das imagens da etapa 6 da F4
 *                     (a linha "imagem: ... gs G" da build sem janela), para medir os menus contra elas;
 *   BT3_SHOT_CHEIA=1  (720p, etapa 3) com BT3_ESCALA maior que 1, as capturas na resolução interna; sem ela, reduzidas a
 *                     640 x 448 (a média dos pixels de saída de cada pixel do GS), comparáveis às da escala 1;
 *   o log (bt3xbx.log) a cada 60 quadros: tempo por quadro, desenhos, texturas novas, o que não foi desenhado, e a
 *                     memória por classe (F5, etapa 1);
 *   BT3_TEMPOS=1      (o padrão; 0 desliga) o tempo de GPU e de CPU de cada passada do fim do quadro no mesmo log
 *                     (F5, etapa 8; tempo_* abaixo), e o tempo da thread de gravação por família, uma linha "gs: gravação
 *                     por família" a cada 60 quadros, também dentro da janela do passos (gs_draw.c, Gs_FamTrecho).
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
void GsDraw_DesenhoResumo(void);                         /* gs_draw.c: a entrega do degrau 2 no log */
extern volatile unsigned gX360Luta; /* estado360.c: o vblank em que a luta começou (0: fora da luta) */
/* Para o vigia da main (main360.cpp): onde o fim do quadro está e quantos quadros já foram apresentados. */
volatile const char *gGsD3dEtapa = "antes do primeiro quadro";
/* (degrau 2, etapa 4) onde a parte do backend que roda na gravação está (alvos, profundidades, memória, a entrega); sem a
   divisão as duas etapas são da mesma thread */
volatile const char *gGsD3dEtapaGrav = "antes do primeiro quadro";
volatile unsigned gGsD3dQuadros;
int Port_AspectMilli(void);  /* plat_stub.c */
}
#include "gs_vs.h" /* gerados pelo fxc no build (gs_d3d.hlsl): kGsVs, kGsPs, kCopiaVs, kCopiaPs */
#include "gs_ps.h"
#include "copia_vs.h"
#include "copia_ps.h"
#include "zcor_ps.h"
#include "zprof_ps.h"
#include "gsz_ps.h"     /* (F5, etapa 7, segunda parte) ps_gsz, ps_tabela, ps_contorno */
#include "tabela_ps.h"
#include "contorno_ps.h"
#include "filme_ps.h"   /* (F6, etapa 6) ps_filme */
#include "vu0_vs.h"
#include "vu4_vs.h"
#include "vu6_vs.h"

#include "escala.h"
#include "vblank_alvo.h" /* (F5, etapa 8, seção 20: a conta da espera pelo vblank de hardware) */
/* (F5, etapa 8, 720p, etapa 2) O tamanho dos alvos, das profundidades e do par da EDRAM, em pixels de saída: a área de
   BT3_ALVO_AREA vezes a escala (escala.h, area_init; docs/x360/plano/F5/08-720p.md). Sem a chave, 1024 x 1024 (o alvo
   lógico do GS com SCALE = 1), o de antes. Lidos no começo do init, antes de qualquer alvo.
   sAlvoWZ (F5, etapa 7): a largura da profundidade vista como cor, arredondada aos tiles de 80 pixels que o alvo ocupa na
   EDRAM. O restauro como cor troca as metades de 40 colunas de cada tile, então com 1024 a profundidade das colunas 984 a
   999 mora nas colunas 1024 a 1039 da cor, que uma superfície de 1024 não alcança (com 640, múltiplo de 80, nada sobra). */
static int sAlvoW = GS_W, sAlvoH = GS_H, sAlvoWZ = (GS_W + 79) / 80 * 80;
/* (720p, etapa 3) A área em pixels do GS e a escala em meios (2 é 1, 3 é 1,5, 4 é 2; BT3_ESCALA): sAlvoW é esc_px(sAreaW).
   O gs_draw.c continua gravando em pixels do GS (o gsScale do 360 fica 1) e o backend leva à escala: a posição no recorte
   (c14, recorte_poe), as tesouras e os retângulos das nativas (esc_px, prepara e nat_ret), o uv de um alvo lido como
   textura (c6), a arte 2D (misc.w), a cópia dos ids e o passo do contorno, a cópia para a tela e as capturas. Os
   retângulos guardados (sSujo, sSujoZ, conteudo) e os Resolves já são em pixels de saída. */
static int sAreaW = GS_W, sAreaH = GS_H, sMeios = 2;
static unsigned sTamAlvo; /* bytes de uma textura de alvo (tam_alvo) */
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
static D3DPixelShader *sPsGs, *sPsCopia, *sPsFilme;
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

/* F5, etapa 7, segunda parte: as passadas nativas (nat_*, docs/x360/formatos/gs-nativos.md), o FBA e o recorte da
   VU1 pelo w. Ajustes no ambiente.txt, cada um desligável para comparar:
     BT3_NATIVOS=7  bits: 1 o contorno (native 1), 2 as passadas de tabela pelos ids (a chave, o brilho de objeto),
                    4 as de tabela pela profundidade (a tinta, o brilho do céu, a névoa); a cópia dos ids (native 4)
                    acontece se 1 ou 2 estiver ligado. Desligado: contado e pulado, como antes.
     BT3_FBA=1      0: o FBA de antes (o alfa da mistura vira 1); 1: duas passadas nos desenhos com FBA que misturam
                    pelo alfa da fonte (a cor com o alfa calculado, depois só o alfa com o bit 7).
     BT3_CORTE=1    0: o z da VU1 preso em 0..w por vértice (o de antes); 1: o recorte pelo w com a profundidade por
                    pixel (vu_pos e ps_gsz em gs_d3d.hlsl).
   O BT3_FX_OFF do gs_draw.c continua valendo (1 o contorno, 2 a chave, 4 as tabelas que colorem pela névoa, 8 o brilho
   aditivo, 16 o borrão pelo alfa do destino): o que ele pula nem chega à lista. */
#define IDS_W 640 /* a cópia dos ids: a imagem do jogo (FBW 10, 448 linhas); fora dali a leitura fica presa na borda */
#define IDS_H 448
static D3DPixelShader *sPsGsZ, *sPsTabela, *sPsContorno;
static D3DTexture *sIds;       /* a cópia do alfa do alvo (os ids), tiled A8R8G8B8, sIdsW x sIdsH */
static int sIdsW = IDS_W, sIdsH = IDS_H; /* (720p, etapa 3) IDS_W x IDS_H na escala, em pixels de saída (area_init) */
static unsigned sIdsQuadro;    /* o quadro da última cópia (+1; 0: nenhuma) */
static unsigned sNativos = 7, sFba = 1, sCorte = 1;

typedef struct Ret { int x0, y0, x1, y1; } Ret; /* vazio quando x1 <= x0 */
typedef struct Alvo {
    D3DTexture *tex;
    Ret conteudo; /* onde ele já foi desenhado (fora dali, zero); só o do desenho vale */
    unsigned ver; /* (degrau 2, etapa 4) a gravação o muda a cada troca da textura da vaga */
} Alvo;
/* (degrau 2, etapa 4) Dois lados: sAlvos é da gravação (a textura de cada vaga: target_ensure, target_color, a fila de
   liberação, alvos_sem_textura); sAlvoD é do desenho (a textura e o conteúdo), posto em dia no começo do quadro certo por
   sincroniza_alvos (o risco 6 do inventário): a vaga cuja textura mudou na gravação do quadro esquece o conteúdo, o dela e o
   da profundidade. O mesmo para sProf e sProfD. Sem a divisão, os dois lados são da mesma thread e a sincronização é no
   começo do frame_end (antes, o target_ensure zerava o conteúdo na hora; ninguém o lê entre um e outro). */
static Alvo sAlvos[MAX_TARGETS];
static Alvo sAlvoD[MAX_TARGETS];
static int sResidente = -1; /* o alvo que está na EDRAM */
static Ret sSujo;           /* o que foi desenhado nele desde que entrou */

/* F5, etapa 7: a profundidade de cada alvo fora da EDRAM. A textura só existe para os alvos em que algum desenho
   escreve z (criada no começo do fim do quadro, antes de refazer a lista); `conteudo` tem x em múltiplos de 80 (a
   largura de um tile da EDRAM, por causa da troca de metades do restauro como cor). */
typedef struct Prof {
    D3DTexture *tex; /* D24S8 tiled, o destino do Resolve da profundidade (NULL: sem) */
    D3DTexture *cor; /* cabeçalho: a mesma memória lida como A8B8G8R8 (o restauro como cor); mora com a textura (ProfXg) */
    Ret conteudo;    /* onde ela vale (fora dali, zero, o valor da limpeza); só o do desenho vale */
    unsigned falhou; /* o quadro em que a criação falhou (tenta de novo 60 quadros depois); só o da gravação */
    unsigned ver;    /* (degrau 2, etapa 4) a gravação o muda ao criar e ao soltar */
} Prof;
static Prof sProf[MAX_TARGETS];  /* a gravação (prof_garante, prof_cria, prof_solta) */
static Prof sProfD[MAX_TARGETS]; /* o desenho */
static int sModoZ;       /* 0 a profundidade volta zerada (o de antes), 1 restauro como cor, 2 pelo oDepth */
static int sZDono = -1;  /* o alvo cuja profundidade está na EDRAM (-1: nenhum; ver prof_entrega) */
static Ret sSujoZ;       /* o que os desenhos com escrita de z do sZDono tocaram desde que ela entrou na EDRAM */
static double sMsZRes, sMsZRest, sMsCorRes, sMsCorRest; /* autoteste: GPU por alvo inteiro (1024 x 1024), ms */

typedef struct Pipe { uint32_t key, bkey, wmask; int ztst, zwrite, topo, vu, fx; } Pipe;
static Pipe sPipes[1024];
static int sPipeCount;

/* recursos soltos que a GPU ainda pode estar lendo: xg_libera só quando IsBusy diz que não (senão BlockUntilNotBusy) */
static D3DTexture *sCova[MAX_COVA]; /* (só recursos do XG, TexXg: ver xg_cria) */
static unsigned sCovaBytes[MAX_COVA];
static unsigned char sCovaClasse[MAX_COVA];
static int sCovaN;

/* (degrau 2, etapa 4) a divisão (BT3_GS_DESENHO=1; gs_draw.c decide antes do init, que roda na thread de desenho) */
static int sDiv;                     /* 1: o frame_end numa thread (a dona do dispositivo) e a gravação noutra */
static DWORD sDono;                  /* a thread dona do dispositivo (a do init; 0 depois de soltar) */
static HANDLE sCovaEv;               /* o desenho atendeu o pedido de memória da gravação (GsD3d_Ocioso) */
static volatile unsigned sPedeLivre; /* bytes livres que a gravação espera (0: nada pedido) */
static unsigned sPendGrav;           /* bytes que a gravação pediu para soltar desde a última entrega (ainda não entregues) */
/* as profundidades das vagas que a gravação reaproveitou (a cor vai pela fila do gs_draw.c): uma lista enche na gravação,
   a outra viaja com o retrato e o desenho as põe na cova depois de desenhá-lo */
#define PROF_FILA (2 * MAX_TARGETS)
static D3DTexture *sProfFila[2][PROF_FILA];
static int sProfFilaN[2], sProfFilaG, sProfFilaD = -1;
static D3DTexture *sMostradoTex;     /* a textura do alvo da última imagem apresentada (GsD3d_Repete); NULL: nenhuma */
static DWORD sUltimoPresent;         /* GetTickCount do último Present */
static uint32_t sVuLo[MAX_DRAWS], sVuNv[MAX_DRAWS]; /* (vu_plano) o primeiro vértice e quantos, por desenho da VU1 que coube */
static const GsQuadro *sQ;           /* o quadro que o frame_end desenha */

/* (F5, etapa 8, picos) os contadores do registro dos quadros lentos (gs_internal.h; o registro em si mais abaixo, depois do
   grav_junta). Os de fora deste arquivo (o jogo, a gravação no gs_core.c e no gs_draw.c, o iso360.c, o diario.c) andam
   sempre; os relógios, só com o limiar. Os daqui: as criações pelo XG por classe (0 textura, 1 alvo, 2 profundidade), o
   KB e o tempo delas (o XPhysicalAlloc), as solturas (o XPhysicalFree), o tempo esperando a GPU soltar um recurso da
   cova (BlockUntilNotBusy), o da gravação esperando o desenho soltar memória (espera_desenho), as medidas da memória
   (GlobalMemoryStatus) e as apresentações repetidas (GsD3d_Repete). Os da gravação se zeram na entrega dela; os que
   mais de uma thread toca, lidos por diferença. */
extern "C" {
unsigned gPicos;
unsigned gPicosTexN, gPicosTexPx, gPicosEnvios, gPicosHash, gPicosJEsperas, gPicosQJogo;
uint64_t gPicosTexNs, gPicosEnvioBytes, gPicosJEspera, gPicosGOcioso;
}
static volatile unsigned sPicoXgN[3], sPicoXgKb, sPicoSoltas, sPicoMemMede, sPicoRepete;
static volatile unsigned __int64 sPicoXgTiques, sPicoCovaTiques, sPicoEspMemTiques;

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
    unsigned esperasDes;               /* no trecho: as esperas da gravação pelo desenho (com a divisão) */
    unsigned acimaTeto;                /* no trecho: texturas criadas acima do teto (nada descartável) */
    unsigned pressoesTot, descNTot, esperasTot, brancaTot[BR_N], falasBranca[BR_N], falasPressao;
    double descBytesTot;
    unsigned maxVerts, maxVu, maxIdx, maxUni, maxDraws;                  /* buffers: o máximo de um quadro no trecho */
    unsigned emUso;                    /* entradas protegidas na última pressão (usadas nos dois últimos quadros) */
    int seguro;                        /* 0: BT3_INTERP ligado, sem descarte */
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
    if (sTamAlvo == 0) {
        sTamAlvo = tam_real(sAlvoW, sAlvoH, D3DFMT_A8R8G8B8);
    }
    return sTamAlvo;
}

/* (F5, etapa 8, degrau 2, etapa 2) As texturas do jogo, os alvos e as profundidades são feitos sem o dispositivo, para a
   gravação poder criá-los fora da thread que desenha: o cabeçalho pelo XG em memória comum e os dados em XPhysicalAlloc
   (alinhamento de página, 4 KB, o que o XG pede para textura; escrita combinada, a que o XDK exige para recurso feito à
   mão e a mesma do D3D sem D3DUSAGE_CPU_CACHED_MEMORY). O D3D não conta esse recurso: nunca Release. xg_libera solta os
   dados e o cabeçalho e só se chama com a GPU sem ele (IsBusy falso ou depois do BlockUntilNotBusy, que valem para o
   cabeçalho do XG). O cabeçalho é o primeiro campo: o ponteiro serve de D3DTexture * e de GsTex. Os bytes são os do
   tam_real (o XPhysicalAlloc arredonda para a página), então a conta da memória e o dwAvailPhys andam juntos. */
typedef struct TexXg {
    D3DTexture cab;
    void *dados;    /* o endereço virtual dos dados (XPhysicalAlloc) */
    unsigned bytes; /* os bytes reservados, os mesmos do tam_real */
    unsigned pitch; /* bytes por linha do nível 0, o do XG */
    int prof;       /* 1: é um ProfXg (o cabeçalho que lê a profundidade como cor vem junto, com a cerca dele) */
} TexXg;
typedef struct ProfXg {
    TexXg x;
    D3DTexture cor;
} ProfXg;

static TexXg *xg_cria_n(UINT w, UINT h, D3DFORMAT f, size_t tam) {
    TexXg *x = (TexXg *)calloc(1, tam);
    XGTEXTURE_DESC d;
    UINT t;
    if (x == NULL) {
        return NULL;
    }
    t = XGSetTextureHeader(w, h, 1, 0, f, (D3DPOOL)0, 0, 0, 0, &x->cab, NULL, NULL);
    x->bytes = (t + 4095u) & ~4095u;
    {
        unsigned __int64 t0 = __mftb();
        x->dados = XPhysicalAlloc(x->bytes, MAXULONG_PTR, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
        sPicoXgTiques += __mftb() - t0;
    }
    if (x->dados == NULL) {
        free(x);
        return NULL;
    }
    sPicoXgN[f == D3DFMT_D24S8 ? 2 : f == D3DFMT_LIN_A8B8G8R8 ? 0 : 1]++;
    sPicoXgKb += x->bytes >> 10;
    XGOffsetResourceAddress(&x->cab, x->dados);
    XGGetTextureDesc(&x->cab, 0, &d);
    x->pitch = d.RowPitch;
    return x;
}
static TexXg *xg_cria(UINT w, UINT h, D3DFORMAT f) { return xg_cria_n(w, h, f, sizeof(TexXg)); }

/* a GPU ainda usa o recurso (os dois cabeçalhos de uma profundidade) */
static int xg_ocupado(D3DTexture *t) {
    TexXg *x = (TexXg *)t;
    return x->cab.IsBusy() || (x->prof && ((ProfXg *)x)->cor.IsBusy());
}
static void xg_espera(D3DTexture *t) {
    TexXg *x = (TexXg *)t;
    x->cab.BlockUntilNotBusy();
    if (x->prof) {
        ((ProfXg *)x)->cor.BlockUntilNotBusy();
    }
}

static void xg_libera(D3DTexture *t) {
    TexXg *x = (TexXg *)t;
    if (x != NULL) {
        sPicoSoltas++;
        XPhysicalFree(x->dados);
        free(x);
    }
}

/* a memória física livre agora, em bytes (e o menor valor visto) */
static unsigned mem_mede(void) {
    MEMORYSTATUS m;
    sPicoMemMede++;
    GlobalMemoryStatus(&m);
    sMem.livres = (unsigned)m.dwAvailPhys;
    sMem.total = (unsigned)m.dwTotalPhys;
    sMem.criadoDesde = 0;
    if (sMem.livresMin == 0 || sMem.livres < sMem.livresMin) {
        sMem.livresMin = sMem.livres;
    }
    return sMem.livres;
}

/* memória física livre agora, sem mexer na medida da gravação (o desenho e os logs) */
static unsigned livres_agora(void) {
    MEMORYSTATUS m;
    sPicoMemMede++;
    GlobalMemoryStatus(&m);
    return (unsigned)m.dwAvailPhys;
}

/* memória física livre, em MB */
static unsigned mb_livres(void) { return livres_agora() >> 20; }

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
    /* F5, etapa 7, segunda parte: as nativas feitas (contorno, tabela, cópia dos ids), as de tabela puladas por motivo
       (0 desligada no BT3_NATIVOS, 1 sem tabela, 2 sem profundidade, 3 alvo não limpo, 4 sem a cópia dos ids), as
       sincronizações da profundidade; o DATE (marcadores, resolves, lidas da textura do alvo, desenhos), o FBA
       (desenhos, os de duas passadas), o alfa alto (desenhos que escrevem alfa com alfa de vértice acima de 0x80, no
       quadro de amostra do trecho) e os pixels das passadas de tela */
    unsigned natCont, natTab, natIds, natTabSem[5], natContSem, natIdsSem, natZSinc, natIdsVelhos;
    unsigned dateMarc, dateRes, dateTex, dateDes, fbaDes, fbaDuas, fbaSemA, alfaAlto, alfaAltoDes;
    double natPx;
    /* (720p, etapa 2) desenhos cortados por a tesoura passar da área dos alvos (primitivas e nativas), e alvos lidos
       como textura além dela (desenhos cujo uv passa da área) */
    unsigned cortados, cortadosNat, lidosAlem;
} sConta;
/* as passadas de tabela por tipo (fonte, mistura e máscara da chave do pipeline), no trecho */
static struct { uint32_t chave; int src; unsigned n; } sTabTipo[8];
static int sTabTipoN;
static LARGE_INTEGER sFreq, sUltimoFim;
static unsigned sAvisado; /* bits: o que já foi dito uma vez no log */
/* (degrau 2, etapa 4) os contadores do trecho que a gravação escreve (o descarte, as brancas, as texturas novas e as
   recusadas); somados aos do log por grav_junta, na entrega (o desenho parado) ou, sem a divisão, no começo do frame_end.
   Os contadores da VU1 do gs_vu1.c (gravação) vão para as cópias do log no mesmo ponto. */
static struct {
    unsigned pressoes, descN, descBytes, esperas, acimaTeto, branca[BR_N], texNovas, semTextura;
} sG;
static unsigned sVuLotes[4][3], sVuDesc[8][2], sVuInstrDesc;

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

/* ---- o vblank de hardware (F5, etapa 8; docs/x360/plano/F5/08-gravacao.md, seção 18) ------------------------------ */
/* A tela troca de imagem no vblank da saída de vídeo (D3DPRESENT_INTERVAL_ONE, main360.cpp); o relógio do port não sabia
   quando ele vem. A callback de vblank do D3D (SetVerticalBlankCallback, instalada no init pela dona do dispositivo) roda
   numa DPC a cada vblank: guarda o mftb, conta quantos vblanks cada imagem ficou na tela (pelo Swap mostrado, que muda
   quando uma imagem nova entra) e sinaliza um evento. Na DPC só isso: sem float, sem espera, sem o dispositivo (o XDK: só
   Interlocked*, SetEvent e afins, com um handle válido; o evento nasce uma vez e nunca se fecha, então segue válido na
   saída e na queda). Os contadores só crescem; quem lê (o passos, por diferença) está noutra thread e lê palavras
   inteiras. Sai no GsD3d_Soltar (o jogo devolve o dispositivo) e a main a tira de novo antes de soltar o dispositivo. */
static HANDLE sVbEv;                        /* auto-reset: um vblank */
static volatile DWORD sVbN;                 /* vblanks vistos desde a instalação */
static volatile unsigned __int64 sVbTb;     /* o mftb do último */
static volatile DWORD sVbTela[4];           /* imagens que ficaram 1, 2, 3 e 4 ou mais vblanks na tela */
static volatile DWORD sVbPerdidas;          /* vblanks com D3DVBLANK_SWAPMISSED (uma troca pedida que não entrou) */
static volatile DWORD sVbPuladas;           /* imagens que nunca ficaram na tela (duas trocas entre dois vblanks) */
static DWORD sVbSwap, sVbSwapN;             /* (só a DPC) o Swap mostrado e o vblank em que entrou */
static int sVbTemSwap;
static volatile int sVbLigada;              /* a callback instalada (entre o init e o Soltar) */
static unsigned __int64 sVbTb0;             /* o mftb e o QueryPerformanceCounter da instalação (a conversão do mftb) */
static LARGE_INTEGER sVbQ0;
static int sVbHw;                           /* BT3_VBLANK_HW: 1 (o padrão) a espera é pelo vblank de hardware; 0, o Sleep */

static void WINAPI vblank_cb(D3DVBLANKDATA *d) {
    DWORD n = sVbN + 1, k;
    sVbTb = __mftb();
    if (d != NULL) {
        if (!sVbTemSwap) {
            sVbTemSwap = 1;
            sVbSwap = d->Swap;
            sVbSwapN = n;
        } else if (d->Swap != sVbSwap) {
            k = n - sVbSwapN; /* os vblanks em que a imagem anterior ficou na tela */
            sVbTela[k >= 4 ? 3 : k == 0 ? 0 : k - 1]++;
            sVbPuladas += d->Swap - sVbSwap - 1;
            sVbSwap = d->Swap;
            sVbSwapN = n;
        }
        if (d->Flags & D3DVBLANK_SWAPMISSED) {
            sVbPerdidas++;
        }
    }
    __lwsync();
    sVbN = n;
    SetEvent(sVbEv);
}

static void vblank_init(void) {
    if (sVbEv == NULL) {
        sVbEv = CreateEvent(NULL, FALSE, FALSE, NULL);
    }
    if (sVbEv == NULL) {
        Diario_Escrever("gs_d3d: vblank: ERRO no evento; sem a medida da tela");
        return;
    }
    sVbTb0 = __mftb();
    QueryPerformanceCounter(&sVbQ0);
    sDev->SetVerticalBlankCallback(vblank_cb); /* (por último: o primeiro vblank pode vir já) */
    sVbLigada = 1;
    Diario_Escrever("gs_d3d: vblank: a callback do vblank de hardware instalada (a medida da tela no passos); a espera de "
                    "vblank %s (BT3_VBLANK_HW=%d)", sVbHw ? "pelo vblank de hardware, o Sleep de reserva" : "pelo Sleep", sVbHw);
}

/* (a dona do dispositivo, antes de soltá-lo) */
static void vblank_tira(void) {
    if (sDev != NULL && sVbEv != NULL) {
        sVbLigada = 0; /* (a espera volta ao Sleep já, sem esperar os 50 ms sem vblank) */
        sDev->SetVerticalBlankCallback(NULL);
    }
}

/* (b) A espera pelo vblank de hardware: o vblank_wait do plat_stub.c (a thread do jogo) espera o evento da callback em vez do
   Sleep. A mesma conta do vblank_wait: um alvo (o vblank que acaba a próxima espera) que anda um por chamada; o vblank que já
   passou não se espera (o passo da luta que passou de um vblank não perde a vez: a segunda espera dele acaba no tempo); mais
   de seis vblanks atrás (100 ms), um alvo novo a partir de agora, sem esperar (a primeira chamada também). Nenhuma espera a
   mais que no Sleep: o número de esperas por passo é o mesmo, e cada uma acaba no vblank de hardware em vez de num momento
   da grade do Sleep, sem fase com o vídeo. O Sleep fica de reserva: sem a callback (antes do init, depois do Soltar), com
   BT3_VBLANK_HW=0, sem vblank há 50 ms ou um evento que não veio em 50 ms (a Guide, o dispositivo parado). Devolve 2 se
   esperou, 1 se o vblank já tinha passado (sem espera), 0 para o Sleep; *desdeNs: de o último vblank até agora.
   (seção 20) A conta do alvo mora no vblank_alvo.h, com o recomeço pela dívida parada: a fila da gravação segurava o passo
   em dois vblanks com o alvo atrasado, e a espera sumia (a janela 1 da rodada da seção 19). Só a thread do jogo a mexe. */
static VbAlvo sVbConta;
static int sVbNaReserva;
static unsigned sVbReservas, sVbDitas;

/* os ns de um tique do mftb (contra o QueryPerformanceCounter desde a instalação; no primeiro segundo, o do contador) */
static double vblank_ns_por_tique(void) {
    LARGE_INTEGER q;
    unsigned __int64 tb = __mftb();
    QueryPerformanceCounter(&q);
    if (tb - sVbTb0 > 50000000ull && q.QuadPart > sVbQ0.QuadPart && sFreq.QuadPart != 0) {
        return (double)(q.QuadPart - sVbQ0.QuadPart) * 1e9 / (double)sFreq.QuadPart / (double)(tb - sVbTb0);
    }
    return sFreq.QuadPart != 0 ? 1e9 / (double)sFreq.QuadPart : 20.0;
}

static int vblank_reserva(const char *motivo) {
    vb_solta(&sVbConta);
    sVbReservas++;
    if (!sVbNaReserva && sVbDitas < 20) { /* (a passagem, não cada espera; poucas vezes: a Guide pode ir e vir) */
        sVbDitas++;
        Diario_Escrever("gs_d3d: vblank: a espera no Sleep (%s; %u esperas no Sleep até aqui)", motivo, sVbReservas);
    }
    sVbNaReserva = 1;
    return 0;
}

extern "C" int GsD3d_VblankEspera(unsigned long long *desdeNs) {
    DWORD n;
    unsigned __int64 tb, t;
    double f;
    int r = 1;
    *desdeNs = 0;
    if (sVbEv == NULL || !sVbHw || !sVbLigada) {
        return 0;
    }
    f = vblank_ns_por_tique();
    n = sVbN;
    __lwsync();
    tb = sVbTb;
    t = __mftb();
    if (n == 0) {
        return 0; /* (instalada há menos de um vblank) */
    }
    if ((double)(t - tb) * f > 50e6) {
        return vblank_reserva("sem vblank de hardware há 50 ms");
    }
    if (sVbNaReserva) {
        sVbNaReserva = 0;
        if (sVbDitas < 20) {
            sVbDitas++;
            Diario_Escrever("gs_d3d: vblank: a espera de volta ao vblank de hardware");
        }
    }
    /* (a primeira, ou 100 ms atrás, ou o vblank já passou: sem esperar; a dívida parada: espera o próximo; vblank_alvo.h) */
    if (vb_decide(&sVbConta, (unsigned)n) == 0) {
        do {
            if (WaitForSingleObject(sVbEv, 50) != WAIT_OBJECT_0) {
                return vblank_reserva("o evento do vblank não veio em 50 ms");
            }
            n = sVbN; /* (o evento pode ser de um vblank já contado: confere pelo número) */
        } while ((int)((unsigned)n - sVbConta.alvo) < 0);
        __lwsync();
        tb = sVbTb;
        t = __mftb();
        vb_esperou(&sVbConta);
        r = 2;
    }
    *desdeNs = (unsigned long long)((double)(t - tb) * f);
    return r;
}

/* para o passos.c: as imagens por vblanks na tela (1, 2, 3, 4 ou mais), as trocas perdidas, as imagens puladas e os
   vblanks vistos, acumulados desde a instalação, e (seção 20) da espera pelo vblank de hardware as chamadas com o vblank já
   passado, os alvos recomeçados pela dívida parada e os novos por estar 100 ms atrás; 0 sem a callback (antes do init) */
extern "C" int Passos_Tela(unsigned *v, int n) {
    unsigned agora[10];
    int i;
    if (sVbEv == NULL) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        agora[i] = sVbTela[i];
    }
    agora[4] = sVbPerdidas;
    agora[5] = sVbPuladas;
    agora[6] = sVbN;
    agora[7] = sVbConta.passadas; /* (só a thread do jogo os escreve, e é ela que lê, pelo passos) */
    agora[8] = sVbConta.recomecos;
    agora[9] = sVbConta.longes;
    for (i = 0; i < n && i < 10; i++) {
        v[i] = agora[i];
    }
    return 10;
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

static unsigned sPicoPress, sPicoDescN, sPicoEsperas, sPicoLotes; /* (picos) da gravação, desde a última entrega */

/* o que a gravação contou desde a última vez passa para os contadores do log (os dois lados parados um para o outro) */
static void grav_junta(void) {
    int i, j, k;
    sMem.pressoes += sG.pressoes;
    sMem.descN += sG.descN;
    sMem.descBytes += sG.descBytes;
    sMem.esperasDes += sG.esperas;
    sMem.acimaTeto += sG.acimaTeto;
    for (i = 0; i < BR_N; i++) {
        sMem.branca[i] += sG.branca[i];
    }
    sConta.texNovas += sG.texNovas;
    sConta.semTextura += sG.semTextura;
    sPicoPress += sG.pressoes; /* (picos: o quadro que a gravação entrega) */
    sPicoDescN += sG.descN;
    sPicoEsperas += sG.esperas;
    memset(&sG, 0, sizeof(sG));
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 3; j++) {
            sVuLotes[i][j] += gGsVu1Lotes[i][j];
            sPicoLotes += gGsVu1Lotes[i][j];
        }
    }
    for (i = 0; i < 8 && gGsVu1Desconhecido[i][1] != 0; i++) {
        for (k = 0; k < 8 && sVuDesc[k][1] != 0 && sVuDesc[k][0] != gGsVu1Desconhecido[i][0]; k++) {
        }
        if (k < 8) {
            sVuDesc[k][0] = gGsVu1Desconhecido[i][0];
            sVuDesc[k][1] += gGsVu1Desconhecido[i][1];
        }
    }
    sVuInstrDesc += gGsVu1InstrDesc;
    memset(gGsVu1Lotes, 0, sizeof(gGsVu1Lotes));
    memset(gGsVu1Desconhecido, 0, sizeof(gGsVu1Desconhecido));
    gGsVu1InstrDesc = 0;
}

/* ---- o registro dos quadros lentos (F5, etapa 8, picos; BT3_PICOS=L) ------------------------------------------- */
/* (gs_internal.h; a leitura em docs/x360/plano/F5/08-gravacao.md, seção 11) Três lados por quadro, cada um escrito pela
   própria thread na fronteira dela, numa vaga do anel pelo número do quadro do GS: o jogo (Picos_Jogo, na entrega da
   lista), a gravação (Picos_Gravou, na entrega do retrato) e o desenho (picos_apresentou e picos_desenho, no frame_end,
   que também põe a marca da GPU antes do Present). O desenho do quadro N escreve a linha do N - 2: os três lados dele já
   passaram e a GPU já chegou à marca dele (se não chegou, a linha diz -1). Por quadro: duas leituras do mftb no jogo
   (mais duas por espera de fato), quatro na gravação, duas no desenho, uma marca na GPU (D3DCALLBACK_IDLE: a GPU esvazia
   o pipeline uma vez por quadro, antes do Present). As linhas vão pelo diário (um anel; a escritora grava noutra thread),
   também dentro da janela do passos. */
extern "C" {
extern unsigned gPicosIsoN, gPicosDiario, gPicosDiarioDiscoUs; /* iso360.c, diario.c */
extern unsigned __int64 gPicosIsoBytes, gPicosIsoTiques;
extern unsigned gPortVBlanks; /* plat_stub.c */
}
#define PICOS_N 16
struct PicoJ {
    unsigned q, esperas, vblanks, vblank, isoN, isoKb, diario;
    float per, espera, vb, isoMs, discoMs;
};
struct PicoG {
    unsigned q, qJogo, texN, texKpx, envios, envioKb, hash, lotes, desenhos, verts, vuVerts, vuIdx, press, descN, esperasMem;
    unsigned xg[3], xgKb, soltas, memMede;
    float per, ocupada, espDes, ocioso, espMem, texMs, xgMs;
};
struct PicoD {
    unsigned q, captura, repete;
    float per, fim, present, cova;
    unsigned __int64 marca; /* o mftb quando a CPU pôs a marca da GPU */
};
static PicoJ sPJ[PICOS_N];
static PicoG sPG[PICOS_N];
static PicoD sPD[PICOS_N];
static volatile unsigned __int64 sPicoGpu[PICOS_N]; /* o mftb quando a GPU terminou o quadro (picos_cb) */
static double sPicoTbMs = 49875.0;                    /* tiques do mftb por ms (calibrado a cada resumo) */
static unsigned __int64 sPicoPresAnt, sPicoPres;      /* o mftb logo depois do Present: o anterior e o deste quadro */

static float picos_ms(unsigned __int64 tiques) {
    return (float)((double)tiques / sPicoTbMs);
}

/* o jogo entregou a lista do quadro (gs_core.c, Port_GsVif1Chain): o período desde a entrega anterior, a espera pela
   gravação nele (render_wait), a espera de vblank, os vblanks; o disco e o diário desde a entrega anterior */
extern "C" void Picos_Jogo(unsigned quadro, uint64_t vblankNs) {
    static unsigned __int64 ant, isoB, isoT;
    static unsigned vbAnt, isoN, diario, discoUs;
    unsigned __int64 t = __mftb(), b, tt;
    PicoJ *p = &sPJ[quadro % PICOS_N];
    unsigned vb = gPortVBlanks, n;
    p->q = ~0u; /* (a vaga em escrita) */
    __lwsync();
    p->per = ant != 0 ? picos_ms(t - ant) : 0.0f;
    ant = t;
    p->espera = picos_ms(gPicosJEspera);
    p->esperas = gPicosJEsperas;
    gPicosJEspera = 0;
    gPicosJEsperas = 0;
    p->vb = (float)((double)vblankNs / 1e6);
    p->vblanks = vb - vbAnt;
    vbAnt = vb;
    p->vblank = vb;
    n = gPicosIsoN;
    p->isoN = n - isoN;
    isoN = n;
    b = gPicosIsoBytes;
    p->isoKb = (unsigned)((b - isoB) >> 10);
    isoB = b;
    tt = gPicosIsoTiques;
    p->isoMs = sFreq.QuadPart != 0 ? (float)((double)(tt - isoT) * 1000.0 / (double)sFreq.QuadPart) : 0.0f;
    isoT = tt;
    n = gPicosDiario;
    p->diario = n - diario;
    diario = n;
    n = gPicosDiarioDiscoUs;
    p->discoMs = (float)(n - discoUs) / 1000.0f;
    discoUs = n;
    __lwsync();
    p->q = quadro;
}

/* a gravação entregou o retrato do quadro (gs_draw.c, desenho_entrega; sem a divisão, antes do frameEnd): o período
   desde a entrega anterior e, dele, a espera pelo desenho, o tempo sem trabalho (esperando a lista do jogo) e o resto,
   ocupada; e o que ela fez no quadro. c: desenhos, vértices de CPU, da VU, índices, constantes. */
extern "C" void Picos_Gravou(unsigned quadro, uint64_t esperaTiques, const uint32_t *c) {
    static unsigned __int64 ant, xgT;
    static unsigned xg[3], xgKb, soltas, memMede;
    unsigned __int64 t = __mftb(), x;
    PicoG *p = &sPG[quadro % PICOS_N];
    unsigned i, n;
    p->q = ~0u;
    __lwsync();
    p->per = ant != 0 ? picos_ms(t - ant) : 0.0f;
    ant = t;
    p->ocioso = picos_ms(gPicosGOcioso);
    gPicosGOcioso = 0;
    p->espDes = picos_ms(esperaTiques);
    p->espMem = picos_ms(sPicoEspMemTiques);
    sPicoEspMemTiques = 0;
    p->ocupada = p->per - p->ocioso - p->espDes;
    p->qJogo = gPicosQJogo;
    p->texN = gPicosTexN;
    p->texKpx = (gPicosTexPx + 500) / 1000;
    p->texMs = (float)((double)gPicosTexNs / 1e6);
    p->envios = gPicosEnvios;
    p->envioKb = (unsigned)(gPicosEnvioBytes >> 10);
    p->hash = gPicosHash;
    gPicosTexN = gPicosTexPx = gPicosEnvios = gPicosHash = 0;
    gPicosTexNs = gPicosEnvioBytes = 0;
    p->lotes = sPicoLotes;
    p->press = sPicoPress;
    p->descN = sPicoDescN;
    p->esperasMem = sPicoEsperas;
    sPicoLotes = sPicoPress = sPicoDescN = sPicoEsperas = 0;
    p->desenhos = c[0];
    p->verts = c[1];
    p->vuVerts = c[2];
    p->vuIdx = c[3];
    for (i = 0; i < 3; i++) { /* (os que outra thread também toca: por diferença) */
        n = sPicoXgN[i];
        p->xg[i] = n - xg[i];
        xg[i] = n;
    }
    n = sPicoXgKb;
    p->xgKb = n - xgKb;
    xgKb = n;
    x = sPicoXgTiques;
    p->xgMs = picos_ms(x - xgT);
    xgT = x;
    n = sPicoSoltas;
    p->soltas = n - soltas;
    soltas = n;
    n = sPicoMemMede;
    p->memMede = n - memMede;
    memMede = n;
    __lwsync();
    p->q = quadro;
}

static void WINAPI picos_cb(DWORD k) {
    sPicoGpu[k] = __mftb();
    __lwsync();
}

/* antes do Present: a marca da GPU (o callback roda quando a GPU acabou tudo o que veio antes, numa DPC da thread de
   hardware 1, como as marcas do BT3_TEMPOS) */
static void picos_marca(unsigned quadro) {
    unsigned k = quadro % PICOS_N;
    sPicoGpu[k] = 0;
    sPD[k].marca = __mftb();
    sDev->InsertCallback(D3DCALLBACK_IDLE | D3DCALLBACK_CPU1, picos_cb, k);
}

static unsigned sPicoHist[5], sPicoQuadros, sPicoJanela, sPicoAcima, sPicoAcimaJogo, sPicoAcimaCap, sPicoPiorQ;
static unsigned sPicoGpuPend;
static float sPicoPior;
static unsigned __int64 sPicoCalTb;
static LARGE_INTEGER sPicoCalQ;

/* a linha do quadro n (dois atrás do que o desenho acabou de apresentar) e, a cada 60, o resumo */
static void picos_escreve(unsigned n) {
    static const PicoJ semJ = {0, 0, 0, 0, 0, 0, 0, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};
    static PicoG semG;
    unsigned k = n % PICOS_N, b;
    const PicoD *d = &sPD[k];
    const PicoJ *j;
    const PicoG *g;
    unsigned __int64 gf = sPicoGpu[k], ga = sPicoGpu[(n - 1) % PICOS_N];
    float per = d->per, gpuFim, gpuPer;
    if (d->q != n) {
        return;
    }
    if (semG.per == 0.0f) {
        semG.per = semG.ocupada = semG.espDes = semG.ocioso = semG.espMem = -1.0f;
    }
    g = sPG[k].q == n ? &sPG[k] : &semG;
    /* o lado do jogo pelo quadro do jogo que a gravação recebeu com a lista (os dois contadores andam juntos na luta; sem a
       gravação, pelo mesmo número) */
    b = g != &semG ? g->qJogo : n;
    j = sPJ[b % PICOS_N].q == b ? &sPJ[b % PICOS_N] : &semJ;
    b = per <= 36.7f ? 0 : per <= 45.0f ? 1 : per <= 58.0f ? 2 : per <= 83.0f ? 3 : 4;
    sPicoHist[b]++;
    sPicoQuadros++;
    sPicoJanela += Passos_Medindo() != 0;
    if (per > sPicoPior) {
        sPicoPior = per;
        sPicoPiorQ = n;
    }
    sPicoGpuPend += gf == 0;
    if (per > (float)gPicos || j->per > (float)gPicos) {
        sPicoAcima++;
        sPicoAcimaJogo += j->per > (float)gPicos;
        sPicoAcimaCap += d->captura != 0;
        gpuFim = gf != 0 ? picos_ms(gf - d->marca) : -1.0f;
        gpuPer = gf != 0 && ga != 0 && gf > ga ? picos_ms(gf - ga) : -1.0f;
        Diario_Escrever("picos: quadro %u (vblank %u)%s%s: apresentação %.1f ms; jogo %.1f (espera da gravação %.1f em %u, "
                        "vblank %.1f, %u vblanks); gravação %.1f (ocupada %.1f, dela memória %.1f; esperando o desenho %.1f, "
                        "sem trabalho %.1f); desenho: frame_end %.1f, Present %.1f, cova %.1f, %u repetidas; GPU: fim %.1f "
                        "depois da CPU, período %.1f",
                        n, j->vblank, d->captura ? ", CAPTURA" : "", Passos_Medindo() ? ", na janela" : "", per, j->per,
                        j->espera, j->esperas, j->vb, j->vblanks, g->per, g->ocupada, g->espMem, g->espDes, g->ocioso, d->fim,
                        d->present, d->cova, d->repete, gpuFim, gpuPer);
        Diario_Escrever("picos:   quadro %u, o que teve: texturas %u (%u mil pixels, %.1f ms), envios %u (%u KB), hash %u, "
                        "VU1 %u lotes, desenhos %u, vértices %u de CPU e %u da VU (%u índices); descarte: %u pressões, %u "
                        "texturas, %u esperas; memória medida %u vezes; XG: %u texturas, %u alvos, %u profundidades (%u KB, "
                        "%.1f ms), %u soltas; disco %u leituras (%u KB, %.1f ms); diário %u linhas, escritora %.1f ms",
                        n, g->texN, g->texKpx, g->texMs, g->envios, g->envioKb, g->hash, g->lotes, g->desenhos, g->verts,
                        g->vuVerts, g->vuIdx, g->press, g->descN, g->esperasMem, g->memMede, g->xg[0], g->xg[1], g->xg[2],
                        g->xgKb, g->xgMs, g->soltas, j->isoN, j->isoKb, j->isoMs, j->diario, j->discoMs);
    }
    if (sPicoQuadros < 60) {
        return;
    }
    {   /* a calibração do mftb contra o QueryPerformanceCounter, no trecho */
        LARGE_INTEGER qn;
        unsigned __int64 tb = __mftb();
        QueryPerformanceCounter(&qn);
        if (sPicoCalTb != 0 && qn.QuadPart > sPicoCalQ.QuadPart && sFreq.QuadPart != 0) {
            sPicoTbMs = (double)(tb - sPicoCalTb) / ((double)(qn.QuadPart - sPicoCalQ.QuadPart) * 1000.0 / (double)sFreq.QuadPart);
        }
        sPicoCalTb = tb;
        sPicoCalQ = qn;
    }
    Diario_Escrever("picos: resumo, quadro %u: %u quadros (%u na janela), %u acima de %u ms (%u pelo jogo, %u com captura); "
                    "apresentação até 36,7 ms %u, até 45 %u, até 58 %u, até 83 %u, acima %u; pior %.1f ms no quadro %u; GPU "
                    "sem chegar à marca em dois quadros %u",
                    n, sPicoQuadros, sPicoJanela, sPicoAcima, gPicos, sPicoAcimaJogo, sPicoAcimaCap, sPicoHist[0], sPicoHist[1],
                    sPicoHist[2], sPicoHist[3], sPicoHist[4], sPicoPior, sPicoPiorQ, sPicoGpuPend);
    memset(sPicoHist, 0, sizeof(sPicoHist));
    sPicoQuadros = sPicoJanela = sPicoAcima = sPicoAcimaJogo = sPicoAcimaCap = sPicoGpuPend = 0;
    sPicoPior = 0.0f;
}

/* logo depois do Present: o período entre duas apresentações de quadro novo */
static void picos_apresentou(void) {
    sPicoPresAnt = sPicoPres;
    sPicoPres = __mftb();
}

/* no fim do frame_end: o lado do desenho do quadro e a linha do de dois atrás */
static void picos_desenho(const GsQuadro *q, double msFim, double msPresent, int captura) {
    static unsigned __int64 covaAnt;
    static unsigned repAnt;
    PicoD *p = &sPD[q->quadro % PICOS_N];
    unsigned __int64 c = sPicoCovaTiques;
    unsigned r = sPicoRepete;
    p->per = sPicoPresAnt != 0 ? picos_ms(sPicoPres - sPicoPresAnt) : 0.0f;
    p->fim = (float)msFim;
    p->present = (float)msPresent;
    p->captura = (unsigned)captura;
    p->cova = picos_ms(c - covaAnt);
    covaAnt = c;
    p->repete = r - repAnt;
    repAnt = r;
    p->q = q->quadro;
    picos_escreve(q->quadro - 2);
}

/* ---- tempos por passada (F5, etapa 8; BT3_TEMPOS, ligado por padrão) ----------------------------------------- */

/* O tempo de GPU de cada classe de trabalho do fim do quadro, pelo método do autoteste da profundidade (a GPU parada
   antes e depois), mas sem parar a CPU: a cada troca de classe uma marca, InsertCallback(D3DCALLBACK_IDLE), que a GPU
   só executa depois de terminar tudo o que veio antes (esvazia o pipeline) e que chama tempo_cb numa DPC da thread de
   hardware 1 (longe do áudio, 4 e 5, e do jogo, 2); a GPU espera a DPC voltar. A DPC guarda o relógio (mftb). O
   trecho entre duas marcas é o tempo da classe da primeira, mais o custo de uma marca: o quadro começa com duas
   marcas seguidas (o trecho vazio, a classe medição), e o log desconta esse custo de cada trecho. As marcas de um
   quadro são lidas dois quadros depois (dois buffers, com uma cerca depois da última marca). A CPU do fim do quadro é
   dividida nas mesmas classes (mais a cópia da VU1 e o Present) pelo mesmo relógio, a cada troca. Sem efeito na
   imagem: só callbacks e cercas. Ver docs/x360/plano/F5/08.md, "Tempo por passada no log". Desde a etapa 2 do 720p as
   "entradas na EDRAM" são a troca e a limpeza, e a "volta da cor" o desenho do conteúdo do alvo de volta por cima. */
enum {
    T_OUTROS, T_COMUM, T_VU0, T_VU4, T_VU6, T_PROF, T_CONTORNO, T_TAB_IDS, T_TAB_PROF, T_IDS, T_FBA, T_DATE, T_COR_RES,
    T_COR_ENTRA, T_COR_VOLTA, T_TELA, T_FILME, T_MED, T_GPU_N, /* da GPU (e da CPU); as de baixo, só da CPU */
    T_VU_COPIA = T_GPU_N, T_PRESENT, T_N
};
static const char *const kTempoNome[T_N] = {"outros", "comum", "vu0", "vu4", "vu6", "profundidade", "contorno",
                                            "tabela pelos ids", "tabela pela profundidade", "cópia dos ids", "FBA",
                                            "DATE", "resolves de cor", "entradas na EDRAM", "volta da cor", "tela",
                                            "filme", "medição",
                                            "cópia da VU1", "Present"};
#define T_MARCAS 4096 /* por quadro; um quadro com mais é descartado (contado); 136 KB nos dois buffers */
static volatile unsigned __int64 sMarcaGpu[2 * T_MARCAS]; /* o relógio quando a GPU chegou à marca (tempo_cb) */
static unsigned __int64 sMarcaCpu[2 * T_MARCAS];          /* o relógio quando a CPU terminou de pô-la */
static unsigned char sMarcaCl[2 * T_MARCAS];              /* a classe do trecho que começa na marca */
static struct {
    int ligado, quadro, aberta, gpu, cpu, buf, transbordou;
    unsigned n[2];
    DWORD cerca[2];
    int estado[2];                   /* 0 nada a ler, 1 a ler, 2 a descartar (captura ou transbordo) */
    unsigned __int64 desde, cq[T_N]; /* a CPU: a última troca e o quadro por classe */
    double freq;                     /* tiques do relógio por ms */
    /* no trecho */
    unsigned __int64 gSoma[T_GPU_N], gEsp[T_GPU_N], quadroSoma, cSoma[T_N], esperaTiques;
    unsigned gTrechos[T_GPU_N], lidos, descartados, perdidos, esperas, cQuadros;
    double gPior[T_GPU_N], quadroPior, medPior, cPior[T_N], cFim;
} sTempo;

static void WINAPI tempo_cb(DWORD k) {
    sMarcaGpu[k] = __mftb();
    __lwsync();
}

/* uma marca: o trecho da GPU que começa nela é da classe c (fim: a última do quadro, que sempre cabe) */
static void tempo_marca(int c, int fim) {
    unsigned k = sTempo.n[sTempo.buf], i = sTempo.buf * T_MARCAS + k;
    unsigned __int64 t0 = __mftb(), t1;
    sTempo.cq[sTempo.cpu] += t0 - sTempo.desde;
    if (k >= (unsigned)T_MARCAS - (fim ? 0 : 1)) {
        sTempo.transbordou = 1;
        sTempo.desde = t0;
        return;
    }
    sMarcaGpu[i] = 0;
    sMarcaCl[i] = (unsigned char)c;
    sDev->InsertCallback(D3DCALLBACK_IDLE | D3DCALLBACK_CPU1, tempo_cb, i);
    t1 = __mftb();
    sMarcaCpu[i] = t1;
    sTempo.n[sTempo.buf] = k + 1;
    sTempo.gpu = c;
    sTempo.cq[T_MED] += t1 - t0;
    sTempo.desde = t1;
}

/* o fim do quadro passa para a classe c: na CPU sempre, na GPU (uma marca) só se a classe mudou */
static void tempo_poe(int c) {
    unsigned __int64 t;
    if (!sTempo.quadro || c == sTempo.cpu) {
        return;
    }
    t = __mftb();
    sTempo.cq[sTempo.cpu] += t - sTempo.desde;
    sTempo.desde = t;
    sTempo.cpu = c;
    if (sTempo.aberta && c < T_GPU_N && c != sTempo.gpu) {
        tempo_marca(c, 0);
    }
}

/* as n primeiras marcas do buffer b estão escritas e em ordem */
static int tempo_valida(int b, unsigned n) {
    const volatile unsigned __int64 *g = &sMarcaGpu[b * T_MARCAS];
    unsigned k;
    for (k = 0; k < n; k++) {
        if (g[k] == 0 || (k > 0 && g[k] < g[k - 1])) {
            return 0;
        }
    }
    return 1;
}

/* O quadro marcado no buffer b (dois quadros atrás): os trechos por classe, o custo de uma marca (o trecho vazio) e
   o limite da espera da GPU pela CPU (o trecho começou na GPU antes de a CPU terminar de pô-lo). */
static void tempo_le(int b) {
    const volatile unsigned __int64 *g = &sMarcaGpu[b * T_MARCAS];
    const unsigned __int64 *cp = &sMarcaCpu[b * T_MARCAS];
    const unsigned char *cl = &sMarcaCl[b * T_MARCAS];
    unsigned __int64 raw[T_GPU_N], esp[T_GPU_N], vazio, total;
    unsigned seg[T_GPU_N], n = sTempo.n[b], k;
    double f = sTempo.freq, v;
    int c, e = sTempo.estado[b];
    sTempo.estado[b] = 0;
    if (e == 0) {
        return;
    }
    if (sDev->IsFencePending(sTempo.cerca[b])) {
        unsigned __int64 t = __mftb();
        sDev->BlockOnFence(sTempo.cerca[b]);
        sTempo.esperas++;
        sTempo.esperaTiques += __mftb() - t;
    }
    __lwsync();
    if (e == 2 || n < 3) {
        sTempo.descartados++;
        return;
    }
    if (!tempo_valida(b, n)) {
        sTempo.perdidos++;
        return;
    }
    memset(raw, 0, sizeof(raw));
    memset(esp, 0, sizeof(esp));
    memset(seg, 0, sizeof(seg));
    for (k = 0; k + 1 < n; k++) {
        unsigned __int64 s = g[k + 1] - g[k];
        c = cl[k];
        raw[c] += s;
        seg[c]++;
        if (cp[k + 1] > g[k]) {
            unsigned __int64 w = cp[k + 1] - g[k];
            esp[c] += w < s ? w : s;
        }
    }
    vazio = raw[T_MED]; /* (só a marca 0 é da medição) */
    total = g[n - 1] - g[0];
    for (c = 0; c < T_GPU_N; c++) {
        sTempo.gSoma[c] += raw[c];
        sTempo.gTrechos[c] += seg[c];
        sTempo.gEsp[c] += esp[c];
        if (c == T_MED) {
            v = (double)vazio * (n - 1) / f;
            if (v > sTempo.medPior) sTempo.medPior = v;
        } else {
            v = ((double)raw[c] - (double)seg[c] * (double)vazio) / f;
            if (v > sTempo.gPior[c]) sTempo.gPior[c] = v;
        }
    }
    sTempo.quadroSoma += total;
    if ((double)total / f > sTempo.quadroPior) sTempo.quadroPior = (double)total / f;
    sTempo.lidos++;
}

/* o começo do fim do quadro: lê o quadro de dois atrás, no buffer que este vai usar */
static void tempo_comeca(void) {
    sTempo.quadro = sTempo.ligado && !Passos_Medindo();
    if (!sTempo.quadro) {
        return;
    }
    memset(sTempo.cq, 0, sizeof(sTempo.cq));
    sTempo.desde = __mftb();
    sTempo.cpu = T_MED;
    sTempo.aberta = 0;
    sTempo.buf ^= 1;
    tempo_le(sTempo.buf);
    sTempo.n[sTempo.buf] = 0;
    sTempo.transbordou = 0;
    tempo_poe(T_OUTROS);
}

/* a GPU do quadro começa: duas marcas seguidas (o trecho vazio: o custo de uma marca neste quadro) */
static void tempo_abre(void) {
    if (!sTempo.quadro) {
        return;
    }
    sTempo.aberta = 1;
    tempo_marca(T_MED, 0);
    tempo_marca(T_OUTROS, 0);
}

/* a GPU do quadro termina (antes do Present): a última marca e a cerca para a leitura */
static void tempo_fecha_gpu(int captura) {
    if (!sTempo.quadro || !sTempo.aberta) {
        return;
    }
    tempo_marca(T_OUTROS, 1);
    sTempo.aberta = 0;
    sTempo.cerca[sTempo.buf] = sDev->InsertFence();
    sTempo.estado[sTempo.buf] = captura || sTempo.transbordou ? 2 : 1;
}

/* o fim do quadro terminou (msFim, pelo QueryPerformanceCounter): a CPU por classe, se o quadro vale */
static void tempo_fecha_cpu(double msFim, int vale) {
    int c;
    if (!sTempo.quadro) {
        return;
    }
    tempo_poe(T_OUTROS);
    sTempo.quadro = 0;
    if (!vale) {
        return;
    }
    for (c = 0; c < T_N; c++) {
        double v = (double)sTempo.cq[c] / sTempo.freq;
        sTempo.cSoma[c] += sTempo.cq[c];
        if (v > sTempo.cPior[c]) sTempo.cPior[c] = v;
    }
    sTempo.cFim += msFim;
    sTempo.cQuadros++;
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

/* (degrau 2, etapa 4) os pendentes mudam dos dois lados com a divisão (a gravação pede, o desenho solta): atômicos */
static void pend_soma(int cl, int bytes, int n) {
    InterlockedExchangeAdd((LONG volatile *)&sMem.pend[cl], (LONG)bytes);
    InterlockedExchangeAdd((LONG volatile *)&sMem.nPend[cl], (LONG)n);
}

static void cova_solta(int i) {
    xg_libera(sCova[i]);
    pend_soma(sCovaClasse[i], -(int)sCovaBytes[i], -1);
}

/* a mais antiga da cova, esperando a GPU (nada do quadro fica preso no dispositivo fora do frame_end; mesmo assim os
   estágios são soltos antes, porque uma textura presa fica ocupada para sempre) */
static void cova_espera_primeira(void) {
    const char *antes = (const char *)gGsD3dEtapa;
    gGsD3dEtapa = "esperando a GPU soltar um recurso descartado";
    sDev->SetTexture(0, NULL);
    sDev->SetTexture(1, NULL);
    esquece_estado();
    {
        unsigned __int64 t0 = __mftb();
        xg_espera(sCova[0]);
        sPicoCovaTiques += __mftb() - t0;
    }
    cova_solta(0);
    memmove(sCova, sCova + 1, (sCovaN - 1) * sizeof(sCova[0]));
    memmove(sCovaBytes, sCovaBytes + 1, (sCovaN - 1) * sizeof(sCovaBytes[0]));
    memmove(sCovaClasse, sCovaClasse + 1, (sCovaN - 1) * sizeof(sCovaClasse[0]));
    sCovaN--;
    sMem.esperas++;
    gGsD3dEtapa = antes;
}

/* os bytes de um recurso solto saem dos vivos e passam a pendentes (na gravação, no pedido) */
static void cova_conta(int cl, unsigned bytes) {
    sMem.vivos[cl] -= sMem.vivos[cl] >= bytes ? bytes : sMem.vivos[cl];
    sMem.nVivos[cl]--;
    pend_soma(cl, (int)bytes, 1);
    if (sDiv) {
        sPendGrav += bytes;
    }
}

/* o recurso na cova, até a GPU o soltar (no desenho; os bytes já contados como pendentes) */
static void cova_poe(D3DTexture *r, int cl, unsigned bytes) {
    if (r == NULL) {
        return;
    }
    if (r == sMostradoTex) {
        sMostradoTex = NULL; /* (o GsD3d_Repete não a usa mais) */
    }
    if (sCovaN == MAX_COVA) {
        cova_espera_primeira();
    }
    sCova[sCovaN] = r;
    sCovaBytes[sCovaN] = bytes;
    sCovaClasse[sCovaN] = (unsigned char)cl;
    sCovaN++;
}

/* um recurso solto, com os bytes dele, que saem dos vivos e passam a pendentes (sem a divisão: os dois de uma vez) */
static void cova(D3DTexture *r, int cl, unsigned bytes) {
    if (r == NULL) {
        return;
    }
    cova_conta(cl, bytes);
    cova_poe(r, cl, bytes);
}

/* o que a GPU já soltou sai (no fim do quadro, depois de soltar as texturas do dispositivo e do InsertFence, e a cada
   pressão de memória) */
static void cova_varre(void) {
    int i, n = 0;
    for (i = 0; i < sCovaN; i++) {
        if (xg_ocupado(sCova[i])) {
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
    if (!sDiv) {
        cova_varre(); /* (com a divisão a cova é do desenho) */
    }
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
    return tex == 0 ? 0 : ((const TexXg *)tex)->bytes;
}

/* (degrau 2, etapa 4) A gravação sem memória, com a divisão: a cova é do desenho. Pede a ele (sPedeLivre, e acorda o
   ocioso; o que está desenhando atende no fim do quadro, GsD3d_Ocioso) e espera o sinal, com limite de 100 ms: o que falta
   pode estar na fila de um retrato que só se entrega depois desta espera (o risco 8 do inventário). Sem pendentes além dos
   pedidos desde a última entrega, ou atendido e com a cova vazia, não há o que esperar. Devolve quantas vezes esperou;
   *livres sai medido. */
static unsigned espera_desenho(unsigned precisa, unsigned *livres) {
    const char *antes = (const char *)gGsD3dEtapaGrav;
    DWORD t0 = GetTickCount();
    unsigned n = 0;
    unsigned __int64 pt0 = __mftb();
    gGsD3dEtapaGrav = "esperando o desenho soltar memória";
    while (*livres < precisa && GetTickCount() - t0 < 100) {
        DWORD r;
        if (sMem.pend[CL_TEX] + sMem.pend[CL_ALVO] + sMem.pend[CL_PROF] <= sPendGrav) {
            break; /* (o que está pendente ainda viaja com o quadro em gravação) */
        }
        ResetEvent(sCovaEv);
        sPedeLivre = precisa;
        GsDraw_DesenhoAcorda();
        r = WaitForSingleObject(sCovaEv, 25);
        n++;
        *livres = mem_mede();
        if (r == WAIT_OBJECT_0 && *(volatile int *)&sCovaN == 0) {
            break;
        }
    }
    sPedeLivre = 0;
    sPicoEspMemTiques += __mftb() - pt0;
    gGsD3dEtapaGrav = antes;
    return n;
}

/* O frontend descartou `n` texturas (`bytes`, a mais recente sem uso há `idade` quadros; `emUso` protegidas). O que a
   GPU já soltou sai da cova; se o piso ainda não deixa, espera a GPU soltar o mais antigo da cova, um por vez (com a
   divisão, pede ao desenho: espera_desenho). 1: o pedido cabe no piso (o teto das texturas é macio: passar dele é melhor
   que a branca). */
extern "C" int GsD3d_Descartou(uint32_t w, uint32_t h, int modo, unsigned n, uint32_t bytes, unsigned idade, unsigned emUso) {
    unsigned pedido = pedido_bytes(w, h, modo), piso = sMem.pisoMb * MB, livres, esperas;
    int cabe;
    sG.pressoes++;
    sMem.pressoesTot++;
    sG.descN += n;
    sMem.descNTot += n;
    sG.descBytes += bytes;
    sMem.descBytesTot += bytes;
    sMem.emUso = emUso;
    if (!sDiv) {
        unsigned antes = sMem.esperas;
        cova_varre();
        livres = mem_mede();
        while (livres < piso + pedido && sCovaN > 0) {
            cova_espera_primeira();
            livres = mem_mede();
        }
        esperas = sMem.esperas - antes;
    } else {
        livres = mem_mede();
        esperas = livres < piso + pedido ? espera_desenho(piso + pedido, &livres) : 0;
        sG.esperas += esperas;
    }
    sMem.esperasTot += esperas;
    cabe = livres >= piso + pedido;
    if (n == 0 && cabe) {
        sG.acimaTeto++; /* (só o teto das texturas, sem nada a descartar: passa dele, sem linha) */
        return 1;
    }
    if (sMem.falasPressao < 300 || sMem.pressoesTot % 50 == 0 || !cabe) {
        sMem.falasPressao++;
        Diario_Escrever("gs_d3d: pressão %u, quadro %u: pedido %u KB (%s %ux%u); descartadas %u texturas (%u KB), a mais "
                        "recente sem uso há %u quadros, %u protegidas; %u esperas pela GPU; agora texturas %u KB + %u KB "
                        "pendentes, alvos e profundidades %u KB, livres %u MB (piso %u): %s",
                        sMem.pressoesTot, gGsFrame, pedido >> 10, modo == MODO_ALVO ? "alvo" : "textura", w, h, n, bytes >> 10,
                        idade, emUso, esperas, sMem.vivos[CL_TEX] >> 10, sMem.pend[CL_TEX] >> 10,
                        (sMem.vivos[CL_ALVO] + sMem.pend[CL_ALVO] + sMem.vivos[CL_PROF] + sMem.pend[CL_PROF]) >> 10, livres >> 20, sMem.pisoMb,
                        cabe ? "cabe" : "NÃO CABE");
    }
    return cabe;
}

/* uma textura que saiu branca, e por quê (as primeiras de cada motivo com o retrato da memória) */
static void branca(int motivo, uint32_t w, uint32_t h) {
    sG.branca[motivo]++;
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
    TexXg *t;
    unsigned bytes = tam_tex(w, h);
    (void)levels;
    if (format != GS_TEXFMT_RGBA8) {
        sG.semTextura++;
        branca(BR_FORMATO, w, h);
        return 0;
    }
    if (mem_livres(bytes) < sMem.pisoMb * MB + bytes) {
        sG.semTextura++;
        return 0; /* (quem chamou conta o motivo) */
    }
    t = xg_cria(w, h, D3DFMT_LIN_A8B8G8R8);
    if (t == NULL) {
        sG.semTextura++;
        return 0;
    }
    mem_cria(CL_TEX, t->bytes);
    sG.texNovas++;
    return (GsTex)t;
}

static int tex_upload(GsTex handle, int level, uint32_t w, uint32_t h, int format, const void *px, uint32_t bytes) {
    TexXg *t = (TexXg *)handle;
    uint32_t y;
    (void)format;
    (void)bytes;
    /* (a textura acabou de ser criada: a GPU nunca a viu, o envio é direto no endereço, sem LockRect) */
    if (t == NULL || level != 0) {
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
        memcpy((BYTE *)t->dados + y * t->pitch, (const uint32_t *)px + y * w, w * 4);
    }
    free((void *)px);
    return 1;
}

static void tex_destroy(GsTex tex) {
    TexXg *t = (TexXg *)tex;
    if (t == NULL) {
        return;
    }
    cova(&t->cab, CL_TEX, t->bytes);
}

/* só as paletas (clut_texture) perguntam: o texture_get do 360 abre espaço por GsDraw_X360Abre */
static int tex_slots_left(void) { return mem_livres(4096) >= sMem.pisoMb * MB + 4096 ? 1 << 20 : 0; }
static GsTex white_tex(void) { return (GsTex)sBranco; }

/* textura tiled (destino de Resolve), zerada: o conteúdo de um alvo novo é o da limpeza */
static D3DTexture *textura_alvo(void) {
    TexXg *t = NULL;
    if (mem_livres(tam_alvo()) < sMem.pisoMb * MB + tam_alvo() || (t = xg_cria(sAlvoW, sAlvoH, D3DFMT_A8R8G8B8)) == NULL) {
        return NULL;
    }
    mem_cria(CL_ALVO, tam_alvo());
    memset(t->dados, 0, t->bytes); /* (nova: a GPU nunca a viu) */
    return &t->cab;
}

/* ---- alvos --------------------------------------------------------------------------------------------------- */

/* A textura de um alvo, abrindo espaço no cache de texturas se o piso não deixa; se o D3D recusa, descarta mais e
   tenta de novo. Sem textura, o alvo ainda desenha na EDRAM, mas perde o conteúdo ao sair e quem o lê recebe a
   branca: o log diz, e o fim do quadro tenta de novo. */
static D3DTexture *alvo_com_espaco(void) {
    D3DTexture *t;
    const char *antes = (const char *)gGsD3dEtapaGrav;
    gGsD3dEtapaGrav = "criando a textura de um alvo";
    GsDraw_X360Abre(sAlvoW, sAlvoH, MODO_ALVO);
    t = textura_alvo();
    if (t == NULL && GsDraw_X360Abre(sAlvoW, sAlvoH, MODO_FALHOU)) {
        t = textura_alvo();
    }
    gGsD3dEtapaGrav = antes;
    return t;
}

static int sAlvoSemTex[MAX_TARGETS]; /* 1: a vaga está sem textura (o fim do quadro tenta de novo) */

static void prof_solta(int i);
/* (na gravação) a textura nova da vaga; o desenho esquece o conteúdo da vaga, o da cor e o da profundidade (que, se houver,
   fica para o novo dono), no começo do quadro desta gravação (sincroniza_alvos). O sResidente e o sZDono não precisam: entre
   dois quadros eles são sempre -1 (o frame_end acaba com sai_da_edram e prof_entrega). */
static void target_ensure(int i) {
    sAlvos[i].tex = alvo_com_espaco();
    sAlvos[i].ver++;
    sAlvoSemTex[i] = sAlvos[i].tex == NULL;
    if (sAlvos[i].tex == NULL) {
        Diario_Escrever("gs_d3d: ERRO: o alvo %d (de %d) ficou sem textura no quadro %u: livres %u MB (piso %u), texturas "
                        "%u KB + %u KB pendentes; o fim do quadro tenta de novo", i, gsTargetCount, gGsFrame, mb_livres(),
                        sMem.pisoMb, sMem.vivos[CL_TEX] >> 10, sMem.pend[CL_TEX] >> 10);
    }
}

/* no fim do quadro (com a divisão, na entrega; sempre na gravação): as vagas que ficaram sem textura tentam de novo (o
   conteúdo delas já se perdeu; volta a valer a partir do próximo desenho). limpa: a marca da entrega, desligada junto */
static void alvos_sem_textura(unsigned char *limpa) {
    int i;
    sMem.semAlvo = 0;
    for (i = 0; i < gsTargetCount; i++) {
        if (sAlvos[i].tex != NULL || !sAlvoSemTex[i]) {
            continue;
        }
        sAlvos[i].tex = alvo_com_espaco();
        if (sAlvos[i].tex != NULL) {
            sAlvos[i].ver++;
            sAlvoSemTex[i] = 0;
            gsTargets[i].cleared = 0;
            if (limpa != NULL) {
                limpa[i] = 0;
            }
            Diario_Escrever("gs_d3d: o alvo %d ganhou a textura no quadro %u", i, gGsFrame);
        } else {
            sMem.semAlvo++;
        }
    }
}

/* (sem a divisão) a vaga reaproveitada: as texturas dela para a cova (o sResidente é -1 e o sDateFonte se refaz no começo
   do quadro) */
static void target_drop(int i) {
    if (sAlvos[i].tex != NULL) {
        cova(sAlvos[i].tex, CL_ALVO, tam_alvo());
    }
    sAlvos[i].tex = NULL;
    sAlvos[i].ver++;
    sAlvoSemTex[i] = 0;
    prof_solta(i);
}

/* (degrau 2, etapa 4, com a divisão) O pedido de soltar, na gravação (no lugar do texDestroy e do targetDrop): os bytes passam
   a pendentes já, e o recurso vai para a cova no desenho (solta) depois de ele desenhar o quadro do pedido. A vaga de alvo
   reaproveitada fica sem as texturas (o targetEnsure seguinte cria as novas); a cor viaja na fila do gs_draw.c (o tex que
   ele guardou é o target_color desta vaga) e a profundidade na sProfFila (prof_solta). */
static void solta_pede(GsTex tex, int alvo) {
    if (alvo < 0) {
        if (tex != 0) {
            cova_conta(CL_TEX, ((const TexXg *)tex)->bytes);
        }
        return;
    }
    if (sAlvos[alvo].tex != NULL) {
        cova_conta(CL_ALVO, tam_alvo());
    }
    sAlvos[alvo].tex = NULL;
    sAlvos[alvo].ver++;
    sAlvoSemTex[alvo] = 0;
    prof_solta(alvo);
}

/* no desenho, depois de desenhar o quadro do pedido: para a cova, que solta quando a GPU largar */
static void solta(GsTex tex, int alvo) {
    if (tex != 0) {
        cova_poe((D3DTexture *)tex, alvo < 0 ? CL_TEX : CL_ALVO, alvo < 0 ? ((const TexXg *)tex)->bytes : tam_alvo());
    }
}

static GsTex target_color(int i) {
    if (sAlvos[i].tex == NULL) {
        branca(BR_ALVO, sAlvoW, sAlvoH);
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
static float sCurEsc[2]; /* o c6 do gs_pixel posto (0: nenhum ainda) */
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
    /* (a chave de uma passada de tabela, bit 22, tem o teste NEVER: ela não usa a profundidade) */
    sDev->SetRenderState(D3DRS_ZENABLE, p->fx ? FALSE : TRUE);
    sDev->SetRenderState(D3DRS_ZWRITEENABLE, p->zwrite && !p->fx ? TRUE : FALSE);
    sDev->SetRenderState(D3DRS_ZFUNC, p->ztst == 0 ? D3DCMP_NEVER : p->ztst == 1 ? D3DCMP_ALWAYS :
                                      p->ztst == 2 ? D3DCMP_GREATEREQUAL : D3DCMP_GREATER);
}

/* bit 0 linear, bit 1 U preso na borda, bit 2 V (bit 3, os níveis menores de um pacote, não existe aqui) */
static void aplica_sampler(DWORD estagio, int s) {
    sDev->SetSamplerState(estagio, D3DSAMP_MINFILTER, (s & 1) ? D3DTEXF_LINEAR : D3DTEXF_POINT);
    sDev->SetSamplerState(estagio, D3DSAMP_MAGFILTER, (s & 1) ? D3DTEXF_LINEAR : D3DTEXF_POINT);
    sDev->SetSamplerState(estagio, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    sDev->SetSamplerState(estagio, D3DSAMP_ADDRESSU, (s & 0x100) ? D3DTADDRESS_BORDER : (s & 2) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP);
    sDev->SetSamplerState(estagio, D3DSAMP_ADDRESSV, (s & 0x100) ? D3DTADDRESS_BORDER : (s & 4) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP);
    if (s & 0x100) {
        /* (720p, etapa 2) um alvo como textura além da área: preto transparente, o que a parte limpa do alvo de 1024 dava */
        sDev->SetSamplerState(estagio, D3DSAMP_BORDERCOLOR, 0);
    }
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
static void alinha8(Ret *r) { esc_alinha8(&r->x0, &r->y0, &r->x1, &r->y1, sAlvoW, sAlvoH); }

/* (profundidade) x em múltiplos de 80, a largura de um tile da EDRAM com 32 bits por pixel: o restauro como cor lê o
   pixel x + 40 ou x - 40 do mesmo tile, que assim fica sempre dentro do que foi resolvido */
static void alinha80(Ret *r) { esc_alinha80(&r->x0, &r->y0, &r->x1, &r->y1, sAlvoW, sAlvoH); }

/* ---- profundidade por alvo (F5, etapa 7) --------------------------------------------------------------------- */

/* A profundidade de r (alinhado) da textura z volta para a EDRAM, que já tem o alvo posto (sEdramCor e sEdramProf).
   Modo 1: escrita como cor na superfície que tem a mesma base da profundidade (o exemplo FastDepthRestore do XDK).
   Modo 2: pelo oDepth, com a cor desligada. */
static void prof_poe(Prof *z, const Ret *r, int modo) {
    D3DVIEWPORT9 vp = {0, 0, sAlvoW, sAlvoH, 0.0f, 1.0f}, vpz = {0, 0, sAlvoWZ, sAlvoH, 0.0f, 1.0f};
    float x0 = (float)r->x0, y0 = (float)r->y0, x1 = (float)r->x1, y1 = (float)r->y1, W = (float)sAlvoW, H = (float)sAlvoH;
    if (modo == 1) {
        float zc[4] = {W, 1.0f / W, 0.0f, 0.0f}; /* (a largura da textura, para o ps_zcor) */
        if (r->x1 == sAlvoW) {
            x1 = (float)sAlvoWZ; /* (o último tile inteiro: ver sAlvoWZ) */
        }
        sDev->SetPixelShaderConstantF(7, zc, 1);
        sDev->SetDepthStencilSurface(NULL);
        sDev->SetRenderTarget(0, sEdramProfCor);
        sDev->SetViewport(&vpz);
        sDev->SetRenderState(D3DRS_ZENABLE, FALSE);
        sDev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        sDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);
        quad(z->cor, sPsZCor, sAlvoWZ, sAlvoH, x0, y0, x1, y1, x0 / W, y0 / H, x1 / W, y1 / H, 0);
        sDev->SetRenderTarget(0, sEdramCor);
        sDev->SetDepthStencilSurface(sEdramProf);
        sDev->SetViewport(&vp);
    } else {
        sDev->SetRenderState(D3DRS_ZENABLE, TRUE);
        sDev->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
        sDev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        sDev->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
        quad(z->tex, sPsZProf, sAlvoW, sAlvoH, x0, y0, x1, y1, x0 / W, y0 / H, x1 / W, y1 / H, 0);
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

/* a textura de profundidade de um alvo e o cabeçalho que a lê como cor (na gravação: o XG, sem o dispositivo); 0: não coube */
static int prof_cria(int i, int abre) {
    Prof *z = &sProf[i];
    ProfXg *p = NULL;
    UINT base = 0;
    if (abre) {
        const char *antes = (const char *)gGsD3dEtapaGrav;
        gGsD3dEtapaGrav = "criando a textura de profundidade de um alvo";
        GsDraw_X360Abre(sAlvoW, sAlvoH, MODO_ALVO); /* (mesmo tamanho de um alvo: 4 MB com 1024 x 1024) */
        gGsD3dEtapaGrav = antes;
    }
    if (mem_livres(tam_alvo()) < sMem.pisoMb * MB + tam_alvo() ||
        (p = (ProfXg *)xg_cria_n(sAlvoW, sAlvoH, D3DFMT_D24S8, sizeof(ProfXg))) == NULL) {
        return 0;
    }
    p->x.prof = 1;
    mem_cria(CL_PROF, tam_alvo());
    /* (nova, como a do CreateTexture: o conteúdo só vale depois do primeiro Resolve; `conteudo` vazio até lá) */
    XGGetTextureLayout(&p->x.cab, &base, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL, 0);
    XGSetTextureHeader(sAlvoW, sAlvoH, 1, 0, D3DFMT_A8B8G8R8, (D3DPOOL)0, base, 0, p->x.pitch, &p->cor, NULL, NULL);
    z->tex = &p->x.cab;
    z->cor = &p->cor;
    z->conteudo.x0 = z->conteudo.x1 = 0;
    z->ver++;
    return 1;
}

/* (na gravação) a profundidade da vaga sai: sem a divisão, para a cova já (que espera os dois cabeçalhos, xg_ocupado); com
   ela, os bytes passam a pendentes e a textura vai na sProfFila do quadro em gravação. O desenho a esquece no começo do
   quadro desta gravação (sincroniza_alvos; o sZDono é -1 entre dois quadros). */
static void prof_solta(int i) {
    Prof *z = &sProf[i];
    if (z->tex != NULL) {
        if (!sDiv) {
            cova(z->tex, CL_PROF, tam_alvo());
        } else if (sProfFilaN[sProfFilaG] < PROF_FILA) {
            cova_conta(CL_PROF, tam_alvo());
            sProfFila[sProfFilaG][sProfFilaN[sProfFilaG]++] = z->tex;
        } else {
            Diario_Escrever("gs_d3d: ERRO: a fila das profundidades soltas encheu no quadro %u; uma fica sem soltar", gGsFrame);
        }
    }
    z->tex = NULL;
    z->cor = NULL;
    z->conteudo.x0 = z->conteudo.x1 = 0;
    z->falhou = 0;
    z->ver++;
}

/* Antes de refazer a lista (nada dela está na GPU ainda e as texturas do quadro estão protegidas do descarte): a textura
   de profundidade de cada alvo em que algum desenho escreve z. Na gravação: na entrega com a divisão, no começo do
   frame_end sem ela. */
static void prof_garante(const GsQuadro *q) {
    static unsigned char quer[MAX_TARGETS];
    static unsigned falas;
    uint32_t n;
    int i;
    if (sModoZ == 0) {
        return;
    }
    memset(quer, 0, sizeof(quer));
    for (n = 0; n < q->drawCount; n++) {
        const GsDraw *d = &q->draws[n];
        if (d->native == 0 && d->count != 0 && d->target >= 0 && d->target < q->targetCount &&
            sPipes[d->pipeline].zwrite && sPipes[d->pipeline].ztst != 0) {
            quer[d->target] = 1;
        }
    }
    for (i = 0; i < q->targetCount; i++) {
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
    if (sZDono >= 0 && sSujoZ.x1 > sSujoZ.x0 && sProfD[sZDono].tex != NULL) {
        Prof *z = &sProfD[sZDono];
        LARGE_INTEGER t;
        Ret rz = sSujoZ;
        QueryPerformanceCounter(&t);
        tempo_poe(T_PROF);
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

/* (F5, etapa 7, segunda parte) A profundidade do dono, que está na EDRAM, em dia na textura dele sem entregar a posse:
   o corpo de prof_entrega sem o sZDono = -1. Antes de uma passada de tabela pela névoa, que lê a textura. */
static void prof_sincroniza(void) {
    int dono = sZDono;
    Ret r = sSujoZ;
    if (dono < 0 || r.x1 <= r.x0) {
        return;
    }
    prof_entrega();
    sZDono = dono;
    sConta.natZSinc++;
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
    if (r.x1 > r.x0 && sAlvoD[sResidente].tex != NULL) {
        tempo_poe(T_COR_RES);
        /* o destino do Resolve não fica preso como textura (a cópia de volta deixou a dele no estágio 0) */
        sDev->SetTexture(0, NULL);
        sDev->SetTexture(1, NULL);
        sCurTex = (GsTex)-1;
        sCurDate = NULL;
        alinha8(&r);
        fonte.x1 = r.x0; fonte.y1 = r.y0; fonte.x2 = r.x1; fonte.y2 = r.y1;
        destino.x = r.x0; destino.y = r.y0;
        sDev->Resolve(D3DRESOLVE_RENDERTARGET0, &fonte, sAlvoD[sResidente].tex, &destino, 0, 0, NULL, 0.0f, 0, NULL);
        ret_une(&sAlvoD[sResidente].conteudo, r.x0, r.y0, r.x1, r.y1);
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
    Alvo *a = &sAlvoD[i];
    D3DVIEWPORT9 vp = {0, 0, sAlvoW, sAlvoH, 0.0f, 1.0f};
    DWORD limpa = D3DCLEAR_TARGET;
    int poeZ = 0;
    sai_da_edram();
    if (!sQ->targets[i].cleared && sZDono == i) {
        sZDono = -1; /* (novo para o jogo: o que a EDRAM tem dele não vale) */
        sSujoZ.x0 = sSujoZ.x1 = 0;
    }
    if (sModoZ == 0) {
        limpa |= D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL;
    } else if (usaZ) {
        sConta.zVisitas++;
        sConta.zSemTex += sProfD[i].tex == NULL;
        if (sZDono == i) {
            sConta.zMantida++;
        } else {
            prof_entrega();
            limpa |= D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL;
            poeZ = 1;
        }
    }
    tempo_poe(T_COR_ENTRA);
    sDev->SetRenderTarget(0, sEdramCor);
    sDev->SetDepthStencilSurface(sEdramProf);
    sDev->SetViewport(&vp);
    sDev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    sDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);
    sDev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    sDev->Clear(0, NULL, limpa, 0x00000000, 0.0f, 0);
    if (!sQ->targets[i].cleared) {
        a->conteudo.x0 = a->conteudo.x1 = 0; /* novo para o jogo: começa limpo */
        sProfD[i].conteudo.x0 = sProfD[i].conteudo.x1 = 0;
    } else {
        if (a->conteudo.x1 > a->conteudo.x0 && a->tex != NULL) {
            Ret r = a->conteudo;
            tempo_poe(T_COR_VOLTA); /* (720p, etapa 2: a volta da cor separada da limpeza na medição) */
            copia(a->tex, sAlvoW, sAlvoH, (float)r.x0, (float)r.y0, (float)r.x1, (float)r.y1, (float)r.x0 / sAlvoW,
                  (float)r.y0 / sAlvoH, (float)r.x1 / sAlvoW, (float)r.y1 / sAlvoH, 0);
            sConta.restauros++;
            sConta.corRestPx += (double)(r.x1 - r.x0) * (r.y1 - r.y0);
        }
        if (poeZ && sProfD[i].tex != NULL && sProfD[i].conteudo.x1 > sProfD[i].conteudo.x0) {
            LARGE_INTEGER t;
            Ret r = sProfD[i].conteudo;
            QueryPerformanceCounter(&t);
            tempo_poe(T_PROF);
            prof_poe(&sProfD[i], &r, sModoZ);
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
    const TexXg *xa = (const TexXg *)a, *xb = (const TexXg *)b; /* (do prof_cria, pelo XG; quem chama parou a GPU) */
    unsigned n, k;
    c[0] = c[1] = c[2] = c[3] = c[4] = 0;
    n = xa->pitch / 4 * sAlvoH;
    c[4] = n;
    for (k = 0; k < n; k++) {
        unsigned x = ((const unsigned *)xa->dados)[k], y = ((const unsigned *)xb->dados)[k];
        c[0] += x == y;
        c[1] += (x & 0xFFFFFF00u) == (y & 0xFFFFFF00u);
        c[2] += (x & 0x00FFFFFFu) == (y & 0x00FFFFFFu);
        c[3] += x != 0;
    }
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
static Ret sInteiro = {0, 0, GS_W, GS_H}; /* um alvo inteiro (area_init) */
static void cr_zres(void *z) { prof_tira((Prof *)z, &sInteiro); }
static void cr_zcor(void *z) { prof_poe((Prof *)z, &sInteiro, 1); }
static void cr_zprof(void *z) { prof_poe((Prof *)z, &sInteiro, 2); }
static void cr_corres(void *t) {
    D3DRECT fonte = {0, 0, sAlvoW, sAlvoH};
    D3DPOINT destino = {0, 0};
    sDev->Resolve(D3DRESOLVE_RENDERTARGET0, &fonte, (D3DTexture *)t, &destino, 0, 0, NULL, 0.0f, 0, NULL);
}
static void cr_correst(void *t) {
    copia((D3DTexture *)t, sAlvoW, sAlvoH, 0.0f, 0.0f, (float)sAlvoW, (float)sAlvoH, 0.0f, 0.0f, 1.0f, 1.0f, 0);
}

/* Antes do primeiro quadro: um padrão de bits qualquer (pseudoaleatório, sem período de 40 nem de 80 colunas) é
   escrito como cor na profundidade da EDRAM e resolvido (A); a EDRAM é limpa, A volta por um modo e é resolvida de
   novo (B); A e B têm de ser iguais. O controle (B sem restauro, tudo zero) tem de dar diferente: prova que a
   comparação acusa. Mede também a GPU do restauro e do resolve de um alvo inteiro, de cor e de profundidade. Escolhe
   o modo: BT3_Z=0 desliga (a profundidade volta zerada, como antes), 1 ou 2 força; sem ele, o 1 se for exato em todos
   os bits, senão o 2 se for exato na profundidade, senão desligado com ERRO. */
static void prof_autoteste(void) {
    D3DVIEWPORT9 vp = {0, 0, sAlvoW, sAlvoH, 0.0f, 1.0f}, vpz = {0, 0, sAlvoWZ, sAlvoH, 0.0f, 1.0f};
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
    if (sEdramProfCor == NULL || sPsZCor == NULL || sPsZProf == NULL) {
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
        xg_libera(za.tex); /* (nunca usada pela GPU) */
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
        copia(pad, sAlvoWZ, sAlvoH, 0.0f, 0.0f, (float)sAlvoWZ, (float)sAlvoH, 0.0f, 0.0f, 1.0f, 1.0f, 0);
        sDev->SetTexture(0, NULL);
        sDev->SetRenderTarget(0, sEdramCor);
        sDev->SetDepthStencilSurface(sEdramProf);
        sDev->SetViewport(&vp);
        prof_tira(&za, &sInteiro);
        sDev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        sDev->Clear(0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 0.0f, 0);
        if (modo != 0) {
            prof_poe(&za, &sInteiro, modo);
        }
        prof_tira(&zb, &sInteiro);
        sDev->BlockUntilIdle();
        prof_compara(za.tex, zb.tex, modo == 0 ? ctl : c[modo]);
    }
    total = (unsigned)sAlvoW * (unsigned)sAlvoH;
    exato[1] = c[1][0] == total && c[1][4] == total;
    exato[2] = c[2][1] == total || c[2][2] == total;
    Diario_Escrever("gs_d3d: profundidade: autoteste com %u pixels, %u do padrão não nulos; controle sem restauro: %u iguais "
                    "(tem de ser perto de 0); modo 1 (como cor): %u iguais, %u sem o byte baixo, %u sem o alto; modo 2 "
                    "(oDepth): %u iguais, %u sem o byte baixo, %u sem o alto",
                    total, ctl[3], ctl[0], c[1][0], c[1][1], c[1][2], c[2][0], c[2][1], c[2][2]);
    /* o tempo da GPU, um alvo inteiro (sAlvoW x sAlvoH) por chamada, 8 vezes */
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
    Diario_Escrever("gs_d3d: profundidade: GPU por alvo inteiro (%d x %d): resolve %.3f ms, restauro como cor %.3f ms, "
                    "pelo oDepth %.3f ms; a cor: resolve %.3f ms, restauro %.3f ms", sAlvoW, sAlvoH, sMsZRes, ms[1], ms[2],
                    sMsCorRes, sMsCorRest);
    Diario_Escrever("gs_d3d: profundidade: %s modo %d (%s)%s; EDRAM sem tile a mais (a cor vista na base da profundidade); "
                    "RAM: %u KB por alvo que escreve z, criada só para esses", forcado > 0 ? "forçado o" : "escolhido o", modo,
                    modo == 1 ? "restauro como cor" : modo == 2 ? "restauro pelo oDepth" : "desligada",
                    modo == 0 && forcado != 0 ? ": ERRO: nenhum modo exato, a profundidade volta zerada como antes" : "",
                    tam_alvo() >> 10);
    za.cor->BlockUntilNotBusy();
    zb.cor->BlockUntilNotBusy();
    za.tex->BlockUntilNotBusy(); /* (o Release do D3D esperava a GPU; o recurso do XG espera aqui) */
    zb.tex->BlockUntilNotBusy();
    xg_libera(za.tex);
    xg_libera(zb.tex);
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

/* (F5, etapa 8, 720p, etapas 2 e 3) A área dos alvos (BT3_ALVO_AREA=LxA, pixels do GS) e a escala (BT3_ESCALA: 1, 1.5 ou
   2), pelas regras do escala.h. Sem as chaves, 1024 x 1024 na escala 1, o de antes, sem linha no log; uma chave recusada
   diz o motivo e fica no padrão (a escala em 1: uma área que não cabe na EDRAM na escala pedida, e a de 1024 x 1024 com
   qualquer escala maior que 1). Um desenho além da área é cortado e contado (corte_conta), nunca desenhado num alvo
   maior: o par da EDRAM e as texturas têm um tamanho só. No começo do init, antes de qualquer alvo. */
static void area_init(void) {
    const char *a = getenv("BT3_ALVO_AREA"), *e = getenv("BT3_ESCALA"), *erroA = NULL, *erroE = NULL;
    int w = GS_W, h = GS_H, meios = 2;
    EscArea ar;
    if (e != NULL && *e != '\0') {
        meios = esc_le_escala(e);
        if (meios == 0) {
            Diario_Escrever("gs_d3d: BT3_ESCALA=%s recusada (1, 1.5 ou 2); fica 1", e);
            meios = 2;
        }
    }
    if (a != NULL && *a != '\0') {
        if (!esc_le_area(a, &w, &h)) {
            erroA = "não é LxA";
        } else if ((erroA = esc_monta(&ar, w, h, meios)) != NULL && meios != 2 && esc_monta(&ar, w, h, 2) == NULL) {
            erroE = erroA; /* (a área vale na escala 1: quem não cabe é a escala) */
            erroA = NULL;
        }
        if (erroA != NULL) {
            Diario_Escrever("gs_d3d: BT3_ALVO_AREA=%s recusada: %s; fica %dx%d", a, erroA, GS_W, GS_H);
            w = GS_W;
            h = GS_H;
        }
    }
    if (erroE == NULL && meios != 2) {
        erroE = esc_monta(&ar, w, h, meios); /* (sem a área, ou com ela recusada: 1024 x 1024, que nenhuma escala maior cabe) */
    }
    if (erroE != NULL) {
        Diario_Escrever("gs_d3d: BT3_ESCALA=%s recusada com a área de %dx%d: %s; fica 1", e, w, h, erroE);
        meios = 2;
    }
    if (esc_monta(&ar, w, h, meios) != NULL) { /* (o padrão sempre passa: o teste_escala.c prova) */
        ar.w = ar.sw = GS_W;
        ar.h = ar.sh = GS_H;
        ar.meios = 2;
        ar.swz = (GS_W + 79) / 80 * 80;
    }
    sAreaW = ar.w;
    sAreaH = ar.h;
    sMeios = ar.meios;
    sAlvoW = ar.sw;
    sAlvoH = ar.sh;
    sAlvoWZ = ar.swz;
    sIdsW = esc_px(IDS_W, sMeios);
    sIdsH = esc_px(IDS_H, sMeios);
    sTamAlvo = 0;
    sInteiro.x0 = sInteiro.y0 = 0;
    sInteiro.x1 = sAlvoW;
    sInteiro.y1 = sAlvoH;
    if (a != NULL && *a != '\0' && erroA == NULL) {
        Diario_Escrever("gs_d3d: área dos alvos %dx%d do GS (BT3_ALVO_AREA), escala %s: alvos e profundidades de %dx%d, a "
                        "profundidade vista como cor com %d de largura, a cópia dos ids de %dx%d; o que passar da área é "
                        "cortado e contado", sAreaW, sAreaH, sMeios == 2 ? "1" : sMeios == 3 ? "1,5" : "2", sAlvoW, sAlvoH,
                        sAlvoWZ, sIdsW, sIdsH);
    }
}

/* (720p, etapa 2) a posição no recorte dos vs_gs e vs_vu* (o c14 de gs_d3d.hlsl): 2 / largura e 2 / altura do alvo em pixels
   do GS. Só ela escreve o c14 dos vertex shaders; posta no init e no começo de cada quadro. (etapa 3) A área em pixels do
   GS, não o alvo em pixels de saída: a viewport (o alvo inteiro, sAlvoW x sAlvoH) leva a posição à escala. */
static void recorte_poe(void) {
    float c[4] = {2.0f / sAreaW, 2.0f / sAreaH, 0.0f, 0.0f};
    sDev->SetVertexShaderConstantF(14, c, 1);
}

/* Os ajustes da memória (o único lugar com os valores), os buffers fixos do gs_draw.c e uma calibração: o que o
   dwAvailPhys mede ao criar texturas pequenas e médias contra o que tam_tex conta (o alinhamento real); desde a etapa 2 do
   degrau 2, pelo mesmo caminho das texturas do jogo (xg_cria). */
static void mem_init(void) {
    static const UINT lado[2] = {8, 256};
    static const int quantas[2] = {64, 16};
    static TexXg *teste[64];
    int k, i;
    sMem.pisoMb = env_num("BT3_MEM_PISO_MB", 32);
    sMem.texTetoMb = env_num("BT3_MEM_TEX_MB", 128);
    sMem.folga = sMem.pisoMb * MB / 2;
    /* o BT3_GS_THREAD do degrau 1 grava e desenha na mesma thread: o descarte vale (o do PC, com a gravação num lado e o
       desenho no outro, é que não valia) */
    sMem.seguro = getenv("BT3_INTERP") == NULL || atoi(getenv("BT3_INTERP")) == 0;
    /* (com a divisão, os dois jogos de listas do gs_draw.c; sem ela, um) */
    sMem.buffers = (GsDraw_Dividido() ? 2u : 1u) *
                   (MAX_VERTS * (unsigned)sizeof(Vtx) + MAX_VU_VERTS * 48u + MAX_VU_IDX * 4u +
                    MAX_VU_UNIFORMS * (unsigned)sizeof(Vu0Uniform) + MAX_DRAWS * (unsigned)sizeof(GsDraw) +
                    MAX_DRAWS * 8u); /* (a faixa de índices de cada desenho da VU1, gs_draw.c) */
    sCorFundo = (DWORD)env_num("BT3_COR_FUNDO", 0xFF010101u);
    mem_mede();
    Diario_Escrever("gs_d3d: memória: piso %u MB livres (BT3_MEM_PISO_MB), teto das texturas %u MB (BT3_MEM_TEX_MB), "
                    "folga do descarte %u MB, descarte %s; buffers do gs_draw.c %u KB; alvo %u KB; %u MB livres de %u; "
                    "cor de fundo 0x%08X (BT3_COR_FUNDO)", sMem.pisoMb, sMem.texTetoMb, sMem.folga >> 20,
                    sMem.seguro ? "ligado" : "DESLIGADO (BT3_INTERP)", sMem.buffers >> 10,
                    tam_alvo() >> 10, sMem.livres >> 20, sMem.total >> 20, (unsigned)sCorFundo);
    for (k = 0; k < 2; k++) {
        unsigned antes = mem_mede(), depois, feitas = 0;
        for (i = 0; i < quantas[k]; i++) {
            teste[i] = xg_cria(lado[k], lado[k], D3DFMT_LIN_A8B8G8R8);
            feitas += teste[i] != NULL;
        }
        depois = mem_mede();
        for (i = 0; i < quantas[k]; i++) {
            xg_libera((D3DTexture *)teste[i]); /* (nunca usadas pela GPU; NULL não faz nada) */
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

static void nat_init(void);

/* (F5, etapa 8) Os tempos por passada (BT3_TEMPOS=0 desliga): o relógio (o mftb contra o QueryPerformanceCounter), o
   verificador das marcas (64 marcas seguidas têm de voltar escritas e em ordem; o controle, uma marca a mais que não
   foi posta, tem de ser recusado), o custo de uma marca na GPU e na CPU, e uma tarefa conhecida medida dos dois jeitos:
   a cópia de um alvo inteiro, 8 vezes, entre duas marcas e com a GPU parada antes e depois (prof_cronometra). */
static void tempo_init(void) {
    D3DVIEWPORT9 vp = {0, 0, sAlvoW, sAlvoH, 0.0f, 1.0f};
    LARGE_INTEGER q0, q1;
    unsigned __int64 m0, m1;
    double usGpu, usCpu, msMarcas, msParada;
    int ok, controle;
    unsigned k;
    sTempo.ligado = env_num("BT3_TEMPOS", 1) != 0;
    if (!sTempo.ligado) {
        Diario_Escrever("gs_d3d: tempos: desligados (BT3_TEMPOS=0)");
        return;
    }
    gGsD3dEtapa = "autoteste dos tempos";
    QueryPerformanceCounter(&q0);
    m0 = __mftb();
    Sleep(20);
    QueryPerformanceCounter(&q1);
    m1 = __mftb();
    sTempo.freq = (double)(m1 - m0) / ((double)(q1.QuadPart - q0.QuadPart) * 1000.0 / (double)sFreq.QuadPart);
    /* 64 marcas seguidas, sem nada entre elas */
    sDev->BlockUntilIdle();
    sTempo.buf = 0;
    sTempo.n[0] = 0;
    sTempo.quadro = 1;
    sTempo.cpu = T_OUTROS;
    sTempo.desde = __mftb();
    m0 = __mftb();
    for (k = 0; k < 64; k++) {
        tempo_marca(T_OUTROS, 0);
    }
    m1 = __mftb();
    sDev->BlockUntilIdle();
    __lwsync();
    sMarcaGpu[64] = 0; /* (o controle: a marca 64 não foi posta) */
    ok = sTempo.n[0] == 64 && tempo_valida(0, 64);
    controle = tempo_valida(0, 65);
    usGpu = ok ? (double)(sMarcaGpu[63] - sMarcaGpu[0]) / 63.0 / sTempo.freq * 1000.0 : 0.0;
    usCpu = (double)(m1 - m0) / 64.0 / sTempo.freq * 1000.0;
    /* a cópia de um alvo inteiro, 8 vezes, entre duas marcas e com a GPU parada (o autoteste da profundidade) */
    sDev->SetRenderTarget(0, sEdramCor);
    sDev->SetDepthStencilSurface(sEdramProf);
    sDev->SetViewport(&vp);
    sTempo.n[0] = 0;
    tempo_marca(T_TELA, 0);
    for (k = 0; k < 8; k++) {
        cr_correst(sDate);
    }
    tempo_marca(T_OUTROS, 0);
    sDev->BlockUntilIdle();
    __lwsync();
    msMarcas = tempo_valida(0, 2) ? ((double)(sMarcaGpu[1] - sMarcaGpu[0]) / sTempo.freq - usGpu / 1000.0) / 8.0 : -1.0;
    msParada = prof_cronometra(cr_correst, sDate, 8);
    sDev->SetTexture(0, NULL);
    sDev->SetRenderTarget(0, sTela);
    sDev->SetDepthStencilSurface(NULL);
    esquece_estado();
    sTempo.n[0] = sTempo.n[1] = 0;
    sTempo.estado[0] = sTempo.estado[1] = 0;
    sTempo.quadro = 0;
    sTempo.gpu = -1;
    memset(sTempo.cq, 0, sizeof(sTempo.cq));
    Diario_Escrever("gs_d3d: tempos: BT3_TEMPOS=1; relógio %.3f MHz (mftb contra o QueryPerformanceCounter de %.3f MHz); 64 "
                    "marcas seguidas %s, controle (uma marca a mais, não posta) %s; uma marca custa %.1f us de GPU e %.2f us "
                    "de CPU; a cópia de um alvo inteiro: %.3f ms pelas marcas, %.3f ms com a GPU parada",
                    sTempo.freq / 1000.0, (double)sFreq.QuadPart / 1e6, ok ? "escritas e em ordem" : "ERRADAS",
                    controle ? "ACEITO" : "recusado", usGpu, usCpu, msMarcas, msParada);
    if (!ok || controle || sTempo.freq <= 0.0) {
        Diario_Escrever("gs_d3d: ERRO: tempos: o autoteste das marcas não bateu; os tempos por passada ficam desligados");
        sTempo.ligado = 0;
    }
    gGsD3dEtapa = "antes do primeiro quadro";
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
    sDono = GetCurrentThreadId();
    /* (degrau 2, etapa 4) com BT3_GS_DESENHO=1 este init roda na thread de desenho (gs_draw.c decide antes) */
    sDiv = GsDraw_Dividido();
    sCovaEv = CreateEvent(NULL, FALSE, FALSE, NULL);
    Diario_Escrever("gs_d3d: o dispositivo é da thread %u (%s)", (unsigned)sDono,
                    sDiv ? "a de desenho, BT3_GS_DESENHO=1" : "a de gravação ou a do jogo");
    if (gsScale != 1) {
        Diario_Escrever("gs_d3d: SCALE %d pedido; o 360 usa 1 (a EDRAM)", gsScale);
    }
    gsScale = 1;
    area_init(); /* (720p, etapa 2) */
    if (sizeof(Vtx) != 28 || sizeof(GsDraw) != 128) { /* o mesmo layout dos dois lados (cl/stdint.h) */
        Diario_Escrever("gs_d3d: ERRO: Vtx %u bytes, GsDraw %u bytes", (unsigned)sizeof(Vtx), (unsigned)sizeof(GsDraw));
        return 0;
    }
    if (FAILED(sDev->CreateVertexShader((const DWORD *)kGsVs, &sVsGs)) || FAILED(sDev->CreatePixelShader((const DWORD *)kGsPs, &sPsGs)) ||
        FAILED(sDev->CreateVertexShader((const DWORD *)kCopiaVs, &sVsCopia)) ||
        FAILED(sDev->CreatePixelShader((const DWORD *)kCopiaPs, &sPsCopia)) || FAILED(sDev->CreateVertexDeclaration(elemGs, &sDeclGs)) ||
        FAILED(sDev->CreatePixelShader((const DWORD *)kZCorPs, &sPsZCor)) ||
        FAILED(sDev->CreatePixelShader((const DWORD *)kZProfPs, &sPsZProf)) ||
        FAILED(sDev->CreatePixelShader((const DWORD *)kGsZPs, &sPsGsZ)) ||
        FAILED(sDev->CreatePixelShader((const DWORD *)kTabelaPs, &sPsTabela)) ||
        FAILED(sDev->CreatePixelShader((const DWORD *)kContornoPs, &sPsContorno)) ||
        FAILED(sDev->CreatePixelShader((const DWORD *)kFilmePs, &sPsFilme)) ||
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
    tilesCor = XGSurfaceSize(sAlvoW, sAlvoH, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE);
    tilesProf = XGSurfaceSize(sAlvoW, sAlvoH, D3DFMT_D24S8, D3DMULTISAMPLE_NONE);
    if (tilesCor != esc_tiles(sAlvoW, sAlvoH) || tilesProf != esc_tiles(sAlvoW, sAlvoH)) {
        Diario_Escrever("gs_d3d: ERRO: o XG dá %u + %u tiles para %dx%d e a conta do escala.h %u", tilesCor, tilesProf, sAlvoW,
                        sAlvoH, esc_tiles(sAlvoW, sAlvoH));
    }
    memset(&sp, 0, sizeof(sp));
    sp.Base = 0;
    if (FAILED(sDev->CreateRenderTarget(sAlvoW, sAlvoH, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &sEdramCor, &sp))) {
        Diario_Escrever("gs_d3d: ERRO na cor da EDRAM");
        return 0;
    }
    sp.Base = tilesCor;
    sp.HierarchicalZBase = 0;
    if (FAILED(sDev->CreateDepthStencilSurface(sAlvoW, sAlvoH, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &sEdramProf, &sp))) {
        Diario_Escrever("gs_d3d: ERRO na profundidade da EDRAM");
        return 0;
    }
    /* (F5, etapa 7) a mesma EDRAM da profundidade vista como cor, para o restauro como cor (exemplo FastDepthRestore) */
    if (XGSurfaceSize(sAlvoWZ, sAlvoH, D3DFMT_A8B8G8R8, D3DMULTISAMPLE_NONE) != tilesProf ||
        FAILED(sDev->CreateRenderTarget(sAlvoWZ, sAlvoH, D3DFMT_A8B8G8R8, D3DMULTISAMPLE_NONE, 0, FALSE, &sEdramProfCor, &sp))) {
        Diario_Escrever("gs_d3d: ERRO na profundidade da EDRAM vista como cor (%ux%u, %u tiles contra %u)", sAlvoWZ, sAlvoH,
                        XGSurfaceSize(sAlvoWZ, sAlvoH, D3DFMT_A8B8G8R8, D3DMULTISAMPLE_NONE), tilesProf);
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
    recorte_poe();
    esquece_estado();
    if (tam_real(sAlvoW, sAlvoH, D3DFMT_D24S8) != tam_alvo()) {
        Diario_Escrever("gs_d3d: ERRO: a textura D24S8 tem %u KB e a contagem usa %u KB", tam_real(sAlvoW, sAlvoH, D3DFMT_D24S8) >> 10,
                        tam_alvo() >> 10);
    }
    prof_autoteste();
    nat_init(); /* (F5, etapa 7, segunda parte) */
    tempo_init(); /* (F5, etapa 8) */
    gPicos = env_num("BT3_PICOS", 0); /* (F5, etapa 8, picos) */
    if (gPicos != 0) {
        Diario_Escrever("gs_d3d: picos: o registro dos quadros lentos ligado, limiar %u ms (BT3_PICOS): uma linha por quadro "
                        "acima dele, dois quadros depois, e um resumo a cada 60", gPicos);
    }
    Diario_Escrever("gs_d3d: pronto; alvo %dx%d, EDRAM: cor %u + profundidade %u de %u tiles; %u MB livres", sAlvoW, sAlvoH,
                    tilesCor, tilesProf, (unsigned)EDRAM_TILES, mb_livres());
    if (tilesCor + tilesProf > EDRAM_TILES) {
        Diario_Escrever("gs_d3d: ERRO: o alvo não cabe na EDRAM");
        return 0;
    }
    QueryPerformanceCounter(&sUltimoFim);
    sVbHw = env_num("BT3_VBLANK_HW", 1) != 0; /* (F5, etapa 8, seção 18: a espera pelo vblank de hardware) */
    vblank_init(); /* (F5, etapa 8: a medida da tela e o vblank de hardware) */
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
   captura levava 260 ms (a primeira rodada no console).
   (720p, etapa 3) Na escala maior que 1, a captura sai reduzida a 640 x 448 (cada pixel do GS a média dos pixels de
   saída dele, esc_reduz), para o compara_quadros.py seguir comparando com a escala 1; BT3_SHOT_CHEIA=1 grava a imagem na
   resolução interna (960 x 672 ou 1280 x 896). Os buffers maiores são pedidos na primeira captura, só nessas escalas. */
static void captura(int alvo, D3DTexture *tex) {
    static int criou, cheia = -1, sw, sh;
    static unsigned linear1[640 * 448], *linear, *red;
    static unsigned char ppm1[16 + 640 * 448 * 3], *ppm;
    char cab[32], nome[64];
    HANDLE h;
    DWORD n;
    RECT ret;
    uint64_t fr = (sQ->frame[0] & 0x1FF) == sQ->targets[alvo].fbp ? sQ->frame[0] :
                  (sQ->frame[1] & 0x1FF) == sQ->targets[alvo].fbp ? sQ->frame[1] : (8ull << 16);
    int fbw = (int)((fr >> 16) & 0x3F), x, y, iw, ih, ifbw, k;
    const unsigned *fonte;
    unsigned char *o;
    LARGE_INTEGER t0;

    QueryPerformanceCounter(&t0);
    if (!criou) {
        CreateDirectory("game:\\shots", NULL);
        criou = 1;
    }
    if (cheia < 0) {
        cheia = sMeios != 2 && env_num("BT3_SHOT_CHEIA", 0) != 0;
        sw = esc_px(640, sMeios);
        sh = esc_px(448, sMeios);
        linear = linear1;
        ppm = ppm1;
        if (sMeios != 2) {
            linear = (unsigned *)malloc((size_t)sw * sh * 4);
            ppm = cheia ? (unsigned char *)malloc(16 + (size_t)sw * sh * 3) : ppm1;
            red = cheia ? linear : (unsigned *)malloc(640 * 448 * 4);
            if (linear == NULL || ppm == NULL || red == NULL) {
                linear = NULL;
                Diario_Escrever("gs_d3d: ERRO: sem memória para as capturas na escala (%dx%d); nenhuma captura", sw, sh);
            }
        }
    }
    if (linear == NULL || ppm == NULL) {
        return;
    }
    fbw = (fbw ? fbw : 10) * 64;
    if (fbw > 640) fbw = 640;
    /* textura presa no dispositivo fica ocupada para sempre (IsBusy): o LockRect esperaria sem fim */
    sDev->SetTexture(0, NULL);
    sDev->SetTexture(1, NULL);
    esquece_estado();
    sDev->BlockUntilIdle();
    /* (a textura do alvo é do XG: lida direto no endereço, com a GPU parada) */
    ret.left = 0; ret.top = 0; ret.right = esc_px(fbw, sMeios); ret.bottom = sh;
    XGUntileSurface(linear, sw * 4, NULL, ((const TexXg *)tex)->dados, sAlvoW, sAlvoH, &ret, 4);
    if (cheia) {
        fonte = linear;
        iw = sw;
        ih = sh;
        ifbw = esc_px(fbw, sMeios);
    } else {
        if (sMeios != 2) {
            esc_reduz(linear, sw, fbw, 448, sMeios, red, 640);
        }
        fonte = sMeios != 2 ? red : linear;
        iw = 640;
        ih = 448;
        ifbw = fbw;
    }
    k = _snprintf(cab, sizeof(cab), "P6\n%d %d\n255\n", iw, ih);
    memcpy(ppm, cab, k);
    o = ppm + k;
    for (y = 0; y < ih; y++) {
        for (x = 0; x < iw; x++, o += 3) {
            unsigned c = x < ifbw ? fonte[y * iw + x] : 0; /* A8R8G8B8 como u32 nativo */
            o[0] = (unsigned char)(c >> 16);
            o[1] = (unsigned char)(c >> 8);
            o[2] = (unsigned char)c;
        }
    }
    _snprintf(nome, sizeof(nome), "game:\\shots\\gpu_%05u.ppm", sQ->quadro);
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
/* (720p, etapa 2) um desenho cortado pela área dos alvos (nat: uma nativa): contado, e o primeiro dito no log */
static void corte_conta(const GsDraw *d, int nat) {
    if (nat) {
        sConta.cortadosNat++;
    } else {
        sConta.cortados++;
    }
    if (!(sAvisado & 1)) {
        sAvisado |= 1;
        Diario_Escrever("gs_d3d: um desenho passa da área dos alvos (%dx%d) e foi cortado: alvo %d, tesoura %dx%d+%d+%d%s (os "
                        "seguintes só contados)", sAreaW, sAreaH, d->target, d->scissor[2], d->scissor[3], d->scissor[0],
                        d->scissor[1], nat ? ", uma nativa" : "");
    }
}

/* (720p, etapa 2) um alvo lido como textura além da área: o maior uv dos vértices (normalizado pelo alvo lógico de 1024),
   preso no rect, contra a área. Só com a área menor que 1024 x 1024 (o desenho em pixels da VU1 não lê alvos). */
static void alvo_le_alem(const GsDraw *d) {
    float mu = 0.0f, mv = 0.0f;
    uint32_t k;
    if (d->vu) {
        return;
    }
    for (k = 0; k < d->count; k++) {
        const Vtx *v = &sQ->verts[d->first + k];
        float q = v->q != 0.0f ? v->q : 1.0f, u = v->s / q, w = v->t / q;
        mu = u > mu ? u : mu;
        mv = w > mv ? w : mv;
    }
    mu = mu < d->rect[2] ? mu : d->rect[2]; /* (o gs_pixel prende o uv no rect antes de ler) */
    mv = mv < d->rect[3] ? mv : d->rect[3];
    if (mu * GS_W > sAreaW + 0.5f || mv * GS_H > sAreaH + 0.5f) {
        sConta.lidosAlem++;
    }
}

static int prepara(const GsDraw *d, int estado, Ret *area) {
    float c[16];
    int x0 = d->scissor[0], y0 = d->scissor[1], x1 = d->scissor[0] + d->scissor[2], y1 = d->scissor[1] + d->scissor[3];
    int borda = 0;

    if (esc_prende(&x0, &y0, &x1, &y1, sAreaW, sAreaH) && d->count != 0) {
        corte_conta(d, 0);
    }
    if (x1 <= x0 || y1 <= y0 || d->count == 0) {
        return 0;
    }
    /* (720p, etapa 3) a tesoura em pixels de saída: a mesma regra nas duas bordas (esc_px) */
    x0 = esc_px(x0, sMeios);
    y0 = esc_px(y0, sMeios);
    x1 = esc_px(x1, sMeios);
    y1 = esc_px(y1, sMeios);
    /* (720p, etapa 2) um alvo lido como textura com a área menor que o alvo lógico: o que passar da área sai preto (a
       borda, o que antes era a parte limpa) e é contado */
    if (d->mode[0] == 2 && (sAreaW < GS_W || sAreaH < GS_H)) {
        borda = 0x100;
        alvo_le_alem(d);
    }
    if (sEstadoGs != estado) {
        sDev->SetVertexDeclaration(estado >= 2 ? sDeclVu : sDeclGs);
        sDev->SetVertexShader(estado >= 2 ? sVsVu[estado - 2] : sVsGs);
        /* (F5, etapa 7, segunda parte) a VU1 com o recorte pelo w: a profundidade por pixel (ps_gsz, vu_pos) */
        sDev->SetPixelShader(estado >= 2 && sCorte ? sPsGsZ : sPsGs);
        if (estado >= 2) {
            float c15[4] = {sCorte ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f};
            sDev->SetVertexShaderConstantF(15, c15, 1);
        }
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
    if ((d->sampler | borda) != sCurSamp) {
        aplica_sampler(0, d->sampler | borda);
        sCurSamp = d->sampler | borda;
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
    c[7] *= sMeios * 0.5f; /* (720p, etapa 3) misc.w da arte 2D: pixels de saída por pixel do GS (0 fica 0) */
    c[12] = 1.0f / sAlvoW; c[13] = 1.0f / sAlvoH; c[14] = sCurTexW; c[15] = sCurTexH;
    if (d->mode[0] == 2) {
        /* (720p, etapa 2) um alvo como textura: o uv do gs_draw.c é normalizado pelo alvo lógico (1024 x 1024), então
           a arte 2D conta os texels nele, e o c6 leva o uv à textura de verdade (1 com a área de 1024 x 1024) */
        c[14] = (float)GS_W;
        c[15] = (float)GS_H;
    }
    if (!sTemC || memcmp(c, sCurC, sizeof(c)) != 0) {
        sDev->SetPixelShaderConstantF(0, c, 4);
        memcpy(sCurC, c, sizeof(c));
        sTemC = 1;
    }
    {
        float e[4] = {1.0f, 1.0f, 0.0f, 0.0f};
        if (d->mode[0] == 2) {
            e[0] = (float)GS_W / sAreaW; /* (etapa 3: a textura cobre a área do GS, em qualquer escala) */
            e[1] = (float)GS_H / sAreaH;
        }
        if (e[0] != sCurEsc[0] || e[1] != sCurEsc[1]) {
            sDev->SetPixelShaderConstantF(6, e, 1); /* (só o gs_pixel lê o c6; ninguém mais o escreve) */
            sCurEsc[0] = e[0];
            sCurEsc[1] = e[1];
        }
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

static int fba_duas(const GsDraw *d);
static void chamadas(const GsDraw *d, uint32_t ini);
static void desenha_fba(const GsDraw *d, uint32_t ini);

/* um desenho preparado: as chamadas (duas passadas no FBA com mistura pelo alfa da fonte) e o que ele sujou */
static void desenha_comum(const GsDraw *d, uint32_t ini, const Ret *a) {
    tempo_poe(d->vu >= 1 && d->vu <= 3 ? T_VU0 + d->vu - 1 : T_COMUM);
    if (fba_duas(d)) {
        desenha_fba(d, ini);
    } else {
        chamadas(d, ini);
    }
    ret_une(&sSujo, a->x0, a->y0, a->x1, a->y1);
    if (sPipes[d->pipeline].zwrite && sPipes[d->pipeline].ztst != 0) {
        ret_une(&sSujoZ, a->x0, a->y0, a->x1, a->y1);
    }
    sConta.desenhos++;
    sConta.vertices += d->count;
    sConta.dateDes += d->misc[1] != 0.0f;
    sConta.fbaDes += d->misc[2] != 0.0f;
}

static void desenha(const GsDraw *d) {
    Ret a;

    if (!prepara(d, 1, &a)) {
        return;
    }
    desenha_comum(d, ~0u, &a);
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
        sDev->SetVertexShaderConstantF(VU_C0, (const float *)&sQ->vuUni[d->uniform], sizeof(Vu0Uniform) / 16);
        sCurUni = d->uniform;
    }
    desenha_comum(d, ini, &a);
    sConta.vuFeitos[d->vu - 1]++;
    sConta.vuTri[d->vu - 1] += d->count / 3;
}

/* F5, etapa 6: os vértices e os índices dos desenhos da VU1 do quadro (as três famílias) vão para um par de buffers da GPU, o
   que não foi usado no quadro anterior. Cada desenho leva só a faixa de vértices que os índices dele tocam, e os
   índices saem já com a posição nova (o desenho começa no índice sVuIni[n]); o que não cabe fica com ~0 e é contado.
   (degrau 2, etapa 4) A conta do que cabe (vu_plano) só depende da lista e da capacidade fixa do par: com a divisão ela é
   feita na entrega, na gravação (o desenho parado), e decide também para que alvos a gravação liga o seu cleared (um alvo
   cujos desenhos são todos da VU1 que não couberam não é desenhado); sem ela, no frame_end. A cópia (vu_copia) é do
   desenho. */
static uint32_t sVuCabem; /* (vu_plano) desenhos da VU1 que couberam */
static void vu_plano(const GsQuadro *q) {
    uint32_t n, f, lo, hi, nv = 0, ni = 0, pedV[3] = {0, 0, 0}, pedI[3] = {0, 0, 0};

    sVuCabem = 0;
    if (sVuCapV == 0) {
        for (n = 0; n < q->drawCount; n++) {
            sVuIni[n] = ~0u;
        }
        return;
    }
    /* (F5, etapa 8, gravação) o menor e o maior índice de cada desenho vêm da faixa que a gravação guardou ao gerar os índices
       (gs_draw.c, vu_faixa): uma passada só pelos desenhos, sem reler os índices (eram uns 129 mil por quadro de luta) */
    for (n = 0; n < q->drawCount; n++) {
        const GsDraw *d = &q->draws[n];
        sVuIni[n] = ~0u;
        if (d->vu < 1 || d->vu > 3 || d->native != 0 || d->count == 0) {
            continue;
        }
        f = (uint32_t)d->vu - 1;
        lo = q->vuFaixa[2 * n];
        hi = q->vuFaixa[2 * n + 1];
        pedV[f] += hi >= lo ? hi - lo + 1 : 0;
        pedI[f] += d->count;
        if (hi >= q->vuVertCount || hi - lo + 1 > sVuCapV - nv || d->count > sVuCapI - ni) {
            continue; /* sem espaço (ou índice fora do quadro): pulado e contado no frame_end */
        }
        sVuIni[n] = ni;
        sVuLo[n] = lo;
        sVuNv[n] = hi - lo + 1;
        nv += hi - lo + 1;
        ni += d->count;
        sVuCabem++;
    }
    for (f = 0; f < 3; f++) { /* o pedido, mesmo o que não coube */
        if (pedV[f] > sConta.vuMaxV[f]) sConta.vuMaxV[f] = pedV[f];
        if (pedI[f] > sConta.vuMaxI[f]) sConta.vuMaxI[f] = pedI[f];
    }
    if (pedV[0] + pedV[1] + pedV[2] > sConta.vuMaxT[0]) sConta.vuMaxT[0] = pedV[0] + pedV[1] + pedV[2];
    if (pedI[0] + pedI[1] + pedI[2] > sConta.vuMaxT[1]) sConta.vuMaxT[1] = pedI[0] + pedI[1] + pedI[2];
}

static void vu_copia(const GsQuadro *q) {
    uint32_t n, k, nv = 0;
    float *vb = NULL;
    uint32_t *ib = NULL;

    if (sVuCabem == 0) {
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
    if (FAILED(sVuVb[sVuPar]->Lock(0, 0, (void **)&vb, 0)) || FAILED(sVuIb[sVuPar]->Lock(0, 0, (void **)&ib, 0))) {
        if (vb != NULL) {
            sVuVb[sVuPar]->Unlock();
        }
        for (n = 0; n < q->drawCount; n++) {
            sVuIni[n] = ~0u; /* (nada copiado: os da VU1 ficam de fora, contados) */
        }
        return;
    }
    for (n = 0; n < q->drawCount; n++) {
        const GsDraw *d = &q->draws[n];
        uint32_t ini = sVuIni[n], lo = sVuLo[n];
        if (ini == ~0u) {
            continue;
        }
        memcpy(vb + nv * 12, q->vuVerts + lo * 12, sVuNv[n] * 48);
        for (k = 0; k < d->count; k++) {
            ib[ini + k] = q->vuIdx[d->first + k] - lo + nv;
        }
        nv += sVuNv[n];
    }
    sVuIb[sVuPar]->Unlock();
    sVuVb[sVuPar]->Unlock();
}

/* o máximo de um quadro em cada buffer fixo do gs_draw.c (no começo do frame_end, com a lista completa) */
static void mem_buffers(const GsQuadro *q) {
    if (q->vertCount > sMem.maxVerts) sMem.maxVerts = q->vertCount;
    if (q->vuVertCount > sMem.maxVu) sMem.maxVu = q->vuVertCount;
    if (q->vuIdxCount > sMem.maxIdx) sMem.maxIdx = q->vuIdxCount;
    if (q->vuUniCount > sMem.maxUni) sMem.maxUni = q->vuUniCount;
    if (q->drawCount > sMem.maxDraws) sMem.maxDraws = q->drawCount;
}

static void log_memoria_zera(void) {
    sMem.pressoes = sMem.descN = sMem.descBytes = sMem.esperas = sMem.acimaTeto = sMem.esperasDes = 0;
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
    Diario_Escrever("gs_d3d:   descarte: %u pressões, %u texturas (%u KB), %u esperas pela GPU, %u da gravação pelo desenho, %u acima "
                    "do teto no trecho; desde o "
                    "começo %u pressões, %u texturas (%.0f MB), %u esperas; brancas no trecho %u (sem espaço %u, cache cheio %u, criação "
                    "%u, formato %u, alvo sem textura %u), alvos sem textura agora %u",
                    sMem.pressoes, sMem.descN, sMem.descBytes >> 10, sMem.esperas, sMem.esperasDes, sMem.acimaTeto, sMem.pressoesTot, sMem.descNTot,
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
    for (k = 0; k < 8 && sVuDesc[k][1] != 0; k++) {
        n += _snprintf(desc + n, sizeof(desc) - n - 1, "%s%u instruções x%u", k ? ", " : "", sVuDesc[k][0],
                       sVuDesc[k][1]);
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
                    "%.2f), desenho %.2f ms (pior %.2f)", sVuLotes[0][0], sVuLotes[0][1], sVuLotes[0][2],
                    sVuLotes[1][0], sVuLotes[1][1], sVuLotes[1][2], sVuLotes[2][0], sVuLotes[2][1],
                    sVuLotes[2][2], sVuLotes[3][1], sVuLotes[3][2], desc[0] ? desc : "nenhum", sVuInstrDesc,
                    sConta.msVuGrav / t, sConta.msVuGravPior, sConta.msVuDes / t, sConta.msVuDesPior);
}

static void log_vu1_zera(void) {
    memset(sVuLotes, 0, sizeof(sVuLotes));
    memset(sVuDesc, 0, sizeof(sVuDesc));
    sVuInstrDesc = 0;
}

/* F5, etapa 7: a profundidade no trecho, por quadro: resolves e restauros com os pixels, as visitas que usaram z,
   a CPU das chamadas e a GPU estimada pelo autoteste (ms por pixel de um alvo inteiro), com a da cor ao lado */
static void log_prof(unsigned t) {
    double px = (double)sAlvoW * sAlvoH, gz = (sConta.zResPx * sMsZRes + sConta.zRestPx * sMsZRest) / px / t,
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

/* F5, etapa 7, segunda parte: as nativas feitas e puladas por motivo, as tabelas por tipo, o DATE, o FBA, o alfa alto
   e a extensão do que cada alvo tem desenhado (a sonda da seção 5 da especificação) */
static void log_nativas(unsigned t) {
    char tipos[256], ext[512];
    int i, n = 0, m = 0;
    tipos[0] = ext[0] = '\0';
    for (i = 0; i < sTabTipoN; i++) {
        int k = _snprintf(tipos + n, sizeof(tipos) - n - 1, "%s%s mistura 0x%03X máscara %X: %u", i ? "; " : "",
                          sTabTipo[i].src ? "ids" : "névoa", sTabTipo[i].chave & 0x1FF, (sTabTipo[i].chave >> 16) & 15,
                          sTabTipo[i].n);
        if (k < 0) {
            tipos[sizeof(tipos) - 1] = '\0';
            break;
        }
        n += k;
    }
    for (i = 0; i < sQ->targetCount; i++) {
        const Ret *c = &sAlvoD[i].conteudo;
        int k;
        if (c->x1 <= c->x0) {
            continue;
        }
        /* (720p, etapa 3) em pixels do GS, para comparar as escalas */
        k = _snprintf(ext + m, sizeof(ext) - m - 1, "%s%d: %dx%d+%d+%d", m ? ", " : "", i, esc_gs(c->x1 - c->x0, sMeios),
                      esc_gs(c->y1 - c->y0, sMeios), c->x0 * 2 / sMeios, c->y0 * 2 / sMeios);
        if (k < 0) {
            ext[sizeof(ext) - 1] = '\0';
            break;
        }
        m += k;
    }
    Diario_Escrever("gs_d3d:   nativas feitas: contorno %u, tabela %u, cópia dos ids %u (%.0f mil pixels de passada por quadro); "
                    "puladas: contorno %u, tabela %u (desligada %u, sem tabela %u, sem profundidade %u, alvo não limpo %u, sem "
                    "a cópia dos ids %u), cópia %u; tabelas com a cópia de outro quadro %u; sincronizações da profundidade %u; "
                    "BT3_NATIVOS=%u, BT3_FX_OFF=%u",
                    sConta.natCont, sConta.natTab, sConta.natIds, sConta.natPx / t / 1000.0, sConta.natContSem,
                    sConta.semNativo[3], sConta.natTabSem[0], sConta.natTabSem[1], sConta.natTabSem[2], sConta.natTabSem[3],
                    sConta.natTabSem[4], sConta.natIdsSem + sConta.semNativo[4], sConta.natIdsVelhos, sConta.natZSinc, sNativos,
                    gsFxOff);
    Diario_Escrever("gs_d3d:   tabelas por tipo: %s", tipos[0] ? tipos : "nenhuma");
    Diario_Escrever("gs_d3d:   DATE: %u marcadores (%u resolvidos, %u lidos da textura do alvo), %u desenhos; FBA: %u desenhos, %u "
                    "em duas passadas (%u sem escrita de alfa); alfa de vértice acima de 0x80 em %u de %u desenhos que "
                    "escrevem alfa (um quadro)", sConta.dateMarc, sConta.dateRes, sConta.dateTex, sConta.dateDes, sConta.fbaDes,
                    sConta.fbaDuas, sConta.fbaSemA, sConta.alfaAlto, sConta.alfaAltoDes);
    Diario_Escrever("gs_d3d:   extensão desenhada por alvo (largura x altura + x + y): %s; além da área de %dx%d: %u desenhos "
                    "e %u nativas cortados, %u leituras de alvo", ext[0] ? ext : "nenhum", sAreaW, sAreaH, sConta.cortados,
                    sConta.cortadosNat, sConta.lidosAlem);
    sTabTipoN = 0;
}

/* (F5, etapa 8) Os tempos por passada no trecho, por quadro (média e pior), em ms: a GPU por classe, sem o custo das
   marcas (o trecho vazio de cada quadro vezes os trechos da classe), a medição (esse custo somado) e o que não bateu
   entre a soma e o quadro da GPU (da primeira à última marca); o limite da espera da GPU pela CPU dentro das classes; a
   CPU do fim do quadro pelas mesmas classes, contra o fim do quadro medido pelo QueryPerformanceCounter. */
static void tempo_zera(void) {
    memset(sTempo.gSoma, 0, sizeof(sTempo.gSoma));
    memset(sTempo.gEsp, 0, sizeof(sTempo.gEsp));
    memset(sTempo.cSoma, 0, sizeof(sTempo.cSoma));
    memset(sTempo.gTrechos, 0, sizeof(sTempo.gTrechos));
    memset(sTempo.gPior, 0, sizeof(sTempo.gPior));
    memset(sTempo.cPior, 0, sizeof(sTempo.cPior));
    sTempo.quadroSoma = sTempo.esperaTiques = 0;
    sTempo.lidos = sTempo.descartados = sTempo.perdidos = sTempo.esperas = sTempo.cQuadros = 0;
    sTempo.quadroPior = sTempo.medPior = sTempo.cFim = 0.0;
}

static int tempo_junta(char *s, int n, int tam, const char *nome, double a, double b) {
    int k;
    if (n >= tam - 1) {
        return n;
    }
    k = _snprintf(s + n, tam - n - 1, "%s%s %.2f (%.2f)", n ? "; " : "", nome, a, b);
    if (k < 0) {
        s[tam - 1] = '\0';
        return tam - 1;
    }
    return n + k;
}

static void log_tempos(double msQuadro) {
    char lg[768], le[768], lc[768];
    int ng = 0, ne = 0, nc = 0, c;
    double q = (double)sTempo.lidos, f = sTempo.freq, vazio, soma = 0.0, esp = 0.0, med, tot, marcas = 0.0, cs = 0.0;
    if (!sTempo.ligado) {
        return;
    }
    lg[0] = le[0] = lc[0] = '\0';
    if (sTempo.lidos > 0) {
        vazio = (double)sTempo.gSoma[T_MED] / q; /* tiques: o custo de uma marca (um trecho vazio por quadro) */
        for (c = 0; c < T_GPU_N; c++) {
            double v = ((double)sTempo.gSoma[c] - (double)sTempo.gTrechos[c] * vazio) / q / f,
                   e = (double)sTempo.gEsp[c] / q / f;
            marcas += (double)sTempo.gTrechos[c] / q;
            if (c == T_MED) {
                continue;
            }
            soma += v;
            esp += e;
            ng = tempo_junta(lg, ng, sizeof(lg), kTempoNome[c], v, sTempo.gPior[c]);
            if (e >= 0.005 && ne < (int)sizeof(le) - 1) {
                int k = _snprintf(le + ne, sizeof(le) - ne - 1, "; %s %.2f", kTempoNome[c], e);
                ne = k < 0 ? (int)sizeof(le) - 1 : ne + k;
                le[sizeof(le) - 1] = '\0';
            }
        }
        med = vazio * marcas / f;
        tot = (double)sTempo.quadroSoma / q / f;
        Diario_Escrever("gs_d3d:   tempos, GPU por quadro (ms, média e pior): quadro %.2f (pior %.2f) de %.2f do quadro "
                        "inteiro, em %u quadros lidos (%u descartados por captura ou mais de %u marcas, %u com marca "
                        "perdida, %u esperas pela cerca, %.3f ms); medição %.2f (pior %.2f): %.1f marcas por quadro, %.1f "
                        "us cada; soma das classes %.2f; não bateu %.3f",
                        tot, sTempo.quadroPior, msQuadro, sTempo.lidos, sTempo.descartados, (unsigned)T_MARCAS,
                        sTempo.perdidos, sTempo.esperas, (double)sTempo.esperaTiques / f, med, sTempo.medPior, marcas + 1.0,
                        vazio / f * 1000.0, soma, tot - soma - med);
        Diario_Escrever("gs_d3d:   tempos, GPU por classe: %s", lg);
        Diario_Escrever("gs_d3d:   tempos, a GPU pode ter esperado pela CPU (limite, ms por quadro): %.2f%s", esp, le);
    } else {
        Diario_Escrever("gs_d3d:   tempos, GPU: nenhum quadro lido no trecho (%u descartados, %u com marca perdida)",
                        sTempo.descartados, sTempo.perdidos);
    }
    if (sTempo.cQuadros > 0) {
        double qc = (double)sTempo.cQuadros;
        for (c = 0; c < T_N; c++) {
            double v = (double)sTempo.cSoma[c] / qc / f;
            cs += v;
            nc = tempo_junta(lc, nc, sizeof(lc), kTempoNome[c], v, sTempo.cPior[c]);
        }
        Diario_Escrever("gs_d3d:   tempos, CPU do fim do quadro (ms, média e pior; %u quadros): fim do quadro %.2f, soma das "
                        "classes %.2f, não bateu %.3f; %s", sTempo.cQuadros, sTempo.cFim / qc, cs, sTempo.cFim / qc - cs, lc);
    }
    tempo_zera();
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
        sTabTipoN = 0;
        tempo_zera();
        return;
    }
    if (q == 0) {
        q = 1;
    }
    Diario_Escrever("gs_d3d: quadro %u: %u quadros sem captura: quadro inteiro %.2f ms (pior %.2f), dos quais fim do quadro "
                    "%.2f (pior %.2f) e nele o Present %.2f (pior %.2f); jogo e gravação %.2f; %u acima de 16,7 e %u acima "
                    "de 33,4 ms; %u capturas, %.1f ms cada", sQ->quadro, sConta.quadros, sConta.msQuadro / q, sConta.msQuadroPior,
                    sConta.msFim / q, sConta.msFimPior, sConta.msPresent / q, sConta.msPresentPior,
                    (sConta.msQuadro - sConta.msFim) / q, sConta.acima16, sConta.acima33, sConta.capturas,
                    sConta.capturas ? sConta.msCaptura / sConta.capturas : 0.0);
    Diario_Escrever("gs_d3d:   por quadro: %.1f desenhos, %.0f vértices, %.1f trocas de alvo; maior desenho %u vértices",
                    (double)sConta.desenhos / t, (double)sConta.vertices / t, (double)sConta.trocas / t, sConta.maiorDesenho);
    Diario_Escrever("gs_d3d:   %u texturas novas (%u KB de textura viva, %d no cache), %d alvos, %d pipelines; fora: %u VU1, "
                    "nativos %u/%u/%u/%u (contorno, chave, tabela, ids), %u imagens direto no quadro, %u texturas recusadas; "
                    "%u MB livres", sConta.texNovas, sMem.vivos[CL_TEX] >> 10, sQ->texCount, sQ->targetCount, sPipeCount, sConta.semVu,
                    sConta.semNativo[1], sConta.semNativo[2], sConta.semNativo[3], sConta.semNativo[4], sConta.semFb,
                    sConta.semTextura, mb_livres());
    log_vu1(t);
    log_vu1_zera();
    log_prof(t);
    log_nativas(t);
    log_tempos(sConta.msQuadro / q);
    log_memoria();
    if (sDiv) {
        GsDraw_DesenhoResumo(); /* gs_draw.c: a entrega (retratos, os que não couberam, o tempo) */
    }
    memset(&sConta, 0, sizeof(sConta));
}

/* (profundidade) se algum desenho da visita que começa em n, no alvo t, testa ou escreve z: só então a profundidade
   do alvo volta para a EDRAM (as trocas de alvo das passadas de tela, que não usam z, não pagam o restauro) */
static int usa_z(uint32_t n, int t) {
    if (sModoZ == 0) {
        return 0;
    }
    for (; n < sQ->drawCount; n++) {
        const GsDraw *d = &sQ->draws[n];
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

/* ---- as passadas nativas (F5, etapa 7, segunda parte; docs/x360/formatos/gs-nativos.md) ------------------------ */

/* a máscara de escrita do D3D a partir da do GS (GS_CC_*) */
static DWORD mascara(uint32_t wmask) {
    return ((wmask & GS_CC_R) ? D3DCOLORWRITEENABLE_RED : 0) | ((wmask & GS_CC_G) ? D3DCOLORWRITEENABLE_GREEN : 0) |
           ((wmask & GS_CC_B) ? D3DCOLORWRITEENABLE_BLUE : 0) | ((wmask & GS_CC_A) ? D3DCOLORWRITEENABLE_ALPHA : 0);
}

/* a tesoura de uma nativa, dentro do alvo; 0: vazia */
static int nat_ret(const GsDraw *d, Ret *r) {
    r->x0 = d->scissor[0];
    r->y0 = d->scissor[1];
    r->x1 = d->scissor[0] + d->scissor[2];
    r->y1 = d->scissor[1] + d->scissor[3];
    if (esc_prende(&r->x0, &r->y0, &r->x1, &r->y1, sAreaW, sAreaH)) {
        corte_conta(d, 1);
    }
    r->x0 = esc_px(r->x0, sMeios); /* (720p, etapa 3: em pixels de saída, a regra do prepara) */
    r->y0 = esc_px(r->y0, sMeios);
    r->x1 = esc_px(r->x1, sMeios);
    r->y1 = esc_px(r->y1, sMeios);
    return r->x1 > r->x0 && r->y1 > r->y0;
}

static void nat_textura(DWORD estagio, D3DTexture *t) {
    sDev->SetTexture(estagio, t);
    aplica_sampler(estagio, 2 | 4); /* no ponto, presa na borda */
}

/* Um retângulo do alvo na EDRAM pelo pixel shader ps (vs_copia; o shader lê pelo VPOS). A mistura e a escrita são do
   chamador; sem profundidade nem tesoura. Os estágios 2 a 4 saem presos a nada depois (as texturas deles são destino
   de Resolve). */
static void passada(D3DPixelShader *ps, const Ret *r) {
    struct { float x, y, u, v; } q[4];
    float cx0 = (float)r->x0 / sAlvoW * 2.0f - 1.0f, cx1 = (float)r->x1 / sAlvoW * 2.0f - 1.0f;
    float cy0 = 1.0f - (float)r->y0 / sAlvoH * 2.0f, cy1 = 1.0f - (float)r->y1 / sAlvoH * 2.0f;
    DWORD k;
    q[0].x = cx0; q[0].y = cy0; q[1].x = cx1; q[1].y = cy0; q[2].x = cx0; q[2].y = cy1; q[3].x = cx1; q[3].y = cy1;
    for (k = 0; k < 4; k++) {
        q[k].u = q[k].v = 0.0f;
    }
    sDev->SetVertexDeclaration(sDeclCopia);
    sDev->SetVertexShader(sVsCopia);
    sDev->SetPixelShader(ps);
    sDev->SetRenderState(D3DRS_ZENABLE, FALSE);
    sDev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    sDev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    sDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(q[0]));
    for (k = 2; k <= 4; k++) {
        sDev->SetTexture(k, NULL);
    }
    ret_une(&sSujo, r->x0, r->y0, r->x1, r->y1);
    sConta.natPx += (double)(r->x1 - r->x0) * (r->y1 - r->y0);
    esquece_estado();
}

/* a passada de tabela em si: estágio 2 a profundidade (src 0), 3 a cópia dos ids, 4 a tabela */
static void nat_tabela_passa(int src, float zmax, D3DTexture *z, D3DTexture *clut, const Ret *r) {
    float c[8] = {(float)src, zmax, 0.0f, 0.0f, 1.0f / sIdsW, 1.0f / sIdsH, 1.0f / sAlvoW, 1.0f / sAlvoH};
    nat_textura(2, z != NULL ? z : sBranco);
    nat_textura(3, sIds);
    nat_textura(4, clut);
    sDev->SetPixelShaderConstantF(4, c, 2);
    passada(sPsTabela, r);
}

/* o contorno em si: tira 0x64 da cor (REVSUBTRACT, ONE, ONE) nas arestas dos ids, o alfa intocado. (720p, etapa 3) passo:
   o vizinho em texels da cópia dos ids, um pixel do GS (a escala; o outline.frag do PC anda em pixels do PS2, e a linha
   fica com a grossura do PS2); o autoteste passa 1, o pixel ao lado do padrão dele */
static void nat_contorno_passa(const Ret *r, float passo) {
    float c[8] = {0.0f, 0.0f, 100.0f / 255.0f, passo, 1.0f / sIdsW, 1.0f / sIdsH, 1.0f / sAlvoW, 1.0f / sAlvoH};
    sDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    sDev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    sDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    sDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
    sDev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT);
    sDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
    nat_textura(3, sIds);
    sDev->SetPixelShaderConstantF(4, c, 2);
    passada(sPsContorno, r);
}

/* o alvo de uma nativa na EDRAM (com a profundidade se os desenhos seguintes da visita a usam) */
static void nat_entra(int t, uint32_t n, int *cur) {
    if (*cur != t || sResidente != t) {
        entra_na_edram(t, usa_z(n, t));
        *cur = t;
    }
}

/* ---- o filme direto (F6, etapa 6, BT3_FILME=1; port/x360/src/f2/filme360.c, docs/x360/plano/F6/06.md, seção 5) ------

   Quatro vagas, cada uma uma textura D3DFMT_LIN_L8 de 512 x 672 pelo XG (Y nas linhas 0 a 447; Cb nas colunas 0 a 255 e
   Cr nas 256 a 511 das linhas 448 a 671), 336 KB, criadas na primeira imagem de um filme e soltas depois do fim dele.
   A imagem n vai para a vaga n % 4. A thread do jogo (GsD3d_FilmeVaga) espera a vaga ficar livre, escreve o número, a
   marca ocupada e copia os planos; a gravação leva o número no desenho do quadro (GsGpu_FbUpload, `first`); esta
   thread (a dona do dispositivo) desenha a vaga no alvo pelo ps_filme e, no fim do quadro, guarda a cerca dela. A vaga
   fica livre quando a cerca passou (a GPU leu), ou quando uma imagem mais nova já foi desenhada e ela não (uma imagem
   pulada nunca vai ser desenhada: a lista anda em ordem). O jogo nunca pergunta à GPU (IsBusy é da dona). Quatro vagas
   são quatro imagens de folga, oito campos: o desenho anda no máximo dois quadros atrás do jogo. Uma vaga ainda ocupada
   depois de 100 ms é escrita assim mesmo (contada: a imagem pode sair trocada, nunca trava). */
#define FILME_VAGAS 4
#define FILME_W 512
#define FILME_H 448
static TexXg *sFilmeTex[FILME_VAGAS];
static volatile unsigned sFilmeImagemDa[FILME_VAGAS]; /* (jogo) o número da imagem na vaga */
static volatile LONG sFilmeOcupada[FILME_VAGAS];      /* (jogo põe 1, desenho põe 0) escrita e a GPU ainda não a leu */
static unsigned sFilmeDesenhadaDa[FILME_VAGAS];       /* (desenho) o número que ele desenhou dela */
static DWORD sFilmeCerca[FILME_VAGAS];                /* (desenho) a cerca do quadro que a desenhou */
static int sFilmeNoQuadro[FILME_VAGAS];               /* (desenho) desenhada no quadro atual: recebe a cerca do fim */
static unsigned sFilmeUltima;                         /* (desenho) o maior número desenhado */
/* 0 sem vagas; 1 vivas; 2 o filme fechou (o jogo pediu para soltar); 3 o desenho está soltando */
static volatile LONG sFilmeEstado;
static unsigned sFilmeSoltarQuadros, sFilmeDes, sFilmeTrocadas, sFilmePuladas; /* (desenho) */
static unsigned sFilmeEsperas, sFilmeForcadas, sFilmeFalhou;                    /* (jogo) */

/* (thread do jogo) a vaga da imagem `imagem`, livre, para os planos; 0 sem memória (a imagem não aparece) */
extern "C" int GsD3d_FilmeVaga(unsigned imagem, unsigned char **dados, unsigned *pitch, double *msEspera) {
    int k;
    *msEspera = 0.0;
    if (sDev == NULL) {
        return 0; /* (sem o backend da GPU ninguém desenharia nem soltaria as vagas) */
    }
    for (;;) {
        LONG e = sFilmeEstado;
        if (e == 1) {
            break;
        }
        if (e == 2) {
            if (InterlockedCompareExchange(&sFilmeEstado, 1, 2) == 2) {
                break; /* (outro filme antes de o desenho soltar: as vagas ficam) */
            }
            continue;
        }
        if (e == 3) {
            Sleep(1); /* (o desenho está soltando as do filme anterior) */
            continue;
        }
        if (sFilmeFalhou) {
            return 0;
        }
        for (k = 0; k < FILME_VAGAS; k++) {
            sFilmeTex[k] = xg_cria(FILME_W, FILME_H * 3 / 2, D3DFMT_LIN_L8);
            if (sFilmeTex[k] == NULL) {
                while (k-- > 0) {
                    xg_libera(&sFilmeTex[k]->cab);
                    sFilmeTex[k] = NULL;
                }
                sFilmeFalhou = 1;
                Diario_Escrever("gs_d3d: filme: SEM MEMÓRIA para as %d vagas: o filme segue sem imagem", FILME_VAGAS);
                return 0;
            }
            sFilmeImagemDa[k] = 0;
            sFilmeOcupada[k] = 0;
            sFilmeDesenhadaDa[k] = 0;
            sFilmeNoQuadro[k] = 0;
        }
        {
            MEMORYSTATUS m;
            GlobalMemoryStatus(&m);
            Diario_Escrever("gs_d3d: filme: %d vagas L8 de %u x %u (pitch %u, %u KB cada); %u KB livres", FILME_VAGAS, FILME_W,
                            FILME_H * 3 / 2, sFilmeTex[0]->pitch, sFilmeTex[0]->bytes >> 10, (unsigned)(m.dwAvailPhys >> 10));
        }
        sFilmeEsperas = sFilmeForcadas = 0;
        InterlockedExchange(&sFilmeEstado, 1);
        break;
    }
    k = (int)(imagem % FILME_VAGAS);
    if (sFilmeOcupada[k]) {
        LARGE_INTEGER t0;
        QueryPerformanceCounter(&t0);
        sFilmeEsperas++;
        while (sFilmeOcupada[k]) {
            if (ms_desde(&t0) > 100.0) {
                sFilmeForcadas++;
                break;
            }
            Sleep(1);
        }
        *msEspera = ms_desde(&t0);
    }
    sFilmeImagemDa[k] = imagem;
    __lwsync(); /* (o número antes da marca: quem vê a vaga ocupada vê o número novo) */
    sFilmeOcupada[k] = 1;
    *dados = (unsigned char *)sFilmeTex[k]->dados;
    *pitch = sFilmeTex[k]->pitch;
    return 1;
}

/* (thread do jogo) o filme fechou: o desenho solta as vagas quando a GPU terminar com elas */
extern "C" void GsD3d_FilmeFim(void) {
    sFilmeFalhou = 0;
    sFilmeSoltarQuadros = 0;
    InterlockedCompareExchange(&sFilmeEstado, 2, 1);
}

/* native 100/101 com o número de uma imagem do filme: a vaga no alvo, de (0, 0) a (512, 448) do GS, no lugar em que o PC
   copia a memória de vídeo (depois da limpeza, antes do fade). Sem mistura, sem profundidade, alfa 1 (0x80 ou mais). */
static void filme_desenha(const GsDraw *d, uint32_t n, int *cur) {
    unsigned im = d->first;
    int k = (int)(im % FILME_VAGAS), w = esc_px(FILME_W, sMeios), h = esc_px(FILME_H, sMeios);
    TexXg *x = sFilmeTex[k];
    if (x == NULL) {
        sConta.semFb++;
        return;
    }
    if (sFilmeImagemDa[k] != im) {
        sFilmeTrocadas++; /* (o jogo já pôs outra: a vaga voltou cedo demais; desenhada assim mesmo) */
    }
    nat_entra(d->target, n, cur);
    sQ->targets[d->target].cleared = 1;
    tempo_poe(T_FILME);
    sDev->SetRenderState(D3DRS_ZENABLE, FALSE);
    sDev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    sDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);
    quad(&x->cab, sPsFilme, sAlvoW, sAlvoH, 0.0f, 0.0f, (float)w, (float)h, 0.0f, 0.0f, 1.0f, 1.0f, 0);
    sDev->SetTexture(0, NULL);
    sCurTex = (GsTex)-1;
    ret_une(&sSujo, 0, 0, w, h);
    sFilmeDesenhadaDa[k] = im;
    sFilmeNoQuadro[k] = 1;
    if (im > sFilmeUltima) {
        sFilmeUltima = im;
    }
    sFilmeDes++;
}

/* (desenho, no fim do quadro, com a cerca dele) as vagas desenhadas recebem a cerca; as que a GPU já leu e as puladas
   ficam livres; depois do fim do filme, todas soltas quando livres (ou oito quadros depois, esperando a GPU) */
static void filme_varre(DWORD cerca) {
    int k, livres = 0;
    LONG e = sFilmeEstado;
    if (e != 1 && e != 2) {
        return;
    }
    for (k = 0; k < FILME_VAGAS; k++) {
        if (sFilmeNoQuadro[k]) {
            sFilmeCerca[k] = cerca;
            sFilmeNoQuadro[k] = 0;
        }
        if (sFilmeOcupada[k]) {
            unsigned im;
            __lwsync(); /* (a marca antes do número: ver GsD3d_FilmeVaga) */
            im = sFilmeImagemDa[k];
            if (sFilmeDesenhadaDa[k] == im) {
                if (!sDev->IsFencePending(sFilmeCerca[k])) {
                    sFilmeOcupada[k] = 0;
                }
            } else if (im < sFilmeUltima) {
                sFilmePuladas++;
                sFilmeOcupada[k] = 0;
            }
        }
        livres += !sFilmeOcupada[k];
    }
    if (e == 2 && (livres == FILME_VAGAS || ++sFilmeSoltarQuadros >= 8) &&
        InterlockedCompareExchange(&sFilmeEstado, 3, 2) == 2) {
        sDev->SetTexture(0, NULL);
        esquece_estado();
        for (k = 0; k < FILME_VAGAS; k++) {
            if (sFilmeTex[k] != NULL) {
                xg_espera(&sFilmeTex[k]->cab);
                xg_libera(&sFilmeTex[k]->cab);
                sFilmeTex[k] = NULL;
            }
        }
        Diario_Escrever("gs_d3d: filme: %u imagens desenhadas, %u trocadas, %u puladas; o jogo esperou a vaga %u vezes (%u "
                        "forçadas); as vagas soltas (%s)", sFilmeDes, sFilmeTrocadas, sFilmePuladas, sFilmeEsperas,
                        sFilmeForcadas, livres == FILME_VAGAS ? "todas livres" : "oito quadros depois do fim");
        sFilmeDes = sFilmeTrocadas = sFilmePuladas = 0;
        InterlockedExchange(&sFilmeEstado, 0);
    }
}

/* native 4 (GfxPost_CopyAlphaToDepth): o alfa do alvo agora, para o contorno e as tabelas pelos ids. A imagem do jogo
   inteira (fora do que já foi desenhado, a EDRAM está limpa: zero, como no PC) */
static void nat_ids(const GsDraw *d, uint32_t n, int *cur) {
    D3DRECT fonte = {0, 0, sIdsW, sIdsH};
    D3DPOINT destino = {0, 0};
    if (!(sNativos & 3) || sIds == NULL) {
        sConta.semNativo[4]++;
        return;
    }
    if (!sQ->targets[d->target].cleared) {
        sConta.natIdsSem++; /* (o PC pula igual) */
        return;
    }
    nat_entra(d->target, n, cur);
    tempo_poe(T_IDS);
    sDev->SetTexture(3, NULL);
    sDev->Resolve(D3DRESOLVE_RENDERTARGET0, &fonte, sIds, &destino, 0, 0, NULL, 0.0f, 0, NULL);
    sIdsQuadro = sQ->quadro + 1;
    sConta.natIds++;
}

/* native 3: o byte (névoa pela profundidade, ou os ids) pela tabela do jogo, com a mistura e a máscara da chave */
static void nat_tabela(const GsDraw *d, uint32_t n, int *cur) {
    int t = d->target, motivo, i;
    Ret r;
    uint32_t chave = sPipes[d->pipeline].key & (0x1FFu | (15u << 16));
    motivo = !(sNativos & (d->src ? 2 : 4)) ? 0 : d->tex == 0 ? 1 : d->src == 0 && (sModoZ == 0 || sProfD[t].tex == NULL) ? 2 :
             !sQ->targets[t].cleared ? 3 : d->src != 0 && sIdsQuadro == 0 ? 4 : -1;
    if (motivo >= 0) {
        sConta.semNativo[3]++;
        sConta.natTabSem[motivo]++;
        return;
    }
    if (!nat_ret(d, &r)) {
        return;
    }
    sConta.natIdsVelhos += d->src != 0 && sIdsQuadro != sQ->quadro + 1;
    nat_entra(t, n, cur);
    if (d->src == 0 && sZDono == t) {
        prof_sincroniza(); /* (a textura de profundidade em dia, a posse fica) */
    }
    tempo_poe(d->src ? T_TAB_IDS : T_TAB_PROF);
    aplica_pipe(d->pipeline); /* a mistura e a máscara do jogo; a chave de tabela desliga a profundidade */
    {
        int b = (int)(d->blendc * 255.0f + 0.5f);
        b = b < 0 ? 0 : b > 255 ? 255 : b;
        sDev->SetRenderState(D3DRS_BLENDFACTOR, D3DCOLOR_ARGB(b, b, b, b));
    }
    nat_tabela_passa(d->src, d->misc[0], d->src == 0 ? sProfD[t].tex : NULL, (D3DTexture *)d->tex, &r);
    sConta.natTab++;
    for (i = 0; i < sTabTipoN && (sTabTipo[i].chave != chave || sTabTipo[i].src != d->src); i++) {
    }
    if (i < sTabTipoN) {
        sTabTipo[i].n++;
    } else if (sTabTipoN < 8) {
        sTabTipo[sTabTipoN].chave = chave;
        sTabTipo[sTabTipoN].src = d->src;
        sTabTipo[sTabTipoN].n = 1;
        sTabTipoN++;
    }
}

/* native 1 (ObjOutline_Draw): o contorno, lendo a cópia dos ids */
static void nat_contorno(const GsDraw *d, uint32_t n, int *cur) {
    Ret r;
    if (!(sNativos & 1) || !sQ->targets[d->target].cleared || sIdsQuadro == 0) {
        sConta.semNativo[1]++;
        sConta.natContSem++;
        return;
    }
    if (!nat_ret(d, &r)) {
        return;
    }
    nat_entra(d->target, n, cur);
    tempo_poe(T_CONTORNO);
    nat_contorno_passa(&r, sMeios * 0.5f);
    sConta.natCont++;
}

/* (F5, etapa 7, segunda parte) FBA num desenho que mistura pelo alfa da fonte. No PC o FBA liga o bit 7 só no byte à
   parte e o fator da mistura é o alfa calculado; aqui o alfa gravado é o mesmo da mistura, e o FBA a deixava opaca
   (os grupos transparentes do estágio). 1: duas passadas (desenha_fba). */
static int fba_duas(const GsDraw *d) {
    const Pipe *p = &sPipes[d->pipeline];
    int k0 = (int)((p->bkey >> 1) & 7), k1 = (int)((p->bkey >> 4) & 7);
    return sFba && d->misc[2] != 0.0f && (p->bkey & 1) && ((p->bkey >> 7) & 3) == 0 &&
           ((k0 >= 2 && k0 <= 4) || (k1 >= 2 && k1 <= 4));
}

/* as chamadas de um desenho já preparado: da VU1 (ini, no par do quadro) ou primitivas do GS (ini ~0) */
static void chamadas(const GsDraw *d, uint32_t ini) {
    const Pipe *p = &sPipes[d->pipeline];
    D3DPRIMITIVETYPE tipo = p->topo == 0 ? D3DPT_TRIANGLELIST : p->topo == 1 ? D3DPT_LINELIST : D3DPT_POINTLIST;
    uint32_t porPrim = p->topo == 0 ? 3 : p->topo == 1 ? 2 : 1, feitos = 0;
    if (d->vu) {
        sDev->DrawIndexedVertices(D3DPT_TRIANGLELIST, 0, ini, d->count);
        return;
    }
    while (feitos + porPrim <= d->count) {
        uint32_t n = d->count - feitos > MAX_UP ? MAX_UP : d->count - feitos;
        n -= n % porPrim;
        sDev->DrawPrimitiveUP(tipo, n / porPrim, &sQ->verts[d->first + feitos], sizeof(Vtx));
        feitos += n;
    }
}

/* As duas passadas do FBA com mistura pelo alfa da fonte: a cor (só RGB) com o FBA desligado no shader e a mistura do
   jogo; depois só o alfa, sem mistura, com o FBA (alfa 1, o bit 7) e os mesmos testes de alfa, de DATE e de
   profundidade (sem escrever z; GREATER com escrita de z vira GEQUAL, para a segunda achar o z que a primeira gravou). */
static void desenha_fba(const GsDraw *d, uint32_t ini) {
    const Pipe *p = &sPipes[d->pipeline];
    float m[4];
    memcpy(m, d->misc, sizeof(m));
    m[2] = 0.0f;
    sDev->SetPixelShaderConstantF(1, m, 1);
    sDev->SetRenderState(D3DRS_COLORWRITEENABLE, mascara(p->wmask & ~GS_CC_A));
    chamadas(d, ini);
    if (p->wmask & GS_CC_A) {
        tempo_poe(T_FBA);
        m[2] = 1.0f;
        sDev->SetPixelShaderConstantF(1, m, 1);
        sDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA);
        sDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        sDev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        if (p->ztst == 3 && p->zwrite) {
            sDev->SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATEREQUAL);
        }
        chamadas(d, ini);
    } else {
        sConta.fbaSemA++;
    }
    sCurPipe = -1; /* (o estado do pipeline e as constantes voltam no próximo prepara) */
    sTemC = 0;
    sConta.fbaDuas++;
}

/* Num quadro por trecho: os desenhos que escrevem alfa e quantos deles têm alfa de vértice acima de 0x80 (o que o alfa
   saturado da cor perde; o tamanho do byte exato, seção 4 da especificação). Primitivas do GS: o alfa de cada
   vértice; VU1: o color0 do bloco, ou o alfa de cada vértice nas camadas 3 e 4 (programas 8 e 7). */
static void alfa_alto(void) {
    const GsQuadro *q = sQ;
    uint32_t n, k;
    for (n = 0; n < q->drawCount; n++) {
        const GsDraw *d = &q->draws[n];
        int alto = 0;
        if (d->native != 0 || d->count == 0 || !(sPipes[d->pipeline].wmask & GS_CC_A)) {
            continue;
        }
        if (d->vu) {
            const Vu0Uniform *u = &q->vuUni[d->uniform];
            alto = u->color0[3] > 128.0f;
            if (u->misc[3] == 3.0f) {
                for (k = 0; k < d->count && !alto; k++) {
                    uint32_t v = q->vuIdx[d->first + k];
                    alto = v < q->vuVertCount && q->vuVerts[v * 12 + 7] > 128.0f;
                }
            }
        } else {
            for (k = 0; k < d->count && !alto; k++) {
                alto = q->verts[d->first + k].a > 0x80;
            }
        }
        sConta.alfaAltoDes++;
        sConta.alfaAlto += alto;
    }
}

/* ---- as nativas: o autoteste do boot e os ajustes (F5, etapa 7, segunda parte) ------------------------------- */

/* as contas do lado da CPU, iguais às do gs_d3d.hlsl (nat_id, nat_contorno_tab) e às do PC (outline.frag, dclut.frag) */
static unsigned at_id(unsigned a8) { return a8 == 255 ? 255 : (a8 * 256 + 255) / 510; }
static unsigned at_cont(unsigned id) {
    unsigned c = (id << 3) & 0xFF;
    return id == 255 ? 0 : c == 0 ? 0x80 : c;
}
static unsigned at_alfa(unsigned a) { return a >= 128 ? 255 : (a * 510 + 128) / 256; } /* round(min(A / 128, 1) * 255) */
static int at_perto(unsigned a, unsigned b) { return a == b || a + 1 == b || b + 1 == a; }

/* textura linear com os texels dados (u32 R | G<<8 | B<<16 | A<<24, como as do jogo) */
static D3DTexture *at_lin(UINT w, UINT h, const unsigned *px) {
    D3DTexture *t = NULL;
    D3DLOCKED_RECT r;
    UINT y;
    if (FAILED(sDev->CreateTexture(w, h, 1, 0, D3DFMT_LIN_A8B8G8R8, D3DPOOL_DEFAULT, &t, NULL)) || t == NULL) {
        return NULL;
    }
    if (SUCCEEDED(t->LockRect(0, &r, NULL, 0))) {
        for (y = 0; y < h; y++) {
            memcpy((BYTE *)r.pBits + y * r.Pitch, px + y * w, w * 4);
        }
        t->UnlockRect(0);
    }
    return t;
}

/* os 256 x 256 do canto de uma textura tiled de lado `lado` (32 bits por texel), em ordem linear */
static int at_le(D3DTexture *t, UINT w, UINT h, unsigned *lin) {
    D3DLOCKED_RECT r;
    RECT ret = {0, 0, 256, 256};
    sDev->BlockUntilIdle();
    if (FAILED(t->LockRect(0, &r, NULL, D3DLOCK_READONLY))) {
        return 0;
    }
    XGUntileSurface(lin, 256 * 4, NULL, r.pBits, w, h, &ret, 4);
    t->UnlockRect(0);
    return 1;
}

/* o retângulo 256 x 256 do canto da EDRAM para a textura t */
static void at_resolve(D3DTexture *t) {
    D3DRECT fonte = {0, 0, 256, 256};
    D3DPOINT destino = {0, 0};
    sDev->Resolve(D3DRESOLVE_RENDERTARGET0, &fonte, t, &destino, 0, 0, NULL, 0.0f, 0, NULL);
}

/* Antes do primeiro quadro, como o da profundidade: um padrão de ids conhecido (blocos de 8 x 8 com todos os 256
   valores de alfa) entra na EDRAM e vai para a cópia dos ids pelo mesmo Resolve do native 4; as passadas rodam sobre
   ele e o resultado é comparado pixel a pixel com as contas da CPU. Cada uma tem um controle que TEM de dar diferente:
   a tabela trocada (cada índice com a cor do seguinte), o contorno sem a passada (só os pixels sem aresta batem), e
   para a profundidade quatro leituras da palavra da D24S8 (só a de cima, depth nos 24 bits altos, é a certa). Uma
   passada que não bate (menos de 99%) é desligada com ERRO, a não ser que o BT3_NATIVOS a force. */
static void nat_autoteste(int forcado) {
    static unsigned pad[256 * 256], c1[256], c2[256], lin[256 * 256], zl[256 * 256];
    D3DVIEWPORT9 vp = {0, 0, sAlvoW, sAlvoH, 0.0f, 1.0f}, vpz = {0, 0, sAlvoWZ, sAlvoH, 0.0f, 1.0f};
    D3DTexture *tp, *t1, *t2, *res = NULL, *zt = NULL;
    Ret r = {0, 0, 256, 256};
    unsigned x = 54321u, i, k, h, tab[2][2], cont[3] = {0, 0, 0}, zc[4][2], total = 256 * 256, inter = 254 * 254;
    int okTab, okCont, okZ = -1;

    memset(tab, 0, sizeof(tab));
    memset(zc, 0, sizeof(zc));
    for (i = 0; i < 256; i++) {
        c1[i] = i | (((i * 7 + 3) & 255) << 8) | ((255 - i) << 16) | (((i * 5 + 1) & 255) << 24);
    }
    for (i = 0; i < 256; i++) {
        c2[i] = c1[(i + 1) & 255];
    }
    for (i = 0; i < total; i++) {
        unsigned b = (i / 256 / 8) * 32 + (i % 256) / 8;
        x = x * 1664525u + 1013904223u;
        pad[i] = ((x >> 8) & 0x00FFFFFFu) | (((b * 37) & 255) << 24);
    }
    tp = at_lin(256, 256, pad);
    t1 = at_lin(256, 1, c1);
    t2 = at_lin(256, 1, c2);
    if (tp == NULL || t1 == NULL || t2 == NULL ||
        FAILED(sDev->CreateTexture(256, 256, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &res, NULL)) || res == NULL) {
        Diario_Escrever("gs_d3d: ERRO: nativas: as texturas do autoteste não foram criadas; as nativas ficam desligadas");
        sNativos = 0;
        if (tp) tp->Release();
        if (t1) t1->Release();
        if (t2) t2->Release();
        return;
    }
    gGsD3dEtapa = "autoteste das nativas";
    sDev->SetRenderTarget(0, sEdramCor);
    sDev->SetDepthStencilSurface(sEdramProf);
    sDev->SetViewport(&vp);
    sDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);
    sDev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 0.0f, 0);
    copia(tp, sAlvoW, sAlvoH, 0.0f, 0.0f, 256.0f, 256.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0);
    sDev->SetTexture(0, NULL);
    {
        D3DRECT fonte = {0, 0, sIdsW, sIdsH};
        D3DPOINT destino = {0, 0};
        sDev->SetTexture(3, NULL);
        sDev->Resolve(D3DRESOLVE_RENDERTARGET0, &fonte, sIds, &destino, 0, 0, NULL, 0.0f, 0, NULL);
    }
    /* a tabela pelos ids, e o controle com a tabela trocada */
    for (k = 0; k < 2; k++) {
        sDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);
        sDev->Clear(0, NULL, D3DCLEAR_TARGET, 0, 0.0f, 0);
        sDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        nat_tabela_passa(1, 0.0f, NULL, k ? t2 : t1, &r);
        at_resolve(res);
        if (!at_le(res, 256, 256, lin)) {
            continue;
        }
        for (i = 0; i < total; i++) {
            unsigned c = c1[at_id(pad[i] >> 24)], g = lin[i], ea = at_alfa(c >> 24);
            int rgb = ((g >> 16) & 255) == (c & 255) && ((g >> 8) & 255) == ((c >> 8) & 255) && (g & 255) == ((c >> 16) & 255);
            tab[k][0] += rgb && (g >> 24) == ea;
            tab[k][1] += rgb && at_perto(g >> 24, ea);
        }
    }
    /* o contorno sobre um cinza 0x80; o controle é a imagem sem a passada (bate só onde não há aresta) */
    sDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);
    sDev->Clear(0, NULL, D3DCLEAR_TARGET, 0xFF808080, 0.0f, 0);
    nat_contorno_passa(&r, 1.0f);
    at_resolve(res);
    if (at_le(res, 256, 256, lin)) {
        unsigned y, xx;
        for (y = 1; y < 255; y++) {
            for (xx = 1; xx < 255; xx++) {
                unsigned q = at_cont(at_id(pad[y * 256 + xx] >> 24)), e, g = lin[y * 256 + xx];
                int aresta = q > at_cont(at_id(pad[y * 256 + xx + 1] >> 24)) || q > at_cont(at_id(pad[y * 256 + xx - 1] >> 24)) ||
                             q > at_cont(at_id(pad[(y + 1) * 256 + xx] >> 24)) || q > at_cont(at_id(pad[(y - 1) * 256 + xx] >> 24));
                e = aresta ? 0x1C : 0x80; /* 0x80 - 0x64 */
                cont[0] += (g >> 24) == 0xFF && at_perto((g >> 16) & 255, e) && at_perto((g >> 8) & 255, e) && at_perto(g & 255, e);
                cont[1] += aresta;
                cont[2] += !aresta; /* sem a passada, o cinza bateria só aqui */
            }
        }
    }
    /* a tabela pela profundidade: o padrão como cor na profundidade da EDRAM, resolvido numa D24S8, lido pela passada
       com o máximo de 16 bits (65535) e uma tabela com o índice no vermelho */
    if (sModoZ != 0 && sEdramProfCor != NULL &&
        SUCCEEDED(sDev->CreateTexture(sAlvoW, sAlvoH, 1, 0, D3DFMT_D24S8, D3DPOOL_DEFAULT, &zt, NULL)) && zt != NULL) {
        D3DRECT fonte = {0, 0, sAlvoW, sAlvoH};
        D3DPOINT destino = {0, 0};
        sDev->SetDepthStencilSurface(NULL);
        sDev->SetRenderTarget(0, sEdramProfCor);
        sDev->SetViewport(&vpz);
        copia(tp, sAlvoWZ, sAlvoH, 0.0f, 0.0f, (float)sAlvoWZ, (float)sAlvoH, 0.0f, 0.0f, 1.0f, 1.0f, 0);
        sDev->SetTexture(0, NULL);
        sDev->SetRenderTarget(0, sEdramCor);
        sDev->SetDepthStencilSurface(sEdramProf);
        sDev->SetViewport(&vp);
        sDev->Resolve(D3DRESOLVE_DEPTHSTENCIL, &fonte, zt, &destino, 0, 0, NULL, 0.0f, 0, NULL);
        sDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);
        sDev->Clear(0, NULL, D3DCLEAR_TARGET, 0, 0.0f, 0);
        sDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        nat_tabela_passa(0, 65535.0f, zt, t1, &r);
        at_resolve(res);
        if (at_le(res, 256, 256, lin) && at_le(zt, sAlvoW, sAlvoH, zl)) {
            for (i = 0; i < total; i++) {
                for (h = 0; h < 4; h++) {
                    unsigned w = zl[i], d24;
                    double z;
                    unsigned idx;
                    if (h >= 2) {
                        w = (w >> 24) | ((w >> 8) & 0xFF00u) | ((w << 8) & 0xFF0000u) | (w << 24);
                    }
                    d24 = (h & 1) ? w & 0xFFFFFFu : w >> 8;
                    z = (double)d24 / 16777215.0 * 65535.0;
                    idx = z >= 65448.0 ? 0 : (unsigned)(255.0 - (double)(unsigned)(z / 256.0));
                    zc[h][0] += ((lin[i] >> 16) & 255) == idx;
                    zc[h][1] += at_perto((lin[i] >> 16) & 255, idx);
                }
            }
        }
        /* (a palavra como a CPU a vê: depth nos 24 bits de cima, o que o modo 2 do autoteste da profundidade mostrou;
           com os bytes trocados também vale, se a memória da D24S8 estiver na ordem da GPU) */
        okZ = zc[0][1] >= total * 99u / 100u || zc[2][1] >= total * 99u / 100u;
    }
    okTab = tab[0][1] >= total * 99u / 100u && tab[1][1] < total / 10u;
    okCont = cont[0] >= inter * 99u / 100u && cont[1] > 0 && cont[2] > 0;
    Diario_Escrever("gs_d3d: nativas: autoteste: tabela pelos ids %u de %u iguais (%u com o alfa exato), controle com a tabela "
                    "trocada %u (tem de ser perto de 0); contorno %u de %u iguais, %u arestas (sem a passada bateriam %u)",
                    tab[0][1], total, tab[0][0], tab[1][1], cont[0], inter, cont[1], cont[2]);
    if (okZ >= 0) {
        Diario_Escrever("gs_d3d: nativas: autoteste: tabela pela profundidade (máximo 65535), a palavra da D24S8 lida como "
                        "depth nos 24 bits altos %u de %u iguais (%u a 1 de distância); controles: nos 24 baixos %u (%u), "
                        "com os bytes trocados %u (%u) e %u (%u)", zc[0][0], total, zc[0][1], zc[1][0], zc[1][1], zc[2][0],
                        zc[2][1], zc[3][0], zc[3][1]);
    }
    if (!okTab || !okCont || okZ == 0) {
        unsigned tira = (okCont ? 0 : 1) | (okTab ? 0 : 2) | (okZ == 0 ? 4 : 0);
        Diario_Escrever("gs_d3d: ERRO: nativas: o autoteste não bateu em%s%s%s; %s", okCont ? "" : " contorno",
                        okTab ? "" : " tabela pelos ids", okZ == 0 ? " tabela pela profundidade" : "",
                        forcado ? "mantidas (BT3_NATIVOS forçado)" : "essas ficam desligadas (contadas e puladas)");
        if (!forcado) {
            sNativos &= ~tira;
        }
    }
    sDev->BlockUntilIdle();
    tp->Release();
    t1->Release();
    t2->Release();
    res->Release();
    if (zt != NULL) {
        zt->Release();
    }
    sDev->SetRenderTarget(0, sTela);
    sDev->SetDepthStencilSurface(NULL);
    esquece_estado();
    sIdsQuadro = 0;
    gGsD3dEtapa = "antes do primeiro quadro";
}

/* os ajustes (BT3_NATIVOS, BT3_FBA, BT3_CORTE), a cópia dos ids e o autoteste */
static void nat_init(void) {
    const char *e = getenv("BT3_NATIVOS");
    int forcado = e != NULL && *e != '\0';
    unsigned bytes = tam_real(sIdsW, sIdsH, D3DFMT_A8R8G8B8);
    sNativos = env_num("BT3_NATIVOS", 7) & 7;
    sFba = env_num("BT3_FBA", 1) != 0;
    sCorte = env_num("BT3_CORTE", 1) != 0;
    if (sNativos != 0) {
        if (mem_livres(bytes) < sMem.pisoMb * MB + bytes ||
            FAILED(sDev->CreateTexture(sIdsW, sIdsH, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &sIds, NULL)) || sIds == NULL) {
            Diario_Escrever("gs_d3d: ERRO: nativas: a cópia dos ids não foi criada (%u MB livres); as nativas ficam desligadas", mb_livres());
            sIds = NULL;
            sNativos = 0;
        } else {
            mem_cria(CL_ALVO, bytes);
            nat_autoteste(forcado);
        }
    }
    Diario_Escrever("gs_d3d: nativas: BT3_NATIVOS=%u (contorno %s, tabelas pelos ids %s, tabelas pela profundidade %s%s); "
                    "cópia dos ids %ux%u (%u KB, contada com os alvos); BT3_FBA=%u (%s); BT3_CORTE=%u (%s)",
                    sNativos, (sNativos & 1) ? "sim" : "não", (sNativos & 2) ? "sim" : "não", (sNativos & 4) ? "sim" : "não",
                    (sNativos & 4) && sModoZ == 0 ? ", mas sem a profundidade persistente: puladas" : "", sIdsW, sIdsH,
                    sIds != NULL ? bytes >> 10 : 0, sFba, sFba ? "duas passadas com mistura pelo alfa da fonte" : "o alfa da mistura vira 1",
                    sCorte, sCorte ? "a VU1 recortada pelo w, profundidade por pixel" : "z da VU1 preso em 0..w por vértice");
}

/* A imagem mostrada na tela de 1280 x 720: o alvo `mostrado` (o buffer de vídeo do GS) no meio, em 4:3, e as barras da cor de
   fundo. Do fim do quadro e de GsD3d_Repete. */
static void tela_copia(D3DTexture *t) {
    D3DVIEWPORT9 vp = {0, 0, TELA_W, TELA_H, 0.0f, 1.0f};
    sDev->SetRenderTarget(0, sTela);
    sDev->SetDepthStencilSurface(NULL);
    sDev->SetViewport(&vp);
    sDev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    sDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);
    sDev->Clear(0, NULL, D3DCLEAR_TARGET, sCorFundo, 1.0f, 0);
    if (t != NULL) {
        /* 512 x 448 do GS na forma da imagem (4:3: 960 x 720 no meio da tela de 1280 x 720); (720p, etapa 3) a textura cobre
           a área do GS em qualquer escala: na 2, 1024 x 896 reduzidos a 960 x 720 */
        int w = (int)((float)TELA_H * (float)Port_AspectMilli() / 1000.0f + 0.5f), x;
        w = w > TELA_W ? TELA_W : w;
        x = (TELA_W - w) / 2;
        copia(t, TELA_W, TELA_H, (float)x, 0.0f, (float)(x + w), (float)TELA_H, 0.0f, 0.0f, 512.0f / sAreaW, 448.0f / sAreaH, 1);
    }
}

/* (degrau 2, etapa 4) as texturas das vagas e o conteúdo do lado do desenho, em dia com a gravação: a vaga cuja cor mudou
   esquece o conteúdo dela e o da profundidade; a cuja profundidade mudou, o da profundidade. No começo do quadro certo:
   na entrega (o desenho parado, e o próximo quadro que ele desenha é o entregue) ou no começo do frame_end sem a
   divisão. O sResidente e o sZDono são -1 aqui (o frame_end acaba com sai_da_edram e prof_entrega). */
static void sincroniza_alvos(void) {
    int i;
    for (i = 0; i < MAX_TARGETS; i++) {
        if (sAlvoD[i].ver != sAlvos[i].ver) {
            sAlvoD[i].ver = sAlvos[i].ver;
            sAlvoD[i].tex = sAlvos[i].tex;
            sAlvoD[i].conteudo.x0 = sAlvoD[i].conteudo.x1 = 0;
            sProfD[i].conteudo.x0 = sProfD[i].conteudo.x1 = 0; /* (a textura de profundidade, se houver, fica para o novo dono) */
        }
        if (sProfD[i].ver != sProf[i].ver) {
            sProfD[i].ver = sProf[i].ver;
            sProfD[i].tex = sProf[i].tex;
            sProfD[i].cor = sProf[i].cor;
            sProfD[i].conteudo.x0 = sProfD[i].conteudo.x1 = 0;
        }
    }
}

/* o alvo de um fbp na tabela do quadro (a cópia da entrega com a divisão): o GsDraw_TargetGet sem escrever o last */
static int alvo_do_fbp(const GsQuadro *q, int fbp) {
    int i;
    for (i = 0; fbp >= 0 && i < q->targetCount; i++) {
        if (q->targets[i].fbp == (uint32_t)fbp) {
            return i;
        }
    }
    return -1;
}

static void frame_end(void) {
    const GsQuadro *q = gsQuadro;
    LARGE_INTEGER t0, tp, tv;
    double msVu;
    double msFim, msPresent;
    unsigned capturas = sConta.capturas;
    int cur = -1, mostrado, i;
    uint32_t n;
    double ms;
    D3DTexture *mtex;
    static int every = -1;

    QueryPerformanceCounter(&t0);
    sQ = q;
    tempo_comeca(); /* (F5, etapa 8: lê o quadro de dois atrás) */
    if (!q->dividido) {
        grav_junta(); /* (com a divisão, na entrega) */
    }
    mem_buffers(q);
    gGsD3dEtapa = "fim do quadro: refazendo a lista";
    if (gX360Luta != sLutaVista) { /* os primeiros quadros da luta, passo a passo (a primeira rodada travou ali) */
        sLutaVista = gX360Luta;
        sPassoAPasso = gX360Luta != 0 ? 8 : 0;
    }
    if (sPassoAPasso > 0) {
        Diario_Escrever("gs_d3d: quadro %u (luta): %u desenhos, %u vértices, %u da VU1, %d alvos, %d texturas", q->quadro,
                        q->drawCount, q->vertCount, q->vuVertCount, q->targetCount, q->texCount);
    }
    sDateFonte = NULL;
    if (!q->dividido) { /* (com a divisão, a gravação fez na entrega) */
        gGsD3dEtapa = "fim do quadro: texturas de profundidade";
        prof_garante(q);
        sincroniza_alvos();
    }
    tempo_poe(T_OUTROS);
    gGsD3dEtapa = "fim do quadro: refazendo a lista";
    sConta.msZQuadro = 0.0;
    if (sConta.quadros == 30 && sConta.alfaAltoDes == 0) {
        alfa_alto(); /* (um quadro por trecho) */
    }
    {
        /* F5, etapa 6: o tempo da VU1 na gravação do quadro (gs_vu1.c) e a cópia das três famílias para a GPU */
        double g = q->vu1Ms;
        if (!q->dividido) {
            gGsVu1Ms = 0.0; /* (com a divisão, a entrega zerou) */
        }
        sConta.msVuGrav += g;
        if (g > sConta.msVuGravPior) sConta.msVuGravPior = g;
        QueryPerformanceCounter(&tv);
        gGsD3dEtapa = "fim do quadro: cópia da VU1";
        tempo_poe(T_VU_COPIA);
        if (!q->dividido) {
            vu_plano(q); /* (com a divisão, a entrega fez a conta) */
        }
        vu_copia(q);
        tempo_poe(T_OUTROS);
        msVu = ms_desde(&tv);
        gGsD3dEtapa = "fim do quadro: refazendo a lista";
    }
    tempo_abre(); /* (a GPU do quadro começa aqui) */
    recorte_poe();
    for (n = 0; n < q->drawCount; n++) {
        const GsDraw *d = &q->draws[n];
        if (d->native >= 100) {
            /* (F6, etapa 6) a imagem do filme: com BT3_FILME=1, a vaga com o número dela; senão contada, como antes */
            if (d->first != 0 && (sFilmeEstado == 1 || sFilmeEstado == 2)) {
                gGsD3dEtapa = "fim do quadro: filme";
                filme_desenha(d, n, &cur);
                gGsD3dEtapa = "fim do quadro: refazendo a lista";
            } else {
                sConta.semFb++;
            }
            continue;
        }
        if (d->native == 5) {
            /* o começo de uma sequência de desenhos com DATE: a cópia da cor do alvo agora */
            sConta.dateMarc++;
            if (!q->targets[d->target].cleared) {
                continue;
            }
            sDateFonte = sAlvoD[d->target].tex; /* fora da EDRAM, a textura do alvo já é a imagem dele agora */
            if (d->target == sResidente) {
                Ret r = sSujo;
                ret_une(&r, sAlvoD[d->target].conteudo.x0, sAlvoD[d->target].conteudo.y0, sAlvoD[d->target].conteudo.x1,
                        sAlvoD[d->target].conteudo.y1);
                if (r.x1 > r.x0 && sDate != NULL) {
                    D3DRECT fonte;
                    D3DPOINT destino;
                    tempo_poe(T_DATE);
                    alinha8(&r);
                    fonte.x1 = r.x0; fonte.y1 = r.y0; fonte.x2 = r.x1; fonte.y2 = r.y1;
                    destino.x = r.x0; destino.y = r.y0;
                    sDev->SetTexture(1, NULL); /* (o destino do Resolve não pode estar em uso) */
                    sCurDate = NULL;
                    sDev->Resolve(D3DRESOLVE_RENDERTARGET0, &fonte, sDate, &destino, 0, 0, NULL, 0.0f, 0, NULL);
                    sConta.dateRes++;
                }
                sDateFonte = sDate;
            } else {
                sConta.dateTex++;
            }
            continue;
        }
        /* (F5, etapa 7, segunda parte) as nativas: a cópia dos ids, as passadas de tabela e o contorno */
        if (d->native == 4) {
            gGsD3dEtapa = "fim do quadro: nativa, cópia dos ids";
            nat_ids(d, n, &cur);
            gGsD3dEtapa = "fim do quadro: refazendo a lista";
            continue;
        }
        if (d->native == 3) {
            gGsD3dEtapa = "fim do quadro: nativa, passada de tabela";
            nat_tabela(d, n, &cur);
            gGsD3dEtapa = "fim do quadro: refazendo a lista";
            continue;
        }
        if (d->native == 1) {
            gGsD3dEtapa = "fim do quadro: nativa, contorno";
            nat_contorno(d, n, &cur);
            gGsD3dEtapa = "fim do quadro: refazendo a lista";
            continue;
        }
        if (d->native != 0) {
            sConta.semNativo[d->native & 7]++;
            continue;
        }
        if (d->vu) {
            /* F5, etapa 6: as três famílias (1 vu0, 2 vu4, 3 vu6); o que vu_plano não pôs no par é contado */
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
            q->targets[d->target].cleared = 1; /* (com a divisão, na cópia do retrato: o cleared do desenho) */
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
        Diario_Escrever("gs_d3d: quadro %u (luta): lista refeita (maior desenho até aqui %u vértices)", q->quadro,
                        sConta.maiorDesenho);
    }

    /* a imagem mostrada: a que o sceGsSwapDBuff montou para este quadro, senão o primeiro alvo (como o gs_gl.c). O alvo
       se busca na tabela do quadro, sem o GsDraw_TargetGet (que escreve o last, da gravação: com a divisão, a entrega o
       chama no lugar deste) */
    if (!q->dividido && q->fbp >= 0) {
        GsDraw_TargetGet((uint32_t)q->fbp, 0); /* (o last, como antes) */
    }
    mostrado = alvo_do_fbp(q, q->fbp);
    for (i = 0; mostrado < 0 && i < q->targetCount; i++) {
        mostrado = i;
    }
    mtex = mostrado >= 0 && q->targets[mostrado].cleared ? sAlvoD[mostrado].tex : NULL;
    if (every < 0) {
        every = getenv("BT3_SHOT") != NULL ? atoi(getenv("BT3_SHOT")) : 0;
    }
    if (((every > 0 && q->quadro % (unsigned)every == 0 &&
          (getenv("BT3_SHOT_FROM") == NULL || (int)q->quadro >= atoi(getenv("BT3_SHOT_FROM"))) &&
          (getenv("BT3_SHOT_TO") == NULL || (int)q->quadro <= atoi(getenv("BT3_SHOT_TO")))) ||
         na_lista(q->quadro)) &&
        mtex != NULL) {
        gGsD3dEtapa = "fim do quadro: captura";
        captura(mostrado, mtex);
    }
    gGsD3dEtapa = "fim do quadro: cópia para a tela";
    tempo_poe(T_TELA);
    tela_copia(mtex);
    sMostradoTex = mtex;
    tempo_fecha_gpu(capturas != sConta.capturas);
    tempo_poe(T_PRESENT);
    if (gPicos != 0) {
        picos_marca(q->quadro); /* (picos: a GPU terminou o quadro) */
    }
    QueryPerformanceCounter(&tp);
    gGsD3dEtapa = "fim do quadro: Present";
    sDev->Present(NULL, NULL, NULL, NULL);
    sUltimoPresent = GetTickCount();
    msPresent = ms_desde(&tp);
    if (gPicos != 0) {
        picos_apresentou();
    }
    tempo_poe(T_OUTROS);
    Passos_Apresentou(q->quadro, (unsigned)(msPresent * 1000.0));
    gGsD3dEtapa = "fim do quadro: texturas soltas";
    /* nada do quadro fica preso no dispositivo: as texturas soltas podem ir quando a GPU terminar com elas */
    sDev->SetTexture(0, NULL);
    sDev->SetTexture(1, NULL);
    /* (F5, etapa 6) o par de buffers da VU1 também: preso no dispositivo, ficaria ocupado para sempre (IsBusy) */
    sDev->SetStreamSource(0, NULL, 0, 0);
    sDev->SetIndices(NULL);
    esquece_estado();
    filme_varre(sDev->InsertFence()); /* (F6, etapa 6) as vagas do filme que a GPU já soltou */
    if (q->dividido && sProfFilaD >= 0) {
        /* (degrau 2, etapa 4) as profundidades das vagas que a gravação deste quadro reaproveitou (a cor e as texturas vêm
           pelo solta, do gs_draw.c, logo depois deste frame_end) */
        int k;
        for (k = 0; k < sProfFilaN[sProfFilaD]; k++) {
            cova_poe(sProfFila[sProfFilaD][k], CL_PROF, tam_alvo());
        }
        sProfFilaN[sProfFilaD] = 0;
        sProfFilaD = -1;
    }
    cova_varre();
    gGsD3dEtapa = "fim do quadro: memória";
    if (!q->dividido) { /* (com a divisão, a gravação mede e tenta de novo na entrega) */
        mem_mede(); /* (uma medida por quadro: o mínimo livre visto e a base da estimativa) */
        alvos_sem_textura(NULL);
    }

    msFim = ms_desde(&t0);
    tempo_fecha_cpu(msFim, capturas == sConta.capturas);
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
        Diario_Escrever("gs_d3d: quadro %u (luta): apresentado em %.2f ms (Present %.2f)", q->quadro, msFim, msPresent);
        sPassoAPasso--;
    }
    if (gPicos != 0) {
        picos_desenho(q, msFim, msPresent, capturas != sConta.capturas);
    }
    gGsD3dQuadros++;
    gGsD3dEtapa = q->dividido ? "esperando o retrato seguinte" : "jogo e gravação do quadro";

    if (q->dividido) {
        sPs2Total += q->skipped; /* (os contadores da gravação, a entrega zerou) */
        return;
    }
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

/* A thread dona do dispositivo, sem quadro há 50 ms (o jogo carrega, não entrega quadro): apresenta de novo a última
   imagem, para a Guide e o sistema terem um present a cada tanto. No degrau 1, a de gravação ociosa; com a divisão (degrau
   2, etapa 4), a de desenho ociosa, com a entrega parada (gs_draw.c, desenho_thread). Usa a textura que o próprio desenho
   guardou (sMostradoTex; a cova a esquece quando ela é solta), nunca a tabela da gravação. Devolve 1 se apresentou. Não
   conta como quadro (gGsD3dQuadros, o que o vigia olha, não anda). */
extern "C" int GsD3d_Repete(void) {
    if (sDev == NULL || sDono == 0 || GetCurrentThreadId() != sDono || sMostradoTex == NULL ||
        GetTickCount() - sUltimoPresent < 50) {
        return 0;
    }
    sPicoRepete++;
    gGsD3dEtapa = "repetindo o último quadro (o jogo não entrega)";
    tela_copia(sMostradoTex);
    sDev->Present(NULL, NULL, NULL, NULL);
    sUltimoPresent = GetTickCount();
    sDev->SetTexture(0, NULL);
    esquece_estado();
    gGsD3dEtapa = sDiv ? "esperando o retrato seguinte" : "jogo e gravação do quadro";
    return 1;
}

/* (degrau 2, etapa 4) O desenho ocioso, ou no fim de um quadro: se a gravação espera memória (espera_desenho), o que a GPU
   já soltou sai da cova e, se ainda não basta, espera a GPU soltar o mais antigo, um por vez; depois o sinal. */
extern "C" void GsD3d_Ocioso(void) {
    unsigned precisa = sPedeLivre;
    const char *antes;
    if (precisa == 0 || sDev == NULL || sDono == 0 || GetCurrentThreadId() != sDono) {
        return;
    }
    antes = (const char *)gGsD3dEtapa;
    gGsD3dEtapa = "soltando memória para a gravação";
    cova_varre();
    while (livres_agora() < precisa && sCovaN > 0) {
        cova_espera_primeira();
    }
    SetEvent(sCovaEv);
    gGsD3dEtapa = antes;
}

/* O jogo terminou nesta thread (Port_Sair ou queda): o dispositivo volta para quem for usá-lo (a main). */
extern "C" void GsD3d_Soltar(void) {
    if (sDev != NULL && sDev->QueryThreadOwnership() == GetCurrentThreadId()) {
        vblank_tira();
        sDev->ReleaseThreadOwnership();
        sDono = 0;
    }
}

/* (degrau 2, etapa 4) A entrega, na gravação, com o retrato pronto e o desenho parado (gs_draw.c, desenho_entrega): o que o
   frame_end fazia do lado da gravação e o que passa de um lado ao outro.
     - o last do alvo mostrado (o GsDraw_TargetGet que o frame_end chamava);
     - a conta da VU1 (vu_plano, o que cabe no par) e, com ela, a regra do cleared da gravação: só o alvo com algum desenho
       não nativo que o desenho vai fazer (o da VU1 que não coube não conta, o sVuIni ~0); limpa[] chega com 1 para todo
       alvo com desenho não nativo e sai com a regra deste backend;
     - as profundidades novas (prof_garante) e as vagas sem textura (alvos_sem_textura), que abrem espaço no cache (só da
       gravação: o desenho não chama mais o GsDraw_X360Abre);
     - a medida da memória do quadro;
     - as texturas das vagas para o lado do desenho (sincroniza_alvos: o começo do quadro entregue), a fila das
       profundidades soltas e os contadores do trecho. */
static void entrega(const GsQuadro *q, unsigned char *limpa) {
    const char *antes = (const char *)gGsD3dEtapaGrav;
    uint32_t n;
#ifdef BT3_GS_ESPERAS
    uint64_t ft; /* (F5, etapa 8, gravação) as partes do gancho por família, fora da soma (gs_internal.h) */
#endif
    gGsD3dEtapaGrav = "entrega do retrato";
    grav_junta();
    sPendGrav = 0; /* (o que foi pedido até aqui viaja com este retrato) */
    if (q->fbp >= 0) {
        GsDraw_TargetGet((uint32_t)q->fbp, 0);
    }
#ifdef BT3_GS_ESPERAS
    GS_FAM_INI(ft);
#endif
    vu_plano(q);
    memset(limpa, 0, MAX_TARGETS);
    for (n = 0; n < q->drawCount; n++) {
        const GsDraw *d = &q->draws[n];
        if (d->native == 0 && d->target >= 0 && d->target < MAX_TARGETS && !(d->vu && sVuIni[n] == ~0u)) {
            limpa[d->target] = 1;
        }
    }
#ifdef BT3_GS_ESPERAS
    GS_FAM_PASSO(GS_FAM_E_PLANO, ft);
#endif
    prof_garante(q);
    alvos_sem_textura(limpa);
#ifdef BT3_GS_ESPERAS
    GS_FAM_PASSO(GS_FAM_E_ALVOS, ft);
#endif
    mem_mede();
#ifdef BT3_GS_ESPERAS
    GS_FAM_PASSO(GS_FAM_E_MEM, ft);
#endif
    sincroniza_alvos();
    sProfFilaD = sProfFilaG;
    sProfFilaG ^= 1;
    sProfFilaN[sProfFilaG] = 0;
    gGsD3dEtapaGrav = antes;
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
    entrega,
    solta_pede,
    solta,
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
