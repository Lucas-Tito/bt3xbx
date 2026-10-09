/*
 * F0: sonda. Abre como um título comum (Title ID, nome e ícone próprios), reserva a memória nos
 * endereços que o port usa, mede a aritmética e a leitura de memória, e escreve tudo em
 * game:\bt3xbx.log. A tela diz a fase pela cor: amarelo começando, azul medindo, verde terminou,
 * vermelho falhou. Qualquer botão volta para o dashboard.
 */
#include <xtl.h>
#include "diario.h"
#include "f0/sonda.h"

static volatile DWORD gCor = 0xFFE0C000; /* amarelo */
static volatile LONG gTerminou = 0;
static int gFalhasReserva = 0;

static LONG WINAPI AoMorrer(LPEXCEPTION_POINTERS p) {
    if (p != NULL && p->ExceptionRecord != NULL && p->ContextRecord != NULL) {
        unsigned acesso = 0, onde = 0;
        if (p->ExceptionRecord->NumberParameters >= 2) {
            acesso = (unsigned)p->ExceptionRecord->ExceptionInformation[0];
            onde = (unsigned)p->ExceptionRecord->ExceptionInformation[1];
        }
        Diario_Escrever("CRASH code=0x%08X Iar=0x%08X Lr=0x%08X r1=0x%08X %s=0x%08X",
                        (unsigned)p->ExceptionRecord->ExceptionCode, (unsigned)p->ContextRecord->Iar,
                        (unsigned)p->ContextRecord->Lr, (unsigned)p->ContextRecord->Gpr1,
                        acesso ? "escrevendo" : "lendo", onde);
    } else {
        Diario_Escrever("CRASH sem contexto");
    }
    Diario_Fechar();
    return EXCEPTION_CONTINUE_SEARCH;
}

static void Memoria(const char *quando) {
    MEMORYSTATUS m;
    GlobalMemoryStatus(&m);
    Diario_Escrever("memória %s: livre %u KB de %u KB", quando, (unsigned)(m.dwAvailPhys / 1024),
                    (unsigned)(m.dwTotalPhys / 1024));
}

static void Consultar(DWORD endereco) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((void *)endereco, &mbi, sizeof(mbi)) == 0) {
        Diario_Escrever("   VirtualQuery(%08X) falhou, erro %u", endereco, (unsigned)GetLastError());
        return;
    }
    Diario_Escrever("   VirtualQuery(%08X): base %08X, %08X bytes, estado %08X, tipo %08X", endereco,
                    (unsigned)(DWORD)mbi.BaseAddress, (unsigned)mbi.RegionSize, (unsigned)mbi.State,
                    (unsigned)mbi.Type);
}

/* As regiões que o port de PC (port/src/plat_mem.c) põe em endereço fixo, porque o jogo guarda
   endereços do PS2. Abaixo de 0x40000000 o 360 só aceita páginas de 4 KB; de 0x40000000 a
   0x7FFFFFFF, só de 64 KB (MEM_LARGE_PAGES), e é ali que moram as pilhas das threads. Por isso o
   scratchpad em 0x70000000 é informativo: o port pode pô-lo em outro lugar. */
struct Regiao {
    DWORD base, tamanho;
    BOOL confirmar, informativa;
    const char *nome;
};
static const Regiao kRegioes[] = {
    {0x00100000, 0x01F00000, TRUE, FALSE, "dados do jogo + heap (0x100000..0x2000000)"},
    {0x02000000, 0x01000000, TRUE, FALSE, "pilha do jogo"},
    {0x10000000, 0x00010000, TRUE, FALSE, "registradores de hardware"},
    {0x12000000, 0x00010000, TRUE, FALSE, "registradores do GS"},
    {0x13000000, 0x0C000000, FALSE, FALSE, "arena baixa do port (só reserva)"},
    {0x70000000, 0x00010000, TRUE, TRUE, "scratchpad (páginas de 64 KB)"},
};

static int Reservar(void) {
    int falhas = 0;
    Diario_Escrever("== memória em endereço fixo");
    for (int i = 0; i < (int)(sizeof(kRegioes) / sizeof(kRegioes[0])); i++) {
        const Regiao &r = kRegioes[i];
        MEMORY_BASIC_INFORMATION antes;
        BOOL livre = VirtualQuery((void *)r.base, &antes, sizeof(antes)) != 0 && antes.State == MEM_FREE;
        DWORD tipo = MEM_RESERVE | (r.confirmar ? MEM_COMMIT : 0) | (r.base >= 0x40000000 ? MEM_LARGE_PAGES : 0);
        void *p = VirtualAlloc((void *)r.base, r.tamanho, tipo, PAGE_READWRITE);

        if (p == (void *)r.base && livre) {
            Diario_Escrever(" ok    %08X +%08X %s", r.base, r.tamanho, r.nome);
            if (r.confirmar) {
                /* escreve e confere uma palavra por página de 4 KB; só em faixa que estava livre */
                volatile DWORD *w = (volatile DWORD *)p;
                for (DWORD o = 0; o < r.tamanho / 4; o += 1024) w[o] = r.base + o;
                for (DWORD o = 0; o < r.tamanho / 4; o += 1024) {
                    if (w[o] != r.base + o) {
                        Diario_Escrever("   leitura errada em %08X", r.base + o * 4);
                        falhas++;
                        break;
                    }
                }
            }
        } else {
            Diario_Escrever(" %s %08X +%08X %s: devolveu %08X, erro %u, estava %s", r.informativa ? "info " : "FALHA",
                            r.base, r.tamanho, r.nome, (unsigned)(DWORD)p, (unsigned)GetLastError(),
                            livre ? "livre" : "ocupada");
            Consultar(r.base);
            if (p != NULL && p != (void *)r.base) {
                VirtualFree(p, 0, MEM_RELEASE);
            }
            if (!r.informativa) {
                falhas++;
            }
        }
    }
    return falhas;
}

static DWORD WINAPI Medir(LPVOID) {
    int local, falhas = gFalhasReserva;

    Diario_Escrever("pilha da thread de medição perto de %08X", (unsigned)(DWORD)&local);
    gCor = 0xFF2060E0; /* azul */
    falhas += Sonda_Memoria();
    falhas += Sonda_Float();
    Memoria("no fim");
    Diario_Escrever("== fim: %s", falhas ? "COM FALHAS" : "tudo certo");
    Diario_Escrever("aperte qualquer botão para voltar ao dashboard");
    gCor = falhas ? 0xFFD02020 : 0xFF20B040; /* vermelho ou verde */
    InterlockedExchange(&gTerminou, 1);
    return 0;
}

void __cdecl main() {
    int local;

    Diario_Abrir("game:\\bt3xbx.log");
    SetUnhandledExceptionFilter(AoMorrer);
    Diario_Escrever("bt3xbx sonda F0 (" __DATE__ " " __TIME__ ")");
    Diario_Escrever("pilha da main perto de %08X", (unsigned)(DWORD)&local);

    /* Antes de tudo, como o port fará: nada do D3D ou da XAPI pode estar nessas faixas ainda. */
    Memoria("no início");
    gFalhasReserva = Reservar();
    Memoria("depois de reservar");

    IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp;
    ZeroMemory(&pp, sizeof(pp));
    pp.BackBufferWidth = 1280;
    pp.BackBufferHeight = 720;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    D3DDevice *dev = NULL;
    HRESULT hr = d3d->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev);
    if (FAILED(hr)) {
        Diario_Escrever("ERRO: CreateDevice = 0x%08X", hr);
        for (;;) {
        }
    }
    Diario_Escrever("D3D pronto em 1280x720");
    Memoria("com o D3D");

    /* As medições rodam noutra thread (hardware 2): a de desenho nunca para, senão a Guide congela. */
    HANDLE t = CreateThread(NULL, 0, Medir, NULL, CREATE_SUSPENDED, NULL);
    XSetThreadProcessor(t, 2);
    ResumeThread(t);

    for (;;) {
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
            if (apertou) {
                break;
            }
        }
    }

    /* Sair: a doc do XLaunchNewImage pede que nenhuma thread esteja desenhando ou tocando som. A de
       medição já terminou; a de desenho para aqui, espera a GPU e solta o device. */
    Diario_Escrever("voltando ao dashboard");
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
    dev->BlockUntilIdle();
    dev->Release();
    d3d->Release();
    Diario_Fechar();
    XLaunchNewImage(XLAUNCH_KEYWORD_DASH, 0);
}
