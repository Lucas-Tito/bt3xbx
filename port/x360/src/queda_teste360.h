/* Teste optativo: só lê a chave no arranque, sem getenv por quadro. */
#ifndef QUEDA_TESTE360_H
#define QUEDA_TESTE360_H
struct QuedaTeste360 {
    unsigned alvo, ultimo, quadros;
    int desenho;
};
static QuedaTeste360 sQuedaTeste;

static void QuedaTeste_Configurar(const char *valor, const char *thread) {
    unsigned n = 0;
    memset(&sQuedaTeste, 0, sizeof(sQuedaTeste));
    sQuedaTeste.ultimo = ~0u;
    if (valor == NULL || *valor == '\0') return;
    for (const char *p = valor; *p != '\0'; p++) {
        if (*p < '0' || *p > '9' || n > 100000 || (n == 100000 && *p != '0')) return;
        n = n * 10 + (unsigned)(*p - '0');
    }
    if (n == 0 || n > 1000000) return;
    if (thread != NULL && strcmp(thread, "jogo") != 0 && strcmp(thread, "desenho") != 0) return;
    sQuedaTeste.alvo = n;
    sQuedaTeste.desenho = thread != NULL && strcmp(thread, "desenho") == 0;
    Diario_Escrever("queda de teste armada: thread=%s quadro=%u de luta", sQuedaTeste.desenho ? "desenho" : "jogo", n);
}

static int QuedaTeste_DeveCair(int desenho, int luta, unsigned quadro) {
    if (desenho != sQuedaTeste.desenho || sQuedaTeste.alvo == 0) return 0;
    if (!luta) {
        sQuedaTeste.quadros = 0;
        sQuedaTeste.ultimo = ~0u;
        return 0;
    }
    if (quadro == sQuedaTeste.ultimo) return 0;
    sQuedaTeste.ultimo = quadro;
    if (++sQuedaTeste.quadros != sQuedaTeste.alvo) return 0;
    sQuedaTeste.alvo = 0; /* uma vez, mesmo que o filtro seja retomado */
    return 1;
}
#endif
