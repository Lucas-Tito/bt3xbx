/*
 * Ferramenta de PC da conversão (e o teste dos conversores na F1).
 *
 *   conv_tool [--formato F] [--mapa MAPA.tsv] [--saida DIR] [--detalhe N] DADOS ARQUIVO...
 *   conv_tool [--mapa ...] [--saida ...] --todos DADOS       todos os arquivos do registro (conv.c)
 *
 * Para cada ARQUIVO (relativo a DADOS, como o jogo o pede: pzs3us1/01473.bin): converte com o formato do
 * registro (ou F), diz a cobertura (quanto do arquivo algum conversor reconheceu) e, com --mapa, confere
 * contra o rastreio (port/x360/tools/rastreio.py): cada posição que o jogo leu como número de largura w tem de
 * ter sido trocada com largura w. Com --saida grava o arquivo convertido em DIR/ARQUIVO e acrescenta o nome a
 * DIR/convertidos.txt (a build PPC da F1 lê esses arquivos e não aplica o mapa neles).
 * Sai com 1 se algum arquivo deu erro ou discordou do mapa.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "conv.h"

typedef struct Trecho { unsigned long ofs, n; int w; } Trecho;

static Trecho *ler_mapa(const char *caminho, const char *arquivo, unsigned *n) {
    FILE *fp = fopen(caminho, "r");
    char nome[512];
    unsigned long ofs, w, rep;
    Trecho *t = NULL;
    unsigned cap = 0;
    *n = 0;
    if (fp == NULL) {
        fprintf(stderr, "sem %s\n", caminho);
        exit(2);
    }
    while (fscanf(fp, "%511[^\t]\t%lu\t%lu\t%lu\n", nome, &ofs, &w, &rep) == 4) {
        if (strcmp(nome, arquivo) != 0) {
            continue;
        }
        if (*n == cap) {
            cap = cap ? cap * 2 : 256;
            t = realloc(t, cap * sizeof(Trecho));
        }
        t[*n].ofs = ofs;
        t[*n].n = rep;
        t[*n].w = (int)w;
        (*n)++;
    }
    fclose(fp);
    return t;
}

static void criar_pastas(char *caminho) {
    char *p;
    for (p = caminho + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(caminho, 0755);
            *p = '/';
        }
    }
}

static const char *formato, *mapa, *saida;
static int detalhe = 10;

/* converte e confere um arquivo; devolve 1 se falhou */
static int um(const char *dados, const char *rel) {
    int falhas = 0;
    char caminho[1024], msg[256];
    FILE *fp;
    long tam;
    conv_u8 *buf, *feito;
    int r;
    unsigned long k, cobertos = 0, trocados = 0;

    sprintf(caminho, "%s/%s", dados, rel);
    fp = fopen(caminho, "rb");
    if (fp == NULL) {
        printf("%s: não abre\n", rel);
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    tam = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = malloc((size_t)tam + 1);
    feito = calloc((size_t)tam + 1, 1);
    if (fread(buf, 1, (size_t)tam, fp) != (size_t)tam) {
        printf("%s: leitura curta\n", rel);
    }
    fclose(fp);
    msg[0] = '\0';
    r = formato ? Conv_Formato(formato, rel, buf, (conv_ofs)tam, feito, msg, sizeof(msg))
                : Conv_Arquivo(rel, buf, (conv_ofs)tam, feito, msg, sizeof(msg));
    if (r == 0) {
        printf("%s: sem conversor\n", rel);
        free(buf);
        free(feito);
        return 0;
    }
    for (k = 0; k < (unsigned long)tam; k++) {
        cobertos += feito[k] != CONV_LIVRE;
        trocados += feito[k] != CONV_LIVRE && feito[k] != CONV_BYTE;
    }
    printf("%s: %s; cobertura %lu de %ld bytes (%.1f%%), %lu em números trocados\n", rel,
           r < 0 ? msg : "convertido", cobertos, tam, tam ? 100.0 * cobertos / tam : 0.0, trocados);
    if (r < 0) {
        falhas++;
    }
    if (mapa != NULL) {
        unsigned n, t;
        unsigned long certo = 0, outra = 0, sem = 0, vistos = 0, fora = 0;
        int mostrados = 0;
        Trecho *tr = ler_mapa(mapa, rel, &n);
        for (t = 0; t < n; t++) {
            unsigned long j;
            for (j = 0; j < tr[t].n; j++) {
                unsigned long a = tr[t].ofs + j * (unsigned long)tr[t].w;
                const char *oque = NULL;
                vistos++;
                if (a >= (unsigned long)tam) {
                    fora++;
                    continue;
                }
                if (feito[a] == tr[t].w) {
                    certo++;
                    continue;
                }
                if (feito[a] == 2 || feito[a] == 4 || feito[a] == 8) {
                    outra++;
                    oque = "trocou com outra largura";
                } else {
                    sem++;
                    oque = feito[a] == CONV_LIVRE ? "não reconheceu" : feito[a] == CONV_BYTE ? "tratou como byte" :
                                                                            "no meio de outro número";
                }
                if (mostrados++ < detalhe) {
                    printf("    0x%lX: rastreio leu %d, conversor marca %u (%s)\n", a, tr[t].w, (unsigned)feito[a], oque);
                }
            }
        }
        printf("  contra o mapa: %lu posições; %lu iguais, %lu com outra largura, %lu não trocadas%s\n", vistos,
               certo, outra, sem, fora ? " (e algumas fora do arquivo)" : "");
        if (outra || sem) {
            falhas++;
        }
        free(tr);
    }
    if (saida != NULL && r > 0) {
        FILE *lista;
        sprintf(caminho, "%s/%s", saida, rel);
        criar_pastas(caminho);
        fp = fopen(caminho, "wb");
        if (fp == NULL || fwrite(buf, 1, (size_t)tam, fp) != (size_t)tam) {
            printf("%s: não gravou\n", caminho);
            falhas++;
        }
        if (fp != NULL) {
            fclose(fp);
        }
        sprintf(caminho, "%s/convertidos.txt", saida);
        lista = fopen(caminho, "a");
        if (lista != NULL) {
            fprintf(lista, "%s\n", rel);
            fclose(lista);
        }
    }
    free(buf);
    free(feito);
    return falhas;
}

int main(int argc, char **argv) {
    const char *dados = NULL;
    int i, falhas = 0, todos = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--formato") == 0 && i + 1 < argc) {
            formato = argv[++i];
        } else if (strcmp(argv[i], "--mapa") == 0 && i + 1 < argc) {
            mapa = argv[++i];
        } else if (strcmp(argv[i], "--saida") == 0 && i + 1 < argc) {
            saida = argv[++i];
        } else if (strcmp(argv[i], "--detalhe") == 0 && i + 1 < argc) {
            detalhe = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--todos") == 0) {
            todos = 1;
        } else if (dados == NULL) {
            dados = argv[i];
        } else {
            falhas += um(dados, argv[i]);
        }
    }
    if (dados == NULL) {
        fprintf(stderr, "uso: conv_tool [--formato F] [--mapa MAPA.tsv] [--saida DIR] [--detalhe N] [--todos] DADOS ARQUIVO...\n");
        return 2;
    }
    for (i = 0; todos && Conv_Registrado(i) != NULL; i++) {
        falhas += um(dados, Conv_Registrado(i));
    }
    return falhas ? 1 : 0;
}
