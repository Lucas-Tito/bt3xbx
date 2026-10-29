/* Testa os corpos reais de parada contra uma API pequena, sem threads ou disco reais. */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <string>
#include <stdexcept>
#include <vector>
typedef unsigned DWORD;
typedef long LONG;
typedef void *LPVOID;
struct Thread { unsigned resultado; int fechou; };
typedef Thread *HANDLE;
struct CRITICAL_SECTION { int livre; };
struct LARGE_INTEGER { long long QuadPart; };
#define WINAPI
#define CREATE_SUSPENDED 4
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#define FALSE 0
#define INFINITE (~0u)
#define WAIT_OBJECT_0 0u
#define WAIT_TIMEOUT 258u
static unsigned relogio;
static bool travaPresa;
static std::string log;
static std::vector<std::string> chamadas;
static void exige(bool ok, const char *s) { if (!ok) throw std::runtime_error(s); }
static void marca(const char *s) { chamadas.push_back(s); }
extern "C" { int gX360Log = 1; }
extern "C" void Diario_Escrever(const char *f, ...) {
    char b[1024]; va_list a; va_start(a, f); vsnprintf(b, sizeof(b), f, a); va_end(a); log += b;
}
static void InitializeCriticalSection(CRITICAL_SECTION *c) { c->livre=1; }
static void DeleteCriticalSection(CRITICAL_SECTION *) {}
static void EnterCriticalSection(CRITICAL_SECTION *) {}
static void LeaveCriticalSection(CRITICAL_SECTION *) {}
static int TryEnterCriticalSection(CRITICAL_SECTION *) { return !travaPresa; }
static DWORD GetTickCount() { return relogio; }
static void Sleep(DWORD ms) { relogio += ms; }
static void QueryPerformanceCounter(LARGE_INTEGER *v) { v->QuadPart=relogio; }
static void QueryPerformanceFrequency(LARGE_INTEGER *v) { v->QuadPart=1000; }
static HANDLE CreateEvent(void *, int, int, void *) { return new Thread{0,0}; }
static HANDLE CreateThread(void *, unsigned, DWORD (*)(LPVOID), LPVOID, int, void *) { return new Thread{0,0}; }
static void XSetThreadProcessor(HANDLE, DWORD) {}
static void SetThreadPriority(HANDLE, int) {}
static void ResumeThread(HANDLE) {}
static void SetEvent(HANDLE) { marca("sinal"); }
static DWORD WaitForSingleObject(HANDLE h, unsigned teto) {
    exige(teto != ~0u || h->resultado == 0, "espera infinita"); marca("espera");
    if (h->resultado) relogio += teto;
    return h->resultado;
}
static void CloseHandle(HANDLE h) { h->fechou++; }
static LONG InterlockedExchange(volatile LONG *p, LONG n) { LONG velho=*p; *p=n; return velho; }

#if defined(TESTE_BLOCOS)
#include "blocos.c"
static int ler_teste(void *, long, void *, long) { return 1; }
static void rodar(const std::string &caso) {
    Blocos_Iniciar360();
    Blocos *b = Blocos_Abrir(ler_teste, NULL, 4096, 1024, 1);
    exige(b != NULL && b->comThread, "não cadastrou o leitor");
    if (caso == "atraso") b->thread->resultado=WAIT_TIMEOUT;
    if (caso == "trava") travaPresa=true;
    int r=Blocos_PararTodos360();
    exige(r == (caso == "normal"), "resultado do leitor incorreto");
    if (caso != "trava") exige(b->sair == 1, "não pediu parada");
    exige(b->buf[0] != NULL && b->thread->fechou == 0, "liberou dados ainda usados");
    exige(log.find(caso == "trava" ? "trava excedeu" : "espera=") != std::string::npos, "não registrou resultado");
    exige(relogio <= 2000, "excedeu teto");
    if (caso == "normal") { Blocos_Fechar(b); exige(sAtivos == NULL, "cadastro reteve leitor fechado"); }
}
#elif defined(TESTE_FLUXO)
static int sLigado=1, sParado, sSair;
static Thread fluxo={0,0};
static HANDLE sThread=&fluxo;
static void trava() {}
static void destrava() {}
static void acorda() { marca("acorda"); }
#include "parar.inc"
static void rodar(const std::string &caso) {
    if (caso == "atraso") fluxo.resultado=WAIT_TIMEOUT;
    if (caso == "terminando") sParado=1;
    Fluxo_Parar();
    exige(sSair == 1, "não pediu parada da leitura");
    exige(!chamadas.empty() && chamadas[0] == "acorda", "não acordou leitor");
    exige(log.find("espera=") != std::string::npos, "não registrou resultado");
    exige((sThread == NULL) == (caso != "atraso"), "fechou handle de thread viva ou perdeu handle terminado");
    exige(relogio <= 2000, "excedeu teto");
}
#else
static volatile LONG sLigado=1, sMorto, sFimMisturador;
static Thread vigia={0,0}, misturador={0,0}, evento={0,0}, pare={0,0}, sistema={0,0};
static HANDLE sVigia=&vigia, sMisturador=&misturador, sEvento=&evento, sPare=&pare, sSistema=&sistema;
struct Motor { void StopEngine() { marca("StopEngine"); } void Release() { marca("motor"); } } motor;
struct Voz { void DestroyVoice() { marca("voz"); } } voz, saida;
static Motor *sX=&motor;
static Voz *sVoz=&voz, *sSaida=&saida;
static void Medidas(const char *) {}
static void Leitura(const char *) {}
static void Contadores(const char *) {}
static void Fluxo_Parar() { marca("fluxo"); }
#include "parar.inc"
static void rodar(const std::string &caso) {
    if (caso == "vigia") vigia.resultado=WAIT_TIMEOUT;
    if (caso == "misturador") misturador.resultado=WAIT_TIMEOUT;
    Som360_Parar();
    exige(sFimMisturador && sMorto && !sLigado, "produtores do som não pararam");
    unsigned esperas=0;
    for (unsigned i=0; i<chamadas.size(); i++) {
        if (chamadas[i] == "espera") esperas++;
        if (chamadas[i] == "motor") exige(esperas == 2, "liberou som antes de esperar ambas as threads");
    }
    exige(esperas == 2, "faltou esperar uma thread de som");
    exige(log.find("som vigia: espera=") != std::string::npos && log.find("som misturador: espera=") != std::string::npos, "faltou resultado de espera");
    exige((sX == NULL) == (caso == "normal"), "liberou recurso de thread viva");
    exige(relogio <= 2000, "excedeu teto individual");
    size_t n=chamadas.size(); Som360_Parar(); exige(n == chamadas.size(), "parada repetida não é idempotente");
}
#endif
int main(int argc, char **argv) {
    try { exige(argc == 2, "informe o cenário"); rodar(argv[1]); printf("parada %s: tudo certo\n", argv[1]); return 0; }
    catch (const std::exception &e) { fprintf(stderr, "FALHA: %s\n", e.what()); return 1; }
}
