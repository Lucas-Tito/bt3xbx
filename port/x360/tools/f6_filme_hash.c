/* Rede 5: hash RGBA por imagem, mesma fonte nas duas arquiteturas.
   Uso: f6_filme_hash arquivo.PSS op|ed. Nenhum pixel vai ao repositório. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/plat_mpeg2.c"

int main(int argc, char **argv) {
    Mpeg2 *m;
    unsigned n = 0, total, i;
    uint8_t *rgba;
    if (argc != 3 || (strcmp(argv[2], "op") && strcmp(argv[2], "ed"))) return 2;
    total = !strcmp(argv[2], "op") ? 2810 : 4200;
    m = Mpeg2_Open(argv[1]);
    rgba = malloc(512 * 448 * 4);
    if (!m || !rgba) return 1;
    while (Mpeg2_Next(m, rgba)) {
        uint64_t h = UINT64_C(14695981039346656037);
        if (Mpeg2_Width(m) != 512 || Mpeg2_Height(m) != 448 || n >= total) return 1;
        for (i = 0; i < 512 * 448 * 4; ++i) {
            h ^= rgba[i];
            h *= UINT64_C(1099511628211);
        }
        printf("%s %u %016llx\n", argv[2], n++, (unsigned long long)h);
    }
    Mpeg2_Close(m);
    free(rgba);
    if (n != total) return 1;
    printf("FIM %s %u\n", argv[2], n);
    return ferror(stdout) ? 1 : 0;
}
