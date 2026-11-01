/* Instalação antes do jogo. A main só apresenta e usa a UI assíncrona do sistema. */
#include <xtl.h>
#include <stdio.h>
#include <string.h>
#include "iso_tela.h"
#include "iso_instalar.h"
#include "iso_avisos.h"
#include "iso_conv.h"
#include "diario.h"
/* Fonte mínima 5x7 do port, só para o progresso. Retângulos Clear dispensam shaders,
   fontes externas e qualquer leitura de arquivo pela thread que apresenta. */
static const unsigned char letras[][7] = {
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14},
    {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17}, {14, 4, 4, 4, 4, 4, 14},
    {7, 2, 2, 2, 18, 18, 12},     {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17}, {14, 17, 17, 17, 17, 17, 14},
    {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},       {17, 17, 17, 17, 17, 17, 14},
    {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31},     {14, 17, 19, 21, 25, 17, 14},
    {4, 12, 4, 4, 4, 4, 14},      {14, 17, 1, 2, 4, 8, 31},     {30, 1, 1, 14, 1, 1, 30},
    {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},   {14, 16, 16, 30, 17, 17, 14},
    {31, 1, 2, 4, 8, 8, 8},       {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14},
    {0, 0, 0, 0, 0, 12, 12},      {17, 2, 4, 8, 16, 17, 0}};
static void texto(D3DDevice *dev, const char *s, int x, int y, int escala) {
    for (; *s; s++, x += 6 * escala) {
        int c = *s, indice = -1;
        if (c >= 'a' && c <= 'z')
            c -= 32;
        if (c >= 'A' && c <= 'Z')
            indice = c - 'A';
        else if (c >= '0' && c <= '9')
            indice = 26 + c - '0';
        else if (c == '.')
            indice = 36;
        else if (c == '%')
            indice = 37;
        if (indice < 0)
            continue;
        for (int j = 0; j < 7; j++)
            for (int i = 0; i < 5; i++)
                if (letras[indice][j] & (16 >> i)) {
                    D3DRECT r = {x + i * escala, y + j * escala, x + (i + 1) * escala, y + (j + 1) * escala};
                    dev->Clear(1, &r, D3DCLEAR_TARGET, 0xffeeeeee, 1, 0);
                }
    }
}
static void tela(D3DDevice *dev, int fase, unsigned pc) {
    D3DRECT fundo = {160, 350, 1120, 378}, barra = {160, 350, 160 + (LONG)(pc * 960 / 100), 378};
    char numero[16];
    dev->Clear(0, NULL, D3DCLEAR_TARGET, 0xff101820, 1, 0);
    texto(dev, ISO_CONV_PROGRESSO[fase >= 0 && fase < 5 ? fase : 0], 160, 260, 4);
    dev->Clear(1, &fundo, D3DCLEAR_TARGET, 0xff303e48, 1, 0);
    if (pc)
        dev->Clear(1, &barra, D3DCLEAR_TARGET, 0xff4aa85a, 1, 0);
    sprintf(numero, "%u%%", pc);
    texto(dev, numero, 160, 410, 3);
    texto(dev, ISO_CONV_AGUARDE, 160, 490, 3);
    dev->Present(NULL, NULL, NULL, NULL);
}
/* Atrás das caixas fica o último estado da conversão, sem voltar a 0%. */
static void fundo(D3DDevice *dev) {
    int fase;
    unsigned pc;
    IsoInstalar_Progresso(&fase, &pc);
    tela(dev, fase, pc);
}
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
    while (WaitForSingleObject(h, 0) == WAIT_TIMEOUT) {
        int fase;
        unsigned pc;
        IsoInstalar_Progresso(&fase, &pc);
        tela(dev, fase, pc);
    }
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
    r = trabalhar(dev, converter);
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
