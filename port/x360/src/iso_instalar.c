/* Adaptador FATX do núcleo C89. Nenhuma E/S na thread que apresenta ou nas 4 e 5. */
#include <xtl.h>
#include <string.h>
#include "iso_instalar.h"
#include "iso360.h"
#include "iso_conv.h"
#include "iso_selo.h"
#include "diario.h"
typedef struct Instalar {
    HANDLE origem, tmp;
} Instalar;
static volatile LONG sFase, sPorcento;
static int tamanho(void *ctx, int qual, conv_u32 *n) {
    Instalar *c = (Instalar *)ctx;
    LARGE_INTEGER t;
    if (!GetFileSizeEx(qual ? c->tmp : c->origem, &t) || t.QuadPart < 0 || t.QuadPart > ((__int64)0xffffffffU))
        return 0;
    *n = (conv_u32)t.QuadPart;
    return 1;
}
static int ler(void *ctx, int qual, conv_u32 pos, void *buf, unsigned n) {
    Instalar *c = (Instalar *)ctx;
    LARGE_INTEGER p;
    DWORD lidos = 0;
    HANDLE h = qual ? c->tmp : c->origem;
    p.QuadPart = pos;
    return SetFilePointerEx(h, p, NULL, FILE_BEGIN) && ReadFile(h, buf, n, &lidos, NULL) && lidos == n;
}
static int gravar(void *ctx, conv_u32 pos, const void *buf, unsigned n) {
    Instalar *c = (Instalar *)ctx;
    LARGE_INTEGER p;
    DWORD escritos = 0;
    p.QuadPart = pos;
    return SetFilePointerEx(c->tmp, p, NULL, FILE_BEGIN) && WriteFile(c->tmp, buf, n, &escritos, NULL) &&
           escritos == n;
}
static int espaco(void *ctx) {
    ULARGE_INTEGER livre, total, disponivel;
    (void)ctx;
    return GetDiskFreeSpaceEx("game:\\", &livre, &total, &disponivel) &&
           livre.QuadPart >= ((unsigned __int64)3221225472U);
}
static int iniciar(void *ctx) {
    Instalar *c = (Instalar *)ctx;
    if (GetFileAttributes("game:\\" ISO_NOME) != (DWORD)-1)
        return 0;
    c->tmp = CreateFile("game:\\" IC_TEMP, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW,
                        FILE_ATTRIBUTE_NORMAL, NULL);
    return c->tmp != INVALID_HANDLE_VALUE;
}
static int reabrir(void *ctx) {
    Instalar *c = (Instalar *)ctx;
    BOOL ok = FlushFileBuffers(c->tmp), fechou = CloseHandle(c->tmp);
    c->tmp = INVALID_HANDLE_VALUE;
    if (!ok || !fechou)
        return 0;
    c->tmp = CreateFile("game:\\" IC_TEMP, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL, NULL);
    return c->tmp != INVALID_HANDLE_VALUE;
}
static int publicar(void *ctx) {
    Instalar *c = (Instalar *)ctx;
    BOOL ok = CloseHandle(c->tmp);
    c->tmp = INVALID_HANDLE_VALUE;
    /* MoveFile, sem MOVEFILE_REPLACE_EXISTING: recusa também um destino que apareceu depois. */
    return ok && MoveFile("game:\\" IC_TEMP, "game:\\" ISO_NOME);
}
static void descartar(void *ctx) {
    Instalar *c = (Instalar *)ctx;
    if (c->tmp != INVALID_HANDLE_VALUE) {
        CloseHandle(c->tmp);
        c->tmp = INVALID_HANDLE_VALUE;
    }
    if (!DeleteFile("game:\\" IC_TEMP))
        Diario_Escrever("conversão: temporário não removido, erro %u", (unsigned)GetLastError());
}
static void progresso(void *ctx, int fase, conv_u32 p, conv_u32 n) {
    static int ultima = -1;
    static DWORD inicio;
    unsigned porcento = (unsigned)((double)p * 100.0 / (n ? n : 1));
    (void)ctx;
    if (fase != ultima) {
        DWORD agora = GetTickCount();
        if (ultima >= 0)
            Diario_Escrever("conversão: fase %d em %u ms", ultima, (unsigned)(agora - inicio));
        ultima = fase;
        inicio = agora;
    }
    InterlockedExchange(&sFase, fase);
    InterlockedExchange(&sPorcento, (LONG)porcento);
}
void IsoInstalar_Progresso(int *fase, unsigned *porcento) {
    *fase = (int)InterlockedCompareExchange(&sFase, 0, 0);
    *porcento = (unsigned)InterlockedCompareExchange(&sPorcento, 0, 0);
}
int IsoInstalar_Limpar(void) {
    DWORD erro;
    if (DeleteFile("game:\\" IC_TEMP))
        return 1;
    erro = GetLastError();
    return erro == ERROR_FILE_NOT_FOUND || erro == ERROR_PATH_NOT_FOUND;
}
int IsoInstalar_Executar(const char *origem) {
    Instalar c;
    IsoConvIO io;
    int r;
    char erro[240];
    DWORD inicio = GetTickCount();
    c.tmp = INVALID_HANDLE_VALUE;
    c.origem =
        CreateFile(origem, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (c.origem == INVALID_HANDLE_VALUE)
        return IC_ORIGEM;
    memset(&io, 0, sizeof(io));
    io.ctx = &c;
    io.tamanho = tamanho;
    io.ler = ler;
    io.gravar = gravar;
    io.espaco = espaco;
    io.iniciar = iniciar;
    io.reabrir = reabrir;
    io.publicar = publicar;
    io.descartar = descartar;
    io.progresso = progresso;
    progresso(&c, IC_IDENTIFICAR, 0, 1);
    r = IsoConv_Executar(&io, sIsoSelo, erro, sizeof(erro));
    CloseHandle(c.origem);
    Diario_Escrever("conversão: resultado %d, %s, total %u ms", r, erro, (unsigned)(GetTickCount() - inicio));
    return r;
}
