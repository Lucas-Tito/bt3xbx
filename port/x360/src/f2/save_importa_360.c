/* A importação e a exportação de saves no 360: o SaveFs do Win32 (CreateFile, MoveFile...) sobre conv/save_importa.c.
   Só funções que o XDK tem: uma que faltasse viraria stub e derrubaria o jogo (conferir build/x360/f2/stubs.txt). */
#include <xtl.h>
#include <stdio.h>
#include <string.h>
#include "save_importa_360.h"
#include "diario.h"

/* As pastas da importação ficam dentro da pasta de saves (pedido do mantenedor, 3/11/2026), para a pasta do jogo continuar
   só com os arquivos do jogo, a ISO e saves: <saves>\import (com done, skipped e failed), <saves>\export e
   <saves>\backup\NNN, ao lado de <saves>\card1, onde o jogo grava. */

static int codigo(void) {
    DWORD e = GetLastError();
    return e ? (int)e : 1;
}

static int fs_ler(void *ctx, const char *cam, unsigned char *buf, unsigned long cap, unsigned long *lidos) {
    HANDLE h;
    LARGE_INTEGER t;
    DWORD n = 0;
    int r = 0;
    (void)ctx;
    h = CreateFile(cam, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return codigo();
    if (!GetFileSizeEx(h, &t)) {
        r = codigo();
    } else if (t.QuadPart > (__int64)cap) {
        r = SI_FS_GRANDE;
    } else if (t.QuadPart > 0 && (!ReadFile(h, buf, (DWORD)t.QuadPart, &n, NULL) || n != (DWORD)t.QuadPart)) {
        r = codigo();
    } else {
        *lidos = (unsigned long)t.QuadPart;
    }
    CloseHandle(h);
    return r;
}

static int fs_gravar(void *ctx, const char *cam, const unsigned char *buf, unsigned long n) {
    HANDLE h;
    DWORD gravados = 0;
    int r = 0;
    (void)ctx;
    h = CreateFile(cam, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return codigo();
    if (n > 0 && (!WriteFile(h, buf, (DWORD)n, &gravados, NULL) || gravados != (DWORD)n)) r = codigo();
    if (r == 0 && !FlushFileBuffers(h)) r = codigo();
    CloseHandle(h);
    return r;
}

static int fs_mover(void *ctx, const char *de, const char *para) {
    (void)ctx;
    return MoveFile(de, para) ? 0 : codigo(); /* não substitui um arquivo que já existe */
}

static int fs_apagar(void *ctx, const char *cam) {
    (void)ctx;
    return DeleteFile(cam) ? 0 : codigo();
}

static int fs_tipo(void *ctx, const char *cam) {
    DWORD a;
    (void)ctx;
    a = GetFileAttributes(cam);
    if (a == (DWORD)-1) return 0;
    return (a & FILE_ATTRIBUTE_DIRECTORY) ? 2 : 1;
}

static int fs_pasta(void *ctx, const char *cam) {
    (void)ctx;
    if (CreateDirectory(cam, NULL)) return 0;
    return GetLastError() == ERROR_ALREADY_EXISTS ? 0 : codigo();
}

static int fs_listar(void *ctx, const char *pasta, char nomes[][SI_NOME], int max, int *n) {
    char padrao[SI_CAMINHO];
    WIN32_FIND_DATA d;
    HANDLE h;
    (void)ctx;
    *n = 0;
    if (strlen(pasta) + 3 > sizeof(padrao)) return 1;
    sprintf(padrao, "%s\\*", pasta);
    h = FindFirstFile(padrao, &d);
    if (h == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND ? 0 : codigo();
    do {
        if ((d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || strlen(d.cFileName) >= SI_NOME) continue;
        if (*n < max) strcpy(nomes[(*n)++], d.cFileName);
    } while (FindNextFile(h, &d));
    FindClose(h);
    return 0;
}

static void fs_log(void *ctx, const char *linha) {
    (void)ctx;
    Diario_Escrever("%s", linha);
}

static SaveFs sFs = {NULL, '\\', fs_ler, fs_gravar, fs_mover, fs_apagar, fs_tipo, fs_pasta, fs_listar, fs_log};

/* A pasta do save: BT3_SAVES (padrão game:\saves) + card1 + BASLUS-21678DBZT3, como o plat_mc.c a monta; import, backup
   e export dentro da mesma raiz. */
static char sSave[SI_CAMINHO], sImport[SI_CAMINHO], sBackup[SI_CAMINHO], sExport[SI_CAMINHO];
static SiPastas sPastas = {sImport, sBackup, sSave, sExport};

static void monta_pastas(void) {
    const char *raiz;
    size_t i, k = 0;
#pragma warning(suppress : 4996) /* (o getenv é o de posix360.c, que lê o environment.txt) */
    raiz = getenv("BT3_SAVES");
    if (raiz == NULL || raiz[0] == '\0') raiz = "saves";
    if (strchr(raiz, ':') == NULL) {
        strcpy(sSave, "game:\\");
        k = strlen(sSave);
    }
    for (i = 0; raiz[i] != '\0' && k + 1 < 400; i++) sSave[k++] = raiz[i] == '/' ? '\\' : raiz[i];
    while (k > 0 && sSave[k - 1] == '\\') k--;
    sSave[k] = '\0';
    strcpy(sImport, sSave);
    strcat(sImport, "\\import");
    strcpy(sBackup, sSave);
    strcat(sBackup, "\\backup");
    strcpy(sExport, sSave);
    strcat(sExport, "\\export");
    strcat(sSave, "\\card1\\" SI_PASTA_SAVE);
}

int Importa360_Procurar(SiLista *lista) {
    monta_pastas();
    return SaveImp_Procurar(&sFs, &sPastas, lista);
}

int Importa360_Importar(const SiLista *lista, int indice, SiFalha *falha) {
    monta_pastas();
    return SaveImp_Importar(&sFs, &sPastas, lista, indice, falha);
}

int Importa360_Pular(const SiLista *lista, SiFalha *falha) {
    monta_pastas();
    return SaveImp_Pular(&sFs, &sPastas, lista, falha);
}

void Port_X360SaveGravado(const char *caminho) {
    monta_pastas();
    SaveImp_AposGravar(&sFs, &sPastas, caminho);
}
