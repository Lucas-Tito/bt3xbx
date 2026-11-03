/*
 * Ferramenta de teste do Linux sobre port/x360/conv/save_importa.c: o SaveFs com a libc e uma falha injetável.
 * Não entra no console. Compilada com gcc -std=gnu99 por teste_save_importa.py.
 *
 *   teste_save_importa_fs procurar RAIZ
 *   teste_save_importa_fs importar RAIZ INDICE
 *   teste_save_importa_fs pular RAIZ
 *   teste_save_importa_fs exportar RAIZ
 *   teste_save_importa_fs gravado RAIZ CAMINHO       (o gancho do jogo, SaveImp_AposGravar)
 * RAIZ tem import, backup, export, saves/card1/BASLUS-21678DBZT3 (as que existirem). A saída é "chave=valor" por linha.
 * Falha injetada: FALHA=OP:N[:*[:TRECHO]]: a N-ésima chamada de OP (ler, gravar, mover, apagar, criar_pasta, listar)
 * falha com ENOSPC; com "*" todas dali em diante; com TRECHO só as que tocam um caminho que contém o TRECHO.
 */
#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "save_importa.h"

static char fOp[32], fTrecho[256];
static int fN, fSempre, fAtivo;
static int cont[8];
static const char *const OPS[] = {"ler", "gravar", "mover", "apagar", "criar_pasta", "listar", "tipo", "x"};

static int injeta(int op, const char *cam) {
    if (!fAtivo || strcmp(fOp, OPS[op]) != 0) return 0;
    if (fTrecho[0] && !strstr(cam, fTrecho)) return 0;
    cont[op]++;
    return fSempre ? cont[op] >= fN : cont[op] == fN;
}

static int t_ler(void *c, const char *cam, unsigned char *buf, unsigned long cap, unsigned long *lidos) {
    FILE *f;
    size_t n;
    (void)c;
    if (injeta(0, cam)) return ENOSPC;
    f = fopen(cam, "rb");
    if (!f) return errno ? errno : EIO;
    n = fread(buf, 1, cap, f);
    if (n == cap) {
        unsigned char x;
        if (fread(&x, 1, 1, f) == 1) {
            fclose(f);
            return SI_FS_GRANDE;
        }
    }
    fclose(f);
    *lidos = (unsigned long)n;
    return 0;
}
static int t_gravar(void *c, const char *cam, const unsigned char *buf, unsigned long n) {
    FILE *f;
    (void)c;
    if (injeta(1, cam)) return ENOSPC;
    f = fopen(cam, "wb");
    if (!f) return errno ? errno : EIO;
    if (fwrite(buf, 1, n, f) != n) { fclose(f); return EIO; }
    return fclose(f) ? EIO : 0;
}
static int t_mover(void *c, const char *de, const char *para) {
    struct stat st;
    (void)c;
    if (injeta(2, de) || injeta(2, para)) return ENOSPC;
    if (stat(para, &st) == 0) return EEXIST; /* como o MoveFile do 360 */
    return rename(de, para) ? errno : 0;
}
static int t_apagar(void *c, const char *cam) {
    (void)c;
    if (injeta(3, cam)) return ENOSPC;
    return remove(cam) ? errno : 0;
}
static int t_tipo(void *c, const char *cam) {
    struct stat st;
    (void)c;
    if (stat(cam, &st) != 0) return 0;
    return S_ISDIR(st.st_mode) ? 2 : 1;
}
static int t_pasta(void *c, const char *cam) {
    (void)c;
    if (injeta(4, cam)) return ENOSPC;
    if (mkdir(cam, 0755) == 0 || errno == EEXIST) return 0;
    return errno;
}
static int t_listar(void *c, const char *pasta, char nomes[][SI_NOME], int max, int *n) {
    DIR *d;
    struct dirent *e;
    (void)c;
    if (injeta(5, pasta)) return ENOSPC;
    d = opendir(pasta);
    if (!d) return errno;
    *n = 0;
    while ((e = readdir(d)) != NULL && *n < max) {
        char cam[1024];
        struct stat st;
        if (strlen(e->d_name) >= SI_NOME) continue;
        snprintf(cam, sizeof(cam), "%s/%s", pasta, e->d_name);
        if (stat(cam, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        strcpy(nomes[(*n)++], e->d_name);
    }
    closedir(d);
    return 0;
}
static void t_log(void *c, const char *linha) {
    (void)c;
    fprintf(stderr, "log: %s\n", linha);
}

int main(int argc, char **argv) {
    SaveFs fs;
    SiPastas p;
    static SiLista lista;
    SiFalha falha;
    char imp[600], bak[600], sav[600], exp[600];
    const char *v = getenv("FALHA");
    int r, i;
    if (argc < 3) return 2;
    if (v) {
        char *s = strdup(v), *op = strtok(s, ":"), *n = strtok(NULL, ":"), *st = strtok(NULL, ":"), *tr = strtok(NULL, "");
        if (!op || !n) return 2;
        strcpy(fOp, op);
        fN = atoi(n);
        fSempre = st && st[0] == '*';
        if (tr) strcpy(fTrecho, tr);
        fAtivo = 1;
    }
    memset(&fs, 0, sizeof(fs));
    fs.sep = '/';
    fs.ler = t_ler; fs.gravar = t_gravar; fs.mover = t_mover; fs.apagar = t_apagar; fs.tipo = t_tipo;
    fs.criar_pasta = t_pasta; fs.listar = t_listar; fs.log = t_log;
    snprintf(imp, sizeof(imp), "%s/import", argv[2]);
    snprintf(bak, sizeof(bak), "%s/backup", argv[2]);
    snprintf(sav, sizeof(sav), "%s/saves/card1/%s", argv[2], SI_PASTA_SAVE);
    snprintf(exp, sizeof(exp), "%s/export", argv[2]);
    p.import = imp; p.backup = bak; p.save = sav; p.exportar = exp;
    if (!strcmp(argv[1], "procurar") || !strcmp(argv[1], "importar") || !strcmp(argv[1], "pular")) {
        int n = SaveImp_Procurar(&fs, &p, &lista);
        if (!strcmp(argv[1], "procurar")) {
            printf("n=%d\nha_save=%d\n", n, lista.ha_save);
            for (i = 0; i < n; i++) printf("item=%s|%d|%s\n", lista.item[i].nome, lista.item[i].erro,
                                           lista.item[i].erro ? SaveImp_TextoErro(lista.item[i].erro) : "OK");
            return 0;
        }
        if (!strcmp(argv[1], "pular")) {
            r = SaveImp_Pular(&fs, &p, &lista, &falha);
        } else {
            if (argc < 4) return 2;
            r = SaveImp_Importar(&fs, &p, &lista, atoi(argv[3]), &falha);
        }
        printf("rc=%d\npasso=%d\nsistema=%d\ncodigo=%d\nrestaurado=%d\nimportado=%d\nmal=%d\nbackup=%d\n", r, falha.passo,
               falha.sistema, falha.codigo, falha.restaurado, falha.importado, falha.movidos_mal, falha.backup);
        return 0;
    }
    if (!strcmp(argv[1], "exportar")) {
        r = SaveImp_Exportar(&fs, &p, &falha);
        printf("rc=%d\npasso=%d\nsistema=%d\n", r, falha.passo, falha.sistema);
        return 0;
    }
    if (!strcmp(argv[1], "gravado") && argc >= 4) {
        r = SaveImp_AposGravar(&fs, &p, argv[3]);
        printf("rc=%d\n", r);
        return 0;
    }
    return 2;
}
