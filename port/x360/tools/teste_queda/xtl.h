/* API determinística para compilar a main real no ensaio, sem SEH ou driver. */
#ifndef TESTE_QUEDA_XTL_H
#define TESTE_QUEDA_XTL_H
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cstdint>
#include <cstdarg>
#include <string>
#include <vector>
#include <stdexcept>
typedef unsigned DWORD;
typedef int LONG;
typedef void *LPVOID;
typedef const wchar_t *LPCWSTR;
#define WINAPI
#define __cdecl
#define __int64 long long
#define __try try
#define __except(x) catch (...)
#define _snprintf snprintf
#define _RC_CHOP 0
#define _MCW_RC 0
#define _controlfp(a,b) ((void)0)
#define __lwsync() ((void)0)
#define EXCEPTION_EXECUTE_HANDLER 1
#define CREATE_SUSPENDED 4
#define MEM_RESERVE 1
#define MEM_COMMIT 2
#define PAGE_READWRITE 4
#define INFINITE (~0u)
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define ERROR_SUCCESS 0
#define ERROR_IO_PENDING 997
#define ERROR_ACCESS_DENIED 5
#define XUSER_INDEX_ANY 255
#define XUSER_MAX_COUNT 4
#define XMB_ERRORICON 1
#define FALSE 0
#define GENERIC_WRITE 1
#define FILE_SHARE_READ 1
#define CREATE_ALWAYS 1
#define FILE_ATTRIBUTE_NORMAL 0
#define ZeroMemory(p,n) memset(p,0,n)
struct ThreadExit {};
struct Dashboard {};
struct Alca { DWORD id, hw; DWORD (*fn)(LPVOID); LPVOID arg; bool fim; FILE *arquivo; };
typedef Alca *HANDLE;
#define INVALID_HANDLE_VALUE ((HANDLE)0)
extern DWORD agora, atual, hardware;
extern std::string caso, registro, relatorio;
extern std::vector<std::string> eventos;
extern bool filtroAtivo;
void Exigir(bool, const char *);
void Evento(const char *);
HANDLE CreateThread(void *, unsigned, DWORD (*)(LPVOID), LPVOID, unsigned, DWORD *);
DWORD ResumeThread(HANDLE);
DWORD WaitForSingleObject(HANDLE, unsigned);
int CloseHandle(HANDLE);
HANDLE GetCurrentThread();
DWORD GetCurrentThreadId();
void XSetThreadProcessor(HANDLE, unsigned);
DWORD GetLastError();
DWORD GetTickCount();
void Sleep(unsigned);
void ExitThread(DWORD);
void XLaunchNewImage(void *, unsigned);
void *VirtualAlloc(void *, unsigned, unsigned, unsigned);
LONG InterlockedExchange(volatile LONG *, LONG);
LONG InterlockedCompareExchange(volatile LONG *, LONG, LONG);
HANDLE CreateFile(const char *, unsigned, unsigned, void *, unsigned, unsigned, void *);
int WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
int FlushFileBuffers(HANDLE);
struct MESSAGEBOX_RESULT { DWORD dwButtonPressed; };
struct XOVERLAPPED { unsigned consultas; };
DWORD XShowMessageBoxUI(unsigned, LPCWSTR, LPCWSTR, unsigned, const wchar_t **, unsigned, unsigned, MESSAGEBOX_RESULT *, XOVERLAPPED *);
int XHasOverlappedIoCompleted(XOVERLAPPED *);
DWORD XGetOverlappedResult(XOVERLAPPED *, DWORD *, int);
struct XINPUT_STATE { struct { unsigned wButtons; } Gamepad; };
DWORD XInputGetState(unsigned, XINPUT_STATE *);
struct CONTEXT {
    unsigned long long Gpr0;
    unsigned long long Gpr1;
    unsigned long long Gpr2;
    unsigned long long Gpr3;
    unsigned long long Gpr4;
    unsigned long long Gpr5;
    unsigned long long Gpr6;
    unsigned long long Gpr7;
    unsigned long long Gpr8;
    unsigned long long Gpr9;
    unsigned long long Gpr10;
    unsigned long long Gpr11;
    unsigned long long Gpr12;
    unsigned long long Gpr13;
    unsigned long long Gpr14;
    unsigned long long Gpr15;
    unsigned long long Gpr16;
    unsigned long long Gpr17;
    unsigned long long Gpr18;
    unsigned long long Gpr19;
    unsigned long long Gpr20;
    unsigned long long Gpr21;
    unsigned long long Gpr22;
    unsigned long long Gpr23;
    unsigned long long Gpr24;
    unsigned long long Gpr25;
    unsigned long long Gpr26;
    unsigned long long Gpr27;
    unsigned long long Gpr28;
    unsigned long long Gpr29;
    unsigned long long Gpr30;
    unsigned long long Gpr31;
    unsigned Iar, Lr;
};
struct EXCEPTION_RECORD { unsigned ExceptionCode, NumberParameters, ExceptionInformation[2]; };
struct EXCEPTION_POINTERS { EXCEPTION_RECORD *ExceptionRecord; CONTEXT *ContextRecord; };
typedef EXCEPTION_POINTERS *LPEXCEPTION_POINTERS;
void SetUnhandledExceptionFilter(LONG (*)(LPEXCEPTION_POINTERS));
#define D3DCLEAR_TARGET 1
#define D3D_SDK_VERSION 1
#define D3DFMT_X8R8G8B8 1
#define D3DSWAPEFFECT_DISCARD 1
#define D3DPRESENT_INTERVAL_ONE 1
#define D3DDEVTYPE_HAL 1
#define D3DCREATE_HARDWARE_VERTEXPROCESSING 1
#define FAILED(x) ((x) != 0)
struct D3DPRESENT_PARAMETERS { unsigned BackBufferWidth, BackBufferHeight, BackBufferFormat, BackBufferCount, SwapEffect, PresentationInterval; };
struct D3DDevice {
    DWORD dono;
    D3DDevice(): dono(1) {}
    void Clear(unsigned, void *, unsigned, DWORD, float, unsigned);
    void Present(void *, void *, void *, void *);
    DWORD QueryThreadOwnership() { return dono; }
    void AcquireThreadOwnership() { dono = atual; }
    void SetThreadOwnership(DWORD n) { dono = n; }
    void ReleaseThreadOwnership() { dono = 0; }
    void SetVerticalBlankCallback(void *) {}
    void BlockUntilIdle() { Evento("GPU"); }
    void Release() { Evento("D3D"); }
};
struct IDirect3D9 { int CreateDevice(unsigned,unsigned,void *,unsigned,D3DPRESENT_PARAMETERS *,D3DDevice **); void Release() {} };
IDirect3D9 *Direct3DCreate9(unsigned);
#endif
