/*
 * F6, etapa 5: o teste da leitura em blocos (port/x360/src/som/fluxo.c), sem o jogo nem dados dele: um arquivo de
 * 1.000.000 bytes com um padrão conhecido, blocos de 4096 (BT3_FLUXO_BLOCO). Roda com port/x360/tools/teste_som360.sh,
 * que também estraga cópias do fluxo.c (mutantes) e confere que este teste acusa cada uma.
 *   1. leitura em sequência por quadros de 36 bytes, com laço que cruza blocos, três voltas: os bytes certos e
 *      nenhuma falta depois do primeiro bloco (o seguinte e o do começo do laço chegam adiantados);
 *   2. muitos fluxos abertos e fechados ao acaso, fechados também enquanto o bloco está sendo lido: os bytes certos,
 *      a tabela volta a zero e a memória fica no teto;
 *   3. arquivo que não existe: FLUXO_ERRO; só o cabeçalho: o primeiro bloco pequeno; depois de Fluxo_Parar, NULL.
 */
#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fluxo.h"

#define TAM 1000000L
#define BLOCO 4096L
static char sArq[256];
static int sFalhas;

static unsigned char byte_em(long i) { return (unsigned char)(((unsigned long)i * 2654435761u) >> 13); }

static void dorme_us(long us) {
    struct timespec t;
    t.tv_sec = us / 1000000;
    t.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&t, NULL);
}

#define CONFERE(c, ...) do { if (!(c)) { printf("FALHA: " __VA_ARGS__); printf("\n"); sFalhas++; } } while (0)

static int espera_pronto(Fluxo *f, long *tam) {
    int e, k;
    for (k = 0; k < 200000 && (e = Fluxo_Estado(f, tam)) == FLUXO_PENDENTE; k++) {
        dorme_us(20);
    }
    return e;
}

/* os n bytes em pos, esperando (quantas vezes não estavam: *faltas) */
static int confere_em(Fluxo *f, long pos, int n, int adianta, unsigned *faltas) {
    const unsigned char *p;
    int k, i;
    for (k = 0; (p = Fluxo_Pegar(f, pos, n, adianta)) == NULL; k++) {
        if (k > 200000) {
            return 0;
        }
        (*faltas)++;
        dorme_us(20);
    }
    for (i = 0; i < n; i++) {
        if (p[i] != byte_em(pos + i)) {
            return 0;
        }
    }
    return 1;
}

static void teste_sequencia(void) {
    const long data = 0x120, quadro = 36, lacoIni = data + 1000 * quadro, lacoFim = data + 20000 * quadro;
    long tam, pos, ultimoBloco = -1;
    unsigned faltas = 0, voltas = 0;
    int ok = 1;
    Fluxo *f = Fluxo_Abrir(sArq, 0);
    CONFERE(f != NULL, "1: Fluxo_Abrir deu NULL");
    if (f == NULL) {
        return;
    }
    CONFERE(espera_pronto(f, &tam) == FLUXO_PRONTO && tam == TAM, "1: o primeiro bloco não chegou (tamanho %ld)", tam);
    Fluxo_Laco(f, lacoIni, lacoFim);
    ok = confere_em(f, 0, 0x40, 0, &faltas);
    pos = data;
    while (ok && voltas < 3) {
        if (pos / BLOCO != ultimoBloco) { /* (um bloco novo: tempo para o adiantado, como o tocar de verdade dá) */
            ultimoBloco = pos / BLOCO;
            dorme_us(5000);
        }
        ok = confere_em(f, pos, (int)quadro, 1, &faltas);
        pos += quadro;
        if (pos >= lacoFim) {
            pos = lacoIni;
            voltas++;
            ultimoBloco = -1;
        }
    }
    CONFERE(ok, "1: bytes errados ou que não chegaram em %ld", pos);
    CONFERE(faltas == 0, "1: %u faltas na leitura em sequência com laço (o adiantado não chegou antes)", faltas);
    Fluxo_Fechar(f);
}

static void teste_muitos(void) {
    Fluxo *abertos[20];
    FluxoContadores c;
    unsigned faltas = 0, semLugar = 0;
    int i, k, ok = 1;
    srand(25);
    memset(abertos, 0, sizeof(abertos));
    for (i = 0; i < 3000 && ok; i++) {
        k = rand() % 20;
        if (abertos[k] != NULL && rand() % 2) {
            Fluxo_Fechar(abertos[k]); /* (às vezes logo depois de abrir: com o primeiro bloco ainda sendo lido) */
            abertos[k] = NULL;
        } else if (abertos[k] == NULL) {
            abertos[k] = Fluxo_Abrir(sArq, 0);
            if (abertos[k] == NULL) {
                semLugar++;
            }
        } else {
            long pos = ((long)rand() * 7919L) % (TAM - 100);
            ok = confere_em(abertos[k], pos, 36, rand() % 2, &faltas);
        }
    }
    CONFERE(ok, "2: bytes errados ou que não chegaram com muitos fluxos");
    CONFERE(semLugar == 0, "2: Fluxo_Abrir sem lugar %u vezes com no máximo 20 abertos (a tabela não se esvazia)", semLugar);
    for (k = 0; k < 20; k++) {
        Fluxo_Fechar(abertos[k]);
    }
    for (k = 0; k < 2000; k++) {
        Fluxo_Contadores(&c);
        if (c.abertos == 0) {
            break;
        }
        dorme_us(500);
    }
    CONFERE(c.abertos == 0, "2: %u fluxos ainda abertos depois de fechar todos", c.abertos);
    CONFERE(c.memoria <= 32u * 2u * (unsigned)(BLOCO + FLUXO_SOBRA), "2: memória de %u bytes passou do teto", c.memoria);
    CONFERE(c.erros == 0, "2: %u erros de leitura", c.erros);
    printf("2: %u aberturas, %u blocos lidos, %u faltas pedidas, %u adiantados, no máximo %u abertos, %u bytes reservados\n",
           c.aberturas, c.leituras, c.faltas, c.adiantados, c.abertosMax, c.memoria);
}

static void teste_bordas(void) {
    long tam = 0;
    unsigned faltas = 0;
    char falso[300];
    Fluxo *f;
    snprintf(falso, sizeof(falso), "%s.nao_existe", sArq);
    f = Fluxo_Abrir(falso, 0);
    CONFERE(f != NULL && espera_pronto(f, &tam) == FLUXO_ERRO, "3: o arquivo que não existe não deu FLUXO_ERRO");
    if (f != NULL) {
        CONFERE(Fluxo_Pegar(f, 0, 16, 1) == NULL, "3: dados de um arquivo que não existe");
        Fluxo_Fechar(f);
    }
    f = Fluxo_Abrir(sArq, 0x800);
    CONFERE(f != NULL && espera_pronto(f, &tam) == FLUXO_PRONTO, "3: só o cabeçalho não chegou");
    if (f != NULL) {
        CONFERE(confere_em(f, 0, 0x40, 0, &faltas) && faltas == 0, "3: o cabeçalho errado");
        CONFERE(Fluxo_Pegar(f, 0x800 + FLUXO_SOBRA, 4, 0) == NULL, "3: o primeiro bloco do cabeçalho veio grande");
        Fluxo_Fechar(f);
    }
    Fluxo_Parar();
    CONFERE(Fluxo_Abrir(sArq, 0) == NULL, "3: Fluxo_Abrir depois de Fluxo_Parar");
}

int main(int argc, char **argv) {
    FILE *o;
    long i;
    snprintf(sArq, sizeof(sArq), "%s/fluxo_teste.bin", argc > 1 ? argv[1] : ".");
    o = fopen(sArq, "wb");
    if (o == NULL) {
        printf("FALHA: não cria %s\n", sArq);
        return 1;
    }
    for (i = 0; i < TAM; i++) {
        fputc(byte_em(i), o);
    }
    fclose(o);
    setenv("BT3_FLUXO_BLOCO", "4096", 1);
    teste_sequencia();
    teste_muitos();
    teste_bordas();
    remove(sArq);
    printf("teste_fluxo: %s\n", sFalhas ? "FALHOU" : "tudo certo");
    return sFalhas ? 1 : 0;
}
