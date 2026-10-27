#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../include/vu_iguais.h"
#include "../include/gs_draw.h"
#include "../include/vu_constantes.h"
static void escritos(Vu0Uniform *u, int familia) {
    memset(u->screen, 0x6d, sizeof(u->screen));
    u->misc[0] = 3.0f; u->misc[1] = -2.0f; u->misc[2] = 65535.0f;
    if (familia == 3) {
        memset(u->boneA, 0x73, sizeof(u->boneA));
        u->pivotA[0] = 2.0f; u->pivotB[0] = 4.0f; u->pivotB[1] = 8.0f;
    }
}
int main(void) {
    uint32_t a[81], b[81];
    unsigned n, i, k, testes = 0;
    Vu0Uniform ref, novo;
    int familia, byte;
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
    for (familia = 2; familia <= 3; familia++) {
        for (byte = 0; byte < 256; byte++) {
            memset(&ref, 0, sizeof(ref));
            memset(&novo, byte, sizeof(novo));
            vu_limpa_parcial(&novo, familia);
            escritos(&ref, familia); escritos(&novo, familia);
            if (memcmp(&ref, &novo, sizeof(ref)) != 0) {
                fprintf(stderr, "FALHA: constantes, família %d, byte %d\n", familia, byte); return 1;
            }
        }
    }
    puts("tudo certo: 512 blocos de constantes iguais byte a byte");
    printf("tudo certo: %u diferenças individuais acusadas\n", testes);
    return 0;
}
