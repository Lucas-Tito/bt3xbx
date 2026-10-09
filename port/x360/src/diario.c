#include <xtl.h>
#include <stdio.h>
#include <stdarg.h>
#include "diario.h"

static HANDLE sArquivo = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION sTrava;
static int sTravaPronta = 0;

void Diario_Abrir(const char *caminho) {
    if (!sTravaPronta) {
        InitializeCriticalSection(&sTrava);
        sTravaPronta = 1;
    }
    sArquivo = CreateFile(caminho, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

void Diario_Escrever(const char *formato, ...) {
    char linha[1024];
    va_list args;
    int n;
    DWORD escritos = 0;

    if (sArquivo == INVALID_HANDLE_VALUE || !sTravaPronta) {
        return;
    }
    va_start(args, formato);
    n = _vsnprintf(linha, sizeof(linha) - 2, formato, args);
    va_end(args);
    /* Truncando, o _vsnprintf devolve -1 e NÃO termina a string. */
    if (n < 0 || n > (int)sizeof(linha) - 2) {
        n = (int)sizeof(linha) - 2;
    }
    linha[n++] = '\n';

    EnterCriticalSection(&sTrava);
    WriteFile(sArquivo, linha, (DWORD)n, &escritos, NULL);
    FlushFileBuffers(sArquivo);
    LeaveCriticalSection(&sTrava);
}

void Diario_Fechar(void) {
    if (sArquivo == INVALID_HANDLE_VALUE) {
        return;
    }
    if (sTravaPronta) {
        EnterCriticalSection(&sTrava);
    }
    FlushFileBuffers(sArquivo);
    CloseHandle(sArquivo);
    sArquivo = INVALID_HANDLE_VALUE;
    if (sTravaPronta) {
        LeaveCriticalSection(&sTrava);
    }
}
