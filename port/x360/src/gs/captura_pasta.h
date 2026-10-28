/* BT3_SHOT_PASTA: um nome FATX simples, com prefixo game:\ opcional. Saída de até 37 bytes com o NUL. */
#ifndef CAPTURA_PASTA_H
#define CAPTURA_PASTA_H
#include <string.h>

static int captura_pasta(const char *e, char pasta[40]) {
    size_t i, n;
    if (e == NULL) e = "shots";
    if (strncmp(e, "game:\\", 6) == 0) e += 6;
    n = strlen(e);
    if (n == 0 || n > 30) return 0;
    for (i = 0; i < n; i++) {
        if (!((e[i] >= 'a' && e[i] <= 'z') || (e[i] >= 'A' && e[i] <= 'Z') ||
              (e[i] >= '0' && e[i] <= '9') || e[i] == '_')) return 0;
    }
    memcpy(pasta, "game:\\", 6);
    memcpy(pasta + 6, e, n + 1);
    return 1;
}
#endif
