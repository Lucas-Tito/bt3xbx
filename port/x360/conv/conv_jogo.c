/* A ponte do jogo para a conversão em tempo de jogo, em host big-endian (as chamadas ficam em #ifdef
   PORT_BIG_ENDIAN no código do jogo). Formato que não bate é erro fatal: seguir com dados meio convertidos só
   adiaria o problema para um lugar pior. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conv.h"

#ifdef _XBOX /* o 360 compila este arquivo com a CRT do XDK, fora do shim: a falha vai ao log e à tela vermelha */
void Diario_Escrever(const char *formato, ...);
void Port_Abortar(void);
#endif

static void falha(const char *formato, ...) {
    char linha[400];
    va_list ap;
    va_start(ap, formato);
    vsprintf(linha, formato, ap);
    va_end(ap);
#ifdef _XBOX
    Diario_Escrever("%s", linha);
    Port_Abortar();
#else
    fprintf(stderr, "%s\n", linha);
    abort();
#endif
}

/* ------------------------------------------------------------------ a guarda do Sprite_Unpack

   Cada chamada de Sprite_Unpack (src/sys/sprite.c) descomprime um formato que só quem chama conhece; o gancho logo
   depois (Port_ConvertMenuPack, Port_ConvertLoadSheet, ou Port_UnpackSemNumeros para um conteúdo só de bytes) troca
   os números. Uma chamada sem gancho deixaria o conteúdo little-endian, e o jogo cairia mais adiante num endereço
   sem relação (o menu principal no console, F3 etapa 8: Res_RelocateOffsets de MainMenu_Init). A guarda:
     - Sprite_Unpack anota o buffer descomprimido e o endereço de retorno (quem chamou);
     - o gancho limpa a anotação, e só a do mesmo buffer;
     - o que vier depois com a anotação ainda lá é falha fatal com o nome de quem chamou: o Sprite_Unpack seguinte,
       o Res_RelocateOffsets (src/sys/gfxm_b_b.c, o primeiro leitor de quase toda tela, antes de usar os
       deslocamentos crus) e o vblank (port/src/plat_stub.c: no máximo um quadro depois);
     - antes de descomprimir, um cabeçalho BPE impossível é contêiner cru: o arquivo chegou sem conversor
       (formato_de, conv.c), e Bpe_Decode cairia. Impossível: tamanho negativo ou acima de 16 MB, ou blocos que não
       terminam exatamente no packSize (Conv_BpeFecha). Só a faixa deixava passar um cabeçalho little-endian com os dois
       tamanhos múltiplos de 256 (lido ao contrário, outro múltiplo de 256 abaixo de 16 MB); o percurso dos blocos usa
       só os bytes deles, então um packSize lido ao contrário não fecha (nos 712 contêineres de menu do disco, nenhum
       packSize 256 a mais ou a menos fecha). Não prova que o conteúdo foi convertido: a rede de segurança é o
       conv_tool --disco (todo arquivo de menu com o formato certo).
   O nome sai da tabela de quem chama Sprite_Unpack (abaixo, conferida por port/x360/tools/unpacks.py): a função de
   maior endereço que não passa do retorno. O endereço vai junto para conferir pelo addr2line (qemu) ou pelo .map
   (360). Só em host big-endian: a build x86 não compila este arquivo, e o jogo chama a guarda em #ifdef
   PORT_BIG_ENDIAN. */

typedef void FuncaoDoJogo(void);
/* as funções do jogo que chamam Sprite_Unpack (unpacks.py confere esta lista contra src/) */
#define CHAMADORES(X) \
    X(MainMenu_Init) X(Title_Init) X(ModeMenu_UpdateImage) X(ModeMenu_Init) X(HistOutro_Init) X(HistSel_Init) \
    X(HistResult_Init) X(HistSave_Init) X(CharSel_UpdateFaceLoad) X(CharSel_UpdateStageLoad) X(CharSel_Init) \
    X(TeamSel_UpdateFaceLoad) X(TeamSel_UpdateStageLoad) X(TeamSel_Init) X(DuelMenu_Init) X(CharRef_Init) \
    X(CharRef_UpdateImage) X(Train_Init) X(Train_UpdateImage) X(BootCard_Init) X(FirstRun_Main) \
    X(EntrySel_UpdateImage) X(EntrySel_Init) X(TourMenu_Init) X(TourBg_Init) X(Bracket_Load) \
    X(Bracket_UpdateImages) X(Bracket_LoadImages) X(SoloSel_UpdateImage) X(SoloSel_Init) \
    X(UbTeamSel_UpdateFaceLoad) X(UbTeamSel_Init) X(UbzSel_Init) X(UbRank_Init) X(UbRank_UpdateFaceLoad) \
    X(UbResult_Init) X(DiscFusion_Init) X(MisSel_Init) X(MisResult_Init) X(UbMenu_Init) X(SimDay_Init) \
    X(SimDay_UpdateFaceLoad) X(SimResult_Init) X(SimTop_Init) X(SurvSel_Init) X(SurvResult_Init) X(EvoZ_Init) \
    X(EvoZ_Load) X(EvoZ_UpdateFace) X(Shop_Init) X(EvoTop_Init) X(Option_Init) X(DcList_UpdateFace) \
    X(DcList_Init) X(DcMenu_Init) X(DcPass_UpdateFace) X(DcPass_Init) X(ReplayMenu_Init) X(Shen_Init) \
    X(ShenSave_Init) X(Load_InitScreen) X(McFlow_Init)
#define DECLARAR(f) extern FuncaoDoJogo f;
#define LINHA(f) {f, #f},
CHAMADORES(DECLARAR)
static const struct { FuncaoDoJogo *f; const char *nome; } sChamadores[] = { CHAMADORES(LINHA) };

static void *sPendente;     /* o buffer do último Sprite_Unpack, ainda sem gancho; NULL se não há */
static void *sPendenteDe;   /* o endereço de retorno daquele Sprite_Unpack */

/* "Funcao+0xDESL" de um endereço de retorno, pela tabela; o endereço cru se nenhuma função da tabela vem antes */
static const char *quem(void *ret, char *s) {
    unsigned long a = (unsigned long)ret, melhor = 0;
    const char *nome = NULL;
    unsigned k;
    for (k = 0; k < sizeof(sChamadores) / sizeof(sChamadores[0]); k++) {
        unsigned long f = (unsigned long)sChamadores[k].f;
        if (f <= a && f >= melhor) {
            melhor = f;
            nome = sChamadores[k].nome;
        }
    }
    if (nome == NULL) {
        sprintf(s, "0x%08lX (fora da tabela de chamadores)", a);
    } else {
        sprintf(s, "%s (retorno 0x%08lX = %s+0x%lX)", nome, a, nome, a - melhor);
    }
    return s;
}

/* BT3_GUARDA=anota (só sem janela, F4 etapa 4): a PPC com o mapa do rastreio (BT3_MAPA, inverter.c), em que o mapa
   já trocou o conteúdo sem gancho (inclusive o de um arquivo convertido por formato: inverter.c, sem_gancho), segue
   adiante e a guarda só nomeia cada chamada sem conversor, numa linha "guarda: ..." da saída
   (port/x360/tools/roteiros.sh). Sem o mapa o jogo cairia logo depois. */
static int anotar(void) {
#ifdef _XBOX
    return 0;
#else
    static int a = -1;
    if (a < 0) {
        a = getenv("BT3_GUARDA") != NULL && strcmp(getenv("BT3_GUARDA"), "anota") == 0;
    }
    return a;
#endif
}

void Port_UnpackCheck(const char *onde) {
    char s[160];
    if (sPendente != NULL) {
        if (anotar()) {
            printf("guarda: Sprite_Unpack sem conversor em %s, até o %s\n", quem(sPendenteDe, s), onde);
            fflush(stdout);
            sPendente = NULL;
            return;
        }
        falha("Sprite_Unpack sem conversor em %s: o conteúdo ficou cru até o %s", quem(sPendenteDe, s), onde);
    }
}

void Port_UnpackBefore(void *src, void *chamador) {
    char s[160];
    long bruto, comp;
    Port_UnpackCheck("Sprite_Unpack seguinte");
    bruto = (long)((int *)src)[0];
    comp = (long)((int *)src)[1];
    if (bruto < 0 || bruto > 0x1000000L || comp < 0 || comp > 0x1000000L ||
        !Conv_BpeFecha((const conv_u8 *)src + 8, (conv_ofs)comp)) {
        falha("Sprite_Unpack em %s sobre um contêiner BPE cru (rawSize 0x%lX, packSize 0x%lX): o arquivo não passou "
              "por conversor (formato_de, port/x360/conv/conv.c)", quem(chamador, s), (unsigned long)bruto,
              (unsigned long)comp);
    }
}

void Port_UnpackAfter(void *res, void *chamador) {
    sPendente = res;
    sPendenteDe = chamador;
}

/* o gancho do buffer pendente: limpa a anotação; outro buffer, ou nenhum pendente, é gancho fora do lugar */
static void convertido(void *buf, const char *formato) {
    char s[160];
    if (sPendente == NULL) {
        falha("gancho %s sem Sprite_Unpack antes", formato);
    }
    if (buf != sPendente) {
        falha("gancho %s sobre outro buffer que o do Sprite_Unpack de %s", formato, quem(sPendenteDe, s));
    }
    sPendente = NULL;
}

/* A marca de "sem números": o conteúdo descomprimido só tem bytes (o chamador diz por quê no comentário do
   gancho). Fica em #ifdef PORT_BIG_ENDIAN logo depois do Sprite_Unpack, como os ganchos. */
void Port_UnpackSemNumeros(void *buf) {
    convertido(buf, "sem números");
}

/* Para o filtro de exceção do 360 (main360.cpp): numa queda com um Sprite_Unpack ainda sem gancho, a causa
   provável vai ao log. 1 se havia. */
int Port_UnpackPendenteNaQueda(char *linha, int n) {
    char s[160];
    if (sPendente == NULL || n < 200) {
        return 0;
    }
    sprintf(linha, "Sprite_Unpack sem conversor pendente na queda: %s", quem(sPendenteDe, s));
    return 1;
}

/* src/battle/bobj_a.c, BtlObjAnim_Load: uma animação de personagem logo depois de Bpe_Decode */
void Port_ConvertAnim(void *buf, int size) {
    char msg[240];
    if (size <= 0) {
        return; /* "BPE vazio": nada decodificado */
    }
    if (Conv_Animacao((conv_u8 *)buf, (conv_ofs)size, msg, sizeof(msg)) < 0) {
        falha("conv: animação de 0x%X bytes: %s", (unsigned)size, msg);
    }
}

/* src/sys/loading.c, Load_InitScreen: a folha da tela de carregamento logo depois de Sprite_Unpack */
void Port_ConvertLoadSheet(void *buf, int size) {
    char msg[240];
    conv_u8 *feito;
    int r;
    convertido(buf, "folha");
    if (size <= 0) {
        return;
    }
    feito = (conv_u8 *)calloc((size_t)size, 1);
    if (feito == NULL) {
        falha("conv: folha da tela de carregamento: sem memória");
    }
    r = Conv_Formato("folha", "folha da tela de carregamento", (conv_u8 *)buf, (conv_ofs)size, feito, msg, sizeof(msg));
    free(feito);
    if (r < 0) {
        falha("conv: folha da tela de carregamento de 0x%X bytes: %s", (unsigned)size, msg);
    }
}

/* Um pacote de tela de menu logo depois de Sprite_Unpack, com o formato que o chamador sabe que é (docs/x360/formatos/
   menus.md 4): BootCard_Init ("menu_cartao"), FirstRun_Main ("menu_logos"), McFlow_Init ("menu_mcflow") e
   Title_Init ("menu_titulo"). Cada Sprite_Unpack desses aloca um buffer novo: não há conversão dupla. */
void Port_ConvertMenuPack(void *buf, int size, const char *formato) {
    char msg[240];
    conv_u8 *feito;
    int r;
    convertido(buf, formato);
    if (size <= 0) {
        falha("conv: %s com 0x%X bytes descomprimidos", formato, (unsigned)size);
    }
    feito = (conv_u8 *)calloc((size_t)size, 1);
    if (feito == NULL) {
        falha("conv: %s: sem memória", formato);
    }
    msg[0] = '\0';
    r = Conv_Formato(formato, formato, (conv_u8 *)buf, (conv_ofs)size, feito, msg, sizeof(msg));
    free(feito);
    if (r == 0) {
        falha("conv: formato %s desconhecido", formato);
    }
    if (r < 0) {
        falha("conv: %s de 0x%X bytes: %s", formato, (unsigned)size, msg);
    }
}
