/* Relatório independente do diário. O filtro só copia memória; a auxiliar grava na thread 1. */
#ifndef QUEDA360_H
#define QUEDA360_H
#include "f2/pilha360.h"

static const wchar_t kQuedaTitulo[] = L"Unexpected error";
static const wchar_t kQuedaTexto[] = L"The game ran into an unexpected error and needs to close. A report was saved to crash.txt in the game folder. Press OK to return to the dashboard.";
static const wchar_t *kQuedaBotoes[] = {L"OK"};
struct QuedaDados {
    unsigned codigo, iar, lr, acesso, endereco, thread, vb, quadro;
    unsigned long long r[32];
    char fase[160];
    unsigned pilha[12], nPilha;
    int contexto;
};
static QuedaDados sQueda;
static volatile LONG sQuedaTomada;
static volatile LONG sRelatorioSalvo;
static HANDLE sRelatorioThread;
extern "C" const char *Port_Carimbo360(void);
extern "C" const char *Port_Versao360(void);

/* Contagens e ponteiros vêm do contexto copiado, nunca da pilha da auxiliar. */
static void Queda_CapturarPilha(Pilha360_Ler ler) {
    unsigned inicio = (unsigned)sQueda.r[1], atual = inicio, anterior, retorno;
    for (unsigned i = 0; i < 12; i++) {
        if (!Pilha360_Proximo(inicio, atual, ler, &anterior, &retorno)) break;
        sQueda.pilha[sQueda.nPilha++] = retorno;
        atual = anterior;
    }
}

static int Queda_Gravar(void) {
    char texto[4096];
    int n = _snprintf(texto, sizeof(texto),
        "BT3 crash report\r\nVersion: %s\r\nBuild: %s\r\nPhase: %s\r\n"
        "Exception: %08X  IAR: %08X  LR: %08X\r\n"
        "Access type: %u (0=read, 1=write, 8=execute)  Address: %08X\r\n"
        "Thread: %u  Vblank: %u  Frame: %u\r\nContext available: %s\r\nRegisters (64-bit):\r\n",
        Port_Versao360(), Port_Carimbo360(), sQueda.fase, sQueda.codigo, sQueda.iar, sQueda.lr,
        sQueda.acesso, sQueda.endereco, sQueda.thread, sQueda.vb, sQueda.quadro,
        sQueda.contexto ? "yes" : "no");
    if (n < 0 || n >= (int)sizeof(texto)) return 0;
    for (unsigned i = 0; i < 32; i++) {
        int k = _snprintf(texto + n, sizeof(texto) - n, "r%-2u=%08X%08X%s", i,
            (unsigned)(sQueda.r[i] >> 32), (unsigned)sQueda.r[i], (i % 2) ? "\r\n" : "  ");
        if (k < 0 || k >= (int)sizeof(texto) - n) return 0;
        n += k;
    }
    int k = _snprintf(texto + n, sizeof(texto) - n,
        "Call chain (IAR, LR, then saved return candidates; leaf/tail calls may be missing):\r\n%08X\r\n%08X\r\n",
        sQueda.iar, sQueda.lr);
    if (k < 0 || k >= (int)sizeof(texto) - n) return 0;
    n += k;
    for (unsigned i = 0; i < sQueda.nPilha; i++) {
        k = _snprintf(texto + n, sizeof(texto) - n, "%08X\r\n", sQueda.pilha[i]);
        if (k < 0 || k >= (int)sizeof(texto) - n) return 0;
        n += k;
    }
    HANDLE f = CreateFile("game:\\crash.txt", GENERIC_WRITE, FILE_SHARE_READ, NULL,
                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    DWORD escritos = 0;
    int ok = WriteFile(f, texto, (DWORD)n, &escritos, NULL) && escritos == (DWORD)n;
    if (!FlushFileBuffers(f)) ok = 0;
    if (!CloseHandle(f)) ok = 0;
    return ok;
}

static DWORD WINAPI Queda_Relatar(LPVOID principal) {
    XSetThreadProcessor(GetCurrentThread(), 1);
    InterlockedExchange(&sRelatorioSalvo, Queda_Gravar());
    Diario_Escrever("CRASH: relatório %s, código=%08X Iar=%08X Lr=%08X thread=%u",
                    sRelatorioSalvo ? "gravado em crash.txt" : "não pôde ser gravado", sQueda.codigo,
                    sQueda.iar, sQueda.lr, sQueda.thread);
    Diario_Descarregar();
    if (principal != NULL) XLaunchNewImage(NULL, 0);
    return 0;
}

/* A main é dona do D3D depois das esperas dos produtores. Buffers persistem até o fim da operação. */
static bool Queda_Mostrar(void (*apresentar)(void)) {
    static MESSAGEBOX_RESULT resultado;
    static XOVERLAPPED ov;
    DWORD r, fim = 0, inicio = GetTickCount();
    if (!sRelatorioSalvo) return false;
    for (;;) {
        apresentar();
        ZeroMemory(&ov, sizeof(ov));
        r = XShowMessageBoxUI(XUSER_INDEX_ANY, kQuedaTitulo, kQuedaTexto, 1,
                             kQuedaBotoes, 0, XMB_ERRORICON, &resultado, &ov);
        if (r != ERROR_ACCESS_DENIED || GetTickCount() - inicio >= 10000) break;
    }
    if (r != ERROR_IO_PENDING && r != ERROR_SUCCESS) return false;
    while (!XHasOverlappedIoCompleted(&ov)) apresentar();
    r = XGetOverlappedResult(&ov, &fim, FALSE);
    return r == ERROR_SUCCESS;
}
#endif
