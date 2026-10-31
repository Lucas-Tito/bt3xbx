/* Ensaio determinístico da desmontagem real, com a API do Xbox substituída.
   Não executa SEH, GPU nem XLaunch: uma chamada bloqueada deixa o handle pendente. */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <string>
#include <vector>
#include <stdexcept>

typedef unsigned DWORD;
typedef long LONG;
typedef void *LPVOID;
#define WINAPI
#define WAIT_OBJECT_0 0u
#define WAIT_TIMEOUT 258u
#define WAIT_FAILED (~0u)
struct Thread { unsigned resultado; const char *nome; };
typedef Thread *HANDLE;
static Thread jogo = {0, "jogo"}, gravacao = {0, "gravação"}, desenho = {0, "desenho"};
static unsigned relogio, apresenta, lancamentos, processador;
static std::string cenario, log;
static std::vector<std::string> chamadas;
static bool semAuxiliar;
static void exige(bool ok, const char *mensagem) { if (!ok) throw std::runtime_error(mensagem); }
static void marca(const char *nome) { chamadas.push_back(nome); }
static void Diario_Escrever(const char *f, ...) {
    char buf[1024]; va_list a; va_start(a, f); vsnprintf(buf, sizeof(buf), f, a); va_end(a);
    log += buf; log += '\n';
}
static void XSetThreadProcessor(HANDLE, unsigned n) { processador = n; }
static HANDLE GetCurrentThread(void) { return NULL; }
static DWORD GetLastError(void) { return 8; }
static DWORD GetTickCount(void) { return relogio; }
static void Sleep(unsigned n) { relogio += n; }
static LONG InterlockedExchange(volatile LONG *p, LONG n) { LONG v = *p; *p = n; return v; }
static DWORD WaitForSingleObject(HANDLE h, unsigned ms) {
    exige(h != NULL, "espera de handle nulo");
    if (ms) marca(h->nome);
    if (h->resultado == WAIT_TIMEOUT) relogio += ms;
    return h->resultado;
}
struct Bloqueada {};
static HANDLE CreateThread(void *, unsigned, DWORD (*fn)(LPVOID), LPVOID arg, unsigned, void *) {
    if (semAuxiliar) return NULL;
    Thread *h = new Thread{0, "auxiliar"};
    try { fn(arg); } catch (const Bloqueada &) { h->resultado = WAIT_TIMEOUT; }
    return h;
}
static void CloseHandle(HANDLE h) { delete h; }
static void XLaunchNewImage(void *, unsigned) { marca("lançar"); lancamentos++; }
static DWORD WINAPI VoltarAoDashboard(LPVOID) { XLaunchNewImage(NULL, 0); return 0; }
static void Port_X360FecharSaida(void) { marca("saída de texto"); }
static void Diario_Fechar(void) { marca("diário"); if (cenario == "diario-preso") throw Bloqueada(); }
extern "C" void Port_GsPararJa(void) { marca("pedir gravação"); }
extern "C" void GsDraw_DesenhoPararJa(void) { marca("pedir desenho"); }
extern "C" void *Port_GsThread360(void) { return &gravacao; }
extern "C" void *GsDraw_Thread360(void) { return &desenho; }
extern "C" void Blocos_Iniciar360(void) {}
extern "C" int Blocos_PararTodos360(void) { marca("cargas"); return cenario != "carga-atrasada"; }
static void Som360_Parar(void) {
    exige(processador == 1, "som fora da thread de hardware 1");
    marca("som");
    if (cenario == "som-preso") throw Bloqueada();
}
struct D3DDevice {
    void ReleaseThreadOwnership() { marca("devolver D3D"); }
    void AcquireThreadOwnership() { marca("tomar D3D"); }
    void SetVerticalBlankCallback(void *) { marca("tirar callback"); }
    void BlockUntilIdle() { marca("GPU"); if (cenario == "gpu-presa") throw Bloqueada(); }
    void Release() { marca("soltar dispositivo"); }
};
struct IDirect3D9 { void Release() { marca("soltar D3D"); } };
#include "saida360.h"
#include "queda_teste360.h"
static void apresentar(void) { apresenta++; }
static unsigned pos(const char *nome) {
    for (unsigned i=0; i<chamadas.size(); i++) if (chamadas[i] == nome) return i;
    throw std::runtime_error(std::string("faltou chamada: ") + nome);
}
static bool tem(const char *nome) {
    for (unsigned i=0; i<chamadas.size(); i++) if (chamadas[i] == nome) return true;
    return false;
}
static void teste_chave(void) {
    const char *invalidos[] = {NULL, "", "0", "-1", "1x", "1000001", "4294967297", "0000000000000000"};
    for (unsigned i=0; i<sizeof(invalidos)/sizeof(*invalidos); i++) {
        QuedaTeste_Configurar(invalidos[i], NULL);
        for (unsigned q=0; q<10; q++) exige(!QuedaTeste_DeveCair(0, 1, q), "chave desligada provocou queda");
    }
    QuedaTeste_Configurar("1", "inválida");
    exige(!QuedaTeste_DeveCair(0, 1, 1), "thread inválida armada");
    for (int d=0; d<2; d++) {
        QuedaTeste_Configurar("3", d ? "desenho" : "jogo");
        exige(!QuedaTeste_DeveCair(d, 0, 1), "queda no menu");
        exige(!QuedaTeste_DeveCair(!d, 1, 1), "queda na thread errada");
        exige(!QuedaTeste_DeveCair(d, 1, 10), "queda antes de N");
        exige(!QuedaTeste_DeveCair(d, 1, 10), "contou o mesmo quadro duas vezes");
        exige(!QuedaTeste_DeveCair(d, 1, 11), "queda antes de N");
        exige(QuedaTeste_DeveCair(d, 1, 12), "faltou queda no quadro N");
        exige(!QuedaTeste_DeveCair(d, 1, 13), "queda repetida");
        QuedaTeste_Configurar("2", d ? "desenho" : "jogo");
        exige(!QuedaTeste_DeveCair(d, 1, 10), "contagem inicial");
        exige(!QuedaTeste_DeveCair(d, 0, 11), "queda fora de luta");
        exige(!QuedaTeste_DeveCair(d, 1, 12), "não reiniciou ao sair da luta");
        exige(QuedaTeste_DeveCair(d, 1, 13), "faltou queda na nova luta");
    }
}
int main(int argc, char **argv) {
    try {
        exige(argc == 2, "informe o cenário"); cenario = argv[1];
        if (cenario == "chave") { teste_chave(); puts("chave: tudo certo"); return 0; }
        if (cenario == "jogo-atrasado") jogo.resultado = WAIT_TIMEOUT;
        if (cenario == "gravacao-atrasada") gravacao.resultado = WAIT_TIMEOUT;
        if (cenario == "desenho-atrasado") desenho.resultado = WAIT_TIMEOUT;
        if (cenario == "espera-falhou") desenho.resultado = WAIT_FAILED;
        Diario_Escrever("ensaio: queda simulada na thread do jogo (sem SEH)");
        HANDLE h = CreateThread(NULL, 0, Saida_Threads, &jogo, 0, NULL);
        int terminou = Saida_Aguardar(h, 15000, apresentar);
        CloseHandle(h);
        bool seguras = terminou && sThreadsSeguras;
        bool atraso = cenario.find("atrasad") != std::string::npos || cenario == "espera-falhou" || cenario == "som-preso";
        exige(seguras == !atraso, "thread pendente considerada segura");
        exige(tem("pedir gravação") && tem("pedir desenho"), "faltou pedido de parada");
        exige(pos("jogo") < pos("cargas") && pos("gravação") < pos("cargas") && pos("desenho") < pos("cargas"), "faltou esperar threads antes das cargas");
        exige(pos("cargas") < pos("som"), "som antes das cargas");
        if (cenario == "som-preso") exige(apresenta > 0 && relogio <= 15000, "espera externa travou a apresentação");
        if (cenario == "sem-auxiliar") semAuxiliar = true;
        D3DDevice dev; IDirect3D9 d3d; SaidaVideo video = {&dev, &d3d};
        Saida_Finalizar(&video, seguras);
        exige(lancamentos == 1, "não chegou ao dashboard uma vez");
        exige(pos("som") < pos("lançar"), "lançou antes de parar som");
        if (seguras && cenario != "sem-auxiliar") {
            exige(pos("desenho") < pos("GPU") && pos("som") < pos("GPU"), "liberou GPU antes das threads e som");
            if (cenario == "gpu-presa") exige(!tem("soltar dispositivo"), "liberou dispositivo com GPU ocupada");
            else exige(pos("GPU") < pos("soltar dispositivo") && pos("soltar dispositivo") < pos("soltar D3D") && pos("soltar D3D") < pos("diário"), "ordem GPU/D3D/diário");
        } else exige(!tem("GPU") && !tem("soltar dispositivo"), "tocou no D3D com thread pendente");
        if (cenario != "sem-auxiliar") exige(pos("diário") < pos("lançar"), "lançou antes do diário");
        if (cenario == "gpu-presa" || cenario == "diario-preso") exige(relogio >= 2000 && relogio <= 4000, "teto da liberação não respeitado");
        if (cenario.find("atrasad") != std::string::npos || cenario == "espera-falhou") exige(log.find("preservado") != std::string::npos, "faltou registro de thread atrasada");
        if (cenario == "gpu-presa" || cenario == "som-preso" || cenario == "diario-preso") exige(log.find("segue ao dashboard") != std::string::npos, "faltou resultado do teto externo");
        if (cenario == "normal") fputs(log.c_str(), stdout);
        printf("%s: tudo certo (%u ms simulados)\n", cenario.c_str(), relogio);
        return 0;
    } catch (const std::exception &e) { fprintf(stderr, "FALHA: %s\n", e.what()); return 1; }
}
