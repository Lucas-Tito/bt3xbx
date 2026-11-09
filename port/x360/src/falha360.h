/* Política da decisão 43. Textos públicos propostos para auditoria do Lucas. */
#ifndef FALHA360_H
#define FALHA360_H
#ifndef BT3_TESTE_FALHAS
#define BT3_TESTE_FALHAS 0
#endif
static DWORD Falha_Cor(DWORD fase) {
#if BT3_TESTE_FALHAS
    return fase;
#else
    (void)fase;
    return 0xFF010101;
#endif
}
static wchar_t sFalhaTexto[512];
static bool sPreparando = true;

/* Copia a fase antes que a desmontagem a substitua. Não depende de diário ligado. */
static void Falha_Registrar(int codigo, const char *fase) {
    const wchar_t *motivo = codigo == 7 ? L"The graphics renderer could not be initialized." :
        codigo == 3 ? L"The game stopped after an internal consistency check failed." :
        sPreparando ? L"The game could not prepare its memory or load its program data." :
        codigo == 2 ? L"The game could not load required data or complete the requested script." :
        L"The game stopped because of a fatal error.";
    unsigned n = 0;
    const wchar_t *p = motivo;
    while (*p && n < 300) sFalhaTexto[n++] = *p++;
    p = L"\nPhase: ";
    while (*p) sFalhaTexto[n++] = *p++;
    while (fase && *fase && n < 450) sFalhaTexto[n++] = (unsigned char)*fase++;
    p = L"\nPress OK to return to the dashboard.";
    while (*p) sFalhaTexto[n++] = *p++;
    sFalhaTexto[n] = 0;
}

/* O sistema usa estes buffers depois da chamada. A main continua apresentando. */
static bool Falha_Mostrar(void (*apresentar)(void)) {
    static const wchar_t *botoes[] = {L"OK"};
    static MESSAGEBOX_RESULT resultado;
    static XOVERLAPPED ov;
    DWORD r, fim = 0, inicio = GetTickCount();
    for (;;) {
        apresentar();
        ZeroMemory(&ov, sizeof(ov));
        r = XShowMessageBoxUI(XUSER_INDEX_ANY, L"Unable to continue", sFalhaTexto, 1,
                             botoes, 0, XMB_ERRORICON, &resultado, &ov);
        if (r != ERROR_ACCESS_DENIED || GetTickCount() - inicio >= 10000) break;
    }
    if (r != ERROR_IO_PENDING && r != ERROR_SUCCESS) return false;
    while (!XHasOverlappedIoCompleted(&ov)) apresentar();
    r = XGetOverlappedResult(&ov, &fim, FALSE);
    return r == ERROR_SUCCESS;
}
#endif
