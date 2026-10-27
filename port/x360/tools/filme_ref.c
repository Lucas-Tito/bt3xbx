/* F6, etapa 7: quatro imagens RGB do decodificador x86, sem janela. Só grava fora do repositório.
   Uso: filme_ref arquivo.PSS pasta op|ed. Índices contados de zero; não escreve o restante do filme. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Compila o decodificador original, sem alterá-lo; a visão da struct permite medir seu ES dinâmico. */
#include "../../src/plat_mpeg2.c"

int main(int argc, char **argv) {
    Mpeg2 *m;
    unsigned n = 0, total, escritos = 0;
    uint8_t *rgba;
    size_t esMax = 0;
    if (argc != 4 || (strcmp(argv[3], "op") && strcmp(argv[3], "ed"))) return 2;
    total = !strcmp(argv[3], "op") ? 2810 : 4200;
    m = Mpeg2_Open(argv[1]);
    rgba = (uint8_t *)malloc(512 * 448 * 4);
    if (m == NULL || rgba == NULL) return 1;
    while (Mpeg2_Next(m, rgba)) {
        if (m->esCap > esMax) esMax = m->esCap;
        if (Mpeg2_Width(m) != 512 || Mpeg2_Height(m) != 448 || n >= total) return 1;
        if (n == 0 || n == 1000 || n == 2000 || n == total - 1) {
            char caminho[4096];
            unsigned i;
            FILE *f;
            if (snprintf(caminho, sizeof(caminho), "%s/filme_%s_%05u.ppm", argv[2], argv[3], n) >= (int)sizeof(caminho)) return 1;
            f = fopen(caminho, "wb");
            if (f == NULL || fprintf(f, "P6\n512 448\n255\n") < 0) return 1;
            for (i = 0; i < 512 * 448; i++) {
                if (fwrite(rgba + i * 4, 1, 3, f) != 3) return 1;
            }
            if (fclose(f)) return 1;
            escritos++;
        }
        n++;
    }
    if (m->esCap > esMax) esMax = m->esCap;
    printf("%s: memória do decodificador x86: objeto %u, planos %u, ES máximo %u bytes\n",
           argv[3], (unsigned)sizeof(*m), 512u * 448u * 3u / 2u, (unsigned)esMax);
    Mpeg2_Close(m);
    free(rgba);
    printf("%s: %u imagens decodificadas, %u capturas\n", argv[3], n, escritos);
    return n == total && escritos == 4 ? 0 : 1;
}
