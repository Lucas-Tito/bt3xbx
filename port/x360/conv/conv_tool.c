/*
 * Ferramenta de PC da conversão (e o teste dos conversores na F1).
 *
 *   conv_tool [--formato F] [--mapa MAPA.tsv] [--saida DIR] [--detalhe N] DADOS ARQUIVO...
 *   conv_tool --disco DADOS        todos os arquivos que algum conversor conhece (pzs3us0..2), sem gravar: a
 *                                  validação estrutural (contagens, deslocamentos, enchimento) no disco inteiro;
 *                                  depois, o conteúdo descomprimido que o jogo converte em tempo de jogo (folhas da
 *                                  tela de carregamento e pacotes de tela dos menus)
 *
 * Para cada ARQUIVO (relativo a DADOS, como o jogo o pede: pzs3us1/01473.bin): converte com o formato do
 * registro (ou F), diz a cobertura (quanto do arquivo algum conversor reconheceu) e, com --mapa, confere
 * contra o rastreio (port/x360/tools/rastreio.py): cada posição que o jogo leu como número de largura w tem de
 * ter sido trocada com largura w. Com --saida grava o arquivo convertido em DIR/ARQUIVO e acrescenta o nome a
 * DIR/convertidos.txt (a build PPC da F1 lê esses arquivos e não aplica o mapa neles).
 * Um ARQUIVO "pzs3us1/00449.bin@0x40#bpe" (o nome que o rastreio dá) é o contêiner BPE nesse deslocamento do arquivo,
 * descomprimido aqui e convertido com o formato que o chamador do jogo aplica (formato_bpe), sem --saida.
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

/* lê o arquivo inteiro (tam + 1 bytes alocados); NULL se não abre */
static conv_u8 *ler_arquivo(const char *dados, const char *rel, long *tam) {
    char caminho[1024];
    FILE *fp;
    conv_u8 *buf;
    sprintf(caminho, "%s/%s", dados, rel);
    fp = fopen(caminho, "rb");
    if (fp == NULL) {
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    *tam = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = malloc((size_t)*tam + 1);
    if (fread(buf, 1, (size_t)*tam, fp) != (size_t)*tam) {
        printf("%s: leitura curta\n", rel);
    }
    fclose(fp);
    return buf;
}

static conv_u32 le32(const conv_u8 *p) {
    return (conv_u32)p[0] | (conv_u32)p[1] << 8 | (conv_u32)p[2] << 16 | (conv_u32)p[3] << 24;
}

/* src/sys/bpe.c (Bpe_Decode) com os limites conferidos: o contêiner em [ofs, tam) do arquivo ainda little-endian.
   Devolve o buffer de rawSize bytes (+ 1) ou NULL. */
static conv_u8 *descomprimir(const conv_u8 *arq, long tam, unsigned long ofs, long *bruto) {
    conv_u8 esq[256], dir[256], pilha[256], *res;
    unsigned long in, fimin, out = 0;
    long n;
    if (ofs + 8 > (unsigned long)tam) {
        return NULL;
    }
    n = (long)le32(arq + ofs);
    fimin = ofs + 8 + le32(arq + ofs + 4);
    if (n <= 0 || fimin > (unsigned long)tam) {
        return NULL;
    }
    res = calloc((size_t)n + 1, 1);
    in = ofs + 8;
    while (in < fimin) {
        int cont = arq[in++], c = 0, i, sp = 0;
        unsigned bloco;
        for (i = 0; i < 256; i++) {
            esq[i] = (conv_u8)i;
        }
        for (;;) {
            if (cont > 127) {
                c += cont - 127;
                cont = 0;
            }
            if (c == 256) {
                break;
            }
            for (i = 0; i <= cont && c < 256; i++, c++) {
                if (in + 2 > fimin) {
                    free(res);
                    return NULL;
                }
                esq[c] = arq[in++];
                if (c != esq[c]) {
                    dir[c] = arq[in++];
                }
            }
            if (c >= 256 || in >= fimin) {
                break;
            }
            cont = arq[in++];
        }
        if (in + 2 > fimin) {
            free(res);
            return NULL;
        }
        bloco = (unsigned)arq[in] << 8 | arq[in + 1];
        in += 2;
        for (;;) {
            int ch;
            if (sp != 0) {
                ch = pilha[--sp];
            } else {
                if (bloco-- == 0) {
                    break;
                }
                if (in >= fimin) {
                    free(res);
                    return NULL;
                }
                ch = arq[in++];
            }
            if (ch == esq[ch]) {
                if (out >= (unsigned long)n) {
                    free(res);
                    return NULL;
                }
                res[out++] = (conv_u8)ch;
            } else {
                if (sp > 254) {
                    free(res);
                    return NULL;
                }
                pilha[sp++] = dir[ch];
                pilha[sp++] = esq[ch];
            }
        }
    }
    if (in != fimin || out != (unsigned long)n) {
        free(res);
        return NULL;
    }
    *bruto = n;
    return res;
}

/* O formato do conteúdo descomprimido do contêiner em `ofs` do arquivo `rel` (ainda little-endian): o que o chamador
   do jogo aplica depois de descomprimir (docs/x360/formatos/menus.md 4; som-e-sprites.md 2.4; personagem.md 6.2).
   NULL: o jogo não converte esse conteúdo (ainda). */
static const char *formato_bpe(const char *rel, const conv_u8 *arq, long tam, unsigned long ofs) {
    const char *f = Conv_FormatoDe(rel);
    unsigned long n, s;
    if (f == NULL) {
        return NULL;
    }
    if (strcmp(f, "personagem_anim") == 0) {
        return "animacao"; /* BtlObjAnim_Load */
    }
    if (ofs == 0 && strcmp(f, "folha_bpe") == 0) {
        return "folha"; /* Load_InitScreen */
    }
    if (ofs == 0 && strcmp(f, "menu_bpe") == 0) {
        return "menu_mcflow"; /* McFlow_Init */
    }
    if (strcmp(f, "menu_pacote") != 0 || tam < 8) {
        return NULL;
    }
    n = le32(arq);
    for (s = 1; s <= n && (s + 1) * 4 <= (unsigned long)tam; s++) {
        if (le32(arq + s * 4) == ofs) {
            break;
        }
    }
    if (strcmp(rel, "pzs3us1/00473.bin") == 0 && s == 2) {
        return "menu_cartao"; /* BootCard_Run(2) */
    }
    if (strcmp(rel, "pzs3us1/00473.bin") == 0 && s == 3) {
        return "menu_logos"; /* FirstRun_Main */
    }
    if (strcmp(rel, "pzs3us1/00449.bin") == 0 && s == 1) {
        return "menu_titulo"; /* Title_Run(1) */
    }
    return NULL;
}

/* converte e confere um arquivo; devolve 1 se falhou */
static int um(const char *dados, const char *rel) {
    int falhas = 0;
    char caminho[1024], msg[256], base[512];
    FILE *fp;
    long tam;
    conv_u8 *buf, *feito;
    int r;
    unsigned long k, cobertos = 0, trocados = 0;
    const char *arroba = strstr(rel, "@0x"), *f = formato;
    int bpe = arroba != NULL && strlen(arroba) > 4 && strcmp(rel + strlen(rel) - 4, "#bpe") == 0;

    if (bpe) {
        long tarq;
        unsigned long ofs = strtoul(arroba + 3, NULL, 16);
        conv_u8 *arq;
        sprintf(base, "%.*s", (int)(arroba - rel), rel);
        arq = ler_arquivo(dados, base, &tarq);
        if (arq == NULL) {
            printf("%s: não abre\n", rel);
            return 1;
        }
        if (f == NULL) {
            f = formato_bpe(base, arq, tarq, ofs);
        }
        buf = f != NULL ? descomprimir(arq, tarq, ofs, &tam) : NULL;
        free(arq);
        if (f == NULL) {
            printf("%s: sem conversor\n", rel);
            return 0;
        }
        if (buf == NULL) {
            printf("%s: BPE inválido em 0x%lX\n", rel, ofs);
            return 1;
        }
    } else {
        buf = ler_arquivo(dados, rel, &tam);
        if (buf == NULL) {
            printf("%s: não abre\n", rel);
            return 1;
        }
    }
    feito = calloc((size_t)tam + 1, 1);
    msg[0] = '\0';
    r = f ? Conv_Formato(f, rel, buf, (conv_ofs)tam, feito, msg, sizeof(msg))
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
    if (saida != NULL && r > 0 && !bpe) {
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
    int i, falhas = 0, disco = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--formato") == 0 && i + 1 < argc) {
            formato = argv[++i];
        } else if (strcmp(argv[i], "--mapa") == 0 && i + 1 < argc) {
            mapa = argv[++i];
        } else if (strcmp(argv[i], "--saida") == 0 && i + 1 < argc) {
            saida = argv[++i];
        } else if (strcmp(argv[i], "--detalhe") == 0 && i + 1 < argc) {
            detalhe = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--disco") == 0) {
            disco = 1;
        } else if (dados == NULL) {
            dados = argv[i];
        } else {
            falhas += um(dados, argv[i]);
        }
    }
    if (dados == NULL) {
        fprintf(stderr, "uso: conv_tool [--formato F] [--mapa MAPA.tsv] [--saida DIR] [--detalhe N] [--disco] DADOS ARQUIVO...\n");
        return 2;
    }
    if (disco) {
        int p, n, ok = 0, erros = 0;
        for (p = 0; p < 3; p++) {
            for (n = 0; n < 100000; n++) {
                char rel[64], caminho[1024];
                FILE *fp;
                sprintf(rel, "pzs3us%d/%05d.bin", p, n);
                if (Conv_FormatoDe(rel) == NULL) {
                    continue;
                }
                sprintf(caminho, "%s/%s", dados, rel);
                if ((fp = fopen(caminho, "rb")) == NULL) {
                    continue;
                }
                fclose(fp);
                if (um(dados, rel)) {
                    erros++;
                } else {
                    ok++;
                }
            }
        }
        printf("disco: %d arquivos convertidos sem erro, %d com erro\n", ok, erros);
        falhas += erros;
        /* o conteúdo descomprimido que o jogo converte: as folhas (contêiner no começo) e as seções dos menus */
        {
            static const struct { const char *rel; int secao; } desc[] = {
                {"pzs3us1/00960.bin", 0}, {"pzs3us1/00961.bin", 0}, {"pzs3us1/00962.bin", 0},
                {"pzs3us1/00477.bin", 0}, {"pzs3us1/00473.bin", 2}, {"pzs3us1/00473.bin", 3},
                {"pzs3us1/00449.bin", 1},
            };
            unsigned d;
            ok = erros = 0;
            for (d = 0; d < sizeof(desc) / sizeof(desc[0]); d++) {
                long t;
                conv_u8 *arq = ler_arquivo(dados, desc[d].rel, &t);
                char rel[64];
                unsigned long ofs = 0;
                if (arq == NULL) {
                    printf("%s: não abre\n", desc[d].rel);
                    erros++;
                    continue;
                }
                if (desc[d].secao > 0 && (desc[d].secao + 1) * 4 <= t) {
                    ofs = le32(arq + desc[d].secao * 4);
                }
                free(arq);
                sprintf(rel, "%s@0x%lX#bpe", desc[d].rel, ofs);
                if (um(dados, rel)) {
                    erros++;
                } else {
                    ok++;
                }
            }
            printf("disco (descomprimidos): %d convertidos sem erro, %d com erro\n", ok, erros);
            falhas += erros;
        }
    }
    return falhas ? 1 : 0;
}
