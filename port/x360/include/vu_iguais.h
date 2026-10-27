/* Comparação binária do uniforme, em grupos de quatro palavras. As origens já são float[76]
   alinhados a quatro; não se exige alinhamento a oito nem se comparam floats numericamente. */
static int vu_iguais4(const void *a, const void *b, unsigned n) {
    const uint32_t *x = (const uint32_t *)a, *y = (const uint32_t *)b;
    unsigned i = 0;
    for (; i + 4 <= n; i += 4) {
        uint32_t d = (x[i] ^ y[i]) | (x[i + 1] ^ y[i + 1]) | (x[i + 2] ^ y[i + 2]) | (x[i + 3] ^ y[i + 3]);
        if (d != 0) return 0;
    }
    for (; i < n; i++) if (x[i] != y[i]) return 0;
    return 1;
}
