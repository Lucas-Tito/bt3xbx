/* Desmontagem comum à saída normal e à queda. Incluída pela main e pelo ensaio nativo.
   Todas as esperas e as APIs que podem bloquear rodam numa auxiliar de hardware 1. */
#ifndef SAIDA360_H
#define SAIDA360_H

extern "C" {
void *Port_GsThread360(void);
void *GsDraw_Thread360(void);
void Blocos_Iniciar360(void);
int Blocos_PararTodos360(void);
}
static const char *volatile sSaidaEtapa = "início";
static int sThreadsSeguras;
static volatile LONG sGraficosParados;

static int Saida_Esperar(const char *nome, HANDLE h, unsigned teto) {
    DWORD r = h != NULL ? WaitForSingleObject(h, teto) : WAIT_OBJECT_0;
    Diario_Escrever("saída: %s: espera=%08X teto=%u ms%s", nome, (unsigned)r, teto, h == NULL ? " (sem thread)" : "");
    return r == WAIT_OBJECT_0;
}

static DWORD WINAPI Saida_Graficos(LPVOID arg) {
    int ok;
    XSetThreadProcessor(GetCurrentThread(), 1);
    sSaidaEtapa = "pedidos de parada do GS";
    Diario_Escrever("saída: desmontagem das threads começou");
    Port_GsPararJa();
    GsDraw_DesenhoPararJa();
    sSaidaEtapa = "thread do jogo";
    ok = Saida_Esperar("jogo", (HANDLE)arg, 3000);
    sSaidaEtapa = "thread de gravação do GS";
    if (!Saida_Esperar("GS gravação", (HANDLE)Port_GsThread360(), 2000)) ok = 0;
    sSaidaEtapa = "thread de desenho do GS";
    if (!Saida_Esperar("GS desenho", (HANDLE)GsDraw_Thread360(), 2000)) ok = 0;
    if (ok) InterlockedExchange(&sGraficosParados, 1);
    sThreadsSeguras = ok;
    return 0;
}

static DWORD WINAPI Saida_Threads(LPVOID arg) {
    int ok;
    XSetThreadProcessor(GetCurrentThread(), 1);
    if (!sGraficosParados) Saida_Graficos(arg);
    ok = sThreadsSeguras;
    sSaidaEtapa = "cargas em blocos";
    if (!Blocos_PararTodos360()) ok = 0;
    sSaidaEtapa = "cargas de áudio, vigia, misturador e XAudio2";
    Som360_Parar();
    sThreadsSeguras = ok;
    Diario_Escrever("saída: desmontagem das threads terminou; D3D %s", ok ? "liberável" : "preservado até o dashboard");
    return 0;
}

struct SaidaVideo { D3DDevice *dev; IDirect3D9 *d3d; };
static DWORD WINAPI Saida_Video(LPVOID arg) {
    SaidaVideo *v = (SaidaVideo *)arg;
    XSetThreadProcessor(GetCurrentThread(), 1);
    sSaidaEtapa = "GPU ociosa";
    Diario_Escrever("saída: GPU: esperando ociosa (teto externo 2000 ms)");
    v->dev->AcquireThreadOwnership();
    v->dev->SetVerticalBlankCallback(NULL);
    v->dev->BlockUntilIdle();
    Diario_Escrever("saída: GPU ociosa");
    sSaidaEtapa = "liberação do D3D";
    v->dev->Release();
    v->d3d->Release();
    Diario_Escrever("saída: D3D solto");
    return 0;
}

/* Espera externa por consultas sem bloqueio. Enquanto a main é dona, ela pode apresentar a tela. */
static int Saida_Aguardar(HANDLE h, unsigned teto, void (*apresentar)(void)) {
    DWORD inicio = GetTickCount(), r;
    if (h == NULL) {
        Diario_Escrever("saída: não criou auxiliar em %s; erro=%u", (const char *)sSaidaEtapa, (unsigned)GetLastError());
        return 0;
    }
    for (;;) {
        r = WaitForSingleObject(h, 0);
        if (r == WAIT_OBJECT_0) return 1;
        if (r != WAIT_TIMEOUT || GetTickCount() - inicio >= teto) {
            Diario_Escrever("saída: auxiliar em %s: espera=%08X teto=%u ms; segue ao dashboard",
                            (const char *)sSaidaEtapa, (unsigned)r, teto);
            return 0;
        }
        if (apresentar != NULL) apresentar();
        Sleep(10);
    }
}

/* O fechamento do diário também tem teto externo, para uma falha de disco não prender a saída. */
static DWORD WINAPI Saida_Diario(LPVOID) {
    XSetThreadProcessor(GetCurrentThread(), 1);
    sSaidaEtapa = "fechamento do diário";
    Diario_Escrever("saída: solicitando XLaunchNewImage(NULL, 0) após fechar o diário");
    Port_X360FecharSaida();
    Diario_Fechar();
    return 0;
}
/* Só a main chama, depois do botão ou da decisão de emergência. */
static void Saida_Finalizar(SaidaVideo *video, int liberarVideo) {
    HANDLE auxiliar;
    if (liberarVideo) {
        sSaidaEtapa = "GPU e D3D";
        video->dev->ReleaseThreadOwnership();
        auxiliar = CreateThread(NULL, 256 * 1024, Saida_Video, video, 0, NULL);
        Saida_Aguardar(auxiliar, 2000, NULL);
        if (auxiliar != NULL) CloseHandle(auxiliar);
    }
    sSaidaEtapa = "fechamento do diário";
    auxiliar = CreateThread(NULL, 64 * 1024, Saida_Diario, NULL, 0, NULL);
    Saida_Aguardar(auxiliar, 2000, NULL);
    if (auxiliar != NULL) CloseHandle(auxiliar);
    auxiliar = CreateThread(NULL, 64 * 1024, VoltarAoDashboard, NULL, 0, NULL);
    if (auxiliar != NULL) CloseHandle(auxiliar);
    else XLaunchNewImage(NULL, 0); /* nem falta de memória para a pilha pode prender a saída */
}
#endif
