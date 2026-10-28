/*
 * Roteiros de controle (F4, etapa 4; o formato está no guia, docs/x360/guia-de-port.md, seção 2, "Roteiros de
 * controle"; os roteiros, em port/x360/roteiros): o controle do jogador 1 tirado de um
 * arquivo de texto, passo a passo, com cada espera ancorada no estado do jogo (a tela do gerenciador de menus, o
 * estado do fluxo do cartão, a luta) e não no número do quadro. Uma diferença de tempo entre a x86, a PPC no qemu e o
 * console (uma carga mais lenta, uma tela que demora a aceitar entrada) não desalinha o resto do roteiro.
 *
 *   BT3_ROTEIRO=<arquivo>   liga o roteiro (sem ela, nada muda: Roteiro_Pad devolve 0 e o controle fica parado)
 *   BT3_DESBLOQUEIO=1       permite os passos que mudam o estado do jogo: "desbloqueia" (Save_UnlockAll do próprio
 *                           jogo, o mesmo da aba de truques da janela do PC) e "termina luta" (o adversário derrubado
 *                           pelo dano do próprio jogo, BtlMember_Damage; o jogo decide a luta) e "funde discos" (o
 *                           Disc Fusion sem a troca de disco: gProgress->discFlags); sem ela o passo para o roteiro
 *                           com erro. O "libera desejo" zera o saldo antes do Shenlong, sob as mesmas proteções.
 *                           Nunca no jogo normal. E só com o cartão vazio no começo da rodada (sem o arquivo de dados
 *                           do save na pasta de saves, BT3_SAVES): o desbloqueio e a vitória dada iriam para o save de
 *                           verdade na próxima gravação
 *
 * Com o roteiro ligado, cada troca de tela vai ao log ("tela N vblank V quadro Q"), e cada espera cumprida também
 * ("roteiro: arquivo:linha ok ..."): é o rastro que os roteiros e as comparações usam. Na build sem janela (x86 e
 * PPC) o log é a saída padrão e o passo "fim" encerra o programa; no 360 o log é o bt3xbx.log e o "fim" só solta o
 * controle. Falha (espera esgotada, arquivo ruim): mensagem e saída com 2 sem janela, tela vermelha no 360.
 *
 * (F5, etapa 3) A luta também: as condições "acao", "seq", "pausa", "dano", "ki" e "janela", o passo "confere luta" e,
 * com "rastreia luta", a sequência, a matriz de ações, o resultado e as janelas da medida de tempo (passos.c) no rastro
 * (o guia, "Roteiros de luta"; os roteiros f5_duelo.txt e f5_treino.txt).
 *
 * (F5, etapa 9) O analógico esquerdo, para o visualizador 3D da Referência (o OrbitCam gira com ele): o passo "gira" e
 * a tela 600, a Referência (modo 60) com o modelo aberto (CHARREF_ST_VIEW_WAIT e _VIEW), para o rastro, a memória e as imagens.
 *
 * Entra pelo Port_PadRead: sem_janela.c (x86 e PPC) e pad360.c (360). O BT3_PAD_PLAY, se também estiver ligado,
 * tem a precedência (plat_stub.c nem chama o Port_PadRead).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stddef.h>

#include "sys/save.h"      /* gSaveData (SaveData), Save_UnlockAll */
#include "battle/battle.h" /* BattleWork (Battle_GetWork): a regra, os lados, o resultado e as bandeiras da luta */

/* O campo do "confere som" onde o jogo o tem (save.h: 0x16A4). Este arquivo compila com os tipos do jogo
   (port_compat.h, forçado como nos fontes do jogo: s64 de 64 bits); sem eles o SaveData sai menor e o campo cai em
   0x1694, lendo outra opção. A build para aqui se isso voltar a acontecer. */
typedef char RoteiroSaveConfere[offsetof(SaveData, soundMode) == 0x16A4 ? 1 : -1];
typedef char RoteiroSagaConfere[offsetof(SaveData, slot) == 0x10 && sizeof(SaveSlot) == 0x10 ? 1 : -1];
/* (F5, etapa 3) o bloco da luta como os passos "confere luta" e o rastro do resultado o leem */
typedef char RoteiroLutaConfere[offsetof(BattleWork, flags) == 0x19F0 && offsetof(BattleWork, result) == 0x1938 &&
                                offsetof(BattleSetup, sides) == 0xC0 && sizeof(BattleSide) == 0x270 ? 1 : -1];

/* O estado do jogo que identifica a tela. Os tipos completos são do jogo; aqui só os campos lidos, nos mesmos
   deslocamentos (as três builds têm ponteiros de 32 bits). */
extern struct { int unk0[5]; int flags; int mode; } *gProgress; /* include/menu/menu_a.h: flags 0x14, mode 0x18 */
extern struct { int unk0[9]; int state; } *gMcFlow;             /* include/sys/mcflow_a.h: state 0x24; NULL fora */
extern struct { int state; } *gBtlSeq;                         /* include/battle/btl_seq.h: >= 2 com a luta andando */
/* (F5, etapa 9) a Referência de personagens (include/menu/menu_h.h: state em 0x1628, CHARREF_ST_VIEW 7, o modelo 3D
   aberto); NULL fora da tela, o jogo o zera ao sair (menu_h_b.c). A mesma struct de 0x1638 bytes nas três builds. */
extern struct { char unk0[0x1628]; int state; } *gCharRef;
#define CHARREF_VIEW 7       /* CHARREF_ST_VIEW: o modelo na tela */
#define CHARREF_VIEW_WAIT 8  /* CHARREF_ST_VIEW_WAIT: a carga do modelo, depois da escolha do traje */
/* o golpe que derruba (passo "termina luta"): o lutador ativo de um lado (BtlChar_Get, src/battle/btl_char_get.c; cada
   arquivo do jogo o declara com a sua visão da struct), se ainda tem vida (BtlCtrl_IsActiveDead, btl_capi_b.c), e o
   dano do próprio jogo (BtlMember_Damage, src/battle/btl_char_member.c) */
void *BtlChar_Get(int side);
int BtlCtrl_IsActiveDead(int side);
int BtlMember_Damage(void *chr, int amount, int flags);
#define BTL_SEQ_LUTANDO 3 /* include/battle/btl_seq.h: o estado 3 (BtlSeqFight), o único em que o fim se confere */
/* include/battle/btl_char_member.h: sem defesa, exato, sem contar como golpe do combo nem mexer no total e no tempo
   dele, a qualquer estado, sem a metade da animação e sem a escala do combo (e sem BTL_DMG_NO_KILL nem
   BTL_DMG_DRAIN, que deixam 1 de vida) */
#define TERMINA_DANO_FLAGS (0x1 | 0x2 | 0x8 | 0x10 | 0x20 | 0x40 | 0x80 | 0x100)
#define TERMINA_DANO 100000000 /* mais que qualquer barra de vida (o jogo prende em 0) */
static int sDerruba = -1;        /* o lado a derrubar no quadro do jogo em andamento (-1: nenhum) */
static int sDerrubados1;         /* adversários (lado 1) derrubados pelo passo em andamento ("perde apos N") */
static char sDerrubaOnde[80];    /* o passo que pediu, para o rastro */
extern int gPortMenuMode;                                      /* port/src/headless.c: 1 enquanto os menus rodam */
extern unsigned gPortVBlanks;                                  /* port/src/plat_stub.c */

/* (F5, etapa 3) Os lutadores, só lidos: o roster (gBtlChars, include/battle/btl_char_get.h: a contagem e o vetor de
   0x1600 bytes por lutador) e, em cada lutador, o lado (+0x08), a ação atual (+0x948, BtlActState.current,
   include/battle/btl_char_action.h), o membro ativo (+0x994) e as barras dele (membros em +0x9A4, 0xA4 bytes cada;
   BtlMemberGauge em +0x40: vida +0, ki +0xC; include/battle/btl_char_member.h). Lidos aqui pelos deslocamentos, sem
   chamar código do jogo: o rastreio (x86r) não vê estas leituras, e nada do jogo muda. */
extern struct { int count; unsigned char *chars; } *gBtlChars;
#define LUT_TAM 0x1600
#define LUT_LADO 0x8
#define LUT_ACAO 0x948
#define LUT_ATIVO 0x994
#define LUT_MEMBROS 0x9A4
#define LUT_MEMBRO_TAM 0xA4
#define LUT_VIDA 0x40
#define LUT_KI 0x4C
#define LUT_BLAST 0x54 /* BtlMemberGauge.blast em membro +0x40 */

#ifdef PORT_X360
void Diario_Escrever(const char *formato, ...);
void Port_Abortar(void); /* main360.cpp: log e tela vermelha */
#endif

#ifndef PORT_X360
/* as imagens das telas (imagens.c, F4 etapa 6, BT3_IMAGENS): cada linha do rastro pode ser a âncora de uma imagem, e
   cada leitura do controle diz se o quadro seguinte se desenha e se grava */
void Imagens_Evento(const char *linha, unsigned q);
void Imagens_Quadro(unsigned q);
#endif
static unsigned sQuadro;        /* leituras do jogador 1 desde o começo do roteiro */

static void diz(const char *linha) {
#ifdef PORT_X360
    Diario_Escrever("%s", linha);
#else
    printf("%s\n", linha);
    fflush(stdout);
    Imagens_Evento(linha, sQuadro);
#endif
}

/* (F5, etapa 3) a medida do tempo por passo (passos.c): a janela aberta fecha no "fim" do roteiro; e o que ela tem de
   determinístico (passos e vblanks de cada janela) vem para o rastro por aqui, só com o roteiro ligado e o passo
   "rastreia luta" (sRastroLuta: a sequência, a matriz, o resultado e as janelas no rastro; sem ele, o rastro dos
   roteiros que já existiam continua o mesmo, e as referências deles valem) */
void Passos_Fim(const char *motivo);
unsigned Passos_NaJanela(void);
int Roteiro_Ativo(void);
static int sRastroLuta;
void Roteiro_Rastro(const char *linha) {
    if (Roteiro_Ativo() && sRastroLuta) {
        diz(linha);
    }
}

static void falha(const char *linha) {
    char s[400];
    sprintf(s, "roteiro: FALHA: %s", linha);
    diz(s);
#ifdef PORT_X360
    Port_Abortar();
#else
    exit(2);
#endif
}

/* ------------------------------------------------------------------ a tela atual */

/* Códigos da tela, pela ordem: o boot (logos e cartão, antes do primeiro título), a luta, a carga entre os menus e
   a luta, e no resto o modo do gerenciador de menus (gProgress->mode, Progress_Main em src/menu/menu_a_b.c). */
#define TELA_INICIO (-1)
#define TELA_LUTA (-2)
#define TELA_CARGA (-3)
#define TELA_REF 60      /* o modo da Referência (CharRef_Run) */
#define TELA_MODELO 600  /* (F5, etapa 9) a Referência com o visualizador 3D aberto (ChrView: a carga do modelo e o
                            modelo na tela), não um modo do jogo */
#define MPROG_FIRST_RUN 0x40

static int tela_atual(void) {
    if (gPortMenuMode && gProgress != NULL) {
        if (gProgress->flags & MPROG_FIRST_RUN) {
            return TELA_INICIO;
        }
        if (gProgress->mode == TELA_REF && gCharRef != NULL &&
            (gCharRef->state == CHARREF_VIEW || gCharRef->state == CHARREF_VIEW_WAIT)) {
            return TELA_MODELO;
        }
        return gProgress->mode;
    }
    if (gBtlSeq != NULL && gBtlSeq->state >= 2) {
        return TELA_LUTA;
    }
    return gProgress == NULL || (gProgress->flags & MPROG_FIRST_RUN) ? TELA_INICIO : TELA_CARGA;
}

static int mcflow_atual(void) {
    return gMcFlow != NULL ? gMcFlow->state : -1;
}

static void nome_tela(int t, char *s) {
    if (t == TELA_INICIO) {
        strcpy(s, "inicio");
    } else if (t == TELA_LUTA) {
        strcpy(s, "luta");
    } else if (t == TELA_CARGA) {
        strcpy(s, "carga");
    } else {
        sprintf(s, "%d", t);
    }
}

/* ------------------------------------------------------------------ a luta (F5, etapa 3) */

/* o lutador do lado (0 o jogador, 1 o adversário), só com a luta andando; NULL fora dela */
static const unsigned char *lutador(int lado) {
    int i;
    if (tela_atual() != TELA_LUTA || gBtlChars == NULL || gBtlChars->chars == NULL) {
        return NULL;
    }
    for (i = 0; i < gBtlChars->count && i < 2; i++) {
        const unsigned char *c = gBtlChars->chars + i * LUT_TAM;
        if (*(const int *)(c + LUT_LADO) == lado) {
            return c;
        }
    }
    return NULL;
}

/* Campos do membro ativo, apenas leitura, inclusive durante a troca de forma. */
static int membro_de(int lado, int campo) {
    const unsigned char *c = lutador(lado);
    int ativo;
    if (c == NULL) return -1;
    ativo = *(const int *)(c + LUT_ATIVO);
    if (ativo < 0 || ativo >= 5) return -1;
    return *(const int *)(c + LUT_MEMBROS + ativo * LUT_MEMBRO_TAM + campo);
}

static int acao_de(int lado) {
    const unsigned char *c = lutador(lado);
    return c != NULL ? *(const int *)(c + LUT_ACAO) : -1;
}

/* uma barra do membro ativo (LUT_VIDA ou LUT_KI); -1 fora da luta */
static int barra_de(int lado, int qual) {
    const unsigned char *c = lutador(lado);
    int ativo;
    if (c == NULL) {
        return -1;
    }
    ativo = *(const int *)(c + LUT_ATIVO);
    if (ativo < 0 || ativo >= 5) {
        return -1;
    }
    return *(const int *)(c + LUT_MEMBROS + ativo * LUT_MEMBRO_TAM + qual);
}

/* o estado da sequência da luta (btl_seq.h: 3 o combate, 4 o anúncio do fim...); -1 fora da luta */
static int seq_atual(void) {
    return tela_atual() == TELA_LUTA ? gBtlSeq->state : -1;
}

/* o menu de pausa aberto (BATTLE_FLAG_PAUSE_MENU, sempre com BATTLE_FLAG_PAUSE); -1 fora da luta */
static int pausa_atual(void) {
    if (tela_atual() != TELA_LUTA) {
        return -1;
    }
    return (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE_MENU) != 0;
}

/* A matriz de ações: as famílias de ações do lutador (o número da ação, gBtlActHandlers; os nomes dos handlers em
   port/data/cod_1C3400.data.s, a tabela em docs/systems/combat.md), contadas por entrada numa ação da família durante
   o combate (o estado 3 da sequência). O rastro a escreve ao sair do estado 3, por lado. */
static const struct { const char *nome; int de, ate; } kFamilias[] = {
    {"movimento", 0x0D, 0x35},     /* andar, dash, subir e descer, passos, sumiços, aproximações */
    {"carga", 0x37, 0x37},         /* BtlAct_ChargeHandler: a carga de ki */
    {"defesa", 0x38, 0x43},        /* BtlAct_GuardHandler e as do bloqueio */
    {"fisico", 0x44, 0x94},        /* rush, smash e os golpes da tabela (BtlAct_AttackHandler, 0x70..0xAD) */
    {"projetil", 0x95, 0x95},      /* BtlAct_KiVolley95, a rajada */
    {"fisico", 0x96, 0xAD},
    {"projetil", 0xAE, 0xB3},      /* as esferas de ki (BtlAct_KiBlastHandler...) */
    {"agarrao", 0xB4, 0xBC},
    {"reacao", 0xBD, 0xEB},        /* golpe recebido, queda, levantar */
    {"transformacao", 0xEC, 0xF0},
    {"fusao", 0xF1, 0xF2},
    {"troca", 0xF3, 0xF9},
    {"choque", 0xFA, 0xFC},
    {"habilidade", 0xFD, 0x103},   /* Blast 1 */
    {"choque", 0x104, 0x104},
    {"tecnica", 0x105, 0x12F},     /* Blast 2 e Ultimate do atacante (BtlAct_Super*), com câmera e efeitos */
    {"choque", 0x130, 0x132},
    {"tecnica_alvo", 0x133, 0x13B}, /* o lado que recebe a técnica */
};
static const char *const kNomesFamilia[] = {"movimento", "carga", "defesa", "fisico", "projetil", "tecnica",
                                            "habilidade", "agarrao", "choque", "reacao", "tecnica_alvo",
                                            "transformacao", "fusao", "troca", "outras"};
#define N_FAMILIAS ((int)(sizeof(kNomesFamilia) / sizeof(kNomesFamilia[0])))
static unsigned sMatriz[2][N_FAMILIAS];
/* (sRastroLuta: o passo "rastreia luta", acima) */
static int sAcaoVista[2] = {-1, -1};
static int sSeqVista = -1;

static int familia(int acao) {
    unsigned k;
    int f;
    for (k = 0; k < sizeof(kFamilias) / sizeof(kFamilias[0]); k++) {
        if (acao >= kFamilias[k].de && acao <= kFamilias[k].ate) {
            for (f = 0; f < N_FAMILIAS; f++) {
                if (strcmp(kNomesFamilia[f], kFamilias[k].nome) == 0) {
                    return f;
                }
            }
        }
    }
    return N_FAMILIAS - 1; /* outras: neutro, espera, busca, as de roteiro */
}

static void diz(const char *linha);

/* a matriz do combate que terminou (ou que o "fim" do roteiro cortou) no rastro */
static void diz_matriz(void) {
    char s[400];
    int lado, f;
    for (lado = 0; lado < 2; lado++) {
        int n = sprintf(s, "roteiro: matriz lado %d", lado);
        for (f = 0; f < N_FAMILIAS; f++) {
            n += sprintf(s + n, " %s %u", kNomesFamilia[f], sMatriz[lado][f]);
        }
        sprintf(s + n, " vblank %u quadro %u", gPortVBlanks, sQuadro);
        diz(s);
    }
}

/* A cada leitura do controle: com o combate andando (estado 3), as entradas em ações (a matriz, zerada ao entrar no
   estado 3); ao sair do estado 3, a matriz e o resultado que o jogo gravou (BattleResult: o vencedor BTL_RESULT_*,
   a razão BTL_REASON_*, include/battle/battle.h) no rastro. A pausa não sai do estado 3. */
static void luta_quadro(void) {
    char s[300];
    int lado, seq = seq_atual();
    if (seq == BTL_SEQ_LUTANDO && sSeqVista != BTL_SEQ_LUTANDO) {
        memset(sMatriz, 0, sizeof(sMatriz));
        sAcaoVista[0] = sAcaoVista[1] = -1;
    }
    if (seq == BTL_SEQ_LUTANDO) {
        for (lado = 0; lado < 2; lado++) {
            int a = acao_de(lado);
            if (a >= 0 && a != sAcaoVista[lado]) {
                sMatriz[lado][familia(a)]++;
            }
            sAcaoVista[lado] = a;
        }
    }
    if (!sRastroLuta) {
        sSeqVista = seq;
        return;
    }
    if (seq != sSeqVista && seq >= 0) {
        sprintf(s, "roteiro: seq %d vblank %u quadro %u", seq, gPortVBlanks, sQuadro);
        diz(s);
    }
    if (sSeqVista == BTL_SEQ_LUTANDO && seq != BTL_SEQ_LUTANDO) {
        diz_matriz();
        if (seq >= 0) {
            const BattleResult *r = &Battle_GetWork()->result;
            sprintf(s, "roteiro: resultado estado %d vencedor 0x%x razao 0x%x vida %d %d vblank %u quadro %u", seq,
                    (unsigned)r->winner, (unsigned)r->reason, barra_de(0, LUT_VIDA), barra_de(1, LUT_VIDA),
                    gPortVBlanks, sQuadro);
            diz(s);
        }
    }
    sSeqVista = seq;
}

/* F7: mudanças de forma, pedido e débito do recurso, sem escrever na luta.
   O custo é observado no quadro em que o estoque cai, sem confundir regeneração durante a animação. */
static void f7_quadro(void) {
    static int habilitado = -1, idAntes = -1, acaoAntes = -1, blastAntes = -1;
    int id, acao, blast;
    const unsigned char *c;
    char s[300];
    if (habilitado < 0) habilitado = getenv("BT3_F7") != NULL;
    if (!habilitado || (c = lutador(0)) == NULL) return;
    id = membro_de(0, 0); acao = acao_de(0); blast = membro_de(0, LUT_BLAST);
    if (id != idAntes || (acao != acaoAntes && acao >= 0xEC && acao <= 0xF0) ||
        (blastAntes >= 0 && blast < blastAntes)) {
        sprintf(s, "f7: membro id %d traje %d variante %d blast %d anterior %d acao 0x%x pedido %d custo %d classe %d vblank %u quadro %u",
                id, membro_de(0, 4), membro_de(0, 0x60), blast, blastAntes, (unsigned)acao,
                *(const int *)(c + 0x12D0), *(const int *)(c + 0x12D8), *(const int *)(c + 0x12DC), gPortVBlanks, sQuadro);
        diz(s);
    }
    idAntes = id; acaoAntes = acao; blastAntes = blast;
}

#ifndef PORT_X360
/* Débito exato observado na chamada do jogo; só a ligação sem janela usa este desvio. */
void __real_BtlMember_SubBlast(void *chr, int amount);
void __wrap_BtlMember_SubBlast(void *chr, int amount) {
    int antes = membro_de(0, LUT_BLAST);
    char s[200];
    __real_BtlMember_SubBlast(chr, amount);
    if (getenv("BT3_F7") != NULL && chr == (void *)lutador(0)) {
        sprintf(s, "f7: debito custo %d antes %d depois %d vblank %u quadro %u",
                amount, antes, membro_de(0, LUT_BLAST), gPortVBlanks, sQuadro);
        diz(s);
    }
}
#endif

/* ------------------------------------------------------------------ o roteiro */

enum { P_ESPERA, P_QUADROS, P_APERTA, P_SEGURA, P_DESBLOQUEIA, P_MARCA, P_FIM, P_REPETE, P_VOLTA, P_CONFERE, P_TERMINA,
       P_FUNDE, P_DESEJO, P_RASTREIA, P_GIRA, P_MISSOES };
enum { C_NADA, C_TELA, C_MCFLOW, C_LUTA, C_ACAO, C_SEQ, C_PAUSA, C_DANO, C_KI, C_JANELA, C_FORMA, C_BLAST };

typedef struct Passo {
    int tipo;
    unsigned botoes;    /* P_APERTA, P_SEGURA: os bits de pad360.c (1 = apertado) */
    int n;              /* P_QUADROS, P_SEGURA: quadros; P_APERTA: vezes (sem "ate") */
    int segura, solta;  /* P_APERTA: o ritmo de cada vez */
    int cond, valor;    /* P_ESPERA e P_APERTA com "ate": a condição e o valor; P_CONFERE: cond 0 som, 1 saga, 2 um
                           u8 e 3 um s32 do save (n: o deslocamento), 4 a luta (luta[]), e o valor esperado ou a saga;
                           P_TERMINA: valor 1 vence, 0 perde */
    int lado, ate;      /* (F5, etapa 3) C_ACAO: o lado e a faixa valor..ate; C_DANO e C_KI: o lado */
    int ref;            /* C_DANO: a vida do lado no começo do passo (-1: ainda sem luta) */
    int lx, ly;         /* (F5, etapa 9) P_GIRA: o analógico esquerdo, de -128 a 127 (0 no centro) */
    int luta[8];        /* P_CONFERE da luta: personagem e traje dos lados 0 e 1, estágio, modo, tempo, nível do COM */
    int max;            /* quadros até a falha */
    int par;            /* P_REPETE: o índice do P_VOLTA; P_VOLTA: o do P_REPETE; P_TERMINA "perde apos N": N */
    char onde[80];      /* arquivo:linha, para o log */
    char texto[80];     /* P_MARCA */
} Passo;

static Passo *sPassos;
static int sN, sCap;
static int sPasso, sT;          /* o passo atual e os quadros dele */
static int sAtivo = -1;         /* -1: ainda não lido */
static int sTela = 99999, sMcflow = -99999;
static int sSegura = 4, sSolta = 8, sMax = 6000;
static int sProf;               /* profundidade dos "inclui" */
static int sAbertos[8], sNAbertos; /* os "repete" ainda sem "volta", na leitura */
static struct { int passo, t, inicio; } sLaco[8]; /* os "repete" em andamento: o passo, quadros nele, quadro da volta */
static int sNLaco;
static int sCartaoVazio = 1;    /* sem o arquivo de dados do save no começo da rodada (P_DESBLOQUEIA) */
static unsigned char sLx = 0x80, sLy = 0x80; /* (F5, etapa 9) o analógico esquerdo deste quadro (0x80 no centro) */

static const struct { const char *nome; unsigned bit; } kBotoes[] = {
    {"SELECT", 0x0001}, {"L3", 0x0002}, {"R3", 0x0004}, {"START", 0x0008}, {"CIMA", 0x0010}, {"DIREITA", 0x0020},
    {"BAIXO", 0x0040}, {"ESQUERDA", 0x0080}, {"L2", 0x0100}, {"R2", 0x0200}, {"L1", 0x0400}, {"R1", 0x0800},
    {"TRIANGULO", 0x1000}, {"CIRCULO", 0x2000}, {"X", 0x4000}, {"QUADRADO", 0x8000},
};

static int igual(const char *a, const char *b) { /* sem distinguir maiúsculas */
    while (*a != '\0' && *b != '\0') {
        char x = *a >= 'a' && *a <= 'z' ? (char)(*a - 32) : *a, y = *b >= 'a' && *b <= 'z' ? (char)(*b - 32) : *b;
        if (x != y) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static void erro_linha(const char *onde, const char *msg, const char *palavra) {
    char s[300];
    sprintf(s, "%s: %s%s%s", onde, msg, palavra != NULL ? ": " : "", palavra != NULL ? palavra : "");
    falha(s);
}

static int numero(const char *onde, const char *p) {
    char *fim;
    long v;
    if (p == NULL) {
        erro_linha(onde, "falta um número", NULL);
    }
    v = strtol(p, &fim, 0);
    if (fim == p || *fim != '\0') {
        erro_linha(onde, "número inválido", p);
    }
    return (int)v;
}

/* "X", "START+X" */
static unsigned botoes(const char *onde, const char *p) {
    unsigned b = 0;
    char nome[32];
    while (p != NULL && *p != '\0') {
        const char *mais = strchr(p, '+');
        size_t n = mais != NULL ? (size_t)(mais - p) : strlen(p);
        unsigned k, achou = 0;
        if (n == 0 || n >= sizeof(nome)) {
            erro_linha(onde, "botão inválido", p);
        }
        memcpy(nome, p, n);
        nome[n] = '\0';
        for (k = 0; k < sizeof(kBotoes) / sizeof(kBotoes[0]); k++) {
            if (igual(nome, kBotoes[k].nome)) {
                achou = kBotoes[k].bit;
            }
        }
        if (!achou) {
            erro_linha(onde, "botão desconhecido", nome);
        }
        b |= achou;
        p = mais != NULL ? mais + 1 : NULL;
    }
    return b;
}

/* a condição em pal[i..]: "tela N" | "mcflow N" | "luta"; depois, opcional, "max N". Devolve o índice seguinte. */
static int condicao(Passo *p, char **pal, int n, int i) {
    if (i < n && igual(pal[i], "tela")) {
        p->cond = C_TELA;
        p->valor = numero(p->onde, i + 1 < n ? pal[i + 1] : NULL);
        i += 2;
    } else if (i < n && igual(pal[i], "mcflow")) {
        p->cond = C_MCFLOW;
        p->valor = numero(p->onde, i + 1 < n ? pal[i + 1] : NULL);
        i += 2;
    } else if (i < n && igual(pal[i], "luta")) {
        p->cond = C_LUTA;
        i += 1;
    } else if (i < n && igual(pal[i], "acao")) {
        /* (F5, etapa 3) acao L A[-B]: a ação atual do lutador do lado L (0 o jogador) é A, ou está entre A e B */
        char *traco;
        p->cond = C_ACAO;
        p->lado = numero(p->onde, i + 1 < n ? pal[i + 1] : NULL);
        if (i + 2 >= n) {
            erro_linha(p->onde, "acao L A[-B]", NULL);
        }
        traco = strchr(pal[i + 2], '-');
        if (traco != NULL) {
            *traco = '\0';
            p->ate = numero(p->onde, traco + 1);
        }
        p->valor = numero(p->onde, pal[i + 2]);
        if (traco == NULL) {
            p->ate = p->valor;
        }
        if (p->lado < 0 || p->lado > 1 || p->valor < 0 || p->ate < p->valor || p->ate >= 0x13C) {
            erro_linha(p->onde, "acao L A[-B]: lado 0 ou 1, ações de 0 a 0x13B", NULL);
        }
        i += 3;
    } else if (i < n && igual(pal[i], "seq")) { /* seq N: o estado da sequência da luta (btl_seq.h) */
        p->cond = C_SEQ;
        p->valor = numero(p->onde, i + 1 < n ? pal[i + 1] : NULL);
        i += 2;
    } else if (i < n && igual(pal[i], "pausa")) { /* pausa 1|0: o menu de pausa da luta aberto ou fechado */
        p->cond = C_PAUSA;
        p->valor = numero(p->onde, i + 1 < n ? pal[i + 1] : NULL) != 0;
        i += 2;
    } else if (i < n && igual(pal[i], "dano")) { /* dano L: o lado L perdeu vida desde o começo do passo */
        p->cond = C_DANO;
        p->lado = numero(p->onde, i + 1 < n ? pal[i + 1] : NULL);
        p->ref = -1;
        i += 2;
    } else if (i < n && igual(pal[i], "janela")) {
        /* janela N: a janela contínua de combate da medida de tempo (passos.c: o estado 3 sem pausa) tem N passos */
        p->cond = C_JANELA;
        p->valor = numero(p->onde, i + 1 < n ? pal[i + 1] : NULL);
        i += 2;
    } else if (i < n && (igual(pal[i], "forma") || igual(pal[i], "blast"))) {
        p->cond = igual(pal[i], "forma") ? C_FORMA : C_BLAST;
        p->lado = numero(p->onde, i + 1 < n ? pal[i + 1] : NULL);
        p->valor = numero(p->onde, i + 2 < n ? pal[i + 2] : NULL);
        if (p->lado < 0 || p->lado > 1 || p->valor < 0) erro_linha(p->onde, "forma/blast L N: lado 0 ou 1, N >= 0", NULL);
        i += 3;
    } else if (i < n && igual(pal[i], "ki")) { /* ki L N: o ki do lado L chegou a N (100000 a barra cheia) */
        p->cond = C_KI;
        p->lado = numero(p->onde, i + 1 < n ? pal[i + 1] : NULL);
        p->valor = numero(p->onde, i + 2 < n ? pal[i + 2] : NULL);
        i += 3;
    } else {
        erro_linha(p->onde, "condição esperada (tela N, mcflow N, luta, acao L A[-B], seq N, pausa N, dano L, ki L N ou "
                   "janela N)",
                   i < n ? pal[i] : NULL);
    }
    if ((p->cond == C_DANO || p->cond == C_KI) && (p->lado < 0 || p->lado > 1)) {
        erro_linha(p->onde, "lado 0 ou 1", NULL);
    }
    p->max = sMax;
    if (i < n && igual(pal[i], "max")) {
        p->max = numero(p->onde, i + 1 < n ? pal[i + 1] : NULL);
        i += 2;
    }
    return i;
}

static Passo *novo(const char *onde, int tipo) {
    Passo *p;
    if (sN == sCap) {
        sCap = sCap ? sCap * 2 : 64;
        sPassos = (Passo *)realloc(sPassos, (size_t)sCap * sizeof(Passo));
    }
    p = &sPassos[sN++];
    memset(p, 0, sizeof(*p));
    p->tipo = tipo;
    strncpy(p->onde, onde, sizeof(p->onde) - 1);
    return p;
}

static void ler(const char *caminho);

/* o nome do arquivo sem a pasta: depois da última "/" ou "\" (o 360 usa as duas) */
static const char *sem_pasta(const char *caminho) {
    const char *a = strrchr(caminho, '/'), *b = strrchr(caminho, '\\');
    if (b != NULL && (a == NULL || b > a)) {
        a = b;
    }
    return a != NULL ? a + 1 : caminho;
}

/* uma linha já sem o comentário, em palavras */
static void linha(const char *caminho, const char *onde, char **pal, int n) {
    Passo *p;
    int i;
    if (n == 0) {
        return;
    }
    if (igual(pal[0], "inclui")) {
        /* relativo à pasta do arquivo que inclui */
        char outro[300];
        size_t k = (size_t)(sem_pasta(caminho) - caminho);
        if (n != 2 || k + strlen(pal[1]) >= sizeof(outro)) {
            erro_linha(onde, "inclui ARQUIVO", NULL);
        }
        memcpy(outro, caminho, k);
        strcpy(outro + k, pal[1]);
        ler(outro);
        return;
    }
    if (igual(pal[0], "ritmo")) { /* ritmo SEGURA SOLTA: quadros apertado e solto de cada "aperta" seguinte */
        if (n != 3) {
            erro_linha(onde, "ritmo SEGURA SOLTA", NULL);
        }
        sSegura = numero(onde, pal[1]);
        sSolta = numero(onde, pal[2]);
        if (sSegura < 1 || sSolta < 1) {
            erro_linha(onde, "ritmo com zero", NULL);
        }
        return;
    }
    if (igual(pal[0], "espera")) { /* espera tela N | mcflow N | luta [max Q] */
        p = novo(onde, P_ESPERA);
        i = condicao(p, pal, n, 1);
    } else if (igual(pal[0], "quadros")) { /* quadros N: o controle parado */
        p = novo(onde, P_QUADROS);
        p->n = numero(onde, n > 1 ? pal[1] : NULL);
        i = 2;
    } else if (igual(pal[0], "aperta")) { /* aperta B[+B] [N] | aperta B ate COND [max Q] */
        p = novo(onde, P_APERTA);
        p->botoes = botoes(onde, n > 1 ? pal[1] : NULL);
        p->segura = sSegura;
        p->solta = sSolta;
        p->n = 1;
        i = 2;
        if (i < n && igual(pal[i], "ate")) {
            i = condicao(p, pal, n, i + 1);
        } else if (i < n) {
            p->n = numero(onde, pal[i++]);
        }
    } else if (igual(pal[0], "segura")) { /* segura B[+B] N: apertado N quadros, sem soltar no fim */
        p = novo(onde, P_SEGURA);
        p->botoes = botoes(onde, n > 1 ? pal[1] : NULL);
        p->n = numero(onde, n > 2 ? pal[2] : NULL);
        i = 3;
    } else if (igual(pal[0], "gira")) { /* (F5, etapa 9) gira LX LY N: o analógico esquerdo em (LX, LY), N quadros */
        p = novo(onde, P_GIRA);
        if (n != 4) {
            erro_linha(onde, "gira LX LY N", NULL);
        }
        p->lx = numero(onde, pal[1]);
        p->ly = numero(onde, pal[2]);
        p->n = numero(onde, pal[3]);
        if (p->lx < -128 || p->lx > 127 || p->ly < -128 || p->ly > 127 || p->n < 1) {
            erro_linha(onde, "gira LX LY N: LX e LY de -128 a 127, N pelo menos 1", NULL);
        }
        i = 4;
    } else if (igual(pal[0], "libera")) {
        if (n != 2 || !igual(pal[1], "desejo")) {
            erro_linha(onde, "libera desejo", NULL);
        }
        p = novo(onde, P_DESEJO);
        i = 2;
    } else if (igual(pal[0], "rastreia")) { /* (F5, etapa 3) rastreia luta: a luta no rastro (sRastroLuta) */
        if (n != 2 || !igual(pal[1], "luta")) {
            erro_linha(onde, "rastreia luta", NULL);
        }
        p = novo(onde, P_RASTREIA);
        i = 2;
    } else if (igual(pal[0], "prepara") && n == 3 && igual(pal[1], "missoes")) {
        p = novo(onde, P_MISSOES);
        p->n = numero(onde, pal[2]);
        if (p->n != 29) erro_linha(onde, "prepara missoes 29: receita fechada da F7", NULL);
        i = 3;
    } else if (igual(pal[0], "confere") && n > 1 && igual(pal[1], "membro")) {
        int k;
        if (n != 8) erro_linha(onde, "confere membro L ID TRAJE VARIANTE BLAST_MIN BLAST_MAX", NULL);
        p = novo(onde, P_CONFERE);
        p->cond = 5;
        for (k = 0; k < 6; k++) p->luta[k] = numero(onde, pal[k + 2]);
        if (p->luta[0] < 0 || p->luta[0] > 1 || p->luta[4] < 0 || p->luta[5] < p->luta[4])
            erro_linha(onde, "confere membro: lado/faixa de Blast inválidos", NULL);
        i = 8;
    } else if (igual(pal[0], "desbloqueia")) {
        p = novo(onde, P_DESBLOQUEIA);
        i = 1;
    } else if (igual(pal[0], "confere") && n > 2 && igual(pal[1], "save")) {
        /* confere save u8|s32 DESL N: um campo do save em memória (gSaveData, include/sys/save.h), pelo deslocamento:
           o que um modo grava no fim (a missão cumprida, os adversários vencidos), conferido no quadro do passo */
        p = novo(onde, P_CONFERE);
        if (n != 5 || !(igual(pal[2], "u8") || igual(pal[2], "s32"))) {
            erro_linha(onde, "confere save u8|s32 DESL N", NULL);
        }
        p->cond = igual(pal[2], "u8") ? 2 : 3;
        p->n = numero(onde, pal[3]);
        if (p->n < 0 || p->n + (p->cond == 2 ? 1 : 4) > (int)sizeof(SaveData) || (p->cond == 3 && p->n % 4 != 0)) {
            erro_linha(onde, "confere save: deslocamento fora do SaveData (ou s32 desalinhado)", pal[3]);
        }
        p->valor = numero(onde, pal[4]);
        i = 5;
    } else if (igual(pal[0], "confere") && n > 1 && igual(pal[1], "luta")) {
        /* (F5, etapa 3) confere luta C0 T0 C1 T1 ESTAGIO MODO TEMPO NIVEL: a luta que o jogo montou (BattleSetup,
           include/battle/battle.h), com ela andando: personagem e traje do membro 0 de cada lado (variante 0), o
           estágio de início e o atual, o modo (BattleRule.mode), o tempo (índice de gBattleTimeLimitTbl) e o nível do
           COM no lado 1 (BattleMember.cpuLevel). Com as opções e a dificuldade padrão o duelo dá tempo 4 e nível 13
           (Save_ResetRules: rule[0] = 3, rule[1] = 2; CharSel_Run) */
        int k;
        p = novo(onde, P_CONFERE);
        p->cond = 4;
        if (n != 10) {
            erro_linha(onde, "confere luta C0 T0 C1 T1 ESTAGIO MODO TEMPO NIVEL", NULL);
        }
        for (k = 0; k < 8; k++) {
            p->luta[k] = numero(onde, pal[2 + k]);
        }
        i = 10;
    } else if (igual(pal[0], "confere")) { /* confere som N | confere saga N: o save em memória (gSaveData) */
        if (n < 2 || !(igual(pal[1], "som") || igual(pal[1], "saga"))) {
            erro_linha(onde, "confere som N | confere saga N", n > 1 ? pal[1] : NULL);
        }
        p = novo(onde, P_CONFERE);
        p->cond = igual(pal[1], "saga"); /* 0: som, 1: saga */
        p->valor = numero(onde, n > 2 ? pal[2] : NULL);
        if (p->cond && (p->valor < 0 || p->valor >= 8)) {
            erro_linha(onde, "confere saga N: N de 0 a 7", pal[2]);
        }
        i = 3;
    } else if (igual(pal[0], "termina")) { /* termina luta [vence|perde [apos N]] [max Q]: o resultado pelo jogo */
        if (n < 2 || !igual(pal[1], "luta")) {
            erro_linha(onde, "termina luta [vence|perde [apos N]] [max Q]", n > 1 ? pal[1] : NULL);
        }
        p = novo(onde, P_TERMINA);
        p->valor = 1;
        p->max = sMax;
        i = 2;
        if (i < n && (igual(pal[i], "vence") || igual(pal[i], "perde"))) {
            p->valor = igual(pal[i], "vence");
            i++;
            /* perde apos N: derruba antes N adversários (os que entram um depois do outro, como na Sobrevivência) e só
               então o lutador do jogador */
            if (!p->valor && i < n && igual(pal[i], "apos")) {
                p->par = numero(onde, i + 1 < n ? pal[i + 1] : NULL);
                if (p->par < 1) {
                    erro_linha(onde, "perde apos N: N de 1 em diante", pal[i + 1]);
                }
                i += 2;
            }
        }
        if (i < n && igual(pal[i], "max")) {
            p->max = numero(onde, i + 1 < n ? pal[i + 1] : NULL);
            i += 2;
        }
    } else if (igual(pal[0], "funde")) { /* funde discos: o Disc Fusion sem trocar de disco (só para os roteiros) */
        if (n != 2 || !igual(pal[1], "discos")) {
            erro_linha(onde, "funde discos", NULL);
        }
        p = novo(onde, P_FUNDE);
        i = 2;
    } else if (igual(pal[0], "marca")) { /* marca TEXTO: uma linha no rastro */
        p = novo(onde, P_MARCA);
        for (i = 1; i < n; i++) {
            if (strlen(p->texto) + strlen(pal[i]) + 2 < sizeof(p->texto)) {
                if (i > 1) {
                    strcat(p->texto, " ");
                }
                strcat(p->texto, pal[i]);
            }
        }
    } else if (igual(pal[0], "repete")) { /* repete ate COND [max Q] ... volta: o bloco até a condição */
        if (n < 2 || !igual(pal[1], "ate") || sNAbertos == 8) {
            erro_linha(onde, "repete ate COND [max Q]", NULL);
        }
        p = novo(onde, P_REPETE);
        i = condicao(p, pal, n, 2);
        sAbertos[sNAbertos++] = sN - 1;
    } else if (igual(pal[0], "volta")) {
        if (sNAbertos == 0) {
            erro_linha(onde, "volta sem repete", NULL);
        }
        p = novo(onde, P_VOLTA);
        p->par = sAbertos[--sNAbertos];
        sPassos[p->par].par = sN - 1;
        i = 1;
    } else if (igual(pal[0], "fim")) {
        p = novo(onde, P_FIM);
        i = 1;
    } else {
        erro_linha(onde, "passo desconhecido", pal[0]);
        return;
    }
    if (i < n) {
        erro_linha(onde, "palavra a mais", pal[i]);
    }
}

static void ler(const char *caminho) {
    FILE *fp;
    char buf[300], onde[80], *pal[16];
    const char *base;
    int num = 0;
    if (++sProf > 8) {
        falha("inclui demais (laço?)");
    }
    fp = fopen(caminho, "r");
    if (fp == NULL) {
        sprintf(buf, "não abre %.200s", caminho);
        falha(buf);
    }
    base = sem_pasta(caminho);
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        char *c = strchr(buf, '#'), *s = buf;
        int n = 0;
        num++;
        if (c != NULL) {
            *c = '\0';
        }
        for (;;) {
            while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
                *s++ = '\0';
            }
            if (*s == '\0' || n == 16) {
                break;
            }
            pal[n++] = s;
            while (*s != '\0' && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n') {
                s++;
            }
        }
        sprintf(onde, "%.60s:%d", base, num);
        linha(caminho, onde, pal, n);
    }
    fclose(fp);
    sProf--;
}

static void iniciar(void) {
    const char *caminho = getenv("BT3_ROTEIRO");
    char s[300];
    sAtivo = caminho != NULL && caminho[0] != '\0';
    if (!sAtivo) {
        return;
    }
    {
        /* o cartão no começo da rodada: o arquivo de dados do save (ou o .new de uma gravação interrompida) na pasta
           de saves, onde port/src/plat_mc.c o põe (host_path: BT3_SAVES, ou "saves", + "/card1/" + o caminho do
           jogo); no 360 o BT3_SAVES é game:\saves (posix360.c) */
        const char *raiz = getenv("BT3_SAVES") != NULL ? getenv("BT3_SAVES") : "saves";
        char dados[400];
        FILE *fp;
        int k;
        for (k = 0; k < 2 && sCartaoVazio; k++) {
            sprintf(dados, "%.300s/card1/BASLUS-21678DBZT3/BASLUS-21678DBZT3%s", raiz, k ? ".new" : "");
            fp = fopen(dados, "rb");
            if (fp != NULL) {
                fclose(fp);
                sCartaoVazio = 0;
            }
        }
    }
    ler(caminho);
    if (sNAbertos != 0) {
        sprintf(s, "%s: repete sem volta", sPassos[sAbertos[0]].onde);
        falha(s);
    }
    sprintf(s, "roteiro: %.200s, %d passos", caminho, sN);
    diz(s);
}

int Roteiro_Ativo(void) {
    if (sAtivo < 0) {
        iniciar();
    }
    return sAtivo;
}

static int cumprida(Passo *p) {
    int a;
    switch (p->cond) {
    case C_TELA:
        return tela_atual() == p->valor;
    case C_MCFLOW:
        return mcflow_atual() == p->valor;
    case C_LUTA:
        return tela_atual() == TELA_LUTA;
    case C_ACAO:
        a = acao_de(p->lado);
        return a >= p->valor && a <= p->ate;
    case C_SEQ:
        return seq_atual() == p->valor;
    case C_PAUSA:
        return pausa_atual() == p->valor;
    case C_DANO:
        /* a vida no começo do passo (ou no primeiro quadro dele com a luta andando) */
        a = barra_de(p->lado, LUT_VIDA);
        if (p->ref < 0) {
            p->ref = a;
            return 0;
        }
        return a >= 0 && a < p->ref;
    case C_FORMA:
        return membro_de(p->lado, 0) == p->valor;
    case C_BLAST:
        return membro_de(p->lado, LUT_BLAST) >= p->valor;
    case C_KI:
        a = barra_de(p->lado, LUT_KI);
        return a >= p->valor;
    case C_JANELA:
        return Passos_NaJanela() >= (unsigned)p->valor;
    }
    return 0;
}

/* o começo de um passo com condição (uma espera, um "aperta ate", um "repete ate"): a referência do "dano" */
static void prepara(Passo *p) {
    if (p->cond == C_DANO) {
        p->ref = barra_de(p->lado, LUT_VIDA);
    }
}

static void cond_texto(const Passo *p, char *s) {
    if (p->cond == C_TELA) {
        sprintf(s, "tela %d", p->valor);
    } else if (p->cond == C_MCFLOW) {
        sprintf(s, "mcflow %d", p->valor);
    } else if (p->cond == C_ACAO) {
        /* (a ação alcançada vai no rastro: a faixa pedida e a que o jogo deu) */
        sprintf(s, "acao %d 0x%x-0x%x (0x%x)", p->lado, (unsigned)p->valor, (unsigned)p->ate, (unsigned)acao_de(p->lado));
    } else if (p->cond == C_SEQ) {
        sprintf(s, "seq %d", p->valor);
    } else if (p->cond == C_PAUSA) {
        sprintf(s, "pausa %d", p->valor);
    } else if (p->cond == C_DANO) {
        sprintf(s, "dano %d (vida %d de %d)", p->lado, barra_de(p->lado, LUT_VIDA), p->ref);
    } else if (p->cond == C_FORMA || p->cond == C_BLAST) {
        sprintf(s, "%s %d %d (%d)", p->cond == C_FORMA ? "forma" : "blast", p->lado, p->valor,
                membro_de(p->lado, p->cond == C_FORMA ? 0 : LUT_BLAST));
    } else if (p->cond == C_KI) {
        sprintf(s, "ki %d %d (%d)", p->lado, p->valor, barra_de(p->lado, LUT_KI));
    } else if (p->cond == C_JANELA) {
        sprintf(s, "janela %d", p->valor);
    } else {
        strcpy(s, "luta");
    }
}

static void proximo(void) {
    sPasso++;
    sT = 0;
}

/* um quadro do roteiro passou: conta nos "repete" em andamento */
static unsigned consome(unsigned b) {
    int k;
    for (k = 0; k < sNLaco; k++) {
        sLaco[k].t++;
    }
    return b;
}

static void ok(const Passo *p) {
    char s[200], c[80];
    cond_texto(p, c);
    sprintf(s, "roteiro: %s ok %s vblank %u quadro %u", p->onde, c, gPortVBlanks, sQuadro);
    diz(s);
}

static void esgotou(const Passo *p) {
    char s[300], c[80], t[16];
    cond_texto(p, c);
    nome_tela(tela_atual(), t);
    sprintf(s, "%s: %d quadros sem %s (tela %s, mcflow %d, vblank %u)", p->onde, p->max, c, t, mcflow_atual(),
            gPortVBlanks);
    falha(s);
}

/* os passos que mudam o estado do jogo (desbloqueia, termina luta): só com BT3_DESBLOQUEIO=1 e com o cartão vazio no
   começo da rodada, para nunca irem a um save de verdade */
static void so_teste(const Passo *p, const char *nome) {
    char s[300];
    if (getenv("BT3_DESBLOQUEIO") == NULL || atoi(getenv("BT3_DESBLOQUEIO")) == 0) {
        sprintf(s, "%s: %s sem BT3_DESBLOQUEIO=1", p->onde, nome);
        falha(s);
    }
    if (!sCartaoVazio) {
        sprintf(s, "%s: %s recusado: o cartão já tinha um save no começo da rodada, e o resultado iria para ele (use "
                "uma pasta de saves vazia em BT3_SAVES)", p->onde, nome);
        falha(s);
    }
}

/* os botões do jogador 1 neste quadro, andando o roteiro */
static unsigned quadro(void) {
    char s[300];
    sLx = sLy = 0x80; /* (o analógico volta ao centro a cada quadro; só o "gira" o move) */
    for (;;) {
        Passo *p;
        int k, saiu = 0;
        /* cada "repete" em andamento termina assim que a condição dele se cumpre, em qualquer ponto do bloco; o de
           fora primeiro, e ele encerra também os de dentro */
        for (k = 0; k < sNLaco && !saiu; k++) {
            Passo *r = &sPassos[sLaco[k].passo];
            if (cumprida(r)) {
                ok(r);
                sPasso = r->par + 1;
                sT = 0;
                sNLaco = k;
                saiu = 1;
            } else if (sLaco[k].t >= r->max) {
                esgotou(r);
            }
        }
        if (saiu) {
            continue;
        }
        if (sPasso >= sN) {
            return 0;
        }
        p = &sPassos[sPasso];
        switch (p->tipo) {
        case P_REPETE:
            prepara(p);
            if (cumprida(p)) { /* já cumprida: o bloco nem roda */
                ok(p);
                sPasso = p->par + 1;
                sT = 0;
                continue;
            }
            sLaco[sNLaco].passo = sPasso;
            sLaco[sNLaco].t = 0;
            sLaco[sNLaco].inicio = (int)sQuadro;
            sNLaco++;
            proximo();
            continue;
        case P_VOLTA:
            if (sLaco[sNLaco - 1].inicio == (int)sQuadro) {
                sprintf(s, "%s: bloco do repete sem nenhum quadro", p->onde);
                falha(s);
            }
            sLaco[sNLaco - 1].inicio = (int)sQuadro;
            sPasso = p->par + 1;
            sT = 0;
            continue;
        case P_ESPERA:
        case P_APERTA:
            if (p->cond != C_NADA) {
                if (sT == 0) {
                    prepara(p);
                }
                if (cumprida(p)) {
                    ok(p);
                    proximo();
                    continue;
                }
                if (sT >= p->max) {
                    esgotou(p);
                }
                sT++;
                if (p->tipo == P_ESPERA) {
                    return consome(0);
                }
                return consome((sT - 1) % (p->segura + p->solta) < p->segura ? p->botoes : 0);
            }
            /* aperta sem condição: n vezes */
            if (sT >= p->n * (p->segura + p->solta)) {
                proximo();
                continue;
            }
            sT++;
            return consome((sT - 1) % (p->segura + p->solta) < p->segura ? p->botoes : 0);
        case P_QUADROS:
        case P_SEGURA:
            if (sT >= p->n) {
                proximo();
                continue;
            }
            sT++;
            return consome(p->tipo == P_SEGURA ? p->botoes : 0);
        case P_GIRA:
            if (sT >= p->n) {
                proximo();
                continue;
            }
            sT++;
            sLx = (unsigned char)(0x80 + p->lx);
            sLy = (unsigned char)(0x80 + p->ly);
            return consome(0);
        case P_MISSOES: {
            int k;
            unsigned char *b = (unsigned char *)gSaveData;
            so_teste(p, "prepara missoes");
            if (gSaveData == NULL || tela_atual() != 4 || gSaveData->unk208 != 0 || b[0x28C] != 0)
                falha("prepara missoes: exige New Game, menu principal e Sobrevivência fechada");
            for (k = 1; k <= 29; k++) {
                if (b[0x28C + 12 * k] != 0) falha("prepara missoes: exige registros ainda vazios");
                b[0x28C + 12 * k] = 1;
            }
            diz("roteiro: receita 29 missoes, ids 1..29; missao 0 inédita; somente bytes cleared");
            proximo();
            continue;
        }
        case P_DESBLOQUEIA:
            so_teste(p, "desbloqueia");
            if (gSaveData == NULL) {
                sprintf(s, "%s: desbloqueia sem save na memória", p->onde);
                falha(s);
            }
            Save_UnlockAll(gSaveData);
            sprintf(s, "roteiro: %s desbloqueado (Save_UnlockAll) vblank %u quadro %u", p->onde, gPortVBlanks, sQuadro);
            diz(s);
            proximo();
            continue;
        case P_CONFERE:
            if (p->cond == 5) {
                int l = p->luta[0], id = membro_de(l, 0), traje = membro_de(l, 4);
                int variante = membro_de(l, 0x60), blast = membro_de(l, LUT_BLAST);
                if (id != p->luta[1] || traje != p->luta[2] || variante != p->luta[3] ||
                    blast < p->luta[4] || blast > p->luta[5]) {
                    sprintf(s, "%s: membro lado %d id %d traje %d variante %d blast %d fora do esperado",
                            p->onde, l, id, traje, variante, blast);
                    falha(s);
                }
                sprintf(s, "roteiro: %s ok membro lado %d id %d traje %d variante %d blast %d vblank %u quadro %u",
                        p->onde, l, id, traje, variante, blast, gPortVBlanks, sQuadro);
                diz(s);
                proximo();
                continue;
            }
            if (p->cond == 4) {
                /* confere luta: o que o jogo montou, lido do BattleSetup com a luta andando */
                const BattleSetup *b;
                int v[8], k, certo = 1;
                if (tela_atual() != TELA_LUTA) {
                    sprintf(s, "%s: confere luta fora da luta", p->onde);
                    falha(s);
                }
                b = &Battle_GetWork()->setup;
                v[0] = b->sides[0].members[0].chara;
                v[1] = b->sides[0].members[0].costume;
                v[2] = b->sides[1].members[0].chara;
                v[3] = b->sides[1].members[0].costume;
                v[4] = b->rule.stage;
                v[5] = b->rule.mode;
                v[6] = b->rule.timeLimit;
                v[7] = b->sides[1].members[0].cpuLevel;
                for (k = 0; k < 8; k++) {
                    certo &= v[k] == p->luta[k];
                }
                certo &= b->rule.curStage == b->rule.stage && b->sides[0].memberCount == 1 &&
                         b->sides[1].memberCount == 1 && b->sides[0].members[0].variant == 0 &&
                         b->sides[1].members[0].variant == 0;
                sprintf(s, "%s luta: lado 0 %d traje %d, lado 1 %d traje %d, estágio %d (atual %d), modo %d, tempo %d, "
                        "nível %d, membros %d e %d, variantes %d e %d", p->onde, v[0], v[1], v[2], v[3], v[4],
                        b->rule.curStage, v[5], v[6], v[7], b->sides[0].memberCount, b->sides[1].memberCount,
                        b->sides[0].members[0].variant, b->sides[1].members[0].variant);
                if (!certo) {
                    char e[400];
                    sprintf(e, "%s; esperado %d %d %d %d %d %d %d %d, um membro por lado, variante 0", s, p->luta[0],
                            p->luta[1], p->luta[2], p->luta[3], p->luta[4], p->luta[5], p->luta[6], p->luta[7]);
                    falha(e);
                }
                {
                    char t[400];
                    sprintf(t, "roteiro: %s vblank %u quadro %u", s, gPortVBlanks, sQuadro);
                    diz(t);
                }
                proximo();
                continue;
            }
            if (gSaveData == NULL) {
                sprintf(s, "%s: confere sem save na memória", p->onde);
                falha(s);
            }
            if (p->cond >= 2) {
                /* confere save u8|s32 DESL N */
                const unsigned char *b = (const unsigned char *)gSaveData + p->n;
                int v = p->cond == 2 ? (int)b[0] : *(const int *)b;
                if (v != p->valor) {
                    sprintf(s, "%s: save +0x%X (%s) %d, esperado %d", p->onde, (unsigned)p->n, p->cond == 2 ? "u8" : "s32",
                            v, p->valor);
                    falha(s);
                }
                sprintf(s, "roteiro: %s ok save +0x%X %d vblank %u quadro %u", p->onde, (unsigned)p->n, v, gPortVBlanks,
                        sQuadro);
                diz(s);
                proximo();
                continue;
            }
            if (p->cond) {
                /* confere saga N: o fim da saga N da História visto (MSLOT_OUTRO_SEEN no SaveSlot N, include/menu/
                   menu_c.h, que o HistOutro_Init grava e o HistSave leva ao cartão) */
                if (!(gSaveData->slot[p->valor].flags & 4)) {
                    sprintf(s, "%s: saga %d sem o fim visto (gSaveData->slot[%d].flags 0x%x, sem o bit 2)", p->onde,
                            p->valor, p->valor, (unsigned)gSaveData->slot[p->valor].flags);
                    falha(s);
                }
                sprintf(s, "roteiro: %s ok saga %d vblank %u quadro %u", p->onde, p->valor, gPortVBlanks, sQuadro);
                diz(s);
                proximo();
                continue;
            }
            if (gSaveData->soundMode != p->valor) {
                sprintf(s, "%s: som %d (gSaveData->soundMode), esperado %d", p->onde, (int)gSaveData->soundMode,
                        p->valor);
                falha(s);
            }
            sprintf(s, "roteiro: %s ok som %d vblank %u quadro %u", p->onde, p->valor, gPortVBlanks, sQuadro);
            diz(s);
            proximo();
            continue;
        case P_TERMINA:
            /* Derruba o lutador do lado que perde pelo dano do próprio jogo (BtlMember_Damage, com a vida inteira):
               ele levanta os eventos de KO (0x4A do lado, 0x48 do outro), e o jogo decide a luta pelo caminho de
               sempre. No Duelo e nos outros modos, BtlSeq_CheckBattleEnd vê o time caído e grava o vencedor (KO); na
               História (modo 1, em que o fim não se confere sozinho) o roteiro de evento do episódio dispara com o KO
               e grava o resultado pelo comando 9 (SetResult, BtlFacade_SetResult). Dar o resultado direto
               (BtlFacade_SetResult fora de um evento) não serve: na História o BtlSeqWinTalk_Enter lê o evento em
               andamento, que não existe, e a x86 caiu. O golpe não pode sair daqui (a leitura do controle, no
               Pad_Update): os eventos levantados ali giram para "anteriores" no Battle_UpdateWork do mesmo quadro, e
               o roteiro de evento (BtlScript_Update, no começo do quadro seguinte) só vê os novos. Ele sai depois do
               giro, onde o jogo dá os golpes: no BtlChars_SampleInput (--wrap nas builds sem janela; no 360, a
               renomeação da chamada no battle.c pelo c89.py; abaixo), logo antes do Battle_Update.
               Espera a luta andar (o estado 3 da sequência); enquanto ela estiver no 3, derruba o ativo do lado que
               perde que estiver de pé (de novo o próximo membro de um time); termina quando a sequência sai do 3 (o
               anúncio do fim). "vence": cai o lado 1; "perde": o 0 */
            so_teste(p, "termina luta");
            if (gPortMenuMode || gBtlSeq == NULL || gBtlSeq->state < BTL_SEQ_LUTANDO) {
                if (p->n) { /* (já derrubou e a luta saiu: não acontece, a sequência passa pelo 4) */
                    p->n = 0;
                    sDerruba = -1;
                    proximo();
                    continue;
                }
                if (sT >= p->max) {
                    sprintf(s, "%s: %d quadros sem a luta andar (o estado 3 da sequência) para terminar", p->onde,
                            p->max);
                    falha(s);
                }
                sT++;
                return consome(0);
            }
            if (gBtlSeq->state != BTL_SEQ_LUTANDO) {
                if (!p->n) {
                    sprintf(s, "%s: a luta saiu do estado 3 (agora %d) sem o passo derrubar ninguém", p->onde,
                            gBtlSeq->state);
                    falha(s);
                }
                sprintf(s, "roteiro: %s luta terminada (%s) vblank %u quadro %u", p->onde,
                        p->valor ? "vence" : "perde", gPortVBlanks, sQuadro);
                diz(s);
                p->n = 0; /* (o passo pode estar num repete) */
                sDerruba = -1;
                proximo();
                continue;
            }
            /* o pedido para o quadro do jogo em andamento: o golpe sai no BtlChars_SampleInput dele */
            if (!p->n) {
                sDerrubados1 = 0; /* (a primeira vez do passo na luta) */
            }
            sDerruba = p->valor || sDerrubados1 < p->par ? 1 : 0;
            strcpy(sDerrubaOnde, p->onde);
            p->n = 1;
            if (sT >= p->max) {
                sprintf(s, "%s: %d quadros e a luta não terminou (estado 3 da sequência)", p->onde, p->max);
                falha(s);
            }
            sT++;
            return consome(0);
        case P_DESEJO:
            /* Todos os dragões têm dinheiro como primeiro desejo. ShenCfm_IsOwned devolve zero para ele,
               mesmo com tudo desbloqueado. Zerar só o saldo deixa espaço para os três prêmios sem saturar;
               a lista, o sorteio, a confirmação e a concessão continuam sendo os do jogo. */
            so_teste(p, "libera desejo");
            if (gSaveData == NULL || gProgress == NULL || gProgress->mode != 4) {
                sprintf(s, "%s: libera desejo exige save na memória e menu principal", p->onde);
                falha(s);
            }
            gSaveData->money = 0;
            sprintf(s, "roteiro: %s desejo de dinheiro livre (saldo 0) vblank %u quadro %u", p->onde,
                    gPortVBlanks, sQuadro);
            diz(s);
            proximo();
            continue;
        case P_FUNDE:
            /* O Disc Fusion pede o disco do BT1 ou do BT2 na bandeja (DiscFusion_UpdateDisc, src/menu/menu_o_b.c): o
               jogo lê o executável do outro disco e só então grava UB_DISC_0 ou UB_DISC_1 em gProgress->discFlags
               (UoProgress +0x684, include/menu/menu_o.h), da sessão, não do save; DiscFusion_Init os lê. No port a
               unidade nunca abre a bandeja (sceCdStatus, port/src/plat_file.c) e o diálogo fica preso. O passo grava os
               dois bits, como se os dois discos tivessem sido lidos, antes da tela; o resto do jogo segue o caminho
               dele. Com as proteções do desbloqueia (só para os roteiros) */
            so_teste(p, "funde discos");
            if (gProgress == NULL) {
                sprintf(s, "%s: funde discos sem gProgress", p->onde);
                falha(s);
            }
            ((int *)gProgress)[0x684 / 4] |= 1 | 2; /* UB_DISC_0 | UB_DISC_1 */
            sprintf(s, "roteiro: %s discos fundidos (gProgress->discFlags 0x%x) vblank %u quadro %u", p->onde,
                    (unsigned)((int *)gProgress)[0x684 / 4], gPortVBlanks, sQuadro);
            diz(s);
            proximo();
            continue;
        case P_RASTREIA:
            sRastroLuta = 1;
            sprintf(s, "roteiro: %s rastreia luta vblank %u quadro %u", p->onde, gPortVBlanks, sQuadro);
            diz(s);
            proximo();
            continue;
        case P_MARCA:
            sprintf(s, "roteiro: %s marca %s vblank %u quadro %u", p->onde, p->texto, gPortVBlanks, sQuadro);
            diz(s);
            proximo();
            continue;
        case P_FIM:
            /* (F5, etapa 3) um combate cortado pelo fim (o Treino): a matriz dele e a janela da medida de tempo */
            if (sRastroLuta && sSeqVista == BTL_SEQ_LUTANDO) {
                diz_matriz();
            }
            Passos_Fim("roteiro");
            sprintf(s, "roteiro: %s fim vblank %u quadro %u", p->onde, gPortVBlanks, sQuadro);
            diz(s);
            sPasso = sN;
            sNLaco = 0;
#ifndef PORT_X360
            exit(0);
#endif
            return 0;
        }
    }
}

/* ------------------------------------------------------------------ o estado do som (F6, etapa 1) */

/* BT3_SOM_ESTADO=N (docs/x360/plano/F6/01.md): o estado dos seis players de som como o jogo o vê, a boca de quem fala
   e a tela de resultado da História, numa linha "som: ..." a cada mudança e, com N > 0, também a cada N leituras do
   controle do jogador 1 (para uma tela parada dizer como parou). Com ou sem roteiro; sem a variável, nada. Fora do
   rastro: a linha não passa pelo diz() (nem pelas âncoras das imagens), e o roteiros.py só lê "tela", "mcflow" e
   "roteiro:". Também as bandeiras do gerenciador de menus (gProgress->flags; 0x100, MPROG_FREEZE, para as telas).
   Só lê memória, sem chamar código do jogo além do ADXT_GetStat do port, como a luta acima.
     players: gAdxPlayerTbl (include/sys/file.h: o handle em +0x10), criados na ordem BGM, MAP, SE0, SE1, VOICE0,
     VOICE1 (File_InitAdx, src/sys/file.c), então o player i do port é o canal i; o estado contado em vblanks é o
     gPortAdxView[i] de port/src/plat_sndstate.c (stat 0 parado, 3 tocando, 5 fim; "p" pausado; l, os vblanks até o
     fim, -1 em laço; a, o arquivo da partição); o ADXT_GetStat, o que o jogo de fato lê, só aparece ("lido") quando
     difere do contado;
     boca: gLipSyncWaiting, gLipSync.active e .time (include/battle/view_b.h, src/battle/view_b_c.c: a boca espera o
     Voice_GetStat(0) == 3 e depois conta um por quadro);
     resultado da História (tela 8): gHistResult (include/menu/menu_c.h) flags +0x80, state +0x90, seState +0x9C (2
     espera o StreamSe_GetStat(0) == 5, HistResult_UpdateSe), rewardIdx/rewardCount +0x13C/+0x138, e do flash[0]
     (+0x18; include/menu/menu_a.h) flags +0x20 (MFLASH_PAD 2: a animação aceita o controle) e trig +0x24. */
typedef struct { int stat, paused, left, vol, pan[2]; char path[256]; } SomVista; /* PortAdxView, plat_sndstate.c */
extern int gPortAdxCount;
extern SomVista gPortAdxView[16];
extern struct { char *label; int workSize, reloadSct; void *work; void *adxt; } gAdxPlayerTbl[6];
int ADXT_GetStat(void *adxt);
extern struct { int active; unsigned char *keys; unsigned short index, count; float time, length; } gLipSync;
extern int gLipSyncWaiting;
extern unsigned char *gHistResult;
#ifdef PORT_X360
#include <time.h> /* (o clock_gettime do posix360.c: o tempo real de cada linha, só no console) */
#endif
#define SOM_CANAIS 6
#define SOM_CAMPOS 40
#define TELA_HIST_RESULTADO 8

static int som_i32(const unsigned char *p, int desl) {
    int v;
    memcpy(&v, p + desl, 4); /* (ordem do host: o jogo grava estes campos como int) */
    return v;
}

/* o número do arquivo do caminho (".../pzs3us2/65151.bin" -> 65151), ou -1 */
static int som_arquivo(const char *caminho) {
    const char *b = strrchr(caminho, '/');
    int n;
    return b != NULL && sscanf(b + 1, "%d.bin", &n) == 1 ? n : -1;
}

void Roteiro_SomEstado(void) {
    static int ligado = -1, periodo;
    static unsigned leituras, ultima;
    static int antes[SOM_CAMPOS];
    static const char *const nomes[SOM_CANAIS] = {"BGM", "MAP", "SE0", "SE1", "V0", "V1"};
    int agora[SOM_CAMPOS], n = 0, i, mudou, t;
    char s[700], *p = s;
    if (ligado < 0) {
        const char *e = getenv("BT3_SOM_ESTADO");
        ligado = e != NULL && e[0] != '\0';
        periodo = ligado ? atoi(e) : 0;
    }
    if (!ligado) {
        return;
    }
    leituras++;
    t = tela_atual();
    agora[n++] = t;
    for (i = 0; i < SOM_CANAIS; i++) {
        const SomVista *v = &gPortAdxView[i];
        agora[n++] = i < gPortAdxCount ? v->stat : -1;
        agora[n++] = i < gPortAdxCount ? v->paused : -1;
        agora[n++] = i < gPortAdxCount ? som_arquivo(v->path) : -1;
        agora[n++] = gAdxPlayerTbl[i].adxt != NULL ? ADXT_GetStat(gAdxPlayerTbl[i].adxt) : -1;
    }
    agora[n++] = gLipSyncWaiting;
    agora[n++] = gLipSync.active;
    agora[n++] = gProgress != NULL ? gProgress->flags : -1;
    if (t == TELA_HIST_RESULTADO && gHistResult != NULL) {
        agora[n++] = som_i32(gHistResult, 0x80);
        agora[n++] = som_i32(gHistResult, 0x90);
        agora[n++] = som_i32(gHistResult, 0x9C);
        agora[n++] = som_i32(gHistResult, 0x13C);
        agora[n++] = som_i32(gHistResult, 0x20) & 2;
        agora[n++] = som_i32(gHistResult, 0x24) & 1;
    }
    while (n < SOM_CAMPOS) {
        agora[n++] = -9;
    }
    mudou = memcmp(agora, antes, sizeof(agora)) != 0;
    if (!mudou && !(periodo > 0 && leituras - ultima >= (unsigned)periodo)) {
        return;
    }
    memcpy(antes, agora, sizeof(agora));
    ultima = leituras;
    p += sprintf(p, "som: vblank %u leitura %u tela %d %s |", gPortVBlanks, leituras, t, mudou ? "muda" : "igual");
    for (i = 0; i < SOM_CANAIS && i < gPortAdxCount; i++) {
        const SomVista *v = &gPortAdxView[i];
        int g = gAdxPlayerTbl[i].adxt != NULL ? ADXT_GetStat(gAdxPlayerTbl[i].adxt) : -1;
        p += sprintf(p, " %s %d%s l%d a%d", nomes[i], v->stat, v->paused ? "p" : "", v->left, som_arquivo(v->path));
        if (g != v->stat) {
            p += sprintf(p, " lido%d", g);
        }
    }
    p += sprintf(p, " | boca espera %d ativa %d t %d | menus 0x%x", gLipSyncWaiting, gLipSync.active, (int)gLipSync.time,
                 gProgress != NULL ? gProgress->flags : -1);
    if (t == TELA_HIST_RESULTADO && gHistResult != NULL) {
        sprintf(p, " | resultado flags 0x%x state %d se %d premio %d/%d pad %d trig %d", som_i32(gHistResult, 0x80),
                som_i32(gHistResult, 0x90), som_i32(gHistResult, 0x9C), som_i32(gHistResult, 0x13C),
                som_i32(gHistResult, 0x138), (som_i32(gHistResult, 0x20) & 2) != 0, som_i32(gHistResult, 0x24) & 1);
    }
#ifdef PORT_X360
    {
        /* no console, o tempo real no fim da linha (" | ms N", desde a primeira linha): a duração de uma fala em ms
           contra a contada em vblanks, a deriva que a decisão 25.1 manda medir. Fora da comparação com o WSL (a
           nota tira o " | ms N" antes do diff) */
        static struct timespec t0;
        struct timespec agora_ts;
        clock_gettime(CLOCK_MONOTONIC, &agora_ts);
        if (t0.tv_sec == 0 && t0.tv_nsec == 0) {
            t0 = agora_ts;
        }
        Diario_Escrever("%s | ms %ld", s,
                        (agora_ts.tv_sec - t0.tv_sec) * 1000L + (agora_ts.tv_nsec - t0.tv_nsec) / 1000000L);
    }
#else
    printf("%s\n", s);
    fflush(stdout);
#endif
}

/* O Port_PadRead das builds com roteiro: 18 bytes como o scePad2Read (botões ativos em 0, analógicos no centro,
   pressões zeradas); 0 sem roteiro, e no jogador 2 (o plat_stub.c preenche o controle parado, como sem janela). */
int Roteiro_Pad(int socket, unsigned char *data) {
    unsigned b;
    int t, m;
    char s[120], n[16];
    if (!Roteiro_Ativo() || socket != 0) {
        return 0;
    }
    /* o rastro: cada troca de tela e de estado do fluxo do cartão, vista na leitura do controle */
    t = tela_atual();
    if (t != sTela) {
        sTela = t;
        nome_tela(t, n);
        sprintf(s, "tela %s vblank %u quadro %u", n, gPortVBlanks, sQuadro);
        diz(s);
    }
    m = mcflow_atual();
    if (m != sMcflow) {
        sMcflow = m;
        sprintf(s, "mcflow %d vblank %u quadro %u", m, gPortVBlanks, sQuadro);
        diz(s);
    }
    luta_quadro();
    f7_quadro();
    b = quadro();
#ifndef PORT_X360
    Imagens_Quadro(sQuadro);
#endif
    sQuadro++;
    memset(data, 0, 18);
    data[0] = (unsigned char)(~b & 0xFF);
    data[1] = (unsigned char)(~(b >> 8) & 0xFF);
    data[2] = data[3] = 0x80;
    data[4] = sLx; /* PAD_DATA_LX e PAD_DATA_LY (include/sys/pad.h) */
    data[5] = sLy;
    return 18;
}

/* O golpe do passo "termina luta" (acima): no quadro do jogo, depois do giro dos eventos (Battle_UpdateWork) e logo
   antes do Battle_Update, onde o jogo dá os seus. Entra por --wrap no BtlChars_SampleInput (chamado do Battle_Loop,
   src/battle/battle.c, outro objeto; headless.py); sem roteiro, ou sem pedido, só chama o original. No 360 o link do
   XDK não tem --wrap: o c89.py (renomes) compila o battle.c com BtlChars_SampleInput renomeada para
   __wrap_BtlChars_SampleInput, e o original é chamado pelo próprio nome. O pedido (sDerruba) só nasce no passo
   "termina luta", depois do so_teste (BT3_DESBLOQUEIO=1 e o cartão vazio no começo): sem ele, uma comparação por
   quadro e nada mais. */
#ifdef PORT_X360
void BtlChars_SampleInput(void);
#define __real_BtlChars_SampleInput BtlChars_SampleInput
#else
void __real_BtlChars_SampleInput(void);
#endif
void __wrap_BtlChars_SampleInput(void) {
    char s[200];
    __real_BtlChars_SampleInput();
    if (sDerruba < 0 || gBtlSeq == NULL || gBtlSeq->state != BTL_SEQ_LUTANDO || BtlChar_Get(sDerruba) == NULL ||
        BtlCtrl_IsActiveDead(sDerruba)) {
        return;
    }
    BtlMember_Damage(BtlChar_Get(sDerruba), TERMINA_DANO, TERMINA_DANO_FLAGS);
    if (sDerruba == 1) {
        sDerrubados1++;
    }
    sprintf(s, "roteiro: %s derrubado lado %d vblank %u quadro %u", sDerrubaOnde, sDerruba, gPortVBlanks, sQuadro);
    diz(s);
}

#ifndef PORT_X360
/* (F4, etapa 9b) As ações do roteiro de evento da luta (o GSCF, src/sys/gsc.c) que começam, no rastro: o critério
   diz quais eventos rodaram nas duas builds, não só que a luta terminou igual. Entram por --wrap (headless.py) nas
   três chamadas de fora do gsc.c: Gsc_StartAction, dos gatilhos do btl_script.c (BtlScript_StartPending e
   BtlScript_StartWaitEvents: as ações de evento, 10001...); Gsc_StartMain, a ação principal (-1) do battle_work.c; e
   Gsc_RunAction, a da carga (1000) do battle_load.c, rodada até o fim. As chamadas de dentro do gsc.c (uma ação que
   chama outra, Gsc_CallAction) não passam pelo --wrap e não aparecem. Só com o roteiro ligado; o 360 não tem --wrap e
   não as escreve (o rastro dele não tem estas linhas). */
int __real_Gsc_StartAction(int handle, int action);
int __real_Gsc_StartMain(int handle);
void __real_Gsc_RunAction(int handle, int action);

static void gsc_rastro(const char *como, int action) {
    char s[160];
    if (Roteiro_Ativo()) {
        sprintf(s, "roteiro: gsc %s %d vblank %u quadro %u", como, action, gPortVBlanks, sQuadro);
        diz(s);
    }
}

int __wrap_Gsc_StartAction(int handle, int action) {
    gsc_rastro("acao", action);
    return __real_Gsc_StartAction(handle, action);
}

int __wrap_Gsc_StartMain(int handle) {
    gsc_rastro("principal", -1);
    return __real_Gsc_StartMain(handle);
}

void __wrap_Gsc_RunAction(int handle, int action) {
    gsc_rastro("roda", action);
    __real_Gsc_RunAction(handle, action);
}
#endif
