/* Base da conversão por formato: acesso com limite, troca com marcação e o registro dos formatos (conv.h). */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conv.h"

void Conv_Iniciar(ConvCtx *c, const char *nome, conv_u8 *buf, conv_ofs tam, conv_u8 *feito) {
    c->buf = buf;
    c->tam = tam;
    c->nome = nome;
    c->feito = feito;
    c->bits = 0;
    c->erro = 0;
    c->msg[0] = '\0';
}

void Conv_Erro(ConvCtx *c, const char *fmt, ...) {
    va_list ap;
    if (c->erro) {
        return; /* fica a primeira mensagem: é ela que explica */
    }
    c->erro = 1;
    va_start(ap, fmt);
    vsprintf(c->msg, fmt, ap); /* C89: sem vsnprintf no cl -TC; as mensagens são curtas */
    va_end(ap);
}

void Conv_Onde(ConvCtx *c, const char *fmt, unsigned long n) {
    char aqui[64];
    size_t t = strlen(c->msg);
    if (!c->erro) {
        return;
    }
    sprintf(aqui, fmt, n);
    if (t + strlen(aqui) + 4 < sizeof(c->msg)) {
        sprintf(c->msg + t, " [%s]", aqui);
    }
}

int Conv_Cabe(ConvCtx *c, conv_ofs ofs, conv_ofs n, const char *oque) {
    if (c->erro) {
        return 0;
    }
    if (ofs > c->tam || n > c->tam - ofs) {
        Conv_Erro(c, "%s: 0x%lX+0x%lX passa do fim (0x%lX)", oque, (unsigned long)ofs, (unsigned long)n,
                  (unsigned long)c->tam);
        return 0;
    }
    return 1;
}

conv_u32 Conv_Ler32(ConvCtx *c, conv_ofs ofs) {
    const conv_u8 *p;
    if (!Conv_Cabe(c, ofs, 4, "leitura u32")) {
        return 0;
    }
    p = c->buf + ofs;
    return (conv_u32)p[0] | (conv_u32)p[1] << 8 | (conv_u32)p[2] << 16 | (conv_u32)p[3] << 24;
}

conv_u16 Conv_Ler16(ConvCtx *c, conv_ofs ofs) {
    const conv_u8 *p;
    if (!Conv_Cabe(c, ofs, 2, "leitura u16")) {
        return 0;
    }
    p = c->buf + ofs;
    return (conv_u16)(p[0] | p[1] << 8);
}

/* a marca do byte i: a largura (mapa de bytes), ou só CONV_BYTE / CONV_LIVRE (mapa de bits) */
static conv_u8 marca_de(const ConvCtx *c, conv_ofs i) {
    if (c->bits) {
        return (c->feito[i >> 3] >> (i & 7)) & 1 ? CONV_BYTE : CONV_LIVRE;
    }
    return c->feito[i];
}

/* marca [ofs, ofs+w) como um número de largura w (ou bytes, w = CONV_BYTE com n bytes); 0 se já estava marcado */
static int marcar(ConvCtx *c, conv_ofs ofs, conv_ofs n, conv_u8 w) {
    conv_ofs i;
    for (i = 0; i < n; i++) {
        if (marca_de(c, ofs + i) != CONV_LIVRE) {
            Conv_Erro(c, "0x%lX convertido duas vezes (marca %u, agora %u)", (unsigned long)(ofs + i),
                      (unsigned)marca_de(c, ofs + i), (unsigned)w);
            return 0;
        }
    }
    if (c->bits) {
        for (i = ofs; i < ofs + n; i++) {
            c->feito[i >> 3] |= (conv_u8)(1u << (i & 7));
        }
    } else if (w == CONV_BYTE) {
        memset(c->feito + ofs, CONV_BYTE, n);
    } else {
        c->feito[ofs] = w;
        memset(c->feito + ofs + 1, CONV_CONT, n - 1);
    }
    return 1;
}

static void trocar(conv_u8 *p, int w) {
    int i;
    for (i = 0; i < w / 2; i++) {
        conv_u8 t = p[i];
        p[i] = p[w - 1 - i];
        p[w - 1 - i] = t;
    }
}

conv_u16 Conv_U16(ConvCtx *c, conv_ofs ofs) {
    conv_u16 v = Conv_Ler16(c, ofs);
    if (!c->erro && marcar(c, ofs, 2, 2)) {
        trocar(c->buf + ofs, 2);
    }
    return v;
}

conv_u32 Conv_U32(ConvCtx *c, conv_ofs ofs) {
    conv_u32 v = Conv_Ler32(c, ofs);
    if (!c->erro && marcar(c, ofs, 4, 4)) {
        trocar(c->buf + ofs, 4);
    }
    return v;
}

void Conv_U64(ConvCtx *c, conv_ofs ofs) {
    if (Conv_Cabe(c, ofs, 8, "u64") && marcar(c, ofs, 8, 8)) {
        trocar(c->buf + ofs, 8);
    }
}

void Conv_U16s(ConvCtx *c, conv_ofs ofs, conv_ofs n) {
    conv_ofs i;
    if (!Conv_Cabe(c, ofs, n * 2, "trecho u16")) {
        return;
    }
    for (i = 0; i < n && !c->erro; i++) {
        Conv_U16(c, ofs + i * 2);
    }
}

void Conv_U32s(ConvCtx *c, conv_ofs ofs, conv_ofs n) {
    conv_ofs i;
    if (!Conv_Cabe(c, ofs, n * 4, "trecho u32")) {
        return;
    }
    for (i = 0; i < n && !c->erro; i++) {
        Conv_U32(c, ofs + i * 4);
    }
}

void Conv_Bytes(ConvCtx *c, conv_ofs ofs, conv_ofs n) {
    if (Conv_Cabe(c, ofs, n, "bytes")) {
        marcar(c, ofs, n, CONV_BYTE);
    }
}

conv_u8 Conv_Marca(ConvCtx *c, conv_ofs ofs) {
    return ofs < c->tam ? marca_de(c, ofs) : CONV_LIVRE;
}

void Conv_Zeros(ConvCtx *c, conv_ofs ini, conv_ofs fim, const char *oque) {
    conv_ofs i;
    if (!Conv_Cabe(c, ini, fim - ini, oque)) {
        return;
    }
    for (i = ini; i < fim; i++) {
        if (marca_de(c, i) == CONV_LIVRE) {
            if (c->buf[i] != 0) {
                Conv_Erro(c, "%s: byte 0x%lX não reconhecido (não é enchimento)", oque, (unsigned long)i);
                return;
            }
            marcar(c, i, 1, CONV_BYTE);
        }
    }
}

/* ------------------------------------------------------------------ registro */

/* os formatos (um arquivo .c cada) */
void Conv_Personagem(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_PersonagemAnim(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_PersonagemEfeitos(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_BobjAnim(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_Comum1(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_Comum2(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_Comum3(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_Comum4(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_Hud(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_Cenario(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_RoteiroLuta(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_TextosLuta(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_LabioLuta(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_BancoSom(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_FolhaBpe(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_Folha(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_Rosto(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuPacote(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuBpe(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuCartao(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuLogos(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuMcFlow(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuTitulo(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuPrincipal(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuDuelo(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuSelecao(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuTreino(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuOpcoes(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuTexFile(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuFundo(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuHistSagas(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuHistLista(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuHistResultado(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuHistSave(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuSaga(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuHistFundo(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuUbMenu(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuSimInicio(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuDiscFusion(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuUbSelecao(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuUbResultado(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuUbSel(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuSimDia(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuUbRanking(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuUbCursos(ConvCtx *c, conv_ofs base, conv_ofs tam);

typedef struct Formato { const char *nome; ConvFn fn; } Formato;
void Conv_MenuEvoTopo(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuLoja(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuEvoZ(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuDcTopo(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuDcLista(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuDcSenha(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuReplay(ConvCtx *c, conv_ofs base, conv_ofs tam);

void Conv_MenuTourMenu(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuEntrySel(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuTourBg(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_MenuBracket(ConvCtx *c, conv_ofs base, conv_ofs tam);

static const Formato sFormatos[] = {
    {"personagem", Conv_Personagem},           /* file[0] do slot: modelo, texturas, parâmetros, lábio */
    {"personagem_anim", Conv_PersonagemAnim},  /* file[1]: animações em BPE (só a tabela e os cabeçalhos) */
    {"personagem_efeitos", Conv_PersonagemEfeitos}, /* file[2]: pacote de efeitos */
    {"animacao", Conv_BobjAnim},               /* uma animação descomprimida (Conv_Animacao, em tempo de jogo) */
    {"comum1", Conv_Comum1},                   /* comuns.md: 00001, tabelas da luta e IA */
    {"comum2", Conv_Comum2},                   /* 00002, efeitos comuns */
    {"comum3", Conv_Comum3},                   /* 00003, personagens e itens */
    {"comum4", Conv_Comum4},                   /* 00004, fontes, ícones, avisos do controle */
    {"hud", Conv_Hud},                         /* 00005 e 00006, HUD e menu de pausa por idioma */
    {"som", Conv_BancoSom},                    /* som-e-sprites.md 1: banco de som */
    {"folha_bpe", Conv_FolhaBpe},              /* 2: folha da tela de carregamento (só o cabeçalho BPE) */
    {"folha", Conv_Folha},                     /* 2.3: a folha descomprimida (em tempo de jogo) */
    {"rosto", Conv_Rosto},                     /* 3: rosto de membro */
    {"cenario", Conv_Cenario},                 /* cenario.md: pacote de 25 entradas do cenário da luta */
    /* historia-luta.md: os arquivos da luta de um episódio da História (F4, etapa 9a) */
    {"roteiro_luta", Conv_RoteiroLuta},        /* 2: o roteiro de evento (GSCF, Gsc_LoadFile) */
    {"textos_luta", Conv_TextosLuta},          /* 3: os textos das falas (BtlScript_GetText) */
    {"labio_luta", Conv_LabioLuta},            /* 4: as faixas de boca das falas (BtlScript_GetFile8Entry) */
    {"menu_pacote", Conv_MenuPacote},          /* menus.md 2: arquivo de menu (tabela de seções em BPE) */
    {"menu_bpe", Conv_MenuBpe},                /* 3: um contêiner BPE inteiro (recursos do cartão, imagens) */
    /* menus.md 5: pacotes de tela descomprimidos, em tempo de jogo (Port_ConvertMenuPack, um por chamador) */
    {"menu_cartao", Conv_MenuCartao},          /* 5.1: tela do cartão (BootCard_Init) */
    {"menu_logos", Conv_MenuLogos},            /* 5.2: logos do boot (FirstRun_Main) */
    {"menu_mcflow", Conv_MenuMcFlow},          /* 5.3: recursos do cartão (McFlow_Init) */
    {"menu_titulo", Conv_MenuTitulo},          /* 5.4: tela de título (Title_Init) */
    {"menu_principal", Conv_MenuPrincipal},    /* menus-pecas.md 6: menu principal (MainMenu_Init) */
    /* menus-telas.md: o resto do caminho essencial (F4, etapa 5b) */
    {"menu_duelo", Conv_MenuDuelo},            /* 2: submenu do Duelo (DuelMenu_Init) */
    {"menu_selecao", Conv_MenuSelecao},        /* 3: CharSel e TeamSel (CharSel_Init, TeamSel_Init) */
    {"menu_treino", Conv_MenuTreino},          /* 4: Treino (Train_Init) */
    {"menu_opcoes", Conv_MenuOpcoes},          /* 5: Opções (Option_Init) */
    {"menu_texfile", Conv_MenuTexFile},        /* 6: imagem única (retratos, páginas do Treino) */
    {"menu_fundo", Conv_MenuFundo},            /* 6: imagem de fundo (estágios) */
    /* menus-historia.md: a História (F4, etapa 9a) */
    {"menu_hist_sagas", Conv_MenuHistSagas},   /* 2: seleção de saga (HistSel_Init) */
    {"menu_hist_lista", Conv_MenuHistLista},   /* 3: lista de episódios e fim da saga (ModeMenu_Init, HistOutro_Init) */
    {"menu_hist_resultado", Conv_MenuHistResultado}, /* 4: resultado (HistResult_Init) */
    {"menu_hist_save", Conv_MenuHistSave},     /* 5: save (HistSave_Init) */
    {"menu_saga", Conv_MenuSaga},              /* 7: o arquivo de uma saga */
    {"menu_hist_fundo", Conv_MenuHistFundo},   /* 8: fundo do resultado */
    /* menus-ub.md: o Ultimate Battle (F4, etapa 9b) */
    {"menu_ub_menu", Conv_MenuUbMenu},         /* A: menu do Ultimate Battle (UbMenu_Init, modo 13) */
    {"menu_sim_inicio", Conv_MenuSimInicio},   /* A: topo do Sim Dragon (SimTop_Init, modo 20) */
    {"menu_discfusion", Conv_MenuDiscFusion},  /* A: Disc Fusion (DiscFusion_Init, modo 24) */
    {"menu_ub_selecao", Conv_MenuUbSelecao},   /* B: seleção da Missão 100 e da Sobrevivência (MisSel_Init, SurvSel_Init) */
    {"menu_ub_resultado", Conv_MenuUbResultado}, /* B: resultados (MisResult_Init, SurvResult_Init, SimResult_Init,
                                                     UbResult_Init) */
    {"menu_ubsel", Conv_MenuUbSel},            /* C: seleção de um lutador e de um time (SoloSel_Init, UbTeamSel_Init) */
    {"menu_sim_dia", Conv_MenuSimDia},         /* D: o tabuleiro do Sim Dragon (SimDay_Init, 0x1DB inteiro) */
    {"menu_ub_ranking", Conv_MenuUbRanking},   /* E: o ranking de 100 lugares (UbRank_Init, modo 26) */
    {"menu_evo_topo", Conv_MenuEvoTopo},
    {"menu_loja", Conv_MenuLoja},
    {"menu_evoz", Conv_MenuEvoZ},
    {"menu_dc_topo", Conv_MenuDcTopo},         /* Data Center e Replay (menus-dc.md): DcMenu_Init, modo 53 */
    {"menu_dc_lista", Conv_MenuDcLista},       /* DcList_Init, modo 55 */
    {"menu_dc_senha", Conv_MenuDcSenha},       /* DcPass_Init, modo 54 */
    {"menu_tourmenu", Conv_MenuTourMenu},       /* menus-torneio.md, modo 33 */
    {"menu_entrysel", Conv_MenuEntrySel},       /* modo 34, os cinco guias */
    {"menu_tourbg", Conv_MenuTourBg},           /* fundos dos modos 34 e 35 */
    {"menu_bracket", Conv_MenuBracket},         /* modo 35, os cinco torneios */
    {"menu_replay", Conv_MenuReplay},          /* ReplayMenu_Init, modo 56 */
    {"menu_ub_cursos", Conv_MenuUbCursos},     /* E: os cinco cursos (UbzSel_Init, modo 28) */
};

/* Que formato tem cada arquivo do disco e cada conteúdo descomprimido dele: a tabela única (arquivo e seção para
   formato). Os arquivos não dizem o próprio formato: vem de como o jogo escolhe o arquivo de cada coisa, pelo id. Os
   ids de arquivo do jogo 1..0xD47 são a partição 1 (pzs3us1/NNNNN.bin, índice id - 1; include/sys/file.h).
   Seção (ConvFaixa.secao): CONV_ARQ é o arquivo como está no disco (Conv_Arquivo, iso360.c); 0 é o conteúdo do
   contêiner BPE que começa no deslocamento 0 do arquivo; n >= 1 é o conteúdo da seção n da tabela do arquivo
   (menu_pacote, menus.md 2.1). O conteúdo descomprimido se converte em tempo de jogo, pelo gancho de quem chama o
   Sprite_Unpack (menus.md 4); a conv_tool usa estas linhas no --mapa e no --disco. As faixas da mesma seção não se
   sobrepõem (a conv_tool confere a cada execução: faixas_conferidas). */
static const ConvFaixa sFaixas[] = {
    /* arquivos comuns (comuns.md): ids 2..5 lidos na partida, 6 e 7 o HUD de cada idioma */
    {1, 1, CONV_ARQ, "comum1"},
    {2, 2, CONV_ARQ, "comum2"},
    {3, 3, CONV_ARQ, "comum3"},
    {4, 4, CONV_ARQ, "comum4"},
    {5, 6, CONV_ARQ, "hud"},
    /* som-e-sprites.md 4: rostos dos membros (id 8 + chara*2 + lado), bancos de som (comuns, menu, cenários,
       vozes) */
    {7, 328, CONV_ARQ, "rosto"},
    {329, 367, CONV_ARQ, "som"},
    {3033, 3354, CONV_ARQ, "som"},
    /* cenários (cenario.md, seção 1): BtlLoad_* pedem 0x171 + cenário (BTL_FILE_STAGE) ou 0x198 + cenário em tela
       dividida (BTL_FILE_STAGE_SPLIT), ShenScene 0x194 + dragão e ChrView 0x197; ids 0x171..0x1BE, índices
       368..445, todos com a tabela de 25 entradas (conferida no conversor) */
    {0x171 - 1, 0x1BE - 1, CONV_ARQ, "cenario"},
    /* historia-luta.md 1: os arquivos da luta de um episódio da História, lidos inteiros na carga da luta. O roteiro de
       evento é 0x1FF + script (BTL_FILE_SCRIPT, battle_load.c; o script do ModeMenu_GetLine, 0..47, e o disco tem 50
       roteiros, ids 0x1FF..0x230); os textos das falas, 0x231 + história (inglês) e 0x263 + história (comando 701,
       BtlScriptCmd_LoadText); as bocas das falas, 0x295 + história e 0x2C7 + história (comando 702, pelo conjunto de
       vozes do save), até os retratos em 0x2F9 */
    {0x1FF - 1, 0x230 - 1, CONV_ARQ, "roteiro_luta"},
    {0x231 - 1, 0x294 - 1, CONV_ARQ, "textos_luta"},
    {0x295 - 1, 0x2F8 - 1, CONV_ARQ, "labio_luta"},
    /* folhas da tela de carregamento (som-e-sprites.md 2, id 0x3C1 + tipo): o arquivo é um contêiner BPE, e a folha
       descomprimida se converte em Load_InitScreen (Port_ConvertLoadSheet) */
    {960, 962, CONV_ARQ, "folha_bpe"},
    {960, 962, 0, "folha"},
    /* menus.md 1: arquivos de menu do caminho do boot ao título, ids a partir de MenuProgress.baseFile = 0x1C1.
       0x1DE (fileBase + 0x1D) é o de McFlow_Init; o conteúdo, os recursos do cartão */
    {0x1DE - 1, 0x1DE - 1, CONV_ARQ, "menu_bpe"},
    {0x1DE - 1, 0x1DE - 1, 0, "menu_mcflow"},
    /* baseFile + 0x19 (0x1DA, cartão e logos, FirstRun_Main): seção 2 a tela do cartão (BootCard_Run(2)), seção 3
       os logos */
    {0x1DA - 1, 0x1DA - 1, CONV_ARQ, "menu_pacote"},
    {0x1DA - 1, 0x1DA - 1, 2, "menu_cartao"},
    {0x1DA - 1, 0x1DA - 1, 3, "menu_logos"},
    /* baseFile + 1 (0x1C2, título e menu principal): seção 1 o título (Title_Run(1)), seção 2 o menu principal
       (MainMenu_Init(2), menu_a_b.c) */
    {0x1C2 - 1, 0x1C2 - 1, CONV_ARQ, "menu_pacote"},
    {0x1C2 - 1, 0x1C2 - 1, 1, "menu_titulo"},
    {0x1C2 - 1, 0x1C2 - 1, 2, "menu_principal"},
    /* os arquivos de menu do resto do caminho essencial (menus-modos.md 1, menus-telas.md), pacotes de seções em BPE
       como o 0x1C2: baseFile + 0 (0x1C1, Duelo: DuelMenu_Init a seção 1, CharSel e TeamSel a 2), + 5 (0x1C6,
       Opções: Option_Init a 1) e + 13 (0x1CE, Treino: Train_Init a 1, o CharSel do treino a 2, com o layout da 2 do
       Duelo) */
    {0x1C1 - 1, 0x1C1 - 1, CONV_ARQ, "menu_pacote"},
    {0x1C1 - 1, 0x1C1 - 1, 1, "menu_duelo"},
    {0x1C1 - 1, 0x1C1 - 1, 2, "menu_selecao"},
    {0x1C6 - 1, 0x1C6 - 1, CONV_ARQ, "menu_pacote"},
    {0x1C6 - 1, 0x1C6 - 1, 1, "menu_opcoes"},
    {0x1CE - 1, 0x1CE - 1, CONV_ARQ, "menu_pacote"},
    {0x1CE - 1, 0x1CE - 1, 1, "menu_treino"},
    {0x1CE - 1, 0x1CE - 1, 2, "menu_selecao"},
    /* imagens únicas, o arquivo inteiro num contêiner BPE (menus-telas.md 6): os retratos (0x2F9 + personagem, todos
       os leitores somam 0x2F9 a um personagem: 0..0xA0 e os marcadores 0xA1..0xA3, ids 0x2F9..0x39C), as imagens de
       estágio (0x39D + estágio, 0..0x23; o cursor da grade só para em estágio desbloqueado, StgGrid_Move*, e os
       marcadores travado e vazio, 0x24 e 0x25 no PS2, dariam as folhas de carregamento) e as páginas do Treino
       (0x402 + página; as aulas vão da página 0 à 397, a última antes dos slots de lutador em 0x590) */
    {0x2F9 - 1, 0x39C - 1, CONV_ARQ, "menu_bpe"},
    {0x2F9 - 1, 0x39C - 1, 0, "menu_texfile"},
    {0x39D - 1, 0x3C0 - 1, CONV_ARQ, "menu_bpe"},
    {0x39D - 1, 0x3C0 - 1, 0, "menu_fundo"},
    {0x402 - 1, 0x58F - 1, CONV_ARQ, "menu_bpe"},
    {0x402 - 1, 0x58F - 1, 0, "menu_texfile"},
    /* a História (menus-historia.md): baseFile + 0xC (0x1CD, gMenuArc2), pacote de quatro seções: 1 a seleção de saga
       (HistSel_Run(1)), 2 a lista de episódios e o fim da saga (ModeMenu_Run(2), HistOutro_Run(2)), 3 o resultado
       (HistResult_Run(3)), 4 o save (HistSave_Run(4)); o arquivo de cada saga, baseFile + 0x10 + saga (0x1D1..0x1D8),
       um contêiner BPE inteiro; as imagens dos episódios (imageBase + episódio, imageBase de 0x3CE a 0x3FA por saga:
       ids 0x3CE..0x3FD, lidas pelo registro 0) e os fundos do resultado (0x3FE + fundo, 0..3) */
    {0x1CD - 1, 0x1CD - 1, CONV_ARQ, "menu_pacote"},
    {0x1CD - 1, 0x1CD - 1, 1, "menu_hist_sagas"},
    {0x1CD - 1, 0x1CD - 1, 2, "menu_hist_lista"},
    {0x1CD - 1, 0x1CD - 1, 3, "menu_hist_resultado"},
    {0x1CD - 1, 0x1CD - 1, 4, "menu_hist_save"},
    {0x1D1 - 1, 0x1D8 - 1, CONV_ARQ, "menu_bpe"},
    {0x1D1 - 1, 0x1D8 - 1, 0, "menu_saga"},
    {0x3CE - 1, 0x3FD - 1, CONV_ARQ, "menu_bpe"},
    {0x3CE - 1, 0x3FD - 1, 0, "menu_texfile"},
    {0x3FE - 1, 0x401 - 1, CONV_ARQ, "menu_bpe"},
    {0x3FE - 1, 0x401 - 1, 0, "menu_hist_fundo"},
    /* o Ultimate Battle (menus-ub.md): baseFile + 3 (0x1C4, gMenuArc3, Ub_Main), pacote de seis seções: 1 o menu
       (UbMenu_Run(1)), 2 a seleção da Missão 100 e da Sobrevivência (MisSel_Run(2), SurvSel_Run(2)), 3 os resultados
       (MisResult, SurvResult, SimResult e UbResult _Run(3)), 4 a seleção de um lutador e de um time (SoloSel_Run(4),
       UbTeamSel_Run(4)), 5 o topo do Sim Dragon (SimTop_Run(5)), 6 o Disc Fusion (DiscFusion_Run(6)); o tabuleiro do
       Sim Dragon, baseFile + 0x1A (0x1DB, SimDay_Init), o ranking, + 0x1B (0x1DC, UbRank_Init), e os cursos, + 0x1C
       (0x1DD, UbzSel_Init), cada um um contêiner BPE inteiro. Os retratos que essas telas carregam já estão na faixa
       dos retratos */
    {0x1C4 - 1, 0x1C4 - 1, CONV_ARQ, "menu_pacote"},
    {0x1C4 - 1, 0x1C4 - 1, 1, "menu_ub_menu"},
    {0x1C4 - 1, 0x1C4 - 1, 2, "menu_ub_selecao"},
    {0x1C4 - 1, 0x1C4 - 1, 3, "menu_ub_resultado"},
    {0x1C4 - 1, 0x1C4 - 1, 4, "menu_ubsel"},
    {0x1C4 - 1, 0x1C4 - 1, 5, "menu_sim_inicio"},
    {0x1C4 - 1, 0x1C4 - 1, 6, "menu_discfusion"},
    {0x1DB - 1, 0x1DB - 1, CONV_ARQ, "menu_bpe"},
    {0x1DB - 1, 0x1DB - 1, 0, "menu_sim_dia"},
    {0x1DC - 1, 0x1DC - 1, CONV_ARQ, "menu_bpe"},
    {0x1DC - 1, 0x1DC - 1, 0, "menu_ub_ranking"},
    {0x1DD - 1, 0x1DD - 1, CONV_ARQ, "menu_bpe"},
    {0x1DD - 1, 0x1DD - 1, 0, "menu_ub_cursos"},
    /* Evolution Z, baseFile + 4 (gMenuArc7): topo, loja e customização, menus-evo.md. */
    {0x1C5 - 1, 0x1C5 - 1, CONV_ARQ, "menu_pacote"},
    {0x1C5 - 1, 0x1C5 - 1, 1, "menu_evo_topo"},
    {0x1C5 - 1, 0x1C5 - 1, 2, "menu_loja"},
    {0x1C5 - 1, 0x1C5 - 1, 3, "menu_evoz"},
    /* Torneio (menus-torneio.md): gMenuArc4, as cinco chaves, convites e fundos. */
    {0x1C3 - 1, 0x1C3 - 1, CONV_ARQ, "menu_pacote"},
    {0x1C3 - 1, 0x1C3 - 1, 1, "menu_tourmenu"},
    {0x1C3 - 1, 0x1C3 - 1, 2, "menu_entrysel"},
    {0x1C7 - 1, 0x1CB - 1, CONV_ARQ, "menu_bpe"},
    {0x1C7 - 1, 0x1CB - 1, 0, "menu_bracket"},
    {0x3C4 - 1, 0x3C8 - 1, CONV_ARQ, "menu_bpe"},
    {0x3C4 - 1, 0x3C8 - 1, 0, "menu_texfile"},
    {0x3C9 - 1, 0x3CD - 1, CONV_ARQ, "menu_bpe"},
    {0x3C9 - 1, 0x3CD - 1, 0, "menu_tourbg"},
    /* Data Center, baseFile + 0xB (gMenuArc8): topo, lista, senha e replay, menus-dc.md. */
    {0x1CC - 1, 0x1CC - 1, CONV_ARQ, "menu_pacote"},
    {0x1CC - 1, 0x1CC - 1, 1, "menu_dc_topo"},
    {0x1CC - 1, 0x1CC - 1, 2, "menu_dc_lista"},
    {0x1CC - 1, 0x1CC - 1, 3, "menu_dc_senha"},
    {0x1CC - 1, 0x1CC - 1, 4, "menu_replay"},
};

const ConvFaixa *Conv_Faixas(int *n) {
    *n = (int)(sizeof(sFaixas) / sizeof(sFaixas[0]));
    return sFaixas;
}

/* o índice do arquivo na partição 1 (id - 1), ou -1 se o nome não é de lá */
static int indice_de(const char *nome) {
    int i;
    char resto[8];
    if (sscanf(nome, "pzs3us1/%5d.%3s", &i, resto) != 2 || strcmp(resto, "bin") != 0 || i < 0 || i >= 0xD47) {
        return -1;
    }
    return i;
}

static const char *da_tabela(int i, int secao) {
    unsigned k;
    for (k = 0; k < sizeof(sFaixas) / sizeof(sFaixas[0]); k++) {
        if (i >= sFaixas[k].ini && i <= sFaixas[k].fim && sFaixas[k].secao == secao) {
            return sFaixas[k].formato;
        }
    }
    return NULL;
}

/* slot do lutador (BtlObj_RequestCharaModel, src/battle/btl_obj.c; CHRVIEW_FILE_FIRST em battle/view_a.h):
   file[0] = chara*10 + traje + 0x590 (+4 na variante danificada), file[1] = chara*10 + 0x598,
   file[2] = chara*10 + 0x599. Uma regra, não uma faixa: fica fora da tabela. */
static const char *do_lutador(int i) {
    int k = i + 1 - 0x590;
    if (k >= 0 && k < 161 * 10) { /* 161 personagens (SAVE_CHARA_COUNT); depois disso a faixa é de outros formatos */
        switch (k % 10) {
        case 8: return "personagem_anim";
        case 9: return "personagem_efeitos";
        default: return "personagem";
        }
    }
    return NULL;
}

static const char *formato_de(const char *nome) {
    int i = indice_de(nome);
    const char *f;
    if (i < 0) {
        return NULL;
    }
    f = da_tabela(i, CONV_ARQ);
    return f != NULL ? f : do_lutador(i);
}

const char *Conv_FormatoSecao(const char *nome, int secao) {
    int i = indice_de(nome);
    const char *f;
    if (i < 0) {
        return NULL;
    }
    f = do_lutador(i);
    if (f != NULL) {
        /* as animações em BPE do file[1], em qualquer deslocamento (BtlObjAnim_Load, Port_ConvertAnim) */
        return strcmp(f, "personagem_anim") == 0 ? "animacao" : NULL;
    }
    return da_tabela(i, secao);
}

static int converter(ConvFn fn, const char *nome, conv_u8 *buf, conv_ofs tam, conv_u8 *feito, int bits, char *msg,
                     int nmsg) {
    ConvCtx c;
    Conv_Iniciar(&c, nome, buf, tam, feito);
    c.bits = bits;
    fn(&c, 0, tam);
    if (c.erro) {
        if (msg != NULL && nmsg > 0) {
            strncpy(msg, c.msg, (size_t)nmsg - 1);
            msg[nmsg - 1] = '\0';
        }
        return -1;
    }
    return 1;
}

static int com_formato(const char *formato, const char *nome, conv_u8 *buf, conv_ofs tam, conv_u8 *feito, int bits,
                       char *msg, int nmsg) {
    unsigned i;
    for (i = 0; i < sizeof(sFormatos) / sizeof(sFormatos[0]); i++) {
        if (strcmp(sFormatos[i].nome, formato) == 0) {
            return converter(sFormatos[i].fn, nome, buf, tam, feito, bits, msg, nmsg);
        }
    }
    return 0;
}

int Conv_Formato(const char *formato, const char *nome, conv_u8 *buf, conv_ofs tam, conv_u8 *feito, char *msg,
                 int nmsg) {
    return com_formato(formato, nome, buf, tam, feito, 0, msg, nmsg);
}

int Conv_FormatoBits(const char *formato, const char *nome, conv_u8 *buf, conv_ofs tam, conv_u8 *bits, char *msg,
                     int nmsg) {
    return com_formato(formato, nome, buf, tam, bits, 1, msg, nmsg);
}

int Conv_Arquivo(const char *nome, conv_u8 *buf, conv_ofs tam, conv_u8 *feito, char *msg, int nmsg) {
    const char *f = formato_de(nome);
    return f != NULL ? Conv_Formato(f, nome, buf, tam, feito, msg, nmsg) : 0;
}

const char *Conv_FormatoDe(const char *nome) {
    return formato_de(nome);
}

int Conv_BpeFecha(const conv_u8 *p, conv_ofs comp) {
    conv_ofs in = 0;
    while (in < comp) {
        int cont = p[in++], c = 0, i;
        for (;;) { /* a tabela de pares: um byte por par igual ao índice, dois nos outros */
            if (cont > 127) {
                c += cont - 127;
                cont = 0;
            }
            if (c == 256) {
                break;
            }
            for (i = 0; i <= cont && c < 256; i++, c++) {
                if (in + 2 > comp) {
                    return 0;
                }
                if (p[in++] != c) {
                    in++;
                }
            }
            if (c >= 256 || in >= comp) {
                break;
            }
            cont = p[in++];
        }
        if (in + 2 > comp) {
            return 0;
        }
        in += 2 + ((conv_ofs)p[in] << 8 | p[in + 1]); /* o tamanho do bloco (big-endian no próprio formato) */
    }
    return in == comp;
}


int Conv_Animacao(conv_u8 *buf, conv_ofs tam, char *msg, int nmsg) {
    conv_u8 *feito = (conv_u8 *)calloc(tam ? tam : 1, 1);
    int r;
    if (feito == NULL) {
        if (msg != NULL && nmsg > 0) {
            strncpy(msg, "sem memória para a marcação", (size_t)nmsg - 1);
            msg[nmsg - 1] = '\0';
        }
        return -1;
    }
    r = Conv_Formato("animacao", "animação", buf, tam, feito, msg, nmsg);
    free(feito);
    return r;
}
