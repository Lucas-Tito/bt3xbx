/* Instalação antes do jogo. A main só apresenta e usa a UI assíncrona do sistema. */
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
/* Atrás das caixas fica o último estado da conversão, sem voltar a 0%. */
static void fundo(D3DDevice *dev) { tela(dev); }
/* Buffers usados pelo sistema ficam estáticos até o término assíncrono. */
static int caixa(D3DDevice *dev, const wchar_t *titulo, const wchar_t *msg, bool pergunta) {
    static LPCWSTR botoes[2];
    static MESSAGEBOX_RESULT resultado;
    static XOVERLAPPED ov;
    DWORD r, inicio = GetTickCount(), fim = 0;
    botoes[0] = pergunta ? ISO_CONV_SIM : ISO_CONV_OK;
    botoes[1] = ISO_CONV_NAO;
    for (;;) {
        fundo(dev);
        ZeroMemory(&ov, sizeof(ov));
        ZeroMemory(&resultado, sizeof(resultado));
        r = XShowMessageBoxUI(XUSER_INDEX_ANY, titulo, msg, pergunta ? 2 : 1, botoes, pergunta ? 1 : 0, 0,
                              &resultado, &ov);
        if (r != ERROR_ACCESS_DENIED || GetTickCount() - inicio >= 10000)
            break;
    }
    if (r != ERROR_IO_PENDING && r != ERROR_SUCCESS)
        return -1;
    while (!XHasOverlappedIoCompleted(&ov))
        fundo(dev);
    r = XGetOverlappedResult(&ov, &fim, FALSE);
    return r == ERROR_SUCCESS ? (int)resultado.dwButtonPressed : -1;
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
        caixa(dev, ISO_CONV_FALHA_TITULO, ISO_CONV_FALHA, false);
        return 0;
    }
    if (caixa(dev, ISO_CONV_TITULO, ISO_CONV_PERGUNTA, true) != 0)
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
        caixa(dev, ISO_CONV_SUCESSO, ISO_CONV_PRONTA, false);
        return 1;
    }
    caixa(dev, r == IC_ESPACO ? ISO_CONV_ESPACO_TITULO : ISO_CONV_FALHA_TITULO,
          r == IC_ESPACO    ? ISO_CONV_ESPACO
          : r == IC_DESTINO ? ISO_CONV_DESTINO
                            : ISO_CONV_FALHA,
          false);
    return 0;
}
