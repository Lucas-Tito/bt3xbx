/*
 * F2: o jogo sem janela no 360 (decisão 15 e plano, F2): a luta de demonstração inteira, sem desenho nem som, para
 * medir quanto a lógica do jogo custa por quadro no Xenon com a FPU de verdade e para comparar o resultado.
 *
 * A thread principal só limpa a tela com a cor da fase (nunca para: a Guide congela o console se ela parar):
 *   preto    preparando a memória e as tabelas do disco (antes do D3D, de propósito: ver Preparar)
 *   amarelo  um instante, entre o D3D e o jogo
 *   azul     a luta rodando
 *   verde    terminou bem; qualquer botão volta para o dashboard
 *   vermelho falhou (o log diz onde: game:\bt3xbx.log, game:\saida.txt), ou a ISO não é a convertida no PC
 *            (decisão 23): a caixa do sistema diz isso por cima da tela vermelha, e o jogo não começa
 * O jogo roda noutra thread, com pilha grande e a FPU em truncamento (o modelo do PS2 para soma e multiplicação).
 * Com BT3_GS=gpu (F3, etapa 7) o jogo desenha: o dispositivo passa para a thread do jogo, que apresenta um quadro por
 * quadro do GS (port/x360/src/gs/gs_d3d.cpp), e a main só espera; quando o jogo termina, ela pega o dispositivo de
 * volta para a cor do fim.
 * Por quadro: game:\hash.txt, só com BT3_HASH no ambiente.txt (o mesmo formato da build de PC, para
 * port/x360/tools/compara.py), e o tempo de cada
 * quadro no log (port/x360/src/f2/estado360.c).
 */
#include <xtl.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "diario.h"

extern "C" {
void Port_MapMemory(void);              /* port/src/plat_mem.c, ramo PORT_X360 */
int __real_main(int argc, char **argv); /* src/main.c, renomeado por c89.py */
void *Port_X360Reserve(unsigned addr, unsigned size);
void Port_Sair(int codigo);
void Port_Abortar(void);
void Ambiente_Carregar(const char *caminho); /* posix360.c */
void Estado_Resumo(void);                  /* estado360.c */
void Port_X360FecharSaida(void);           /* posix360.c */
void Iso_Iniciar(void);                    /* iso360.c */
const char *Iso_Pedida(void);              /* iso360.c: o caminho do BT3_ISO que não abriu */
int Iso_Recusada(void);                    /* iso360.c: a ISO aberta não tem a marca da convertida */
extern const char *volatile gPortStage;    /* plat_mem.c: até onde o arranque chegou */
void GsD3d_Soltar(void);                   /* port/x360/src/gs/gs_d3d.cpp */
int Port_UnpackPendenteNaQueda(char *linha, int n); /* port/x360/conv/conv_jogo.c: a guarda do Sprite_Unpack */
D3DDevice *gX360Dev;                       /* o dispositivo, para o backend do GS */
/* o que o vigia lê da thread do jogo (sem xbdm não há como ler o contexto de outra thread no console) */
extern volatile const char *gGsD3dEtapa;   /* gs_d3d.cpp: onde o fim do quadro está */
extern volatile unsigned gGsD3dQuadros;    /* quadros apresentados */
extern volatile unsigned gGsVu1Instr, gGsVu1Prog; /* port/src/gs/gs_vu1.c: instruções interpretadas, programa */
extern unsigned gGsFrame, gPortVBlanks;    /* gs_core.c, plat_stub.c */
extern volatile unsigned gX360Luta;        /* estado360.c */
}

/* BT3_GS=gpu: a main só espera; a cada 50 ms olha se a thread do jogo andou (vblank ou quadro apresentado). Parada
   há 3 s, o log diz onde ela estava, de novo a cada 5 s: a primeira rodada travou no começo da luta sem dizer nada, e
   com o jogo parado ninguém apresenta (nem a Guide abre). */
static void Vigiar(void) {
    static unsigned vb = ~0u, qd = ~0u, vu, falas;
    static DWORD desde, ultimaFala;
    DWORD agora = GetTickCount();
    if (gPortVBlanks != vb || gGsD3dQuadros != qd) {
        if (falas != 0 && agora - desde >= 3000) {
            Diario_Escrever("vigia: a thread do jogo voltou a andar depois de %u ms", (unsigned)(agora - desde));
        }
        vb = gPortVBlanks;
        qd = gGsD3dQuadros;
        vu = gGsVu1Instr;
        desde = agora;
        falas = 0;
        return;
    }
    if (agora - desde < 3000 || (falas != 0 && agora - ultimaFala < 5000) || falas >= 30) {
        return;
    }
    Diario_Escrever("vigia: a thread do jogo está parada há %u ms: vblank %u, quadro do GS %u, %u apresentados, %s, luta "
                    "desde o vblank %u; VU1: programa de %u instruções, %u instruções interpretadas desde a última olhada",
                    (unsigned)(agora - desde), gPortVBlanks, gGsFrame, gGsD3dQuadros, (const char *)gGsD3dEtapa, gX360Luta,
                    gGsVu1Prog, gGsVu1Instr - vu);
    vu = gGsVu1Instr;
    ultimaFala = agora;
    falas++;
}

static volatile DWORD gCor = 0xFFE0C000;     /* amarelo */
static volatile LONG gTerminou = 0;
static volatile LONG gCodigo = 0;
static DWORD gThreadJogo;

extern "C" void *Port_X360Reserve(unsigned addr, unsigned size) {
    return VirtualAlloc((void *)addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

/* exit() e abort() do jogo e do port (c89.py os renomeia): grava o que falta e encerra só a thread do jogo. */
extern "C" void Port_Sair(int codigo) {
    Diario_Escrever("saída do jogo com código %d (%s)", codigo, gPortStage);
    Estado_Resumo();
    Port_X360FecharSaida();
    _flushall();
    Diario_Descarregar(); /* o que o anel ainda tem, no disco antes de a thread do jogo acabar */
    GsD3d_Soltar(); /* o dispositivo volta para a main mostrar a cor do fim */
    InterlockedExchange(&gCodigo, codigo);
    InterlockedExchange((volatile LONG *)&gCor, codigo == 0 ? 0xFF20A040 : 0xFFC02020);
    InterlockedExchange(&gTerminou, 1);
    ExitThread((DWORD)codigo);
}

extern "C" void Port_Abortar(void) {
    Port_Sair(3);
}

static LONG WINAPI AoMorrer(LPEXCEPTION_POINTERS p) {
    if (p != NULL && p->ExceptionRecord != NULL && p->ContextRecord != NULL) {
        unsigned acesso = 0, onde = 0;
        if (p->ExceptionRecord->NumberParameters >= 2) {
            acesso = (unsigned)p->ExceptionRecord->ExceptionInformation[0];
            onde = (unsigned)p->ExceptionRecord->ExceptionInformation[1];
        }
        Diario_Escrever("CRASH code=0x%08X Iar=0x%08X Lr=0x%08X r1=0x%08X %s=0x%08X (fase: %s)",
                        (unsigned)p->ExceptionRecord->ExceptionCode, (unsigned)p->ContextRecord->Iar,
                        (unsigned)p->ContextRecord->Lr, (unsigned)p->ContextRecord->Gpr1,
                        acesso ? "escrevendo" : "lendo", onde, gPortStage);
    } else {
        Diario_Escrever("CRASH sem contexto (fase: %s)", gPortStage);
    }
    Diario_Descarregar(); /* a linha CRASH no disco já: o que vem a seguir (Port_UnpackPendenteNaQueda) pode falhar de novo */
    {   /* um Sprite_Unpack ainda sem gancho é a causa provável (dados de menu little-endian): diz quem chamou */
        static char linha[256]; /* (fora da pilha, que pode ser o problema) */
        if (Port_UnpackPendenteNaQueda(linha, sizeof(linha))) {
            Diario_Escrever("%s", linha);
        }
    }
    _flushall();
    Diario_Descarregar();
    gCor = 0xFFC02020;
    InterlockedExchange(&gTerminou, 1);
    if (GetCurrentThreadId() == gThreadJogo) {
        GsD3d_Soltar();
        ExitThread(5); /* só o jogo morre: a main segue com a tela vermelha e volta ao dashboard no botão */
    }
    Diario_Fechar();
    return EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI Jogo(LPVOID arg) {
    static char nome[] = "default.xex";
    static char *argv[] = {nome, NULL};
    int r;
    (void)arg;
    _controlfp(_RC_CHOP, _MCW_RC); /* soma e multiplicação truncadas, como no PS2 (decisões 4 e 5) */
    Diario_Escrever("jogo começando (%s)", gPortStage);
    gCor = 0xFF2040C0; /* azul */
    r = __real_main(1, argv);
    Port_Sair(r);
    return 0;
}

/* A memória do jogo (endereços fixos) e as tabelas do disco, antes do D3D e da pilha grande do jogo: no 360 os
   endereços baixos saem de baixo para cima, e o que for alocado antes pode ocupar a faixa do heap do jogo. Numa
   thread própria porque uma falha aqui sai por Port_Sair (ExitThread), e a main tem de seguir para a tela vermelha. */
static DWORD WINAPI Preparar(LPVOID arg) {
    (void)arg;
    Diario_Escrever("preparando a memória e as tabelas do disco");
    Port_MapMemory();
    return 0;
}

/* A ISO original (sem a marca da convertida) foi recusada: a caixa de mensagem do sistema por cima da tela preta.
   A caixa só aparece com a tela sendo apresentada, então a main apresenta enquanto ela está aberta (sem bloquear:
   a Guide congela o console se ninguém apresentar). O texto, os botões, o resultado e o XOVERLAPPED ficam fora da
   pilha: o sistema os usa depois da chamada voltar. Se outra tela do sistema estiver aberta (ERROR_ACCESS_DENIED),
   tenta de novo por 10 s; se mesmo assim não abrir, fica só a tela preta, e o log diz por quê. Devolve true se a caixa
   abriu e fechou: o OK leva direto ao dashboard (o jogo nunca começou). Se não abriu, a tela parada e um botão voltam
   ao dashboard. */
static bool MostrarRecusa(D3DDevice *dev, int motivo) {
    static const WCHAR kTituloOriginal[] = L"ISO original";
    static const WCHAR kTituloAusente[] = L"ISO n\x00E3o encontrada";
    static const WCHAR kTextoOriginal[] =
        L"Esta \x00E9 a ISO original do jogo. O port l\x00EA a ISO convertida no PC: converta-a com "
        L"port/x360/tools/iso_convertida.py, copie a convertida para a pasta do jogo e aponte o BT3_ISO do "
        L"ambiente.txt para ela.";
    static const WCHAR kPrefixoAusente[] =
        L"O arquivo do BT3_ISO n\x00E3o existe ou n\x00E3o \x00E9 uma ISO. Caminho pedido no ambiente.txt: ";
    static WCHAR sTextoAusente[600]; /* estático: o sistema o usa depois da chamada voltar */
    const WCHAR *kTitulo = motivo == 2 ? kTituloAusente : kTituloOriginal;
    const WCHAR *kTexto = kTextoOriginal;
    static LPCWSTR kBotoes[1] = {L"OK"};
    static MESSAGEBOX_RESULT sResultado;
    static XOVERLAPPED sOv;
    DWORD r, inicio = GetTickCount(), fim = 0;
    if (motivo == 2) {
        const char *c = Iso_Pedida();
        size_t n = wcslen(kPrefixoAusente), i;
        wcscpy(sTextoAusente, kPrefixoAusente);
        for (i = 0; c[i] != '\0' && n < 598; i++) {
            sTextoAusente[n++] = (WCHAR)(unsigned char)c[i];
        }
        sTextoAusente[n] = 0;
        kTexto = sTextoAusente;
    }
    for (;;) {
        dev->Clear(0, NULL, D3DCLEAR_TARGET, gCor, 1.0f, 0);
        dev->Present(NULL, NULL, NULL, NULL);
        ZeroMemory(&sOv, sizeof(sOv));
        r = XShowMessageBoxUI(XUSER_INDEX_ANY, kTitulo, kTexto, 1, kBotoes, 0, XMB_ERRORICON, &sResultado, &sOv);
        if (r != ERROR_ACCESS_DENIED || GetTickCount() - inicio >= 10000) {
            break;
        }
    }
    if (r != ERROR_IO_PENDING && r != ERROR_SUCCESS) {
        Diario_Escrever("recusa: a caixa do sistema não abriu (XShowMessageBoxUI devolveu %u): fica a tela preta",
                        (unsigned)r);
        return false;
    }
    Diario_Escrever("recusa: caixa do sistema aberta, \"%s\"", motivo == 2 ? "ISO não encontrada" : "ISO original");
    while (!XHasOverlappedIoCompleted(&sOv)) {
        dev->Clear(0, NULL, D3DCLEAR_TARGET, gCor, 1.0f, 0);
        dev->Present(NULL, NULL, NULL, NULL);
    }
    r = XGetOverlappedResult(&sOv, &fim, FALSE);
    Diario_Escrever("recusa: caixa fechada (resultado %u, botão %d); o jogo não começa, volta ao dashboard", (unsigned)r, (int)sResultado.dwButtonPressed);
    return true;
}

/* XLaunchNewImage não volta: de uma thread nova, com a main parada esperando (pesquisa/01, CLAUDE.md) */
static DWORD WINAPI VoltarAoDashboard(LPVOID arg) {
    (void)arg;
    XLaunchNewImage(NULL, 0);
    return 0;
}

void __cdecl main() {
    IDirect3D9 *d3d;
    D3DPRESENT_PARAMETERS pp;
    D3DDevice *dev = NULL;
    HANDLE t;
    int local;
    int motivo;
    bool gpu, recusada, saidaDireta = false, solto = false;

    Diario_Abrir("game:\\bt3xbx.log");
    SetUnhandledExceptionFilter(AoMorrer);
    Diario_Escrever("bt3xbx F2, jogo sem janela (" __DATE__ " " __TIME__ "), pilha da main perto de %08X",
                    (unsigned)(DWORD)&local);
    Ambiente_Carregar("game:\\ambiente.txt");
    Iso_Iniciar(); /* antes de qualquer thread: a ISO e a trava dela */
    motivo = Iso_Recusada();
    recusada = motivo != 0;

    if (!recusada) { /* recusada a ISO, nem as tabelas do disco: o jogo não começa */
        t = CreateThread(NULL, 256 * 1024, Preparar, NULL, CREATE_SUSPENDED, &gThreadJogo);
        ResumeThread(t);
        WaitForSingleObject(t, INFINITE);
        CloseHandle(t);
    }
    t = NULL;

    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    ZeroMemory(&pp, sizeof(pp));
    pp.BackBufferWidth = 1280;
    pp.BackBufferHeight = 720;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    if (FAILED(d3d->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev))) {
        Diario_Escrever("ERRO: CreateDevice");
        for (;;) {
        }
    }

    /* BT3_GS=gpu: quem desenha é o jogo (gs_d3d.cpp pega o dispositivo no começo do desenho) */
#pragma warning(suppress : 4996) /* (o getenv é o de posix360.c, que lê o ambiente.txt, não o da CRT) */
    gpu = !recusada && getenv("BT3_GS") != NULL && strcmp(getenv("BT3_GS"), "gpu") == 0;
    gX360Dev = dev;
    if (recusada) {
        gCor = 0xFF010101; /* preto (o exato não limpa no console, guia 1.3) */
        InterlockedExchange(&gTerminou, 1);
        saidaDireta = MostrarRecusa(dev, motivo);
    }
    if (gpu) {
        dev->Clear(0, NULL, D3DCLEAR_TARGET, 0xFF000000, 1.0f, 0);
        dev->Present(NULL, NULL, NULL, NULL);
        dev->ReleaseThreadOwnership();
        Diario_Escrever("BT3_GS=gpu: o jogo desenha");
    }

    /* o jogo no núcleo 1 (thread de hardware 2), sozinho; pilha de 16 MB como a build de PC de 64 bits */
    if (!gTerminou) {
        t = CreateThread(NULL, 16 * 1024 * 1024, Jogo, NULL, CREATE_SUSPENDED, &gThreadJogo);
        XSetThreadProcessor(t, 2);
        ResumeThread(t);
    }

    for (;;) {
        if (saidaDireta) { /* a caixa da recusa fechou: direto ao dashboard */
            break;
        }
        if (gpu) {
            if (!gTerminou) {
                Sleep(50);
                Vigiar();
                continue;
            }
            /* o jogo terminou: o dispositivo volta (GsD3d_Soltar); se ele morreu sem soltar, toma-se à força */
            for (int espera = 0; espera < 40 && dev->QueryThreadOwnership() != 0 &&
                                 dev->QueryThreadOwnership() != GetCurrentThreadId(); espera++) {
                Sleep(50);
            }
            if (dev->QueryThreadOwnership() == 0) {
                dev->AcquireThreadOwnership();
            } else if (dev->QueryThreadOwnership() != GetCurrentThreadId()) {
                Diario_Escrever("o dispositivo ainda era da thread %u: tomado", (unsigned)dev->QueryThreadOwnership());
                dev->SetThreadOwnership(GetCurrentThreadId());
            }
            gpu = false;
        }
        dev->Clear(0, NULL, D3DCLEAR_TARGET, gCor, 1.0f, 0);
        dev->Present(NULL, NULL, NULL, NULL);
        if (gTerminou) {
            bool apertou = false;
            for (DWORD porta = 0; porta < XUSER_MAX_COUNT; porta++) {
                XINPUT_STATE e;
                if (XInputGetState(porta, &e) == ERROR_SUCCESS && e.Gamepad.wButtons != 0) {
                    apertou = true;
                }
            }
            if (apertou && solto) { /* um botão apertado de novo: o A que fechou a caixa não conta (caixa que não abriu) */
                break;
            }
            solto = solto || !apertou;
        }
    }

    Diario_Escrever("voltando ao dashboard");
    if (t != NULL) {
        WaitForSingleObject(t, INFINITE);
        CloseHandle(t);
    }
    dev->BlockUntilIdle();
    dev->Release();
    d3d->Release();
    Diario_Fechar();
    CloseHandle(CreateThread(NULL, 64 * 1024, VoltarAoDashboard, NULL, 0, NULL));
    for (;;) {
        Sleep(1000);
    }
}
