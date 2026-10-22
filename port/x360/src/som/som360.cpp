/*
 * F6, etapa 3: a camada de áudio do 360 sobre o XAudio2 (plano/F6/03.md, decisões 25.2 e 25.3).
 *
 * Uma voz de fonte só, PCM s16 de 48 kHz estéreo na ordem nativa (a do console: o XDK manda trocar os bytes do PCM
 * de 16 bits que vem do Windows, "Audio Data and Endianness"), sobre a voz de saída padrão do console (5.1, 48 kHz).
 * O XAudio2 mora nas threads de hardware 4 e 5 (XAUDIO2_DEFAULT_PROCESSOR). Ao fim de cada buffer (OnBufferEnd) o
 * retorno de lá só conta e acorda o misturador (thread própria na 5), que pede ao núcleo portátil (som_nucleo.c) mais
 * um bloco (a soma dos fluxos do jogo e os retornos de chamada deles: o feed do snd_adx.c e o mix do snd_se.c, que só
 * leem memória) e o devolve à fila da voz. Nada de E/S nem de diário nas duas: o vigia (thread 1, baixa prioridade)
 * escreve os números no diário.
 *
 * A trava do núcleo é uma seção crítica recursiva; o misturador nunca espera por ela mais de TRAVA_MS (dá silêncio e
 * conta), e uma falha dentro da mistura (o código do jogo roda ali) é pega por __try: a mistura passa a dar silêncio,
 * a trava é devolvida e o vigia escreve onde foi (Iar, Lr), sem derrubar o jogo.
 *
 * Chaves do ambiente.txt (o getenv do posix360.c):
 *   BT3_SOM=1          o som do jogo: o SDL_InitSubSystem do snd_adx.c passa a abrir o dispositivo. Sem ela, o jogo
 *                      fica como antes da etapa 3. Só tem efeito com desenho (BT3_GS=gpu), como no PC
 *   BT3_SOM_TESTE=1    o tom de teste no começo, pelo mesmo caminho (fluxos do núcleo), em cinco fases
 *   BT3_SOM_TESTE_HZ   a frequência do tom (440)
 *   BT3_SOM_TESTE_S    a duração da primeira fase, em segundos (3; 600 para a prova de 10 min do escopo)
 */
#include <xtl.h>
#include <xaudio2.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "diario.h"
#include "som360.h"
#include "som_nucleo.h"

#pragma warning(disable : 4996) /* (o getenv é o de posix360.c, que lê o ambiente.txt) */

#define BLOCO 512   /* quadros por buffer: 10,7 ms */
#define NBUF 4      /* buffers na fila: até 42,7 ms de folga para o retorno atrasar */
#define TRAVA_MS 20 /* o máximo que o misturador espera pela trava do núcleo (a fila tem 32 ms além do buffer que toca) */

static short sBuf[NBUF][BLOCO * 2];
static IXAudio2 *sX;
static IXAudio2MasteringVoice *sSaida;
static IXAudio2SourceVoice *sVoz;
static CRITICAL_SECTION sTrava;
static volatile LONG sTravaPronta, sLigado, sJogo, sMorto;
static volatile int sProf;          /* a profundidade da trava (só muda com ela pega) */
static HANDLE sVigia, sPare;
static LARGE_INTEGER sFreq;

static HANDLE sEvento, sMisturador; /* o retorno do XAudio2 acorda o misturador */
static volatile LONG sLivres, sFimMisturador;
static int sProx;                    /* o próximo buffer a encher (só o misturador, depois do começo) */

/* contadores (o retorno e o misturador escrevem; o vigia lê) */
static volatile DWORD sThreadXa;    /* a thread do retorno do XAudio2 (a última vista) */
static volatile unsigned sBlocos, sFaltas, sFilaMin = NBUF, sPiorUs, sTravasPerdidas, sSilencios;
static volatile HRESULT sErroVoz, sErroMotor;
/* a falha pega na mistura */
static volatile unsigned sQuedaCodigo, sQuedaIar, sQuedaLr, sQuedaEnd, sQuedaVista;

/* ------------------------------------------------------------------------------------------- a trava do núcleo */

extern "C" void som_travar(void) {
    EnterCriticalSection(&sTrava);
    sProf++;
}

extern "C" void som_destravar(void) {
    sProf--;
    LeaveCriticalSection(&sTrava);
}

/* --------------------------------------------------------------------------------------- o dispositivo da SDL */

extern "C" const char *SDL_GetError(void) {
    if (!sJogo) {
        return "som do jogo desligado (BT3_SOM=1 no ambiente.txt liga)";
    }
    if (!sLigado) {
        return "o XAudio2 não abriu (ver o diário)";
    }
    return SomNucleo_Erro();
}

extern "C" int SDL_InitSubSystem(unsigned flags) {
    (void)flags;
    return sJogo && sLigado;
}

extern "C" const char *SDL_GetCurrentAudioDriver(void) {
    return sLigado ? "xaudio2 (360, 48 kHz estéreo)" : NULL;
}

extern "C" SDL_AudioDeviceID SDL_OpenAudioDevice(SDL_AudioDeviceID d, const SDL_AudioSpec *s) {
    (void)d;
    (void)s;
    if (!(sJogo && sLigado)) {
        return 0;
    }
    Diario_Escrever("som: o jogo abriu o dispositivo");
    return 1;
}

extern "C" int SDL_ResumeAudioDevice(SDL_AudioDeviceID d) {
    return d != 0;
}

/* ------------------------------------------------------------------------------------ o retorno do XAudio2 */

static unsigned Microssegundos(LONGLONG t) {
    return sFreq.QuadPart > 0 ? (unsigned)(t * 1000000 / sFreq.QuadPart) : 0;
}

static int FiltroQueda(LPEXCEPTION_POINTERS p) {
    if (p != NULL && p->ExceptionRecord != NULL && p->ContextRecord != NULL) {
        sQuedaCodigo = (unsigned)p->ExceptionRecord->ExceptionCode;
        sQuedaIar = (unsigned)p->ContextRecord->Iar;
        sQuedaLr = (unsigned)p->ContextRecord->Lr;
        sQuedaEnd = p->ExceptionRecord->NumberParameters >= 2 ? (unsigned)p->ExceptionRecord->ExceptionInformation[1] : 0;
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

/* a mistura com a trava pega, protegida: devolve 0 se caiu */
static int MisturarProtegido(short *saida) {
    volatile int pegou = 0; /* (lido no __except) */
    __try {
        som_travar();
        pegou = 1;
        SomNucleo_Misturar(saida, BLOCO);
        som_destravar();
        pegou = 0;
    } __except (FiltroQueda(GetExceptionInformation())) {
        /* a trava inteira de volta (o retorno do jogo pode ter caído com ela pega mais de uma vez) */
        while (pegou && sProf > 0) {
            som_destravar();
        }
        InterlockedExchange(&sMorto, 1);
        return 0;
    }
    return 1;
}

/* o buffer i: a mistura (ou silêncio) e de volta à fila da voz */
static void Encher(int i) {
    XAUDIO2_BUFFER b;
    LARGE_INTEGER t0, t1;
    unsigned us;

    QueryPerformanceCounter(&t0);
    if (sMorto) {
        memset(sBuf[i], 0, sizeof(sBuf[i]));
        sSilencios++;
    } else {
        int pegou = 0;
        for (;;) { /* (o jogo pode ter caído com a trava pega: nunca esperar para sempre) */
            LARGE_INTEGER t;
            if (TryEnterCriticalSection(&sTrava)) {
                pegou = 1;
                break;
            }
            QueryPerformanceCounter(&t);
            if (sMorto || Microssegundos(t.QuadPart - t0.QuadPart) > TRAVA_MS * 1000) {
                break;
            }
            YieldProcessor();
        }
        if (!pegou) {
            memset(sBuf[i], 0, sizeof(sBuf[i]));
            sTravasPerdidas++;
        } else {
            int ok = MisturarProtegido(sBuf[i]);
            LeaveCriticalSection(&sTrava); /* (a do TryEnter; a recursiva da mistura já voltou) */
            if (!ok) {
                memset(sBuf[i], 0, sizeof(sBuf[i]));
            }
        }
    }
    QueryPerformanceCounter(&t1);
    us = Microssegundos(t1.QuadPart - t0.QuadPart);
    if (us > sPiorUs) {
        sPiorUs = us;
    }
    ZeroMemory(&b, sizeof(b));
    b.AudioBytes = sizeof(sBuf[i]);
    b.pAudioData = (const BYTE *)sBuf[i];
    sVoz->SubmitSourceBuffer(&b);
    sBlocos++;
}

/* O retorno do XAudio2 (threads 4 e 5) só conta e acorda o misturador: a documentação pede que ele volte em menos de
   1 ms, sem esperar por trava do programa nem gastar CPU ("XAudio2 Callbacks"). Os buffers voltam na ordem em que
   entraram, então o misturador enche o próximo da roda. */
class RetornoVoz : public IXAudio2VoiceCallback {
public:
    STDMETHOD_(void, OnVoiceProcessingPassStart)(UINT32) {}
    STDMETHOD_(void, OnVoiceProcessingPassEnd)() {}
    STDMETHOD_(void, OnStreamEnd)() {}
    STDMETHOD_(void, OnBufferStart)(void *) {}
    STDMETHOD_(void, OnBufferEnd)(void *) {
        XAUDIO2_VOICE_STATE st;
        sThreadXa = GetCurrentThreadId();
        sVoz->GetState(&st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (st.BuffersQueued == 0) {
            sFaltas++; /* a voz ficou sem nada: um buraco no som */
        }
        if (st.BuffersQueued < sFilaMin) {
            sFilaMin = st.BuffersQueued;
        }
        InterlockedIncrement(&sLivres);
        SetEvent(sEvento);
    }
    STDMETHOD_(void, OnLoopEnd)(void *) {}
    STDMETHOD_(void, OnVoiceError)(void *, HRESULT h) { sErroVoz = h; }
};

/* O misturador: thread própria na thread de hardware 5 (uma das duas do XAudio2), prioridade alta, sem E/S. Enche cada
   buffer que voltou e o devolve. */
static DWORD WINAPI Misturador(LPVOID arg) {
    (void)arg;
    _controlfp(_RC_NEAR, _MCW_RC); /* (o arredondamento normal; o truncado é só da thread do jogo) */
    while (!sFimMisturador) {
        WaitForSingleObject(sEvento, 100);
        while (sLivres > 0 && !sFimMisturador) {
            Encher(sProx);
            sProx = (sProx + 1) % NBUF;
            InterlockedDecrement(&sLivres);
        }
    }
    return 0;
}

class RetornoMotor : public IXAudio2EngineCallback {
public:
    STDMETHOD_(void, OnProcessingPassStart)() {}
    STDMETHOD_(void, OnProcessingPassEnd)() {}
    STDMETHOD_(void, OnCriticalError)(HRESULT h) { sErroMotor = h; }
};

static RetornoVoz sRetVoz;
static RetornoMotor sRetMotor;

/* ---------------------------------------------------------------------------------- o vigia e o tom de teste */

typedef struct Tom {
    double fase, passo;
    int canais, lado; /* lado: 0 os dois, 1 só o esquerdo, 2 só o direito */
} Tom;

static void tom_retorno(void *user, SDL_AudioStream *s, int additional, int total) {
    static short buf[256 * 2];
    Tom *t = (Tom *)user;
    int tam = t->canais * 2, quadros = (additional + tam - 1) / tam;
    (void)total;
    while (quadros > 0) {
        int n = quadros > 256 ? 256 : quadros;
        for (int k = 0; k < n; k++) {
            short v = (short)(6554.0 * sin(t->fase)); /* 0,2 da escala: -14 dBFS */
            t->fase += t->passo;
            if (t->fase >= 6.283185307179586) {
                t->fase -= 6.283185307179586;
            }
            if (t->canais == 1) {
                buf[k] = v;
            } else {
                buf[k * 2] = t->lado == 2 ? 0 : v;
                buf[k * 2 + 1] = t->lado == 1 ? 0 : v;
            }
        }
        SDL_PutAudioStreamData(s, buf, n * tam);
        quadros -= n;
    }
}

static void Contadores(const char *quando) {
    SomContadores c;
    XAUDIO2_VOICE_STATE st;
    SomNucleo_Contadores(&c);
    st.BuffersQueued = 0;
    if (sVoz != NULL) {
        sVoz->GetState(&st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    }
    Diario_Escrever("som (%s): blocos %u, BuffersQueued %u (mínimo %u), faltas %u, pior bloco %u us, travas perdidas %u, "
                    "silêncios %u; núcleo: fluxos %u (ligados %u), retornos %u, quadros curtos %u, saturados %u; erros: "
                    "voz 0x%08X, motor 0x%08X",
                    quando, sBlocos, (unsigned)st.BuffersQueued, sFilaMin, sFaltas, sPiorUs, sTravasPerdidas, sSilencios,
                    c.fluxos, c.ligados, c.retornos, c.curtos, c.saturados, (unsigned)sErroVoz, (unsigned)sErroMotor);
    if (sQuedaCodigo != 0 && !sQuedaVista) {
        sQuedaVista = 1;
        Diario_Escrever("som: CRASH na mistura (thread do XAudio2) code=0x%08X Iar=0x%08X Lr=0x%08X endereço=0x%08X; o som "
                        "fica mudo e o jogo segue",
                        sQuedaCodigo, sQuedaIar, sQuedaLr, sQuedaEnd);
    }
}

/* espera ms ou o pedido de parar (1) */
static int Esperar(DWORD ms) {
    return WaitForSingleObject(sPare, ms) == WAIT_OBJECT_0;
}

static int NumeroDoAmbiente(const char *v, int padrao, int min, int max) {
    int n;
    if (v == NULL || v[0] == '\0') {
        return padrao;
    }
    n = atoi(v);
    return n < min ? min : n > max ? max : n;
}

static int TocarTom(void) {
    struct Fase {
        int taxa, canais, lado;
        float ganho;
        int ms;
        const char *nome;
    } fases[] = {
        {48000, 2, 0, 1.0f, 3000, "48 kHz estéreo, os dois lados"},
        {24000, 1, 0, 1.0f, 3000, "24 kHz mono (conversão de taxa e de canais): a mesma altura da fase 1"},
        {48000, 2, 1, 1.0f, 2000, "só o lado esquerdo"},
        {48000, 2, 2, 1.0f, 2000, "só o lado direito"},
        {48000, 2, 0, 0.25f, 2000, "os dois lados com ganho 0,25 (-12 dB): mais baixo"},
    };
    int hz = NumeroDoAmbiente(getenv("BT3_SOM_TESTE_HZ"), 440, 20, 20000);
    fases[0].ms = 1000 * NumeroDoAmbiente(getenv("BT3_SOM_TESTE_S"), 3, 1, 3600);
    Diario_Escrever("som: tom de teste de %d Hz, %d fases", hz, (int)(sizeof(fases) / sizeof(fases[0])));
    for (int f = 0; f < (int)(sizeof(fases) / sizeof(fases[0])); f++) {
        Tom t;
        SDL_AudioSpec e;
        SDL_AudioStream *s;
        e.format = SOM_AUDIO_S16;
        e.channels = fases[f].canais;
        e.freq = fases[f].taxa;
        t.fase = 0.0;
        t.passo = 6.283185307179586 * hz / fases[f].taxa;
        t.canais = fases[f].canais;
        t.lado = fases[f].lado;
        s = SDL_CreateAudioStream(&e, NULL);
        if (s == NULL) {
            Diario_Escrever("som: tom, fase %d: o fluxo não abriu (%s)", f + 1, SomNucleo_Erro());
            return 0;
        }
        SDL_SetAudioStreamGain(s, fases[f].ganho);
        SDL_SetAudioStreamGetCallback(s, tom_retorno, &t);
        SDL_BindAudioStream(1, s);
        Diario_Escrever("som: tom, fase %d de %d (%d ms): %s", f + 1, (int)(sizeof(fases) / sizeof(fases[0])), fases[f].ms,
                        fases[f].nome);
        int parou = 0;
        for (int ms = 0; ms < fases[f].ms && !parou; ms += 1000) {
            parou = Esperar(fases[f].ms - ms < 1000 ? fases[f].ms - ms : 1000);
            if (!parou && fases[f].ms > 5000) {
                Contadores("tom"); /* (a fase longa: uma linha por segundo) */
            }
        }
        SDL_DestroyAudioStream(s); /* (espera um retorno do tom em andamento: o Tom da pilha fica válido até aqui) */
        Contadores("fim da fase");
        if (parou || Esperar(500)) {
            return 0;
        }
    }
    Diario_Escrever("som: tom de teste acabou");
    return 1;
}

static DWORD WINAPI Vigia(LPVOID arg) {
    unsigned faltas = 0, travas = 0, segundos = 0;
    (void)arg;
    if (getenv("BT3_SOM_TESTE") != NULL && strcmp(getenv("BT3_SOM_TESTE"), "1") == 0) {
        if (!TocarTom()) {
            return 0;
        }
    }
    /* depois do tom: uma linha quando aparecer falta, trava perdida ou queda, e a cada 60 s com o som do jogo */
    while (!Esperar(1000)) {
        segundos++;
        if (sFaltas != faltas || sTravasPerdidas != travas || (sQuedaCodigo != 0 && !sQuedaVista) ||
            (sJogo && segundos % 60 == 0)) {
            faltas = sFaltas;
            travas = sTravasPerdidas;
            Contadores("vigia");
        }
    }
    return 0;
}

/* ----------------------------------------------------------------------------------------------- liga e desliga */

extern "C" void Som360_Iniciar(void) {
    const char *jogo = getenv("BT3_SOM"), *teste = getenv("BT3_SOM_TESTE");
    WAVEFORMATEX w;
    XAUDIO2_VOICE_DETAILS d;
    XAUDIO2_VOICE_STATE st;
    HRESULT hr;
    UINT32 flags;

    if (!InterlockedExchange(&sTravaPronta, 1)) {
        InitializeCriticalSection(&sTrava);
    }
    sJogo = jogo != NULL && strcmp(jogo, "1") == 0;
    if (!sJogo && !(teste != NULL && strcmp(teste, "1") == 0)) {
        Diario_Escrever("som: desligado (BT3_SOM=1 liga o som do jogo; BT3_SOM_TESTE=1, o tom de teste)");
        return;
    }
    QueryPerformanceFrequency(&sFreq);
    hr = XAudio2Create(&sX, 0, XAUDIO2_DEFAULT_PROCESSOR);
    Diario_Escrever("som: XAudio2Create 0x%08X (threads de hardware 4 e 5)", (unsigned)hr);
    if (FAILED(hr)) {
        sX = NULL;
        return;
    }
    sX->RegisterForCallbacks(&sRetMotor);
    hr = sX->CreateMasteringVoice(&sSaida, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, NULL);
    if (FAILED(hr)) {
        Diario_Escrever("som: CreateMasteringVoice 0x%08X", (unsigned)hr);
        sX->Release();
        sX = NULL;
        return;
    }
    sSaida->GetVoiceDetails(&d);
    Diario_Escrever("som: voz de saída com %u canais a %u Hz", (unsigned)d.InputChannels, (unsigned)d.InputSampleRate);
    ZeroMemory(&w, sizeof(w));
    w.wFormatTag = WAVE_FORMAT_PCM;
    w.nChannels = 2;
    w.nSamplesPerSec = SOM_TAXA;
    w.wBitsPerSample = 16;
    w.nBlockAlign = 4;
    w.nAvgBytesPerSec = SOM_TAXA * 4;
    /* sem conversão de taxa nem de altura no XAudio2 (a voz de saída também é de 48 kHz); se ele recusar, com elas */
    flags = d.InputSampleRate == SOM_TAXA ? XAUDIO2_VOICE_NOSRC | XAUDIO2_VOICE_NOPITCH : 0;
    hr = sX->CreateSourceVoice(&sVoz, &w, flags, 1.0f, &sRetVoz, NULL, NULL);
    if (FAILED(hr) && flags != 0) {
        Diario_Escrever("som: CreateSourceVoice com NOSRC|NOPITCH 0x%08X; de novo sem", (unsigned)hr);
        flags = 0;
        hr = sX->CreateSourceVoice(&sVoz, &w, flags, 1.0f, &sRetVoz, NULL, NULL);
    }
    Diario_Escrever("som: CreateSourceVoice 0x%08X (PCM s16 48 kHz estéreo, flags 0x%X), %d buffers de %d quadros (%d ms)",
                    (unsigned)hr, (unsigned)flags, NBUF, BLOCO, NBUF * BLOCO * 1000 / SOM_TAXA);
    if (FAILED(hr)) {
        sVoz = NULL;
        sSaida->DestroyVoice();
        sSaida = NULL;
        sX->Release();
        sX = NULL;
        return;
    }
    InterlockedExchange(&sLigado, 1);
    for (int i = 0; i < NBUF; i++) {
        Encher(i); /* (o começo: silêncio, nenhum fluxo ainda; o próximo a voltar é o 0) */
    }
    sEvento = CreateEvent(NULL, FALSE, FALSE, NULL);
    sMisturador = CreateThread(NULL, 64 * 1024, Misturador, NULL, CREATE_SUSPENDED, NULL);
    if (sMisturador != NULL) {
        XSetThreadProcessor(sMisturador, 5);
        SetThreadPriority(sMisturador, THREAD_PRIORITY_HIGHEST);
        ResumeThread(sMisturador);
    }
    hr = sVoz->Start(0);
    sVoz->GetState(&st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    Diario_Escrever("som: Start 0x%08X, BuffersQueued %u; o jogo %s o som", (unsigned)hr, (unsigned)st.BuffersQueued,
                    sJogo ? "abre" : "não abre (BT3_SOM=1 liga)");
    sPare = CreateEvent(NULL, TRUE, FALSE, NULL);
    sVigia = CreateThread(NULL, 64 * 1024, Vigia, NULL, CREATE_SUSPENDED, NULL);
    if (sVigia != NULL) {
        XSetThreadProcessor(sVigia, 1);
        SetThreadPriority(sVigia, THREAD_PRIORITY_BELOW_NORMAL);
        ResumeThread(sVigia);
    }
}

extern "C" void Som360_Parar(void) {
    if (!InterlockedExchange(&sLigado, 0)) {
        return;
    }
    if (sVigia != NULL) {
        SetEvent(sPare);
        if (WaitForSingleObject(sVigia, 2000) != WAIT_OBJECT_0) {
            Diario_Escrever("som: o vigia não parou em 2 s");
        }
        CloseHandle(sVigia);
        sVigia = NULL;
    }
    Contadores("saída");
    sX->StopEngine();
    if (sMisturador != NULL) {
        InterlockedExchange(&sFimMisturador, 1);
        SetEvent(sEvento);
        if (WaitForSingleObject(sMisturador, 2000) != WAIT_OBJECT_0) {
            Diario_Escrever("som: o misturador não parou em 2 s");
        }
        CloseHandle(sMisturador);
        sMisturador = NULL;
    }
    sVoz->DestroyVoice();
    sVoz = NULL;
    sSaida->DestroyVoice();
    sSaida = NULL;
    sX->Release();
    sX = NULL;
    Diario_Escrever("som: parado (StopEngine, vozes e motor soltos)");
}

extern "C" void Som360_Queda(void) {
    InterlockedExchange(&sMorto, 1);
    if (!sLigado || sX == NULL) {
        return;
    }
    if (GetCurrentThreadId() == sThreadXa) {
        Diario_Escrever("som: a queda foi na thread do XAudio2 (sem StopEngine: não vale de dentro do retorno)");
        return;
    }
    sX->StopEngine();
    Diario_Escrever("som: StopEngine na queda");
}
