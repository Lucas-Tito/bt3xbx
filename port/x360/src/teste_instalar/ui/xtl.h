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
extern unsigned gApresentados, gCriadas, gConvertidas, gLimpas;
extern int gLimpar, gConverter, gThreadFalha;
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
/* A tela própria não usa mais a caixa do sistema: qualquer chamada reprova o teste. */
inline DWORD XShowMessageBoxUI(unsigned, const wchar_t *, const wchar_t *, unsigned, LPCWSTR *, unsigned, unsigned,
                               MESSAGEBOX_RESULT *, XOVERLAPPED *) {
    TesteExigir(false, "XShowMessageBoxUI chamada: a pergunta tem de ser própria");
    return ERROR_ACCESS_DENIED;
}
/* O controle simulado: o roteiro tem um caractere por quadro (uma leitura da porta gPorta por quadro).
   '.' solto, 'A', 'B', 'L', 'R', 'U' e 'D' do d-pad, 'l', 'r', 'u' e 'd' analógico esquerdo forte, 'w' analógico fraco.
   Depois do fim do roteiro tudo fica solto. As outras portas estão desconectadas. */
typedef unsigned short WORD;
#define XUSER_MAX_COUNT 4
#define ERROR_DEVICE_NOT_CONNECTED 1167
#define XINPUT_GAMEPAD_DPAD_UP 1
#define XINPUT_GAMEPAD_DPAD_DOWN 2
#define XINPUT_GAMEPAD_DPAD_LEFT 4
#define XINPUT_GAMEPAD_DPAD_RIGHT 8
#define XINPUT_GAMEPAD_A 0x1000
#define XINPUT_GAMEPAD_B 0x2000
struct XINPUT_GAMEPAD {
    WORD wButtons;
    short sThumbLX;
    short sThumbLY;
};
struct XINPUT_STATE {
    XINPUT_GAMEPAD Gamepad;
};
extern const char *gRoteiro;
extern unsigned gLidos, gPorta;
inline DWORD XInputGetState(DWORD porta, XINPUT_STATE *e) {
    char c;
    if (porta != gPorta)
        return ERROR_DEVICE_NOT_CONNECTED;
    TesteExigir(gLidos < 100000, "pergunta não fechou");
    c = gLidos < std::strlen(gRoteiro) ? gRoteiro[gLidos] : '.';
    gLidos++;
    e->Gamepad.wButtons = c == 'A' ? XINPUT_GAMEPAD_A : c == 'B' ? XINPUT_GAMEPAD_B
                        : c == 'U' ? XINPUT_GAMEPAD_DPAD_UP : c == 'D' ? XINPUT_GAMEPAD_DPAD_DOWN
                        : c == 'L' ? XINPUT_GAMEPAD_DPAD_LEFT : c == 'R' ? XINPUT_GAMEPAD_DPAD_RIGHT : 0;
    e->Gamepad.sThumbLX = c == 'l' ? -20000 : c == 'r' ? 20000 : c == 'w' ? -10000 : 0;
    e->Gamepad.sThumbLY = c == 'u' ? 20000 : c == 'd' ? -20000 : 0;
    return ERROR_SUCCESS;
}
#endif
