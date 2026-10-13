/*
 * Ferramenta de PC da conversão (e o teste dos conversores na F1).
 *
 *   conv_tool [--formato F] [--mapa MAPA.tsv] [--saida DIR] [--marcas DIR] [--detalhe N] DADOS ARQUIVO...
 *   conv_tool --disco DADOS        todos os arquivos que algum conversor conhece (pzs3us0..2), sem gravar: a
 *                                  validação estrutural (contagens, deslocamentos, enchimento) no disco inteiro;
 *                                  depois, o conteúdo descomprimido que o jogo converte em tempo de jogo (folhas da
 *                                  tela de carregamento e pacotes de tela dos menus); no fim, o censo dos arquivos
 *                                  de menu por faixa de id (o que ainda está sem conversor)
 *
 * Para cada ARQUIVO (relativo a DADOS, como o jogo o pede: pzs3us1/01473.bin): converte com o formato do
 * registro (ou F), diz a cobertura (quanto do arquivo algum conversor reconheceu) e, com --mapa, confere
 * contra o rastreio (port/x360/tools/rastreio.py): cada posição que o jogo leu como número de largura w tem de
 * ter sido trocada com largura w. Com --saida grava o arquivo convertido em DIR/ARQUIVO e acrescenta o nome a
 * DIR/convertidos.txt (a build PPC da F1 lê esses arquivos e não aplica o mapa neles).
 * Com --marcas grava em DIR a marcação de cada byte (a largura com que o conversor o trocou, 1 para byte, 0 se nenhum
 * conversor o reconheceu, 0x80 na continuação de um número), um arquivo por ARQUIVO com '/' trocado por '_' e
 * ".marcas" no fim: só larguras, nenhum byte do jogo. É o que memdiff.py --marcas usa para comparar um buffer do jogo
 * campo a campo, pela largura de cada número, em vez de palavra a palavra.
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

static const char *formato, *mapa, *saida, *marcas;
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

/* A seção do arquivo `rel` (ainda little-endian) onde começa o contêiner em `ofs`: 0 no começo do arquivo, n se é a
   seção n da tabela de um menu_pacote (menus.md 2.1), -2 se não é nenhuma das duas. */
static int secao_de(const char *rel, const conv_u8 *arq, long tam, unsigned long ofs) {
    const char *f = Conv_FormatoDe(rel);
    unsigned long n, s;
    if (ofs == 0) {
        return 0;
    }
    if (f == NULL || strcmp(f, "menu_pacote") != 0 || tam < 8) {
        return -2;
    }
    n = le32(arq);
    for (s = 1; s <= n && (s + 1) * 4 <= (unsigned long)tam; s++) {
        if (le32(arq + s * 4) == ofs) {
            return (int)s;
        }
    }
    return -2;
}

/* O formato do conteúdo descomprimido do contêiner em `ofs` do arquivo `rel`: o que o chamador do jogo aplica depois
   de descomprimir, pela tabela única de conv.c (Conv_FormatoSecao; docs/x360/formatos/menus.md 4; som-e-sprites.md
   2.4; personagem.md 6.2). NULL: o jogo não converte esse conteúdo (ainda). */
static const char *formato_bpe(const char *rel, const conv_u8 *arq, long tam, unsigned long ofs) {
    if (Conv_FormatoDe(rel) == NULL) {
        return NULL; /* o arquivo chegaria cru: nem o contêiner se leria */
    }
    /* (as animações do lutador estão em qualquer deslocamento: Conv_FormatoSecao não olha a seção delas) */
    return Conv_FormatoSecao(rel, secao_de(rel, arq, tam, ofs));
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
    if (marcas != NULL && r > 0) {
        char *q;
        sprintf(caminho, "%s/%s.marcas", marcas, rel);
        for (q = caminho + strlen(marcas) + 1; *q; q++) {
            if (*q == '/') {
                *q = '_';
            }
        }
        fp = fopen(caminho, "wb");
        if (fp == NULL || fwrite(feito, 1, (size_t)tam, fp) != (size_t)tam) {
            printf("%s: não gravou\n", caminho);
            falhas++;
        }
        if (fp != NULL) {
            fclose(fp);
        }
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

/* O censo dos arquivos de menu (F4, docs/x360/formatos/menus-modos.md): por faixa de id, quantos arquivos há, quantos
   o disco converte (o arquivo e o conteúdo de cada contêiner BPE dele, pela tabela única de conv.c) e quantos ficam
   "sem conversor". A forma de cada arquivo sem formato se reconhece pela estrutura (menus.md 2.1 e 3): tabela de
   seções que fecha no tamanho do arquivo (pacote), ou um contêiner BPE inteiro. Só contagens: nenhum byte sai. */
typedef struct Censo { int arqs, arq_conv, pacote, bpe, outra, cont, cont_conv; } Censo;

static int bpe_plausivel(const conv_u8 *p, unsigned long n) {
    unsigned long bruto, comp;
    if (n < 8) {
        return 0;
    }
    bruto = le32(p);
    comp = le32(p + 4);
    return bruto > 0 && bruto < 0x4000000UL && comp > 0 && comp <= n - 8;
}

/* n de seções se arq é uma tabela de seções que fecha no tamanho (menus.md 2.1); 0 se não */
static unsigned long pacote_n(const conv_u8 *arq, long tam) {
    unsigned long n, s, ant;
    if (tam < 16) {
        return 0;
    }
    n = le32(arq);
    if (n < 1 || n > 255 || (n + 2) * 4 > (unsigned long)tam || le32(arq + (n + 1) * 4) != (unsigned long)tam) {
        return 0;
    }
    ant = (n + 2) * 4;
    for (s = 1; s <= n + 1; s++) {
        unsigned long o = le32(arq + s * 4);
        if (o < ant || o > (unsigned long)tam) {
            return 0;
        }
        ant = o;
    }
    return n;
}

static void censo_arquivo(const char *dados, const char *rel, Censo *c, int detalhe_arq) {
    long tam;
    conv_u8 *arq = ler_arquivo(dados, rel, &tam);
    unsigned long n, s;
    int conv, sem = 0, cont = 0;
    if (arq == NULL) {
        return;
    }
    c->arqs++;
    conv = Conv_FormatoDe(rel) != NULL;
    c->arq_conv += conv;
    if ((n = pacote_n(arq, tam)) > 0) {
        c->pacote++;
        for (s = 1; s <= n; s++) {
            unsigned long o = le32(arq + s * 4), fim = le32(arq + (s + 1) * 4);
            if (fim > o && bpe_plausivel(arq + o, fim - o)) {
                int f = conv && Conv_FormatoSecao(rel, (int)s) != NULL;
                cont++;
                c->cont++;
                c->cont_conv += f;
                sem += !f;
            }
        }
    } else if (bpe_plausivel(arq, (unsigned long)tam)) {
        int f = conv && Conv_FormatoSecao(rel, 0) != NULL;
        c->bpe++;
        cont = 1;
        c->cont++;
        c->cont_conv += f;
        sem += !f;
    } else {
        c->outra++;
    }
    if (detalhe_arq) {
        printf("  %s: %s, arquivo %s, %d contêineres BPE, %d sem conversor\n", rel,
               n > 0 ? "pacote" : cont ? "BPE inteiro" : "outra forma", conv ? "convertido" : "sem conversor", cont, sem);
    }
    free(arq);
}

static void censo(const char *dados) {
    /* as faixas dos arquivos que os menus pedem (menus-modos.md 1): baseFile 0x1C1 + 0..0x1D, retratos 0x2F9 +
       personagem, imagens de estágio 0x39D + estágio (até 0x3C0, que o EvoZ usa de fundo), convites e fundos do
       torneio, páginas do Treino; índice = id - 1 */
    static const struct { int ini, fim; const char *nome; int detalhe; } faixas[] = {
        {0x1C1 - 1, 0x1DE - 1, "arquivos de menu (baseFile 0x1C1 + 0..0x1D)", 1},
        {0x2F9 - 1, 0x39C - 1, "retratos (0x2F9 + personagem)", 0},
        {0x39D - 1, 0x3C0 - 1, "imagens de estágio (0x39D + estágio)", 0},
        {0x3C4 - 1, 0x3CD - 1, "convites (0x3C4 + torneio) e fundos (0x3C9 + torneio) do torneio", 0},
        {0x3CE - 1, 0x401 - 1, "imagens da História (0x3CE.. episódios, 0x3FE.. fundos do resultado)", 0},
        /* as páginas do Treino (0x402 + página, Train_UpdateImage): o teto é hipótese, até os slots de lutador */
        {0x402 - 1, 0x590 - 2, "páginas do Treino (0x402 + página; o fim da faixa é hipótese)", 0},
    };
    unsigned f;
    for (f = 0; f < sizeof(faixas) / sizeof(faixas[0]); f++) {
        Censo c;
        int i;
        memset(&c, 0, sizeof(c));
        printf("censo: %s, índices %d..%d\n", faixas[f].nome, faixas[f].ini, faixas[f].fim);
        for (i = faixas[f].ini; i <= faixas[f].fim; i++) {
            char rel[64];
            sprintf(rel, "pzs3us1/%05d.bin", i);
            censo_arquivo(dados, rel, &c, faixas[f].detalhe);
        }
        printf("censo: %d arquivos (%d pacotes, %d BPE inteiros, %d de outra forma); arquivo: %d convertidos, %d sem "
               "conversor; conteúdo BPE: %d contêineres, %d com conversor, %d sem conversor\n", c.arqs, c.pacote,
               c.bpe, c.outra, c.arq_conv, c.arqs - c.arq_conv, c.cont, c.cont_conv, c.cont - c.cont_conv);
    }
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
        } else if (strcmp(argv[i], "--marcas") == 0 && i + 1 < argc) {
            marcas = argv[++i];
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
        fprintf(stderr, "uso: conv_tool [--formato F] [--mapa MAPA.tsv] [--saida DIR] [--marcas DIR] [--detalhe N] [--disco] DADOS "
                        "ARQUIVO...\n");
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
        /* o conteúdo descomprimido que o jogo converte: as linhas da tabela única com seção (as folhas, com o
           contêiner no começo, e as seções dos menus) */
        {
            int nf, d, a;
            const ConvFaixa *fx = Conv_Faixas(&nf);
            ok = erros = 0;
            for (d = 0; d < nf; d++) {
                for (a = fx[d].ini; fx[d].secao >= 0 && a <= fx[d].fim; a++) {
                    long t;
                    char arqrel[64], rel[96];
                    conv_u8 *arq;
                    unsigned long ofs = 0;
                    sprintf(arqrel, "pzs3us1/%05d.bin", a);
                    arq = ler_arquivo(dados, arqrel, &t);
                    if (arq == NULL) {
                        printf("%s: não abre\n", arqrel);
                        erros++;
                        continue;
                    }
                    if (fx[d].secao > 0 && (fx[d].secao + 1) * 4 <= t) {
                        ofs = le32(arq + fx[d].secao * 4);
                    }
                    free(arq);
                    sprintf(rel, "%s@0x%lX#bpe", arqrel, ofs);
                    if (um(dados, rel)) {
                        erros++;
                    } else {
                        ok++;
                    }
                }
            }
            printf("disco (descomprimidos): %d convertidos sem erro, %d com erro\n", ok, erros);
            falhas += erros;
        }
        censo(dados);
    }
    return falhas ? 1 : 0;
}
