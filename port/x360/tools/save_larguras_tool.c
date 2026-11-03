/*
 * Ferramenta de PC sobre o mapa de larguras do save (port/x360/conv/save_larguras.c): compilada com gcc -std=c89 pelo
 * save_larguras.py e pelo teste_save_larguras.py; não entra no console.
 *
 *   save_larguras_tool imprime     uma linha por faixa: arquivo desl qtd larg cat nome (hexadecimal no desl)
 *   save_larguras_tool confere     confere o mapa por dentro e sai com 1 no primeiro defeito:
 *                                    - cobertura exata de cada arquivo: sem buraco, sem sobreposição, no tamanho certo
 *                                    - larguras só 1, 2, 4 ou 8, cada número alinhado ao próprio tamanho
 *                                    - categorias válidas (e as largura-1 sem número marcadas SEM_ACESSO / DISCO)
 *                                    - SaveLarg_Inverte com bytes sintéticos: duas vezes devolve o original; uma vez
 *                                      inverte cada número na largura do mapa e deixa os bytes de largura 1
 *                                    - tamanho errado recusado sem mexer no buffer
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "save_larguras.h"

static int erros;

static void erro(const char *fmt, int arq, unsigned long a, unsigned long b) {
    printf("ERRO: ");
    printf(fmt, SaveLarg_NomeArquivo(arq), a, b);
    printf("\n");
    erros++;
}

static int confere_cobertura(int arq) {
    int n, i;
    const SaveFaixa *f = SaveLarg_Faixas(arq, &n);
    unsigned long pos = 0, fim;
    if (f == 0 || n <= 0) {
        erro("%s: sem faixas (%lu %lu)", arq, 0, 0);
        return 1;
    }
    for (i = 0; i < n; i++) {
        if (f[i].larg != 1 && f[i].larg != 2 && f[i].larg != 4 && f[i].larg != 8) {
            erro("%s: faixa em 0x%lx com largura %lu", arq, f[i].desl, (unsigned long)f[i].larg);
            return 1;
        }
        if (f[i].qtd == 0) {
            erro("%s: faixa vazia em 0x%lx (%lu)", arq, f[i].desl, 0);
            return 1;
        }
        if (f[i].desl % (unsigned long)f[i].larg) {
            erro("%s: número em 0x%lx sem alinhamento natural (largura %lu)", arq, f[i].desl, (unsigned long)f[i].larg);
            return 1;
        }
        if (f[i].cat < SAVELARG_TIPADO || f[i].cat > SAVELARG_DISCO) {
            erro("%s: faixa em 0x%lx com categoria %lu inválida", arq, f[i].desl, (unsigned long)f[i].cat);
            return 1;
        }
        if (f[i].larg != 1 && (f[i].cat == SAVELARG_SEM_ACESSO || f[i].cat == SAVELARG_DISCO)) {
            erro("%s: faixa em 0x%lx sem acesso / de disco com largura %lu", arq, f[i].desl, (unsigned long)f[i].larg);
            return 1;
        }
        if (f[i].desl < pos) {
            erro("%s: sobreposição em 0x%lx (a faixa anterior vai até 0x%lx)", arq, f[i].desl, pos);
            return 1;
        }
        if (f[i].desl > pos) {
            erro("%s: buraco de 0x%lx a 0x%lx", arq, pos, f[i].desl);
            return 1;
        }
        fim = f[i].desl + f[i].qtd * (unsigned long)f[i].larg;
        pos = fim;
    }
    if (pos != SaveLarg_Tamanho(arq)) {
        erro("%s: a cobertura acaba em 0x%lx, o arquivo tem 0x%lx", arq, pos, SaveLarg_Tamanho(arq));
        return 1;
    }
    return 0;
}

static unsigned long sem;

static unsigned char aleat(void) {
    sem = sem * 1103515245UL + 12345UL;
    return (unsigned char)((sem >> 16) & 0xFF);
}

static int confere_troca(int arq) {
    unsigned long tam = SaveLarg_Tamanho(arq), i, p;
    int n, k, j;
    const SaveFaixa *f = SaveLarg_Faixas(arq, &n);
    unsigned char *orig = (unsigned char *)malloc(tam), *b = (unsigned char *)malloc(tam);
    unsigned long q;
    int ret = 0;

    if (!orig || !b) {
        erro("%s: sem memória (%lu %lu)", arq, 0, 0);
        return 1;
    }
    sem = 12345UL + (unsigned long)arq;
    for (i = 0; i < tam; i++) {
        orig[i] = aleat();
    }
    memcpy(b, orig, tam);
    if (SaveLarg_Inverte(b, tam, arq) != 0) {
        erro("%s: SaveLarg_Inverte recusou o tamanho certo (%lu %lu)", arq, 0, 0);
        ret = 1;
    }
    /* uma vez: cada número invertido na largura do mapa (conferido número a número, byte a byte) */
    for (j = 0; j < n && !ret; j++) {
        for (q = 0; q < f[j].qtd && !ret; q++) {
            p = f[j].desl + q * (unsigned long)f[j].larg;
            for (k = 0; k < f[j].larg; k++) {
                if (b[p + (unsigned long)k] != orig[p + (unsigned long)(f[j].larg - 1 - k)]) {
                    erro("%s: número em 0x%lx não ficou invertido (%lu)", arq, p, (unsigned long)f[j].larg);
                    ret = 1;
                    break;
                }
            }
        }
    }
    /* duas vezes: o original */
    if (!ret) {
        SaveLarg_Inverte(b, tam, arq);
        if (memcmp(b, orig, tam) != 0) {
            erro("%s: trocar duas vezes não devolveu o original (%lu %lu)", arq, 0, 0);
            ret = 1;
        }
    }
    /* tamanho errado: recusado, buffer intacto */
    if (!ret) {
        memcpy(b, orig, tam);
        if (SaveLarg_Inverte(b, tam - 1, arq) != -1 || memcmp(b, orig, tam) != 0) {
            erro("%s: tamanho errado não foi recusado (%lu %lu)", arq, 0, 0);
            ret = 1;
        }
    }
    free(orig);
    free(b);
    return ret;
}

int main(int argc, char **argv) {
    int arq, n, i;
    const SaveFaixa *f;

    if (argc != 2) {
        printf("uso: save_larguras_tool imprime|confere\n");
        return 2;
    }
    if (strcmp(argv[1], "imprime") == 0) {
        for (arq = 0; arq < SAVELARG_ARQ_NUM; arq++) {
            f = SaveLarg_Faixas(arq, &n);
            for (i = 0; i < n; i++) {
                printf("%s 0x%lx %lu %d %d %s\n", SaveLarg_NomeArquivo(arq), f[i].desl, f[i].qtd, f[i].larg, f[i].cat,
                       f[i].nome);
            }
        }
        return 0;
    }
    if (strcmp(argv[1], "confere") == 0) {
        if (SaveLarg_Inverte(0, 5, 99) != -1 || SaveLarg_NomeArquivo(99) != 0 || SaveLarg_Faixas(-1, &n) != 0) {
            printf("ERRO: arquivo inválido aceito\n");
            return 1;
        }
        for (arq = 0; arq < SAVELARG_ARQ_NUM; arq++) {
            if (confere_cobertura(arq) || confere_troca(arq)) {
                return 1;
            }
            f = SaveLarg_Faixas(arq, &n);
            printf("ok %s: %d faixas, 0x%lx bytes\n", SaveLarg_NomeArquivo(arq), n, SaveLarg_Tamanho(arq));
        }
        return erros ? 1 : 0;
    }
    printf("modo desconhecido: %s\n", argv[1]);
    return 2;
}
