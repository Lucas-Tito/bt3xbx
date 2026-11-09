/*
 * O amostrador de perfil da thread do jogo (F8, docs/x360/plano/F8/08-explosao-planeta.md). BT3_AMOSTRA=US no
 * environment.txt (com BT3_LOG=1): uma thread na thread de hardware 1 (longe do jogo, na 2, e do XAudio2, na 4 e na 5)
 * que a cada US microssegundos suspende a thread do jogo, lê onde ela estava e a retoma. Sem a chave, nada: nem memória,
 * nem thread.
 *
 * O XDK não tem GetThreadContext (só o DmGetThreadContext do xbdm, que o console não tem). O que existe: SuspendThread e
 * ResumeThread (winbase.h) e o KTHREAD do núcleo (layout do xkelib, kePrivateStructs.h; o hiddriver usa o mesmo). Com a
 * thread suspensa, ela está esperando o próprio semáforo de suspensão, e o r1 salvo na troca de contexto
 * (KTHREAD.KernelStack) leva, pelo encadeamento da pilha, ao quadro de armadilha (KTRAP_FRAME) que o núcleo gravou ao
 * interrompê-la: dali o Iar, o Lr e o r1 do jogo. A busca está em amostra_hist.c (AmostraHist_LerPilha), testada no host.
 *
 * Entre o SuspendThread e o ResumeThread só se lê memória (Ler, com SEH): nada de diário, heap ou CRT, que a thread do
 * jogo pode estar segurando. A thread do jogo fica parada uns microssegundos por amostra (a média e o pior vão no
 * despejo). O autoteste, antes do jogo, faz o mesmo com duas iscas (uma girando numa função conhecida, outra num Sleep)
 * e diz no diário se o Iar e a pilha conferem; sem saber quando a thread parou, o amostrador não liga.
 *
 * As amostras de cada quadro do jogo (gGsFrameJogo, o contador do lado do jogo) esperam o fim dele e vão para o
 * histograma da classe pela duração: normal (abaixo de BT3_AMOSTRA_LIMIAR ms, padrão o BT3_PICOS, ou 40), lento, longo
 * (400 ms ou mais: carregamentos). O despejo vai para o diário quando um trecho lento acaba (120 quadros normais
 * seguidos), a cada 5 minutos e na saída; port/x360/tools/amostra_perfil.py o lê com o default.map desta build.
 */
#include <xtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "diario.h"
#include "f2/amostra360.h"
#include "f2/amostra_hist.h"

extern "C" {
LONG ObLookupThreadByThreadId(DWORD id, PVOID *thread); /* xboxkrnl: o KTHREAD com uma referência */
VOID ObDereferenceObject(PVOID objeto);
const char *Port_Carimbo360(void);           /* port/x360/headless/passos.c */
int __real_main(int argc, char **argv);      /* src/main.c: âncora do .map */
void Port_Sair(int codigo);                  /* main360.cpp: âncora do .map */
extern unsigned gGsFrameJogo;                /* gs_core.c: avança quando o jogo entrega a lista do quadro */
}

/* KTHREAD (xkelib, kePrivateStructs.h) */
#define KT_PILHA_BASE 0x5Cu
#define KT_PILHA_LIMITE 0x60u
#define KT_PILHA_NUCLEO 0x64u /* KernelStack: o r1 salvo na troca de contexto */
#define KT_ESTADO 0x6Cu       /* State (byte) */
#define KT_BLOCOS 0xA8u       /* WaitBlockList */
#define KT_PRCB 0xC0u         /* CurrentPrcb; KPRCB.CurrentThread em +0 */
#define KT_SEMAFORO 0xFCu     /* SuspendSemaphore */
#define KT_ID 0x14Cu          /* ThreadId */
#define BLOCO_OBJETO 0x0Cu    /* KWAIT_BLOCK.Object */

#define TETO_US 500u          /* espera pela parada numa amostra */
#define FOLGA_TIQUES 250u     /* ~5 us depois de vista a parada: a troca de contexto termina de gravar o r1 */
#define NORMAIS_PARA_DESPEJO 120u
#define DESPEJO_PERIODO_MS (5u * 60u * 1000u)

struct Alvo {
    HANDLE h;
    DWORD id;
    unsigned kt, limite, base;
};

static void *sMemoria;
static AmostraImagem sImagem;
static unsigned sLigador;             /* TimeDateStamp do cabeçalho PE na memória (o "Timestamp is" do .map) */
static unsigned sIntervaloUs, sEstadoParada = 5;
static unsigned __int64 sTiquesPorUs = 50;
static char sAutoteste[160] = "não rodou";
static Alvo sJogo;
static HANDLE sThread;
static volatile LONG sParar;
static unsigned __int64 sSuspensaTiques, sSuspensaPior;
static unsigned sSuspensaN, sNaoParou, sFalhaSuspender;

static int Ler(unsigned e, unsigned *v) {
    if ((e & 3) != 0 || e < 0x10000u) {
        return 0;
    }
    __try {
        *v = *(volatile unsigned *)e;
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static int LerByte(unsigned e, unsigned *v) {
    unsigned w;
    if (!Ler(e & ~3u, &w)) {
        return 0;
    }
    *v = (w >> (8 * (3 - (e & 3)))) & 0xFF; /* big-endian */
    return 1;
}

static unsigned Le32(unsigned e) { /* o cabeçalho PE é little-endian */
    unsigned w = 0;
    if ((e & 3) == 0) {
        Ler(e, &w);
        return _byteswap_ulong(w);
    }
    return 0;
}

static unsigned Le16(unsigned e) {
    unsigned w = 0;
    Ler(e & ~3u, &w);
    w = (e & 2) ? (w & 0xFFFF) : (w >> 16);
    return ((w & 0xFF) << 8) | (w >> 8);
}

/* O código e o .pdata da imagem, pelo cabeçalho PE que o carregador deixa na base. Sem ele: o código pela faixa das
   imagens de título e nenhum .pdata (a ferramenta resolve os endereços crus pelo .map do mesmo jeito). */
static void Imagem(void) {
    unsigned modulo = (unsigned)(size_t)GetModuleHandle(NULL), base = modulo, pe, opt, nSec, i, sec, va, tam, car,
             anterior = 0;
    sImagem.textoIni = 0x82000000u;
    sImagem.textoFim = 0x84000000u;
    if (base < 0x10000u || Le16(base) != 0x5A4D) {
        base = 0x82000000u; /* (o GetModuleHandle do 360 pode dar a entrada do carregador; a base do .map) */
    }
    if (Le16(base) != 0x5A4D) {
        Diario_Escrever("amostra: sem o cabeçalho MZ (módulo %08X, base %08X): código pela faixa %08X-%08X, sem .pdata",
                        modulo, base, sImagem.textoIni, sImagem.textoFim);
        return;
    }
    pe = base + Le32(base + 0x3C);
    if (Le32(pe) != 0x4550 || Le16(pe + 24) != 0x10B) {
        Diario_Escrever("amostra: sem o cabeçalho PE em %08X", pe);
        return;
    }
    sLigador = Le32(pe + 8);
    nSec = Le16(pe + 6);
    opt = pe + 24;
    sec = opt + Le16(pe + 20);
    for (i = 0; i < nSec && i < 32; i++, sec += 40) {
        car = Le32(sec + 36);
        if (car & 0x20000000u) { /* IMAGE_SCN_MEM_EXECUTE */
            va = base + Le32(sec + 12);
            tam = Le32(sec + 8);
            sImagem.textoIni = va;
            sImagem.textoFim = va + tam;
            break;
        }
    }
    va = Le32(opt + 96 + 3 * 8); /* IMAGE_DIRECTORY_ENTRY_EXCEPTION */
    tam = Le32(opt + 96 + 3 * 8 + 4);
    if (va != 0 && tam >= 8) {
        unsigned *p = (unsigned *)(size_t)(base + va), n = tam / 8, ok = 1, w;
        for (i = 0; i < n && ok; i++) {
            ok = Ler((unsigned)(size_t)&p[2 * i], &w) && w >= anterior;
            anterior = w;
        }
        if (ok) {
            sImagem.pdata = p;
            sImagem.nPdata = n;
        }
    }
    Diario_Escrever("amostra: imagem em %08X, ligador %08X, código %08X-%08X, .pdata com %u funções", base, sLigador,
                    sImagem.textoIni, sImagem.textoFim, sImagem.nPdata);
}

static int Alvo_Abrir(Alvo *a, HANDLE h, DWORD id) {
    PVOID kt = NULL;
    unsigned idLido = 0;
    a->h = h;
    a->id = id;
    a->kt = 0;
    if (ObLookupThreadByThreadId(id, &kt) < 0 || kt == NULL) {
        Diario_Escrever("amostra: ObLookupThreadByThreadId(%u) falhou", (unsigned)id);
        return 0;
    }
    a->kt = (unsigned)(size_t)kt;
    if (!Ler(a->kt + KT_ID, &idLido) || !Ler(a->kt + KT_PILHA_BASE, &a->base) || !Ler(a->kt + KT_PILHA_LIMITE, &a->limite) ||
        idLido != id || a->limite >= a->base) {
        Diario_Escrever("amostra: o KTHREAD %08X não confere: ThreadId %u (esperado %u), pilha %08X-%08X", a->kt, idLido,
                        (unsigned)id, a->limite, a->base);
        return 0;
    }
    return 1;
}

static void Alvo_Fechar(Alvo *a) {
    if (a->kt != 0) {
        ObDereferenceObject((PVOID)(size_t)a->kt);
        a->kt = 0;
    }
}

/* Parada de fato: esperando o próprio semáforo de suspensão, fora do processador, por FOLGA_TIQUES seguidos. */
static int Parou(const Alvo *a) {
    unsigned estado, bloco, objeto, prcb, atual;
    return LerByte(a->kt + KT_ESTADO, &estado) && estado == sEstadoParada && Ler(a->kt + KT_BLOCOS, &bloco) &&
           Ler(bloco + BLOCO_OBJETO, &objeto) && objeto == a->kt + KT_SEMAFORO && Ler(a->kt + KT_PRCB, &prcb) &&
           Ler(prcb, &atual) && atual != a->kt;
}

static int EsperarParada(const Alvo *a, unsigned tetoUs, unsigned *ks) {
    unsigned __int64 t0 = __mftb(), visto = 0, agora;
    for (;;) {
        agora = __mftb();
        if (Parou(a)) {
            if (visto == 0) {
                visto = agora;
            } else if (agora - visto >= FOLGA_TIQUES) {
                return Ler(a->kt + KT_PILHA_NUCLEO, ks);
            }
        } else {
            visto = 0;
        }
        if (agora - t0 > tetoUs * sTiquesPorUs) {
            return 0;
        }
    }
}

/* Uma amostra. Entre o SuspendThread e o ResumeThread, só Ler. -1: o SuspendThread falhou (a thread acabou). */
static int Amostrar(const Alvo *a, AmostraCrua *c) {
    unsigned __int64 t0 = __mftb(), dt;
    unsigned ks = 0;
    int parou;
    if (SuspendThread(a->h) == (DWORD)-1) {
        sFalhaSuspender++;
        return -1;
    }
    parou = EsperarParada(a, TETO_US, &ks);
    if (parou) {
        AmostraHist_LerPilha(&sImagem, Ler, ks, a->limite, a->base, c);
    } else {
        c->tipo = AMOSTRA_NADA;
        c->nRet = 0;
    }
    ResumeThread(a->h);
    dt = __mftb() - t0;
    sSuspensaTiques += dt;
    sSuspensaN++;
    if (dt > sSuspensaPior) {
        sSuspensaPior = dt;
    }
    if (!parou) {
        sNaoParou++;
    }
    return parou;
}

/* ---- o autoteste: duas iscas antes do jogo -------------------------------------------------------------------- */

static volatile LONG sIscaParar;
static volatile unsigned sIscaGiros, sIscaPilha;

/* (não pode virar inline: o Iar tem de cair nela) */
static __declspec(noinline) void IscaGira(void) {
    while (!sIscaParar) {
        sIscaGiros++;
    }
}

static DWORD WINAPI IscaGiratoria(LPVOID arg) {
    volatile unsigned local = 0;
    (void)arg;
    sIscaPilha = (unsigned)(size_t)&local;
    IscaGira();
    return local;
}

static DWORD WINAPI IscaDorminhoca(LPVOID arg) {
    volatile unsigned local = 0;
    (void)arg;
    sIscaPilha = (unsigned)(size_t)&local;
    while (!sIscaParar) {
        Sleep(1);
        sIscaGiros++;
    }
    return local;
}

static void Despejar_Palavras(unsigned de, unsigned n) {
    unsigned i, j, w[8];
    for (i = 0; i < n; i += 8) {
        for (j = 0; j < 8; j++) {
            if (!Ler(de + 4 * (i + j), &w[j])) {
                w[j] = 0xDEADDEAD;
            }
        }
        Diario_Escrever("amostra:   %08X: %08X %08X %08X %08X %08X %08X %08X %08X", de + 4 * i, w[0], w[1], w[2], w[3],
                        w[4], w[5], w[6], w[7]);
    }
}

/* Uma isca: 1 se passou. giratoria: a folha tem de cair em IscaGira (uma folha sem quadro nem .pdata, 0x38 bytes nesta
   build) e o chamador ser IscaGiratoria, pelo Lr; senão, IscaDorminhoca tem de estar na pilha. Na primeira amostra, o que o KTHREAD mostra, e as palavras da pilha se a busca não achou nada. */
static int Isca(int giratoria, unsigned amostras, unsigned *aprendido) {
    HANDLE h;
    DWORD id = 0, ini = GetTickCount();
    Alvo a;
    AmostraCrua c;
    AmostraUma u;
    unsigned i, j, boas = 0, tipos[AMOSTRA_TIPOS] = {0, 0, 0, 0}, estadoAndando = 0xFF, estado = 0xFF, bloco = 0, objeto = 0,
                   prcb = 0, atual = 0, ks = 0, presa = 0;
    unsigned funcIsca = AmostraHist_Funcao(&sImagem, (unsigned)(size_t)(giratoria ? (void *)IscaGira : (void *)IscaDorminhoca));
    unsigned funcCham = AmostraHist_Funcao(&sImagem, (unsigned)(size_t)IscaGiratoria);
    const char *nome = giratoria ? "girando" : "dormindo";
    sIscaParar = 0;
    sIscaGiros = 0;
    sIscaPilha = 0;
    h = CreateThread(NULL, 64 * 1024, giratoria ? IscaGiratoria : IscaDorminhoca, NULL, CREATE_SUSPENDED, &id);
    if (h == NULL) {
        return 0;
    }
    XSetThreadProcessor(h, 2); /* a do jogo, que ainda não existe */
    ResumeThread(h);
    while (sIscaGiros < 3 && GetTickCount() - ini < 1000) {
        Sleep(1);
    }
    if (!Alvo_Abrir(&a, h, id)) {
        InterlockedExchange(&sIscaParar, 1);
        WaitForSingleObject(h, 1000);
        CloseHandle(h);
        return 0;
    }
    if (sIscaPilha < a.limite || sIscaPilha >= a.base) {
        Diario_Escrever("amostra: isca %s: a pilha do KTHREAD (%08X-%08X) não contém a variável local (%08X)", nome, a.limite,
                        a.base, sIscaPilha);
    }
    /* o estado andando, e o da parada (depois de um Sleep, sem dúvida já parada) */
    if (giratoria) {
        LerByte(a.kt + KT_ESTADO, &estadoAndando);
    }
    if (SuspendThread(h) != (DWORD)-1) {
        Sleep(5);
        LerByte(a.kt + KT_ESTADO, &estado);
        Ler(a.kt + KT_BLOCOS, &bloco);
        Ler(bloco + BLOCO_OBJETO, &objeto);
        Ler(a.kt + KT_PRCB, &prcb);
        Ler(prcb, &atual);
        Ler(a.kt + KT_PILHA_NUCLEO, &ks);
        AmostraHist_LerPilha(&sImagem, Ler, ks, a.limite, a.base, &c);
        Diario_Escrever("amostra: isca %s, parada: KTHREAD %08X, estado %u (andando %u), bloco %08X objeto %08X (semáforo "
                        "%08X), processador com %08X; r1 salvo %08X, pilha %08X-%08X; achou tipo %u em %08X: Iar %08X Lr "
                        "%08X r1 %08X, %u retornos",
                        nome, a.kt, estado, estadoAndando, bloco, objeto, a.kt + KT_SEMAFORO, atual, ks, a.limite, a.base,
                        c.tipo, c.armadilha, c.iar, c.lr, c.r1, c.nRet);
        if (c.tipo == AMOSTRA_NADA || c.tipo == AMOSTRA_VARREDURA || (giratoria && c.tipo != AMOSTRA_ARMADILHA)) {
            Diario_Escrever("amostra:   a pilha a partir do r1 salvo (até 0x600 bytes):");
            Despejar_Palavras(ks, 0x180);
        }
        for (j = 0; j < c.nRet && j < 8; j++) {
            Diario_Escrever("amostra:   retorno %u: %08X", j, c.ret[j]);
        }
        if (giratoria && estado != estadoAndando && objeto == a.kt + KT_SEMAFORO) {
            *aprendido = estado;
        }
        ResumeThread(h);
    }
    for (i = 0; i < amostras; i++) {
        int r = Amostrar(&a, &c);
        if (r < 0) {
            break;
        }
        if (r == 0) {
            presa++;
        }
        AmostraHist_Resumir(&sImagem, &c, &u);
        tipos[u.tipo]++;
        if (giratoria) {
            if (u.folha - funcIsca < 0x40 && u.chamador == funcCham) { /* (a isca é folha sem .pdata: o Iar fica cru) */
                boas++;
            }
        } else {
            for (j = 0; j < u.n; j++) {
                if (u.funcs[j] == funcIsca) {
                    boas++;
                    break;
                }
            }
        }
        Sleep(1);
    }
    InterlockedExchange(&sIscaParar, 1);
    WaitForSingleObject(h, 1000);
    CloseHandle(h);
    Alvo_Fechar(&a);
    Diario_Escrever("amostra: isca %s: %u de %u amostras conferem (armadilha %u, varredura %u, sem Iar %u, nada %u; "
                    "sem parar %u); isca %08X, chamador %08X",
                    nome, boas, amostras, tipos[AMOSTRA_ARMADILHA], tipos[AMOSTRA_VARREDURA], tipos[AMOSTRA_SEM_IAR],
                    tipos[AMOSTRA_NADA], presa, funcIsca, funcCham);
    return boas * 10 >= amostras * 9;
}

static unsigned Num(const char *v, unsigned padrao) {
    return v != NULL && *v != 0 ? (unsigned)strtoul(v, NULL, 10) : padrao;
}

extern "C" void Amostra_Preparar(void) {
    LARGE_INTEGER f;
    unsigned limiar, aprendido = 0, tam;
    int gira, dorme;
#pragma warning(suppress : 4996)
    if (getenv("BT3_AMOSTRA") == NULL) {
        return;
    }
    /* (o getenv é o de posix360.c, que lê o environment.txt; sem BT3_LOG=1 as três dão NULL: diagnostico360.h) */
#pragma warning(suppress : 4996)
    sIntervaloUs = Num(getenv("BT3_AMOSTRA"), 1000);
    if (sIntervaloUs < 100) {
        sIntervaloUs = 100;
    }
#pragma warning(suppress : 4996)
    limiar = Num(getenv("BT3_PICOS"), 40);
#pragma warning(suppress : 4996)
    limiar = Num(getenv("BT3_AMOSTRA_LIMIAR"), limiar);
    if (limiar == 0) {
        limiar = 40;
    }
    QueryPerformanceFrequency(&f); /* (o mftb: 49,875 MHz) */
    sTiquesPorUs = (unsigned __int64)f.QuadPart / 1000000u;
    if (sTiquesPorUs == 0) {
        sTiquesPorUs = 1;
    }
    tam = AmostraHist_Tamanho();
    sMemoria = VirtualAlloc(NULL, tam, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (sMemoria == NULL || !AmostraHist_Iniciar(sMemoria, tam, limiar, 400)) {
        Diario_Escrever("amostra: sem memória (%u KB): desligado", tam / 1024);
        sMemoria = NULL;
        return;
    }
    Diario_Escrever("amostra: BT3_AMOSTRA=%u us, limiar %u ms, %u KB; o autoteste com duas iscas na thread de hardware 2",
                    sIntervaloUs, limiar, tam / 1024);
    Imagem();
    gira = Isca(1, 100, &aprendido);
    if (aprendido != 0 && aprendido != sEstadoParada) {
        Diario_Escrever("amostra: o estado da parada visto é %u, não %u: fica o visto, e a isca de novo", aprendido,
                        sEstadoParada);
        sEstadoParada = aprendido;
        sSuspensaTiques = sSuspensaPior = 0;
        sSuspensaN = sNaoParou = 0;
        gira = Isca(1, 100, &aprendido);
    }
    dorme = Isca(0, 50, &aprendido);
    _snprintf(sAutoteste, sizeof(sAutoteste) - 1, "%s (girando %s, dormindo %s)", gira && dorme ? "passou" : "falhou",
              gira ? "ok" : "falhou", dorme ? "ok" : "falhou");
    if (sNaoParou * 10 > sSuspensaN) {
        Diario_Escrever("amostra: o autoteste não soube quando a isca parou em %u de %u amostras: desligado", sNaoParou,
                        sSuspensaN);
        VirtualFree(sMemoria, 0, MEM_RELEASE);
        sMemoria = NULL;
        return;
    }
    Diario_Escrever("amostra: autoteste %s; suspensa em média %.1f us, pior %.1f us", sAutoteste,
                    sSuspensaN ? (double)sSuspensaTiques / sSuspensaN / sTiquesPorUs : 0.0,
                    (double)sSuspensaPior / sTiquesPorUs);
    sSuspensaTiques = sSuspensaPior = 0;
    sSuspensaN = sNaoParou = 0;
}

static void Cabecalho(char *b, unsigned n) {
    _snprintf(b, n - 1,
              "carimbo %s; ligador %08X; âncoras __real_main=%08X Port_Sair=%08X Diario_Escrever=%08X "
              "Amostra_Comecar=%08X; código %08X-%08X; .pdata %u; intervalo %u us; autoteste %s; suspensa em média "
              "%.1f us, pior %.1f us, em %u amostras; sem parar %u",
              Port_Carimbo360(), sLigador, (unsigned)(size_t)__real_main, (unsigned)(size_t)Port_Sair,
              (unsigned)(size_t)Diario_Escrever, (unsigned)(size_t)Amostra_Comecar, sImagem.textoIni, sImagem.textoFim,
              sImagem.nPdata, sIntervaloUs, sAutoteste,
              sSuspensaN ? (double)sSuspensaTiques / sSuspensaN / sTiquesPorUs : 0.0, (double)sSuspensaPior / sTiquesPorUs,
              sSuspensaN, sNaoParou);
    b[n - 1] = 0;
}

static void Despejar(const char *motivo) {
    static char cabecalho[640];
    Cabecalho(cabecalho, sizeof(cabecalho));
    AmostraHist_Despejar(motivo, cabecalho);
}

static void Esperar(unsigned us) {
    unsigned __int64 fim;
    if (us >= 1000) {
        Sleep(us / 1000);
        return;
    }
    fim = __mftb() + us * sTiquesPorUs;
    while (__mftb() < fim) {
        YieldProcessor();
    }
}

#define TRAVA_MS 3000 /* um quadro parado há tanto tempo é uma trava: despeja sem esperar o fim dele */
static DWORD WINAPI Amostrador(LPVOID arg) {
    LARGE_INTEGER t, f, tQuadro, tDespejo;
    unsigned quadro, quadroAnt, normaisSeguidos = 0, lentosAntes = 0, lentos;
    unsigned travas = 0; /* despejos já feitos neste quadro parado */
    AmostraCrua c;
    AmostraUma u;
    (void)arg;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&tQuadro);
    tDespejo = tQuadro;
    quadroAnt = *(volatile unsigned *)&gGsFrameJogo;
    while (!sParar) {
        Esperar(sIntervaloUs);
        QueryPerformanceCounter(&t);
        quadro = *(volatile unsigned *)&gGsFrameJogo;
        if (quadro != quadroAnt) {
            lentos = AmostraHist_Quadros(AMOSTRA_LENTO);
            AmostraHist_FimDoQuadro(quadro - quadroAnt, (double)(t.QuadPart - tQuadro.QuadPart) * 1000.0 / f.QuadPart);
            normaisSeguidos = AmostraHist_Quadros(AMOSTRA_LENTO) != lentos ? 0 : normaisSeguidos + (quadro - quadroAnt);
            quadroAnt = quadro;
            tQuadro = t;
            if (AmostraHist_Quadros(AMOSTRA_LENTO) != lentosAntes && normaisSeguidos >= NORMAIS_PARA_DESPEJO) {
                lentosAntes = AmostraHist_Quadros(AMOSTRA_LENTO);
                Despejar("fim de um trecho lento");
                QueryPerformanceCounter(&tDespejo);
            } else if ((t.QuadPart - tDespejo.QuadPart) * 1000 / f.QuadPart >= DESPEJO_PERIODO_MS) {
                Despejar("periódico");
                tDespejo = t;
            }
            travas = 0;
        } else if ((t.QuadPart - tQuadro.QuadPart) * 1000 / f.QuadPart >= TRAVA_MS && travas < 3) {
            /* (29/10) O quadro não acaba: a thread do jogo travou. Sem isto as amostras ficariam esperando o fim do quadro
               e nenhum despejo as mostraria (o Father-Son do Gohan SSJ2 congelou e o Lucas saiu pela Guide). Fecha o
               quadro como longo e despeja, três vezes no máximo por trava (a cada 3 s), para ver se o lugar muda. */
            travas++;
            AmostraHist_FimDoQuadro(0, (double)(t.QuadPart - tQuadro.QuadPart) * 1000.0 / f.QuadPart);
            Despejar("trava: o quadro do jogo não acaba");
            tDespejo = tQuadro = t; /* o resto do quadro conta a partir daqui, sem somar duas vezes */
        }
        if (WaitForSingleObject(sJogo.h, 0) == WAIT_OBJECT_0 || Amostrar(&sJogo, &c) < 0) {
            break; /* a thread do jogo acabou (uma queda, ou a saída sem passar por Port_Sair) */
        }
        AmostraHist_Resumir(&sImagem, &c, &u);
        AmostraHist_Guardar(&u);
    }
    Despejar(sParar ? "saída do jogo" : "a thread do jogo acabou");
    Alvo_Fechar(&sJogo);
    return 0;
}

extern "C" void Amostra_Comecar(HANDLE jogo, DWORD id) {
    if (sMemoria == NULL || jogo == NULL) {
        return;
    }
    if (!Alvo_Abrir(&sJogo, jogo, id)) {
        Diario_Escrever("amostra: sem o KTHREAD da thread do jogo: desligado");
        return;
    }
    sThread = CreateThread(NULL, 64 * 1024, Amostrador, NULL, CREATE_SUSPENDED, NULL);
    if (sThread == NULL) {
        Alvo_Fechar(&sJogo);
        return;
    }
    XSetThreadProcessor(sThread, 1);
    SetThreadPriority(sThread, THREAD_PRIORITY_HIGHEST);
    ResumeThread(sThread);
    Diario_Escrever("amostra: amostrando a thread do jogo (KTHREAD %08X, pilha %08X-%08X) a cada %u us, na thread de "
                    "hardware 1", sJogo.kt, sJogo.limite, sJogo.base, sIntervaloUs);
}

/* Da thread do jogo (Port_Sair): o amostrador pode suspendê-la enquanto ela espera aqui, e a retoma logo. */
extern "C" void Amostra_Parar(void) {
    if (sThread == NULL) {
        return;
    }
    InterlockedExchange(&sParar, 1);
    WaitForSingleObject(sThread, 3000);
    CloseHandle(sThread);
    sThread = NULL;
}
