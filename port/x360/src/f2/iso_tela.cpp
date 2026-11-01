/* Instalação antes do jogo. A main só apresenta; as perguntas são desenhadas aqui, sem a caixa do sistema. */
#include <xtl.h>
#include <stdio.h>
#include <string.h>
#include "iso_tela.h"
#include "iso_instalar.h"
#include "iso_avisos.h"
#include "iso_conv.h"
#include "diario.h"
#include "iso_tela_desenho.h"
#ifdef BT3_VERSAO_NUM
#define TELA_STR_(x) #x
#define TELA_STR(x) TELA_STR_(x)
static const char *const kVersaoTela = TELA_STR(BT3_VERSAO_NUM);
#else
static const char *const kVersaoTela = "DEV";
#endif
/* Relógio e falha da conversão, só da thread que desenha. O tempo conta desde o início da conversão e para no fim. */
static DWORD sInicio, sFim;
static int sFalhou;
static void tela(D3DDevice *dev) {
    TelaEstado e;
    IsoInstalar_Progresso(&e.fase, &e.pc);
    IsoInstalar_Contas(&e.c);
    e.falhou = sFalhou;
    e.ms = sInicio ? (sFim ? sFim : GetTickCount()) - sInicio : 0;
    telaDesenhar(dev, e, kVersaoTela);
    dev->Present(NULL, NULL, NULL, NULL);
}
/* A pergunta é nossa, não do sistema: num Bad Avatar sem DashLaunch a caixa do sistema fechava sozinha e o jogo
   a tratava como Cancel. A tela é apresentada a cada volta (a Guide congela o console se ninguém apresenta), sem
   espera bloqueante, e a entrada vem do XInput. */
#define PAD_ESQ 1
#define PAD_DIR 2
#define PAD_A 4
#define PAD_B 8
#define PAD_LIMIAR 16000
#define PAD_PORTAS 4
static unsigned padLer(DWORD porta) {
    XINPUT_STATE e;
    unsigned m = 0;
    if (XInputGetState(porta, &e) != ERROR_SUCCESS)
        return 0;
    if ((e.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) || e.Gamepad.sThumbLX < -PAD_LIMIAR)
        m |= PAD_ESQ;
    if ((e.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) || e.Gamepad.sThumbLX > PAD_LIMIAR)
        m |= PAD_DIR;
    if (e.Gamepad.wButtons & XINPUT_GAMEPAD_A)
        m |= PAD_A;
    if (e.Gamepad.wButtons & XINPUT_GAMEPAD_B)
        m |= PAD_B;
    return m;
}
/* Resultado: 0 = primeiro botão (Convert ou OK), 1 = Cancel. Cancel é o selecionado ao abrir. Um botão só vale
   depois de visto solto em algum quadro desde a abertura, por porta: o A que já estava apertado nunca confirma. */
static int caixa(D3DDevice *dev, const char *nome, const wchar_t *titulo, const wchar_t *msg, bool dois) {
    unsigned armado[PAD_PORTAS] = {0, 0, 0, 0};
    int sel = dois ? 1 : 0, escolha = -1;
    DWORD inicio = GetTickCount();
    bool apertado = false;
    for (DWORD p = 0; p < PAD_PORTAS; p++) { /* a leitura da abertura já conta como um quadro */
        unsigned agora = padLer(p);
        armado[p] = ~agora;
        if (agora)
            apertado = true;
    }
    Diario_Escrever("tela: pergunta '%s' aberta (%s)", nome, apertado ? "com botão apertado" : "tudo solto");
    while (escolha < 0) {
        for (DWORD p = 0; p < PAD_PORTAS && escolha < 0; p++) {
            unsigned agora = padLer(p), borda = agora & armado[p];
            armado[p] = ~agora; /* só solto arma o botão */
            if (dois && (borda & PAD_ESQ) && sel != 0) {
                sel = 0;
                Diario_Escrever("tela: '%s' seleção Convert (esquerda, porta %u)", nome, (unsigned)p);
            } else if (dois && (borda & PAD_DIR) && sel != 1) {
                sel = 1;
                Diario_Escrever("tela: '%s' seleção Cancel (direita, porta %u)", nome, (unsigned)p);
            }
            if (borda & PAD_A) {
                escolha = sel;
                Diario_Escrever("tela: '%s' botão A (porta %u)", nome, (unsigned)p);
            } else if (borda & PAD_B) {
                escolha = dois ? 1 : 0;
                Diario_Escrever("tela: '%s' botão B (porta %u)", nome, (unsigned)p);
            }
        }
        telaPergunta(dev, kVersaoTela, titulo, msg, dois ? ISO_CONV_SIM : ISO_CONV_OK, dois ? ISO_CONV_NAO : NULL,
                     sel);
        dev->Present(NULL, NULL, NULL, NULL);
    }
    Diario_Escrever("tela: pergunta '%s' escolheu %d após %u ms", nome, escolha,
                    (unsigned)(GetTickCount() - inicio));
    return escolha;
}
static char origemConversao[512];
static DWORD WINAPI limpar(LPVOID p) {
    (void)p;
    return IsoInstalar_Limpar() ? 0 : IC_IO;
}
static DWORD WINAPI converter(LPVOID p) {
    (void)p;
    return (DWORD)IsoInstalar_Executar(origemConversao);
}
static int trabalhar(D3DDevice *dev, LPTHREAD_START_ROUTINE fn) {
    HANDLE h = CreateThread(NULL, 256 * 1024, fn, NULL, CREATE_SUSPENDED, NULL);
    DWORD codigo = IC_IO;
    if (h == NULL)
        return IC_IO;
    XSetThreadProcessor(h, 2);
    ResumeThread(h);
    while (WaitForSingleObject(h, 0) == WAIT_TIMEOUT)
        tela(dev);
    GetExitCodeThread(h, &codigo);
    CloseHandle(h);
    return (int)codigo;
}
int IsoTela_Converter(D3DDevice *dev, const char *origem) {
    int r;
    /* Descarta uma instalação interrompida mesmo se o usuário cancelar a nova. */
    r = trabalhar(dev, limpar);
    if (r != IC_OK) {
        caixa(dev, "falha-limpeza", ISO_CONV_FALHA_TITULO, ISO_CONV_FALHA, false);
        return 0;
    }
    if (caixa(dev, "conversao", ISO_CONV_TITULO, ISO_CONV_PERGUNTA, true) != 0)
        return 0;
    if (strlen(origem) >= sizeof(origemConversao))
        return 0;
    strcpy(origemConversao, origem);
    sFalhou = 0;
    sFim = 0;
    sInicio = GetTickCount();
    if (sInicio == 0)
        sInicio = 1;
    r = trabalhar(dev, converter);
    sFim = GetTickCount();
    sFalhou = r != IC_OK;
    if (r == IC_OK) {
        caixa(dev, "sucesso", ISO_CONV_SUCESSO, ISO_CONV_PRONTA, false);
        return 1;
    }
    caixa(dev, r == IC_ESPACO ? "espaco" : r == IC_DESTINO ? "destino" : "falha",
          r == IC_ESPACO ? ISO_CONV_ESPACO_TITULO : ISO_CONV_FALHA_TITULO,
          r == IC_ESPACO    ? ISO_CONV_ESPACO
          : r == IC_DESTINO ? ISO_CONV_DESTINO
                            : ISO_CONV_FALHA,
          false);
    return 0;
}
