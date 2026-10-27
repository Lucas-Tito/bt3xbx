#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../include/vu_iguais.h"
int main(void) {
    uint32_t a[81], b[81];
    unsigned n, i, k, testes = 0;
    for (i = 0; i < 81; i++) a[i] = b[i] = i * 0x1020304u;
    for (n = 0; n <= 76; n++) {
        if (!vu_iguais4(a + 1, b + 1, n)) return 1;
        for (k = 0; k < n; k++) {
            b[k + 1] ^= 0x80000000u;
            if (vu_iguais4(a + 1, b + 1, n)) { fprintf(stderr, "FALHA: palavra %u/%u\n", k, n); return 1; }
            b[k + 1] ^= 0x80000000u;
            testes++;
        }
    }
    printf("tudo certo: %u diferenças individuais acusadas\n", testes);
    return 0;
}
