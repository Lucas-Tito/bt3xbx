/*
 * Ferramenta de PC sobre a conversão do save (port/x360/conv/save_conv.c), para os testes e os roteiros do WSL; não
 * entra no console. Compilada com gcc -std=c89 por save_conv.py e teste_save_conv.py.
 *
 *   save_conv_tool psu-para-360 ARQ.psu PASTA [--data AAAA-MM-DDTHH:MM:SS]
 *       lê o .psu do PS2 e grava os três arquivos na ordem do 360 em PASTA (que já existe)
 *   save_conv_tool 360-para-psu PASTA ARQ.psu [--data AAAA-MM-DDTHH:MM:SS]
 *       o caminho de volta; a data (a mesma em criação e modificação de todas as entradas) é 2000-01-01T00:00:00 sem --data
 *   save_conv_tool confere ARQ.psu|PASTA
 *       diz a ordem (PS2 ou 360) e se a soma confere: "ps2 ok", "360 ok" ou "danificado"; sai com 1 se danificado
 * Erros em inglês, na saída de erro, com saída 1.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "save_conv.h"

static unsigned char psu_buf[3UL * 512 + 3UL * 512 + 16384UL + 40090UL + 964UL + 3UL * 1024 + 4096];
static SaveConvTres tres, tmp;
static PsuArquivo vet[16];
static PsuSave save;

static int falha(const char *msg, const char *extra) {
    fprintf(stderr, "error: %s%s%s\n", msg, extra ? ": " : "", extra ? extra : "");
    return 1;
}

static long le_arquivo(const char *caminho, unsigned char *buf, unsigned long cap) {
    FILE *f = fopen(caminho, "rb");
    long n;
    if (!f) return -1;
    n = (long)fread(buf, 1, cap, f);
    if (n == (long)cap) { /* cheio: tem mais? */
        unsigned char c;
        if (fread(&c, 1, 1, f) == 1) n = -2;
    }
    fclose(f);
    return n;
}

static int grava_arquivo(const char *caminho, const unsigned char *buf, unsigned long n) {
    FILE *f = fopen(caminho, "wb");
    if (!f) return -1;
    if (fwrite(buf, 1, n, f) != n) { fclose(f); return -1; }
    return fclose(f);
}

static int caminho(char *dest, size_t cap, const char *pasta, const char *nome) {
    if (strlen(pasta) + strlen(nome) + 2 > cap) return -1;
    sprintf(dest, "%s/%s", pasta, nome);
    return 0;
}

static int le_tres(const char *pasta, SaveConvTres *t) {
    char c[1024];
    long n;
    if (caminho(c, sizeof c, pasta, "icon.sys")) return falha("path too long", pasta);
    n = le_arquivo(c, t->iconsys, sizeof t->iconsys);
    if (n < 0) return falha("cannot read", c);
    if ((unsigned long)n != sizeof t->iconsys) return falha("save file has the wrong size", c);
    if (caminho(c, sizeof c, pasta, "dbzsm.ico")) return falha("path too long", pasta);
    n = le_arquivo(c, t->ico, sizeof t->ico);
    if (n < 0) return falha("cannot read", c);
    if ((unsigned long)n != sizeof t->ico) return falha("save file has the wrong size", c);
    if (caminho(c, sizeof c, pasta, "BASLUS-21678DBZT3")) return falha("path too long", pasta);
    n = le_arquivo(c, t->dados, sizeof t->dados);
    if (n < 0) return falha("cannot read", c);
    if ((unsigned long)n != sizeof t->dados) return falha("save file has the wrong size", c);
    return 0;
}

static int le_psu(const char *arq) {
    long n = le_arquivo(arq, psu_buf, sizeof psu_buf);
    int r;
    if (n == -1) return falha("cannot read", arq);
    if (n == -2) return falha("file too big for a BT3 save", arq);
    r = Psu_Ler(psu_buf, (unsigned long)n, &save, vet, 16);
    if (r) return falha(SaveConv_TextoErro(r), 0);
    return 0;
}

static int le_data(int argc, char **argv, PsuData *d) {
    unsigned a, me, di, h, mi, s;
    d->seg = 0; d->min = 0; d->hora = 0; d->dia = 1; d->mes = 1; d->ano = 2000;
    if (argc == 0) return 0;
    if (argc != 2 || strcmp(argv[0], "--data") != 0) return -1;
    if (sscanf(argv[1], "%4u-%2u-%2uT%2u:%2u:%2u", &a, &me, &di, &h, &mi, &s) != 6) return -1;
    d->ano = (unsigned short)a; d->mes = (unsigned char)me; d->dia = (unsigned char)di;
    d->hora = (unsigned char)h; d->min = (unsigned char)mi; d->seg = (unsigned char)s;
    return 0;
}

static int psu_para_360(const char *arq, const char *pasta) {
    static const char *const nomes[3] = {"icon.sys", "dbzsm.ico", "BASLUS-21678DBZT3"};
    const unsigned char *dados[3];
    unsigned long tams[3];
    char c[1024];
    int i, r;
    if (le_psu(arq)) return 1;
    r = SaveConv_Ps2Para360(&save, &tres);
    if (r) return falha(SaveConv_TextoErro(r), 0);
    dados[0] = tres.iconsys; tams[0] = sizeof tres.iconsys;
    dados[1] = tres.ico; tams[1] = sizeof tres.ico;
    dados[2] = tres.dados; tams[2] = sizeof tres.dados;
    for (i = 0; i < 3; i++) {
        if (caminho(c, sizeof c, pasta, nomes[i])) return falha("path too long", pasta);
        if (grava_arquivo(c, dados[i], tams[i])) return falha("cannot write", c);
    }
    return 0;
}

static int p360_para_psu(const char *pasta, const char *arq, const PsuData *d) {
    unsigned long n;
    int r;
    if (le_tres(pasta, &tres)) return 1;
    r = SaveConv_360ParaPs2(&tres, &tmp, vet, &save, d, d);
    if (r) return falha(SaveConv_TextoErro(r), 0);
    r = Psu_Escrever(&save, psu_buf, sizeof psu_buf, &n);
    if (r) return falha(SaveConv_TextoErro(r), 0);
    if (grava_arquivo(arq, psu_buf, n)) return falha("cannot write", arq);
    return 0;
}

static int confere(const char *alvo) {
    const unsigned char *dados = 0;
    char c[1024];
    int r;
    /* pasta se existe ALVO/BASLUS-21678DBZT3; senão, .psu */
    if (caminho(c, sizeof c, alvo, "BASLUS-21678DBZT3") == 0 && le_arquivo(c, tmp.dados, sizeof tmp.dados) >= 0) {
        if (le_tres(alvo, &tres)) return 1;
        dados = tres.dados;
    } else {
        unsigned i;
        if (le_psu(alvo)) return 1;
        r = Psu_ConferirBT3(&save);
        if (r) return falha(SaveConv_TextoErro(r), 0);
        for (i = 0; i < save.nArquivos; i++)
            if (strcmp(save.arquivos[i].nome, "BASLUS-21678DBZT3") == 0) dados = save.arquivos[i].dados;
    }
    if (SaveConv_ConferirSoma(dados, SAVECONV_PS2)) { printf("ps2 ok\n"); return 0; }
    if (SaveConv_ConferirSoma(dados, SAVECONV_360)) { printf("360 ok\n"); return 0; }
    printf("danificado\n");
    return 1;
}

int main(int argc, char **argv) {
    PsuData d;
    if (argc >= 4 && strcmp(argv[1], "psu-para-360") == 0) {
        return psu_para_360(argv[2], argv[3]);
    }
    if (argc >= 4 && strcmp(argv[1], "360-para-psu") == 0) {
        if (le_data(argc - 4, argv + 4, &d)) return falha("bad date", 0);
        return p360_para_psu(argv[2], argv[3], &d);
    }
    if (argc == 3 && strcmp(argv[1], "confere") == 0) return confere(argv[2]);
    fprintf(stderr, "usage: save_conv_tool psu-para-360 ARQ.psu PASTA | 360-para-psu PASTA ARQ.psu [--data D] | confere ARQ.psu|PASTA\n");
    return 2;
}
