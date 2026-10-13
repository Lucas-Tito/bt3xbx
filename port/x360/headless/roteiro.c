/*
 * Roteiros de controle (F4, etapa 4; o formato está no guia, docs/x360/guia-de-port.md, seção 2, "Roteiros de
 * controle"; os roteiros, em port/x360/roteiros): o controle do jogador 1 tirado de um
 * arquivo de texto, passo a passo, com cada espera ancorada no estado do jogo (a tela do gerenciador de menus, o
 * estado do fluxo do cartão, a luta) e não no número do quadro. Uma diferença de tempo entre a x86, a PPC no qemu e o
 * console (uma carga mais lenta, uma tela que demora a aceitar entrada) não desalinha o resto do roteiro.
 *
 *   BT3_ROTEIRO=<arquivo>   liga o roteiro (sem ela, nada muda: Roteiro_Pad devolve 0 e o controle fica parado)
 *   BT3_DESBLOQUEIO=1       permite o passo "desbloqueia" (Save_UnlockAll do próprio jogo, o mesmo da aba de truques
 *                           da janela do PC); sem ela o passo para o roteiro com erro. Nunca no jogo normal. E só com
 *                           o cartão vazio no começo da rodada (sem o arquivo de dados do save na pasta de saves,
 *                           BT3_SAVES): o desbloqueio iria para o save de verdade na próxima gravação
 *
 * Com o roteiro ligado, cada troca de tela vai ao log ("tela N vblank V quadro Q"), e cada espera cumprida também
 * ("roteiro: arquivo:linha ok ..."): é o rastro que os roteiros e as comparações usam. Na build sem janela (x86 e
 * PPC) o log é a saída padrão e o passo "fim" encerra o programa; no 360 o log é o bt3xbx.log e o "fim" só solta o
 * controle. Falha (espera esgotada, arquivo ruim): mensagem e saída com 2 sem janela, tela vermelha no 360.
 *
 * Entra pelo Port_PadRead: sem_janela.c (x86 e PPC) e pad360.c (360). O BT3_PAD_PLAY, se também estiver ligado,
 * tem a precedência (plat_stub.c nem chama o Port_PadRead).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stddef.h>

#include "sys/save.h" /* gSaveData (SaveData), Save_UnlockAll */

/* O campo do "confere som" onde o jogo o tem (save.h: 0x16A4). Este arquivo compila com os tipos do jogo
   (port_compat.h, forçado como nos fontes do jogo: s64 de 64 bits); sem eles o SaveData sai menor e o campo cai em
   0x1694, lendo outra opção. A build para aqui se isso voltar a acontecer. */
typedef char RoteiroSaveConfere[offsetof(SaveData, soundMode) == 0x16A4 ? 1 : -1];

/* O estado do jogo que identifica a tela. Os tipos completos são do jogo; aqui só os campos lidos, nos mesmos
   deslocamentos (as três builds têm ponteiros de 32 bits). */
extern struct { int unk0[5]; int flags; int mode; } *gProgress; /* include/menu/menu_a.h: flags 0x14, mode 0x18 */
extern struct { int unk0[9]; int state; } *gMcFlow;             /* include/sys/mcflow_a.h: state 0x24; NULL fora */
extern struct { int state; } *gBtlSeq;                         /* include/battle/btl_seq.h: >= 2 com a luta andando */
extern int gPortMenuMode;                                      /* port/src/headless.c: 1 enquanto os menus rodam */
extern unsigned gPortVBlanks;                                  /* port/src/plat_stub.c */

#ifdef PORT_X360
void Diario_Escrever(const char *formato, ...);
void Port_Abortar(void); /* main360.cpp: log e tela vermelha */
#endif

static void diz(const char *linha) {
#ifdef PORT_X360
    Diario_Escrever("%s", linha);
#else
    printf("%s\n", linha);
    fflush(stdout);
#endif
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
#define MPROG_FIRST_RUN 0x40

static int tela_atual(void) {
    if (gPortMenuMode && gProgress != NULL) {
        return (gProgress->flags & MPROG_FIRST_RUN) ? TELA_INICIO : gProgress->mode;
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

/* ------------------------------------------------------------------ o roteiro */

enum { P_ESPERA, P_QUADROS, P_APERTA, P_SEGURA, P_DESBLOQUEIA, P_MARCA, P_FIM, P_REPETE, P_VOLTA, P_CONFERE };
enum { C_NADA, C_TELA, C_MCFLOW, C_LUTA };

typedef struct Passo {
    int tipo;
    unsigned botoes;    /* P_APERTA, P_SEGURA: os bits de pad360.c (1 = apertado) */
    int n;              /* P_QUADROS, P_SEGURA: quadros; P_APERTA: vezes (sem "ate") */
    int segura, solta;  /* P_APERTA: o ritmo de cada vez */
    int cond, valor;    /* P_ESPERA e P_APERTA com "ate": a condição; P_CONFERE: o valor esperado */
    int max;            /* quadros até a falha */
    int par;            /* P_REPETE: o índice do P_VOLTA; P_VOLTA: o do P_REPETE */
    char onde[80];      /* arquivo:linha, para o log */
    char texto[80];     /* P_MARCA */
} Passo;

static Passo *sPassos;
static int sN, sCap;
static int sPasso, sT;          /* o passo atual e os quadros dele */
static unsigned sQuadro;        /* leituras do jogador 1 desde o começo do roteiro */
static int sAtivo = -1;         /* -1: ainda não lido */
static int sTela = 99999, sMcflow = -99999;
static int sSegura = 4, sSolta = 8, sMax = 6000;
static int sProf;               /* profundidade dos "inclui" */
static int sAbertos[8], sNAbertos; /* os "repete" ainda sem "volta", na leitura */
static struct { int passo, t, inicio; } sLaco[8]; /* os "repete" em andamento: o passo, quadros nele, quadro da volta */
static int sNLaco;
static int sCartaoVazio = 1;    /* sem o arquivo de dados do save no começo da rodada (P_DESBLOQUEIA) */

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
    } else {
        erro_linha(p->onde, "condição esperada (tela N, mcflow N ou luta)", i < n ? pal[i] : NULL);
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
    } else if (igual(pal[0], "desbloqueia")) {
        p = novo(onde, P_DESBLOQUEIA);
        i = 1;
    } else if (igual(pal[0], "confere")) { /* confere som N: o campo do save em memória (gSaveData) igual a N */
        if (n < 2 || !igual(pal[1], "som")) {
            erro_linha(onde, "confere som N", n > 1 ? pal[1] : NULL);
        }
        p = novo(onde, P_CONFERE);
        p->valor = numero(onde, n > 2 ? pal[2] : NULL);
        i = 3;
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

static int cumprida(const Passo *p) {
    switch (p->cond) {
    case C_TELA:
        return tela_atual() == p->valor;
    case C_MCFLOW:
        return mcflow_atual() == p->valor;
    case C_LUTA:
        return tela_atual() == TELA_LUTA;
    }
    return 0;
}

static void cond_texto(const Passo *p, char *s) {
    if (p->cond == C_TELA) {
        sprintf(s, "tela %d", p->valor);
    } else if (p->cond == C_MCFLOW) {
        sprintf(s, "mcflow %d", p->valor);
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
    char s[200], c[40];
    cond_texto(p, c);
    sprintf(s, "roteiro: %s ok %s vblank %u quadro %u", p->onde, c, gPortVBlanks, sQuadro);
    diz(s);
}

static void esgotou(const Passo *p) {
    char s[300], c[40], t[16];
    cond_texto(p, c);
    nome_tela(tela_atual(), t);
    sprintf(s, "%s: %d quadros sem %s (tela %s, mcflow %d, vblank %u)", p->onde, p->max, c, t, mcflow_atual(),
            gPortVBlanks);
    falha(s);
}

/* os botões do jogador 1 neste quadro, andando o roteiro */
static unsigned quadro(void) {
    char s[300];
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
        case P_DESBLOQUEIA:
            if (getenv("BT3_DESBLOQUEIO") == NULL || atoi(getenv("BT3_DESBLOQUEIO")) == 0) {
                sprintf(s, "%s: desbloqueia sem BT3_DESBLOQUEIO=1", p->onde);
                falha(s);
            }
            if (gSaveData == NULL) {
                sprintf(s, "%s: desbloqueia sem save na memória", p->onde);
                falha(s);
            }
            if (!sCartaoVazio) {
                sprintf(s, "%s: desbloqueia recusado: o cartão já tinha um save no começo da rodada, e o desbloqueio "
                        "iria para ele (use uma pasta de saves vazia em BT3_SAVES)", p->onde);
                falha(s);
            }
            Save_UnlockAll(gSaveData);
            sprintf(s, "roteiro: %s desbloqueado (Save_UnlockAll) vblank %u quadro %u", p->onde, gPortVBlanks, sQuadro);
            diz(s);
            proximo();
            continue;
        case P_CONFERE:
            if (gSaveData == NULL) {
                sprintf(s, "%s: confere sem save na memória", p->onde);
                falha(s);
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
        case P_MARCA:
            sprintf(s, "roteiro: %s marca %s vblank %u quadro %u", p->onde, p->texto, gPortVBlanks, sQuadro);
            diz(s);
            proximo();
            continue;
        case P_FIM:
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
    b = quadro();
    sQuadro++;
    memset(data, 0, 18);
    data[0] = (unsigned char)(~b & 0xFF);
    data[1] = (unsigned char)(~(b >> 8) & 0xFF);
    data[2] = data[3] = data[4] = data[5] = 0x80;
    return 18;
}
