/* Só zera as palavras que os programas 4 e 6 não sobrescrevem em seguida.
   Os demais campos recebem exatamente os mesmos valores do caminho anterior. */
static void vu_limpa_parcial(Vu0Uniform *u, int familia) {
    if (familia == 3) {
        memset(u->boneB, 0, sizeof(u->boneB));
        memset(u->pivotA + 1, 0, 3 * sizeof(float));
        memset(u->pivotB + 2, 0, 2 * sizeof(float));
    } else {
        memset(u->boneA, 0, 40 * sizeof(float));
    }
    memset(u->light, 0, 12 * sizeof(float));
    u->misc[3] = 0.0f;
    memset(u->light2, 0, sizeof(u->light2));
}
