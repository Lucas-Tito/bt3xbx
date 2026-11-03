/* Programa de teste da biblioteca psu (conv/psu.c), usado por teste_psu.py.
 *   ler ENTRADA.psu DIR   : valida, imprime pasta/arquivos e grava cada arquivo em DIR
 *   bt3 ENTRADA.psu       : Psu_Ler + Psu_ConferirBT3, imprime o código
 *   escrever LISTA SAIDA  : lê a lista (ver teste_psu.py) e grava o .psu
 * Saída de "ler": uma linha por item, "erro N texto" em falha (código de saída 2). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../conv/psu.h"

static unsigned char *carrega(const char *nome, unsigned long *tam) {
    FILE *f = fopen(nome, "rb");
    unsigned char *b;
    long n;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = (unsigned char *)malloc(n ? (size_t)n : 1);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        return NULL;
    }
    fclose(f);
    *tam = (unsigned long)n;
    return b;
}
static void imp_data(const char *rot, const PsuData *d) {
    printf(" %s=%04u-%02u-%02uT%02u:%02u:%02u", rot, d->ano, d->mes, d->dia, d->hora, d->min, d->seg);
}
static int le_data(const char *t, PsuData *d) {
    unsigned a, me, di, h, mi, s;
    if (sscanf(t, "%u-%u-%uT%u:%u:%u", &a, &me, &di, &h, &mi, &s) != 6) return 0;
    d->ano = (unsigned short)a; d->mes = (unsigned char)me; d->dia = (unsigned char)di;
    d->hora = (unsigned char)h; d->min = (unsigned char)mi; d->seg = (unsigned char)s;
    return 1;
}

int main(int argc, char **argv) {
    unsigned long tam;
    unsigned char *buf;
    static PsuArquivo vet[64];
    PsuSave s;
    int r;
    unsigned i;
    if (argc >= 3 && !strcmp(argv[1], "ler") && argc == 4) {
        buf = carrega(argv[2], &tam);
        if (!buf) { printf("erro-io\n"); return 3; }
        r = Psu_Ler(buf, tam, &s, vet, 64);
        if (r) { printf("erro %d %s\n", r, Psu_TextoErro(r)); return 2; }
        printf("pasta %s modo=%u", s.pasta, s.modo);
        imp_data("c", &s.criada); imp_data("m", &s.modificada);
        printf(" n=%u\n", s.nArquivos);
        for (i = 0; i < s.nArquivos; i++) {
            char cam[1024];
            FILE *f;
            printf("arq %s modo=%u tam=%lu", s.arquivos[i].nome, s.arquivos[i].modo, s.arquivos[i].tamanho);
            imp_data("c", &s.arquivos[i].criado); imp_data("m", &s.arquivos[i].modificado);
            printf("\n");
            sprintf(cam, "%s/%s", argv[3], s.arquivos[i].nome);
            f = fopen(cam, "wb");
            if (!f) return 3;
            fwrite(s.arquivos[i].dados, 1, s.arquivos[i].tamanho, f);
            fclose(f);
        }
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "bt3")) {
        buf = carrega(argv[2], &tam);
        if (!buf) { printf("erro-io\n"); return 3; }
        r = Psu_Ler(buf, tam, &s, vet, 64);
        if (!r) r = Psu_ConferirBT3(&s);
        printf("codigo %d %s\n", r, Psu_TextoErro(r));
        return r ? 2 : 0;
    }
    if (argc == 4 && !strcmp(argv[1], "escrever")) {
        /* lista: "pasta NOME MODO CRIADA MODIFICADA" e "arq NOME MODO CRIADO MODIFICADO CAMINHO" */
        FILE *l = fopen(argv[2], "r"), *o;
        char lin[2048], t[6][512];
        unsigned long n;
        unsigned char *saida;
        static unsigned char *dados[64];
        unsigned long tams[64];
        unsigned k = 0;
        memset(&s, 0, sizeof(s));
        if (!l) return 3;
        s.arquivos = vet;
        while (fgets(lin, sizeof lin, l)) {
            if (sscanf(lin, "pasta %511s %511s %511s %511s", t[0], t[1], t[2], t[3]) == 4) {
                strcpy(s.pasta, t[0]); s.modo = (unsigned)atoi(t[1]);
                if (!le_data(t[2], &s.criada) || !le_data(t[3], &s.modificada)) return 3;
            } else if (sscanf(lin, "arq %511s %511s %511s %511s %511s", t[0], t[1], t[2], t[3], t[4]) == 5) {
                memset(&vet[k], 0, sizeof vet[k]);
                strcpy(vet[k].nome, t[0]); vet[k].modo = (unsigned)atoi(t[1]);
                if (!le_data(t[2], &vet[k].criado) || !le_data(t[3], &vet[k].modificado)) return 3;
                dados[k] = carrega(t[4], &tams[k]);
                if (!dados[k]) return 3;
                vet[k].dados = dados[k]; vet[k].tamanho = tams[k];
                k++;
            }
        }
        s.nArquivos = k;
        n = Psu_TamanhoEscrita(&s);
        if (!n) { printf("erro-tamanho\n"); return 2; }
        saida = (unsigned char *)malloc(n);
        r = Psu_Escrever(&s, saida, n, &n);
        if (r) { printf("erro %d %s\n", r, Psu_TextoErro(r)); return 2; }
        o = fopen(argv[3], "wb");
        if (!o) return 3;
        fwrite(saida, 1, n, o);
        fclose(o);
        return 0;
    }
    fprintf(stderr, "uso incorreto\n");
    return 4;
}
