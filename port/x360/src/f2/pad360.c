/* Os controles do 360 pelo XInput, no lugar de port/src/gs/gs_input.c (F3, etapa 8). O jogo lê dois controles do
   PS2 (scePad2Read, port/src/plat_stub.c). O jogador 1 é o primeiro controle ligado, em qualquer quadrante do anel,
   e o jogador 2 o seguinte (decisão 21; o título não escolhe o quadrante, e o jogo espera o controle 1 no boot). Um
   jogador só troca de controle quando o dele some e há outro livre: sem controle, scePad2GetState deixa de dizer
   "pronto" e o jogo mostra o aviso de controle desconectado.

   Botões como na build de PC com controle (as posições do DualShock 2): A = X, B = círculo, X = quadrado,
   Y = triângulo, LB/RB = L1/R1, LT/RT = L2/R2, cliques dos analógicos = L3/R3, START = START, BACK = SELECT. */
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

/* Roteiro de controle (BT3_ROTEIRO, port/x360/headless/roteiro.c, F4 etapa 4): o jogador 1 vem do roteiro e os
   dois controles contam como ligados, como na build sem janela (scePad2GetState sempre "pronto"); o XInput não é
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
int Port_PadRead(int socket, unsigned char *data) {
    const XINPUT_STATE *e;
    unsigned btn = 0, i;

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
