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
 * campo a campo, pela largura de cada número, em vez de palavra a palavra. Ao lado de cada .marcas, o .orig: o conteúdo
 * antes da conversão (bytes do jogo: só no WSL), com que o memdiff.py acha o bloco do heap pelo conteúdo.
 * Um ARQUIVO "pzs3us1/00449.bin@0x40#bpe" (o nome que o rastreio dá) é o contêiner BPE nesse deslocamento do arquivo,
 * descomprimido aqui e convertido com o formato que o chamador do jogo aplica (formato_bpe), sem --saida; e de novo
 * pelo caminho do jogo (Conv_FormatoBits, o mapa de bits de conv_jogo.c), que tem de dar os mesmos bytes, o mesmo
 * resultado e a mesma cobertura, num conteúdo de até CONV_JOGO_MAX bytes.
 *   conv_tool --pecas DADOS [--mapa MAPA.tsv]   as peças comuns dos menus (menus-pecas.md), cada uma só na seção dela
 *                                  dentro do conteúdo descomprimido de cada pacote que a usa (a tabela sUsos), antes
 *                                  de o pacote inteiro ganhar conversor; com --mapa, só as posições dessas seções
 * Sai com 1 se algum arquivo deu erro ou discordou do mapa.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "conv.h"
#include "conv_menus.h"

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
static long sMaiorJogo; /* o maior conteúdo descomprimido convertido (o que o jogo converte em tempo de jogo) */
static char sMaiorJogoNome[96];

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

/* Uma leitura de w bytes no meio de uma cadeia de texto que o conversor deixou em bytes: o rastreio vê o laço que
   procura o fim do rótulo de um quadro do Flash (FlashTl_RunFrame, `while (frame[ofs++] != 0)`, que o compilador da
   x86r pode trocar por strlen) e as cópias de nome (Flash_ReadString, memcpy do tamanho da cadeia) como leituras de 2,
   4 ou 8 bytes, e o mapa as dá como número. É cadeia quando os w bytes estão marcados como byte e são todos texto
   imprimível ou o zero do fim. Contados à parte na linha "contra o mapa", nunca em silêncio. */
static int cadeia_lida(const conv_u8 *buf, const conv_u8 *feito, unsigned long tam, unsigned long a, int w) {
    int k;
    if (a + (unsigned long)w > tam) {
        return 0;
    }
    for (k = 0; k < w; k++) {
        conv_u8 x = buf[a + k];
        if (feito[a + k] != CONV_BYTE || !((x >= 0x20 && x < 0x7F) || (x == 0 && k > 0))) {
            return 0;
        }
    }
    return 1;
}

/* confere a marcação contra o rastreio (--mapa): cada posição que o jogo leu como número de largura w, dentro de
   [ini, fim), tem de ter sido trocada com largura w (ou ser uma cadeia lida por inteiro, cadeia_lida). Imprime a linha
   "contra o mapa" e devolve 1 se discordou */
static int contra_mapa(const char *rel, const conv_u8 *buf, const conv_u8 *feito, unsigned long tam, unsigned long ini,
                       unsigned long fim) {
    unsigned n, t;
    unsigned long certo = 0, outra = 0, sem = 0, vistos = 0, fora = 0, cadeias = 0;
    int mostrados = 0;
    Trecho *tr = ler_mapa(mapa, rel, &n);
    for (t = 0; t < n; t++) {
        unsigned long j;
        for (j = 0; j < tr[t].n; j++) {
            unsigned long a = tr[t].ofs + j * (unsigned long)tr[t].w;
            const char *oque = NULL;
            if (a < ini || a >= fim) {
                continue;
            }
            vistos++;
            if (a >= tam) {
                fora++;
                continue;
            }
            if (feito[a] == tr[t].w) {
                certo++;
                continue;
            }
            if (cadeia_lida(buf, feito, tam, a, tr[t].w)) {
                cadeias++;
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
    printf("  contra o mapa: %lu posições; %lu iguais, %lu com outra largura, %lu não trocadas%s", vistos, certo, outra,
           sem, fora ? " (e algumas fora do arquivo)" : "");
    if (cadeias) {
        printf("; %lu em cadeia de texto lida por inteiro", cadeias);
    }
    printf("\n");
    free(tr);
    return outra || sem;
}

/* converte e confere um arquivo; devolve 1 se falhou */
static int um(const char *dados, const char *rel) {
    int falhas = 0;
    char caminho[1024], msg[256], base[512];
    FILE *fp;
    long tam;
    conv_u8 *buf, *feito, *copia = NULL, *bits = NULL;
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
    if (marcas != NULL) {
        /* ao lado das marcas, o conteúdo como o jogo o carrega (antes da conversão): o memdiff.py --marcas-vivas acha
           pelo conteúdo o bloco do heap que guarda este arquivo, quando há vários do mesmo tamanho. Fica no WSL, como
           os dados (port/build) */
        char *q;
        sprintf(caminho, "%s/%s.orig", marcas, rel);
        for (q = caminho + strlen(marcas) + 1; *q; q++) {
            if (*q == '/') {
                *q = '_';
            }
        }
        fp = fopen(caminho, "wb");
        if (fp == NULL || fwrite(buf, 1, (size_t)tam, fp) != (size_t)tam) {
            printf("%s: não gravou\n", caminho);
            falhas++;
        }
        if (fp != NULL) {
            fclose(fp);
        }
    }
    if (bpe) {
        copia = (conv_u8 *)malloc((size_t)tam + 1);
        bits = (conv_u8 *)calloc(((size_t)tam + 7) / 8 + 1, 1);
        if (copia == NULL || bits == NULL) {
            printf("%s: sem memória para a conferência do mapa de bits\n", rel);
            return 1;
        }
        memcpy(copia, buf, (size_t)tam);
    }
    r = f ? Conv_Formato(f, rel, buf, (conv_ofs)tam, feito, msg, sizeof(msg))
          : Conv_Arquivo(rel, buf, (conv_ofs)tam, feito, msg, sizeof(msg));
    if (r == 0) {
        printf("%s: sem conversor\n", rel);
        free(buf);
        free(feito);
        free(copia);
        free(bits);
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
    if (bpe) {
        /* o conteúdo que o jogo converte em tempo de jogo, de novo pelo caminho do jogo (Conv_FormatoBits, conv_jogo.c,
           com o mapa de bits): os mesmos bytes, o mesmo resultado e a mesma cobertura que o mapa de bytes. Depois da
           linha da cobertura, para a linha de erro ser a última do nome (roteiros.py, conv()) */
        char msg2[256];
        unsigned long cob2 = 0;
        int r2;
        msg2[0] = '\0';
        r2 = Conv_FormatoBits(f, rel, copia, (conv_ofs)tam, bits, msg2, sizeof(msg2));
        for (k = 0; k < (unsigned long)tam; k++) {
            cob2 += (bits[k >> 3] >> (k & 7)) & 1;
        }
        if (r2 != r || (r > 0 && memcmp(copia, buf, (size_t)tam) != 0) || (r < 0 && strcmp(msg, msg2) != 0) ||
            (r > 0 && cob2 != cobertos)) {
            printf("%s: o mapa de bits da conversão em jogo diverge do mapa de bytes (resultado %d e %d, cobertura %lu "
                   "e %lu, bytes %s)\n", rel, r, r2, cobertos, cob2,
                   memcmp(copia, buf, (size_t)tam) == 0 ? "iguais" : "diferentes");
            falhas++;
        }
        if (tam > sMaiorJogo) {
            sMaiorJogo = tam;
            sprintf(sMaiorJogoNome, "%.95s", rel);
        }
        if ((unsigned long)tam > CONV_JOGO_MAX) {
            printf("%s: 0x%lX bytes descomprimidos, acima do mapa da conversão em jogo (CONV_JOGO_MAX 0x%lX, conv.h)\n",
                   rel, (unsigned long)tam, (unsigned long)CONV_JOGO_MAX);
            falhas++;
        }
        free(copia);
        free(bits);
    }
    if (mapa != NULL) {
        falhas += contra_mapa(rel, buf, feito, (unsigned long)tam, 0, (unsigned long)tam);
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
        /* as páginas do Treino (0x402 + página, Train_UpdateImage): as aulas vão da página 0 à 397
           (menus-telas.md 4.2), a última antes dos slots de lutador */
        {0x402 - 1, 0x590 - 2, "páginas do Treino (0x402 + página, 0..397 pelas aulas)", 0},
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

/* A tabela única de conv.c (sFaixas) promete que as faixas não se sobrepõem: duas linhas da mesma seção com ids em
   comum dariam a um arquivo dois formatos, e a primeira ganharia em silêncio. Confere a cada execução, e também que
   cada linha decide o formato de todos os ids dela (a regra do lutador, fora da tabela, não pode tomar uns). */
static int faixas_conferidas(void) {
    int n, a, b, erros = 0;
    const ConvFaixa *f = Conv_Faixas(&n);
    for (a = 0; a < n; a++) {
        int i;
        for (b = a + 1; b < n; b++) {
            if (f[a].secao == f[b].secao && f[a].ini <= f[b].fim && f[b].ini <= f[a].fim) {
                fprintf(stderr, "conv_tool: faixas sobrepostas em sFaixas (conv.c): %d..%d \"%s\" e %d..%d \"%s\", "
                        "seção %d\n", f[a].ini, f[a].fim, f[a].formato, f[b].ini, f[b].fim, f[b].formato, f[a].secao);
                erros++;
            }
        }
        for (i = f[a].ini; i <= f[a].fim; i++) {
            char rel[32];
            const char *g;
            sprintf(rel, "pzs3us1/%05d.bin", i);
            g = f[a].secao == CONV_ARQ ? Conv_FormatoDe(rel) : Conv_FormatoSecao(rel, f[a].secao);
            if (g == NULL || strcmp(g, f[a].formato) != 0) {
                fprintf(stderr, "conv_tool: o id %d da faixa %d..%d \"%s\" (seção %d) sai com o formato %s\n", i,
                        f[a].ini, f[a].fim, f[a].formato, f[a].secao, g != NULL ? g : "nenhum");
                erros++;
                break;
            }
        }
    }
    return erros == 0;
}

/* --pecas (F4, etapa 5a): as peças comuns dos menus (docs/x360/formatos/menus-pecas.md) validadas sozinhas, antes de
   o pacote de tela que as contém ganhar conversor. Para cada uso da tabela abaixo (arquivo, seção do pacote externo ou
   0 para o contêiner no começo, seção do conteúdo descomprimido, peça), em cada arquivo da faixa: descomprime, aplica
   a peça só à seção dela e diz a cobertura dela; com --mapa, confere as posições do rastreio que caem nessa seção
   (o nome do conteúdo é o do rastreio, "pzs3us1/00448.bin@0x40#bpe"). Os usos vêm da tabela de uso por pacote de
   menus-pecas.md (seções 1 e 5); as peças das telas fora do caminho essencial entram também, para o disco dizer já
   o que as especificações ainda não cobrem. */
static const struct { const char *nome; ConvFn fn; } sPecas[] = {
    {"JanelaMsg", Conv_JanelaMsg}, {"JanelaIcone", Conv_JanelaIcone}, {"LipPack", Conv_LipPack},
    {"Textos", Conv_Textos}, {"Flash", Conv_Flash}, {"Dialogo", Conv_Dialogo},
    {"ItemPanel", Conv_ItemPanel}, {"ItemHelp", Conv_ItemHelp}, {"ListaIds", Conv_ListaIds},
    {"ListaEstagios", Conv_ListaEstagios}, {"ListaMusicas", Conv_ListaMusicas},
    {"ListaMusicasOpcoes", Conv_ListaMusicasOpcoes}, {"Celulas", Conv_Celulas},
    {"FichasPersonagem", Conv_FichasPersonagem}, {"IconesEstagio", Conv_IconesEstagio},
    {"LicoesTreino", Conv_LicoesTreino},
};
typedef struct UsoPeca { int ini, fim, secao, sub; const char *peca; } UsoPeca;
static const UsoPeca sUsos[] = {
    /* caminho essencial: Duelo (448 s1), seleção (448 s2 e 461 s2), Treino (461 s1), Opções (453 s1) */
    {448, 448, 1, 21, "Textos"}, {448, 448, 1, 22, "JanelaMsg"}, {448, 448, 1, 23, "JanelaIcone"},
    {448, 448, 1, 25, "Dialogo"}, {448, 448, 1, 26, "LipPack"}, {448, 448, 1, 27, "Textos"},
    {448, 448, 1, 19, "Flash"},
    {448, 448, 2, 39, "ListaEstagios"}, {448, 448, 2, 42, "Celulas"}, {448, 448, 2, 43, "Textos"},
    {448, 448, 2, 44, "Textos"}, {448, 448, 2, 45, "FichasPersonagem"}, {448, 448, 2, 46, "IconesEstagio"},
    {448, 448, 2, 51, "ItemHelp"}, {448, 448, 2, 53, "ItemPanel"}, {448, 448, 2, 54, "JanelaIcone"},
    {448, 448, 2, 56, "ListaMusicas"},
    {461, 461, 2, 39, "ListaEstagios"}, {461, 461, 2, 42, "Celulas"}, {461, 461, 2, 43, "Textos"},
    {461, 461, 2, 44, "Textos"}, {461, 461, 2, 45, "FichasPersonagem"}, {461, 461, 2, 46, "IconesEstagio"},
    {461, 461, 2, 51, "ItemHelp"}, {461, 461, 2, 53, "ItemPanel"}, {461, 461, 2, 54, "JanelaIcone"},
    {461, 461, 2, 56, "ListaMusicas"},
    {461, 461, 1, 1, "Flash"}, {461, 461, 1, 10, "JanelaIcone"}, {461, 461, 1, 11, "JanelaMsg"},
    {461, 461, 1, 12, "LipPack"}, {461, 461, 1, 13, "LipPack"}, {461, 461, 1, 14, "Textos"},
    {461, 461, 1, 15, "Textos"}, {461, 461, 1, 16, "Textos"}, {461, 461, 1, 17, "Textos"},
    {461, 461, 1, 24, "ListaIds"}, {461, 461, 1, 26, "ListaIds"}, {461, 461, 1, 22, "LicoesTreino"},
    {453, 453, 1, 8, "Flash"}, {453, 453, 1, 31, "JanelaMsg"}, {453, 453, 1, 32, "JanelaIcone"},
    {453, 453, 1, 33, "Textos"}, {453, 453, 1, 35, "Dialogo"}, {453, 453, 1, 36, "LipPack"},
    {453, 453, 1, 37, "ListaMusicasOpcoes"}, {453, 453, 1, 38, "Textos"},
    /* os outros modos (etapa 9): EntrySel (450 s2), SoloSel e UbTeamSel (451 s4), MisSel (451 s2), SimTop (451 s5),
       Loja e EvoZ (452 s2, s3), chave do torneio (454..458), Data Center (459 s2..s4), ranking e cursos do Disc
       Fusion (475, 476) */
    {450, 450, 2, 29, "Celulas"}, {450, 450, 2, 32, "FichasPersonagem"}, {450, 450, 2, 35, "ItemHelp"},
    {450, 450, 2, 39, "ItemPanel"},
    {451, 451, 4, 22, "ItemPanel"}, {451, 451, 4, 24, "ItemHelp"}, {451, 451, 4, 27, "FichasPersonagem"},
    {451, 451, 4, 30, "Celulas"}, {451, 451, 2, 15, "FichasPersonagem"}, {451, 451, 5, 7, "FichasPersonagem"},
    {452, 452, 2, 25, "ItemHelp"}, {452, 452, 3, 2, "ItemHelp"}, {452, 452, 3, 33, "Celulas"},
    {452, 452, 3, 36, "FichasPersonagem"},
    {454, 458, 0, 23, "FichasPersonagem"}, {454, 458, 0, 24, "Celulas"},
    {459, 459, 2, 16, "ItemHelp"}, {459, 459, 3, 20, "Celulas"}, {459, 459, 4, 8, "FichasPersonagem"},
    {475, 475, 0, 6, "FichasPersonagem"}, {476, 476, 0, 6, "FichasPersonagem"},
};

static int pecas(const char *dados) {
    unsigned u, k;
    int ok = 0, erros = 0, i;
    for (u = 0; u < sizeof(sUsos) / sizeof(sUsos[0]); u++) {
        ConvFn fn = NULL;
        for (k = 0; k < sizeof(sPecas) / sizeof(sPecas[0]); k++) {
            if (strcmp(sPecas[k].nome, sUsos[u].peca) == 0) {
                fn = sPecas[k].fn;
            }
        }
        if (fn == NULL) {
            printf("--pecas: peça %s sem função\n", sUsos[u].peca);
            erros++;
            continue;
        }
        for (i = sUsos[u].ini; i <= sUsos[u].fim; i++) {
            char arqrel[64], rel[96];
            long tarq, tam;
            unsigned long ofs = 0, n, a, b, cob = 0, j;
            conv_u8 *arq, *buf, *feito;
            ConvCtx c;
            sprintf(arqrel, "pzs3us1/%05d.bin", i);
            arq = ler_arquivo(dados, arqrel, &tarq);
            if (arq == NULL) {
                printf("%s: não abre\n", arqrel);
                erros++;
                continue;
            }
            if (sUsos[u].secao > 0) {
                ofs = (unsigned long)(sUsos[u].secao + 1) * 4 <= (unsigned long)tarq ? le32(arq + sUsos[u].secao * 4) : 0;
            }
            sprintf(rel, "%s@0x%lX#bpe", arqrel, ofs);
            buf = descomprimir(arq, tarq, ofs, &tam);
            free(arq);
            if (buf == NULL) {
                printf("%s: BPE inválido\n", rel);
                erros++;
                continue;
            }
            n = tam >= 4 ? le32(buf) : 0;
            if (sUsos[u].sub > (int)n || (unsigned long)(sUsos[u].sub + 2) * 4 > (unsigned long)tam) {
                printf("%s seção %d (%s): o conteúdo tem %lu seções\n", rel, sUsos[u].sub, sUsos[u].peca, n);
                free(buf);
                erros++;
                continue;
            }
            a = le32(buf + sUsos[u].sub * 4) >> 2 << 2; /* MPACK_AT */
            b = le32(buf + (sUsos[u].sub + 1) * 4);
            if (a > b || b > (unsigned long)tam) {
                printf("%s seção %d (%s): 0x%lX..0x%lX fora do conteúdo\n", rel, sUsos[u].sub, sUsos[u].peca, a, b);
                free(buf);
                erros++;
                continue;
            }
            feito = calloc((size_t)tam + 1, 1);
            Conv_Iniciar(&c, rel, buf, (conv_ofs)tam, feito);
            fn(&c, (conv_ofs)a, (conv_ofs)b);
            for (j = a; j < b; j++) {
                cob += feito[j] != CONV_LIVRE;
            }
            printf("%s seção %d (%s): %s; cobertura %lu de %lu bytes (%.1f%%)\n", rel, sUsos[u].sub, sUsos[u].peca,
                   c.erro ? c.msg : "convertido", cob, b - a, b > a ? 100.0 * cob / (b - a) : 100.0);
            if (c.erro || cob != b - a) {
                erros++;
            } else {
                ok++;
            }
            if (mapa != NULL) {
                erros += contra_mapa(rel, buf, feito, (unsigned long)tam, a, b);
            }
            free(feito);
            free(buf);
        }
    }
    printf("peças: %d usos convertidos sem erro, %d com erro\n", ok, erros);
    return erros;
}

int main(int argc, char **argv) {
    const char *dados = NULL;
    int i, falhas = 0, disco = 0, so_pecas = 0;
    if (!faixas_conferidas()) {
        return 1;
    }
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
        } else if (strcmp(argv[i], "--pecas") == 0) {
            so_pecas = 1;
        } else if (dados == NULL) {
            dados = argv[i];
        } else {
            falhas += um(dados, argv[i]);
        }
    }
    if (dados == NULL) {
        fprintf(stderr, "uso: conv_tool [--formato F] [--mapa MAPA.tsv] [--saida DIR] [--marcas DIR] [--detalhe N] [--disco] [--pecas] DADOS "
                        "ARQUIVO...\n");
        return 2;
    }
    if (so_pecas) {
        falhas += pecas(dados);
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
            printf("conversão em jogo: o maior conteúdo tem 0x%lX bytes (%s); o mapa de bits do jogo se reserva para "
                   "0x%lX (CONV_JOGO_MAX)\n", (unsigned long)sMaiorJogo, sMaiorJogoNome, (unsigned long)CONV_JOGO_MAX);
            falhas += erros;
        }
        censo(dados);
    }
    return falhas ? 1 : 0;
}
