/* Simula a fila de apresentação e a UI assíncrona para testar o fluxo real. */
#ifndef TESTE_ISO_UI_XTL_H
#define TESTE_ISO_UI_XTL_H
#include <cstddef>
#include <cstring>
#include <cwchar>
#define WINAPI
#define FALSE 0
typedef unsigned DWORD;
typedef int LONG;
typedef void *LPVOID;
typedef DWORD (*LPTHREAD_START_ROUTINE)(LPVOID);
typedef const wchar_t *LPCWSTR;
struct TesteThread {
    LPTHREAD_START_ROUTINE fn;
    unsigned hw, espera, codigo;
};
typedef TesteThread *HANDLE;
struct D3DRECT {
    LONG x1, y1, x2, y2;
};
#define D3DCLEAR_TARGET 1
#define XUSER_INDEX_ANY 255
#define ERROR_SUCCESS 0
#define ERROR_ACCESS_DENIED 5
#define ERROR_IO_PENDING 997
#define CREATE_SUSPENDED 4
#define WAIT_TIMEOUT 258
struct MESSAGEBOX_RESULT {
    DWORD dwButtonPressed;
};
struct XOVERLAPPED {
    unsigned espera;
};
extern unsigned gApresentados, gCriadas, gConvertidas, gLimpas, gCaixas;
extern int gEscolha, gLimpar, gConverter, gThreadFalha, gNegar;
void TesteExigir(bool ok, const char *msg);
struct D3DDevice {
    void Clear(unsigned n, const D3DRECT *r, unsigned, unsigned, float, unsigned) {
        TesteExigir(n == 0 || (n == 1 && r && r->x1 >= 0 && r->x2 <= 1280 && r->y1 >= 0 && r->y2 <= 720 &&
                               r->x1 <= r->x2 && r->y1 <= r->y2),
                    "retângulo inválido");
    }
    void Present(void *, void *, void *, void *) {
        gApresentados++;
    }
};
inline DWORD GetTickCount() {
    static DWORD t;
    return t += 100;
}
inline HANDLE CreateThread(void *, unsigned, LPTHREAD_START_ROUTINE fn, void *, unsigned flags, void *) {
    TesteExigir(flags == CREATE_SUSPENDED, "thread iniciou antes da afinidade");
    if (gThreadFalha)
        return NULL;
    TesteThread *t = new TesteThread;
    t->fn = fn;
    t->hw = 99;
    t->espera = 0;
    t->codigo = 0;
    gCriadas++;
    return t;
}
inline void XSetThreadProcessor(HANDLE t, DWORD hw) {
    t->hw = hw;
}
inline void ResumeThread(HANDLE t) {
    TesteExigir(t->hw == 2, "conversão fora da thread de hardware 2");
}
inline DWORD WaitForSingleObject(HANDLE t, unsigned ms) {
    TesteExigir(ms == 0, "thread que apresenta fez espera bloqueante");
    if (t->espera++ < 3)
        return WAIT_TIMEOUT;
    t->codigo = t->fn(NULL);
    return 0;
}
inline void GetExitCodeThread(HANDLE t, DWORD *codigo) {
    *codigo = t->codigo;
}
inline void CloseHandle(HANDLE t) {
    delete t;
}
inline void ZeroMemory(void *p, std::size_t n) {
    std::memset(p, 0, n);
}
inline DWORD XShowMessageBoxUI(unsigned, const wchar_t *titulo, const wchar_t *texto, unsigned n, LPCWSTR *b,
                               unsigned padrao, unsigned, MESSAGEBOX_RESULT *r, XOVERLAPPED *) {
    TesteExigir(titulo && texto && b && n >= 1 && n <= 2, "caixa inválida");
    TesteExigir(!std::wcsstr(texto, L"ambiente.txt") && !std::wcsstr(texto, L"BT3_"), "texto interno exposto");
    if (n == 2)
        TesteExigir(padrao == 1, "pergunta sem Cancel padrão");
    if (gNegar) {
        gNegar--;
        return ERROR_ACCESS_DENIED;
    }
    gCaixas++;
    r->dwButtonPressed = n == 2 ? (unsigned)gEscolha : 0;
    return ERROR_IO_PENDING;
}
inline bool XHasOverlappedIoCompleted(XOVERLAPPED *o) {
    return o->espera++ >= 3;
}
inline DWORD XGetOverlappedResult(XOVERLAPPED *, DWORD *fim, int) {
    *fim = 0;
    return ERROR_SUCCESS;
}
#endif
