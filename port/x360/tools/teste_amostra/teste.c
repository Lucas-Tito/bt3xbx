/*
 * O teste no host do amostrador (port/x360/src/f2/amostra_hist.c, o mesmo arquivo que vai para o 360). Rodado por
 * port/x360/tools/amostra_confere.sh.
 *   teste pilha        a busca do quadro de armadilha e o resumo por função, em pilhas montadas à mão (sai com 1 se
 *                      algum caso não confere)
 *   teste hist         lê um roteiro da entrada (amostra_perfil.py sintetico) e escreve as linhas do diário na saída:
 *                        C limiar longo         configura
 *                        H texto                o cabeçalho do despejo
 *                        S tipo folha chamador f1 f2 ...   uma amostra (endereços em hexadecimal)
 *                        Q quadros ms           o quadro do jogo terminou
 *                        D motivo               despeja
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amostra_hist.h"

void Diario_Escrever(const char *formato, ...) {
    va_list a;
    va_start(a, formato);
    vprintf(formato, a);
    va_end(a);
    putchar('\n');
}

/* ---- a memória de mentira: a pilha da thread, de LIMITE a BASE --------------------------------------------------- */

#define LIMITE 0x70000000u
#define BASE 0x70010000u
static unsigned sMem[(BASE - LIMITE) / 4];

static int Ler(unsigned e, unsigned *v) {
    if (e < LIMITE || e >= BASE || (e & 3) != 0) {
        return 0;
    }
    *v = sMem[(e - LIMITE) / 4];
    return 1;
}

static void P(unsigned e, unsigned v) {
    if (e < LIMITE || e >= BASE || (e & 3) != 0) {
        fprintf(stderr, "endereço de teste fora: %08X\n", e);
        exit(2);
    }
    sMem[(e - LIMITE) / 4] = v;
}

/* a imagem: cinco funções com .pdata e uma sem (0x82060600) */
#define FA 0x82060000u /* o laço de cima */
#define FB 0x82060100u
#define FC 0x82060200u
#define FX 0x82060300u /* a folha */
#define FW 0x82060400u /* o invólucro de uma espera (o SleepEx) */
#define FSEM 0x82060600u
static const unsigned kPdata[] = {FA, (0x100 / 4) << 8, FB, (0x100 / 4) << 8, FC, (0x100 / 4) << 8,
                                  FX, (0x80 / 4) << 8,  FW, (0x100 / 4) << 8};
static const AmostraImagem kImg = {0x82060000u, 0x82100000u, kPdata, 5};

/* A pilha do jogo comum a todos os casos: C em D000, B em E000, A em F000, o topo em FF00. O Lr de cada uma fica em -8
   do quadro de quem a chamou. */
static void PilhaDoJogo(void) {
    memset(sMem, 0, sizeof(sMem));
    P(0x7000D000, 0x7000E000);
    P(0x7000E000, 0x7000F000);
    P(0x7000F000, 0x7000FF00);
    P(0x7000FF00, 0);
    P(0x7000E000 - 8, FB + 0x50); /* C volta para B */
    P(0x7000F000 - 8, FA + 0x50); /* B volta para A */
    P(0x7000FF00 - 8, 0x80001000); /* A volta para o núcleo: fora da imagem */
}

static void Armadilha(unsigned o, unsigned r1, unsigned iar, unsigned lr) {
    P(o + AMOSTRA_TF_R1, 0);
    P(o + AMOSTRA_TF_R1 + 4, r1);
    P(o + AMOSTRA_TF_IAR, iar);
    P(o + AMOSTRA_TF_LR, lr);
}

static int sFalhas;

static void Conferir(const char *caso, int ok, const char *o_que) {
    if (!ok) {
        printf("FALHA: %s: %s\n", caso, o_que);
        sFalhas++;
    }
}

static int Tem(const AmostraUma *u, unsigned f) {
    unsigned i;
    for (i = 0; i < u->n; i++) {
        if (u->funcs[i] == f) {
            return 1;
        }
    }
    return 0;
}

static void Rodar(const char *caso, unsigned ks, unsigned tipo, unsigned armadilha, unsigned folha, unsigned chamador,
                  const unsigned *funcs, unsigned nFuncs) {
    AmostraCrua c;
    AmostraUma u;
    unsigned i;
    AmostraHist_LerPilha(&kImg, Ler, ks, LIMITE, BASE, &c);
    AmostraHist_Resumir(&kImg, &c, &u);
    Conferir(caso, c.tipo == tipo, "tipo");
    if (armadilha != 0) {
        Conferir(caso, c.armadilha == armadilha, "onde está o quadro de armadilha");
    }
    Conferir(caso, u.folha == folha, "folha");
    Conferir(caso, u.chamador == chamador, "chamador");
    Conferir(caso, u.n == nFuncs, "quantas funções no inclusivo");
    for (i = 0; i < nFuncs; i++) {
        Conferir(caso, Tem(&u, funcs[i]), "função do inclusivo");
    }
    printf("caso %s: tipo %u, quadro %08X, Iar %08X, Lr %08X, folha %08X, chamador %08X, %u funções, %u retornos\n",
           caso, c.tipo, c.armadilha, c.iar, c.lr, u.folha, u.chamador, u.n, c.nRet);
}

static int Pilha(void) {
    static const unsigned xcba[] = {FX, FC, FB, FA}, wcba[] = {FW, FC, FB, FA}, cba[] = {FC, FB, FA};
    static const unsigned semcba[] = {FSEM, FC, FB, FA};

    /* 1. a folha X sem quadro (r1 é o de C), o quadro de armadilha é um elo do encadeamento */
    PilhaDoJogo();
    P(0x7000A000, 0x7000A100);
    P(0x7000A100, 0x7000A200);
    P(0x7000A200, 0x7000D000);
    Armadilha(0x7000A200, 0x7000D000, FX + 0x10, FC + 0x40);
    Rodar("folha sem quadro", 0x7000A000, AMOSTRA_ARMADILHA, 0x7000A200, FX, FC, xcba, 4);

    /* 2. o quadro de armadilha dentro do quadro do manipulador, com um falso antes (r1 que não é o próximo elo) */
    PilhaDoJogo();
    P(0x7000A000, 0x7000A100);
    P(0x7000A100, 0x7000A200);
    P(0x7000A200, 0x7000D000);
    Armadilha(0x7000A208, 0x7000C000, FB + 0x10, FA + 0x10);
    Armadilha(0x7000A260, 0x7000D000, FX + 0x10, FC + 0x40);
    Rodar("dentro do manipulador", 0x7000A000, AMOSTRA_ARMADILHA, 0x7000A260, FX, FC, xcba, 4);

    /* 3. uma espera: nenhum quadro de armadilha; o núcleo foi chamado por W, que C chamou */
    PilhaDoJogo();
    P(0x7000A000, 0x7000A100);
    P(0x7000A100, 0x7000A200);
    P(0x7000A200, 0x7000C800);
    P(0x7000C800, 0x7000D000);
    P(0x7000A100 - 8, 0x80002000);
    P(0x7000A200 - 8, 0x80002100);
    P(0x7000C800 - 8, FW + 0x20); /* o núcleo volta para W */
    P(0x7000D000 - 8, FC + 0x60); /* W volta para C */
    Rodar("espera", 0x7000A000, AMOSTRA_SEM_IAR, 0, AMOSTRA_FOLHA_ESPERA, FW, wcba, 4);

    /* 4. o encadeamento acaba no quadro de armadilha (elo zero): vale o r1 dentro da pilha */
    PilhaDoJogo();
    P(0x7000A000, 0x7000A200);
    P(0x7000A200, 0);
    Armadilha(0x7000A200, 0x7000D000, FX + 0x10, FC + 0x40);
    Rodar("elo zero", 0x7000A000, AMOSTRA_ARMADILHA, 0x7000A200, FX, FC, xcba, 4);

    /* 5. o Iar no núcleo, chamado de C */
    PilhaDoJogo();
    P(0x7000A000, 0x7000A200);
    P(0x7000A200, 0x7000D000);
    Armadilha(0x7000A200, 0x7000D000, 0x80004000, FC + 0x40);
    Rodar("Iar no núcleo", 0x7000A000, AMOSTRA_ARMADILHA, 0x7000A200, AMOSTRA_FOLHA_NUCLEO, FC, cba, 3);

    /* 6. a folha X com quadro, que já chamou outra: o Lr cai nela; o chamador vem do primeiro retorno */
    PilhaDoJogo();
    P(0x7000CF00, 0x7000D000);
    P(0x7000D000 - 8, FC + 0x40); /* X volta para C */
    P(0x7000A000, 0x7000A200);
    P(0x7000A200, 0x7000CF00);
    Armadilha(0x7000A200, 0x7000CF00, FX + 0x30, FX + 0x20);
    Rodar("folha com quadro", 0x7000A000, AMOSTRA_ARMADILHA, 0x7000A200, FX, FC, xcba, 4);

    /* 7. uma folha sem .pdata: fica o endereço cru (a ferramenta resolve pelo .map) */
    PilhaDoJogo();
    P(0x7000A000, 0x7000A200);
    P(0x7000A200, 0x7000D000);
    Armadilha(0x7000A200, 0x7000D000, FSEM + 0x10, FC + 0x40);
    {
        static unsigned crua[4];
        memcpy(crua, semcba, sizeof(crua));
        crua[0] = FSEM + 0x10;
        Rodar("sem .pdata", 0x7000A000, AMOSTRA_ARMADILHA, 0x7000A200, FSEM + 0x10, FC, crua, 4);
    }

    /* 8. o encadeamento quebrado logo no r1 salvo: a varredura acha o quadro de armadilha */
    PilhaDoJogo();
    P(0x7000A000, 0x12345678);
    Armadilha(0x7000A300, 0x7000D000, FX + 0x10, FC + 0x40);
    Rodar("varredura", 0x7000A000, AMOSTRA_VARREDURA, 0x7000A300, FX, FC, xcba, 4);

    /* 9. o r1 salvo fora da pilha da thread: nada */
    PilhaDoJogo();
    Rodar("fora da pilha", 0x60000000, AMOSTRA_NADA, 0, AMOSTRA_FOLHA_ESPERA, 0, NULL, 0);

    if (sFalhas != 0) {
        printf("pilha: %d falhas\n", sFalhas);
        return 1;
    }
    printf("pilha: tudo certo\n");
    return 0;
}

/* ---- o roteiro dos histogramas ------------------------------------------------------------------------------------ */

static int Hist(void) {
    static char memoria[4 * 1024 * 1024];
    char linha[2048], cabecalho[1024] = "sem cabeçalho", *p, *fim;
    unsigned limiar = 40, longo = 400, q;
    double ms;
    AmostraUma u;
    if (AmostraHist_Tamanho() > sizeof(memoria)) {
        fprintf(stderr, "memória do teste pequena: %u\n", AmostraHist_Tamanho());
        return 2;
    }
    AmostraHist_Iniciar(memoria, sizeof(memoria), limiar, longo);
    while (fgets(linha, sizeof(linha), stdin) != NULL) {
        linha[strcspn(linha, "\r\n")] = 0;
        switch (linha[0]) {
        case 'C':
            if (sscanf(linha + 1, "%u %u", &limiar, &longo) != 2) {
                return 2;
            }
            AmostraHist_Iniciar(memoria, sizeof(memoria), limiar, longo);
            break;
        case 'H':
            {
                size_t n = strlen(linha + 2);
                n = n < sizeof(cabecalho) - 1 ? n : sizeof(cabecalho) - 1;
                memcpy(cabecalho, linha + 2, n);
                cabecalho[n] = 0;
            }
            break;
        case 'S':
            p = linha + 1;
            u.tipo = (unsigned)strtoul(p, &fim, 10);
            p = fim;
            u.folha = (unsigned)strtoul(p, &fim, 16);
            p = fim;
            u.chamador = (unsigned)strtoul(p, &fim, 16);
            p = fim;
            u.n = 0;
            while (u.n < AMOSTRA_FUNCS) {
                unsigned f = (unsigned)strtoul(p, &fim, 16);
                if (fim == p) {
                    break;
                }
                u.funcs[u.n++] = f;
                p = fim;
            }
            AmostraHist_Guardar(&u);
            break;
        case 'Q':
            if (sscanf(linha + 1, "%u %lf", &q, &ms) != 2) {
                return 2;
            }
            AmostraHist_FimDoQuadro(q, ms);
            break;
        case 'D':
            AmostraHist_Despejar(linha + 2, cabecalho);
            break;
        default:
            break;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "pilha") == 0) {
        return Pilha();
    }
    if (argc == 2 && strcmp(argv[1], "hist") == 0) {
        return Hist();
    }
    fprintf(stderr, "uso: teste pilha | teste hist < roteiro\n");
    return 2;
}
