/* Os controles do 360 pelo XInput, no lugar de port/src/gs/gs_input.c (F3, etapa 8). O jogo lê dois controles do
   PS2 (scePad2Read, port/src/plat_stub.c). O jogador 1 é o primeiro controle ligado, em qualquer quadrante do anel,
   e o jogador 2 o seguinte (decisão 21; o título não escolhe o quadrante, e o jogo espera o controle 1 no boot). Um
   jogador só troca de controle quando o dele some e há outro livre: sem controle, scePad2GetState deixa de dizer
   "pronto" e o jogo mostra o aviso de controle desconectado.

   Botões como na build de PC com controle (as posições do DualShock 2): A = X, B = círculo, X = quadrado,
   Y = triângulo, LB/RB = L1/R1, LT/RT = L2/R2, cliques dos analógicos = L3/R3, START = START, BACK = SELECT.

   A vibração do jogo vai para o controle do jogador pelo XInputSetState (F8, no fim deste arquivo). */
#include <xtl.h>
#include <string.h>
#include "diario.h"

/* os bits dos dois bytes de botões juntos (o byte 0 embaixo), como include/sys/pad.h os tem; 1 = apertado aqui */
enum {
    B_SELECT = 0x0001, B_L3 = 0x0002, B_R3 = 0x0004, B_START = 0x0008, B_UP = 0x0010, B_RIGHT = 0x0020,
    B_DOWN = 0x0040, B_LEFT = 0x0080, B_L2 = 0x0100, B_R2 = 0x0200, B_L1 = 0x0400, B_R1 = 0x0800,
    B_TRIANGLE = 0x1000, B_CIRCLE = 0x2000, B_CROSS = 0x4000, B_SQUARE = 0x8000
};

static const struct { WORD xinput; unsigned ps2; } kBotoes[] = {
    {XINPUT_GAMEPAD_DPAD_UP, B_UP}, {XINPUT_GAMEPAD_DPAD_DOWN, B_DOWN},
    {XINPUT_GAMEPAD_DPAD_LEFT, B_LEFT}, {XINPUT_GAMEPAD_DPAD_RIGHT, B_RIGHT},
    {XINPUT_GAMEPAD_A, B_CROSS}, {XINPUT_GAMEPAD_B, B_CIRCLE},
    {XINPUT_GAMEPAD_X, B_SQUARE}, {XINPUT_GAMEPAD_Y, B_TRIANGLE},
    {XINPUT_GAMEPAD_LEFT_SHOULDER, B_L1}, {XINPUT_GAMEPAD_RIGHT_SHOULDER, B_R1},
    {XINPUT_GAMEPAD_LEFT_THUMB, B_L3}, {XINPUT_GAMEPAD_RIGHT_THUMB, B_R3},
    {XINPUT_GAMEPAD_START, B_START}, {XINPUT_GAMEPAD_BACK, B_SELECT},
};

/* um eixo do XInput (-32768..32767, para cima positivo) como o PS2 o dá (0..255, 0x80 no centro, para cima 0) */
static unsigned char eixo(SHORT v, int vertical) {
    int x = (vertical ? -(int)v : v) + 32768; /* (o centro dá 0x80 nos dois eixos) */
    return (unsigned char)((x > 65535 ? 65535 : x) >> 8);
}

/* o gatilho conta como L2/R2 a partir de uns 37% do curso, como na build de PC (12000 de 32767, gs_input.c); o
   limiar do XInput (30 de 255) faz um dedo apoiado virar aperto */
#define GATILHO 93

/* O estado de cada controle, lido uma vez por quadro: o jogo (Pad_Update, src/sys/pad.c) pergunta se o controle está
   pronto (scePad2GetState) logo antes de lê-lo (scePad2Read), e as duas respostas vêm da mesma leitura. */
static XINPUT_STATE sEstado[2];
static int sLigado[2];
static int sQuadrante[2] = {-1, -1}; /* o controle de cada jogador (índice do XInput), -1 antes do primeiro */

/* zerado antes: com a Guide aberta, o hiddriver devolve sucesso sem preencher o estado dos controles que ele emula
   (~/Documentos/hiddriver/hiddriver/main.cpp, "HUD is open"); zerado, o controle fica parado */
static int ler(int quadrante, XINPUT_STATE *e) {
    ZeroMemory(e, sizeof(*e));
    return XInputGetState((DWORD)quadrante, e) == ERROR_SUCCESS;
}

/* Roteiro de controle (BT3_ROTEIRO, port/x360/headless/roteiro.c, F4 etapa 4): os dois jogadores vêm do roteiro e
   seus controles contam como ligados, como na build sem janela (scePad2GetState sempre "pronto"); o XInput não é
   lido. */
extern int Roteiro_Ativo(void);
extern int Roteiro_Pad(int socket, unsigned char *data);

int Port_PadConnected(int socket) {
    int q;

    if (socket < 0 || socket > 1) {
        return 0;
    }
    if (Roteiro_Ativo()) {
        return 1;
    }
    sLigado[socket] = 1;
    if (sQuadrante[socket] >= 0 && ler(sQuadrante[socket], &sEstado[socket])) {
        return 1;
    }
    /* o controle dele sumiu (ou ainda não tinha): o primeiro ligado que não é o do outro jogador, mesmo que o do
       outro esteja desligado agora (ele o pega de volta ao religar) */
    for (q = 0; q < XUSER_MAX_COUNT; q++) {
        if (q != sQuadrante[!socket] && q != sQuadrante[socket] && ler(q, &sEstado[socket])) {
            sQuadrante[socket] = q;
            Diario_Escrever("controle: jogador %d no quadrante %d", socket + 1, q + 1);
            return 1;
        }
    }
    ZeroMemory(&sEstado[socket], sizeof(sEstado[socket]));
    sLigado[socket] = 0;
    return 0;
}

/* 18 bytes no formato do scePad2Read (botões ativos em 0, analógicos direito e esquerdo, pressões zeradas como na
   build de PC); 0 sem controle naquela porta (plat_stub.c preenche o controle parado) */
extern void Passos_Leitura(void); /* (F5, etapa 3) a fronteira de passo da medida de tempo: port/x360/headless/passos.c */
extern void Roteiro_SomEstado(void); /* (F6, etapa 1) BT3_SOM_ESTADO: o estado do som no log (port/x360/headless/roteiro.c) */

int Port_PadRead(int socket, unsigned char *data) {
    const XINPUT_STATE *e;
    unsigned btn = 0, i;

    if (socket == 0) {
        Passos_Leitura();
        Roteiro_SomEstado();
    }
    if (Roteiro_Ativo()) {
        return Roteiro_Pad(socket, data);
    }
    if (socket < 0 || socket > 1 || !sLigado[socket]) {
        return 0;
    }
    e = &sEstado[socket];
    for (i = 0; i < sizeof(kBotoes) / sizeof(kBotoes[0]); i++) {
        if (e->Gamepad.wButtons & kBotoes[i].xinput) {
            btn |= kBotoes[i].ps2;
        }
    }
    if (e->Gamepad.bLeftTrigger > GATILHO) {
        btn |= B_L2;
    }
    if (e->Gamepad.bRightTrigger > GATILHO) {
        btn |= B_R2;
    }
    memset(data, 0, 18);
    data[0] = (unsigned char)(~btn & 0xFF);
    data[1] = (unsigned char)(~(btn >> 8) & 0xFF);
    data[2] = eixo(e->Gamepad.sThumbRX, 0);
    data[3] = eixo(e->Gamepad.sThumbRY, 1);
    data[4] = eixo(e->Gamepad.sThumbLX, 0);
    data[5] = eixo(e->Gamepad.sThumbLY, 1);
    return 18;
}

/* ---- A vibração (F8, decisão 33.4, plano/F8/07-vibracao.md) ----
   O jogo pede a vibração de cada lutador uma vez por quadro (BtlChar_UpdateVibration, src/battle/btl_char_get.c, só com
   a opção de vibração daquele jogador ligada, fora do replay e nunca para a CPU) e o Pad_Update manda o pedido do quadro
   ao controle (Pad_SendActuator, src/sys/pad.c, pelo sceVibSetActParam de port/src/plat_stub.c): bit 0 o motor pequeno
   ligado ou não, bits 1 a 8 a força do grande, que o jogo nunca passa de 127. Todo quadro sem pedido manda zero: a pausa
   da luta (BATTLE_FLAG_PAUSE pula o BtlChar_EndFrame), os menus e a opção desligada param os motores pelo próprio jogo.
   No 360: o grande vai no motor esquerdo (o grave), com 127 na força máxima, e o pequeno no direito (o agudo), ligado ou
   desligado, no controle que o jogador tem agora (sQuadrante). O XInputSetState só é chamado quando o valor muda.
   O que o jogo não cobre, a vigia (uma thread na 1, a cada 20 ms) cobre: com a Guide ou outra tela do sistema aberta
   (XN_SYS_UI, o que inclui a caixa de queda) e quando o jogo para de mandar por mais de VIGIA_MS (queda, saída, carga
   presa), os motores param; com a tela do sistema aberta o jogo também só manda zero. Um controle que some perde a
   vibração com ele, e o que entra começa parado. */
#define VIGIA_MS 250

static CRITICAL_SECTION sVibTrava;       /* o último enviado; o jogo espera, a vigia só tenta (TryEnter) */
static int sVibPronta;                   /* 1 depois da trava, da vigia e do ouvinte (só a thread do jogo muda) */
static int sVibQuad[2] = {-1, -1};       /* onde está o último valor enviado de cada jogador (-1: nenhum) */
static WORD sVibGrave[2], sVibAgudo[2];  /* o último valor enviado */
static int sVibVisto[2];                 /* a primeira vibração de cada jogador já foi para o diário */
static volatile DWORD sVibQuando;        /* GetTickCount da última chamada do jogo */
static volatile LONG sVibSistema;        /* a Guide ou outra tela do sistema aberta (XN_SYS_UI) */
static HANDLE sOuvinte;

/* com a trava: leva o jogador ao valor no quadrante (-1: sem controle); o quadrante antigo, se vibrava, para antes */
static void motores(int socket, int quad, WORD grave, WORD agudo) {
    XINPUT_VIBRATION v;
    DWORD r;

    if (sVibQuad[socket] >= 0 && sVibQuad[socket] != quad && (sVibGrave[socket] != 0 || sVibAgudo[socket] != 0)) {
        v.wLeftMotorSpeed = 0;
        v.wRightMotorSpeed = 0;
        XInputSetState((DWORD)sVibQuad[socket], &v); /* (o controle que sumiu já parou: o erro não importa) */
    }
    if (quad < 0) {
        sVibQuad[socket] = -1;
        sVibGrave[socket] = 0;
        sVibAgudo[socket] = 0;
        return;
    }
    if (quad == sVibQuad[socket] && grave == sVibGrave[socket] && agudo == sVibAgudo[socket]) {
        return;
    }
    v.wLeftMotorSpeed = grave;
    v.wRightMotorSpeed = agudo;
    r = XInputSetState((DWORD)quad, &v);
    sVibQuad[socket] = quad;
    sVibGrave[socket] = grave;
    sVibAgudo[socket] = agudo;
    if (!sVibVisto[socket] && (grave != 0 || agudo != 0)) {
        sVibVisto[socket] = 1;
        Diario_Escrever("vibração: a primeira do jogador %d, quadrante %d, grave %u, agudo %u (XInputSetState %u)",
                        socket + 1, quad + 1, (unsigned)grave, (unsigned)agudo, (unsigned)r);
    }
}

/* a vigia: para os motores ligados, se a trava estiver livre (senão, na volta seguinte) */
static void pararMotores(const char *motivo) {
    int s, parou = 0;

    if (!TryEnterCriticalSection(&sVibTrava)) {
        return;
    }
    for (s = 0; s < 2; s++) {
        if (sVibQuad[s] >= 0 && (sVibGrave[s] != 0 || sVibAgudo[s] != 0)) {
            motores(s, sVibQuad[s], 0, 0);
            parou = 1;
        }
    }
    LeaveCriticalSection(&sVibTrava);
    if (parou) {
        Diario_Escrever("vibração: motores parados (%s)", motivo);
    }
}

static DWORD WINAPI Vigia(LPVOID arg) {
    DWORD id;
    ULONG_PTR valor;
    (void)arg;
    for (;;) {
        Sleep(20);
        while (sOuvinte != NULL && XNotifyGetNext(sOuvinte, 0, &id, &valor)) {
            if (id == XN_SYS_UI && (valor != 0) != (sVibSistema != 0)) {
                InterlockedExchange(&sVibSistema, valor != 0);
                Diario_Escrever("vibração: tela do sistema %s", valor != 0 ? "aberta" : "fechada");
            }
        }
        if (sVibSistema) {
            pararMotores("tela do sistema aberta");
        } else if (GetTickCount() - sVibQuando > VIGIA_MS) {
            pararMotores("o jogo parou de mandar");
        }
    }
}

static void iniciarVibracao(void) {
    HANDLE t;

    InitializeCriticalSection(&sVibTrava);
    sVibQuando = GetTickCount();
    sOuvinte = XNotifyCreateListener(XNOTIFY_SYSTEM); /* (o do som360.cpp é outro: cada ouvinte recebe tudo) */
    if (sOuvinte == INVALID_HANDLE_VALUE) {
        sOuvinte = NULL;
    }
    t = CreateThread(NULL, 64 * 1024, Vigia, NULL, CREATE_SUSPENDED, NULL); /* (o diário com BT3_LOG_SINCRONO grava aqui) */
    if (t != NULL) {
        XSetThreadProcessor(t, 1); /* (a 1, com o diário e o vigia do som: nunca a 4 e a 5 do XAudio2) */
        ResumeThread(t);
        CloseHandle(t);
    }
    Diario_Escrever("vibração: pronta (ouvinte do sistema %s, vigia %s)", sOuvinte != NULL ? "aberto" : "recusado",
                    t != NULL ? "na thread de hardware 1" : "recusada");
    sVibPronta = 1;
}

/* O sceVibSetActParam do 360 (plat_stub.c): dados é o vibData do Pad_SendActuator, na ordem da máquina. */
int Port_PadVibrar(int socket, unsigned dados) {
    unsigned forca = (dados >> 1) & 0xFF;
    WORD grave, agudo;
    int quad;

    if (socket < 0 || socket > 1 || Roteiro_Ativo()) {
        return 0;
    }
    if (!sVibPronta) {
        iniciarVibracao();
    }
    sVibQuando = GetTickCount();
    grave = (WORD)(forca >= 127 ? 65535 : forca * 65535 / 127);
    agudo = (WORD)((dados & 1) ? 65535 : 0);
    if (sVibSistema) {
        grave = 0;
        agudo = 0;
    }
    quad = sLigado[socket] ? sQuadrante[socket] : -1;
    EnterCriticalSection(&sVibTrava);
    motores(socket, quad, grave, agudo);
    LeaveCriticalSection(&sVibTrava);
    return 1;
}
