/* O uniforme do cenário só depende destes 64 bytes da VU, do XYOFFSET e de ZBUF.PSM.
   A vaga lembrada pertence à gravação. O fecho copia para a lista antes de qualquer reutilização. */
static struct {
    int valido;
    uint64_t xy;
    unsigned z, versao, gravadaVersao, quadro;
    const void *gravado;
    Vu0Uniform u;
} sVu4Const;
static Vu0Uniform *vu4_uniforme(const float *consts, uint64_t xy, uint64_t zbuf, int *acerto) {
    unsigned z = (unsigned)((zbuf >> 24) & 15);
    *acerto = sVu4Const.valido && sVu4Const.xy == xy && sVu4Const.z == z &&
              memcmp(sVu4Const.u.screen, consts, 64) == 0;
    if (!*acerto) {
        memset(&sVu4Const.u, 0, sizeof(sVu4Const.u));
        memcpy(sVu4Const.u.screen, consts, 64);
        sVu4Const.u.misc[0] = (float)(xy & 0xFFFF) / 16.0f;
        sVu4Const.u.misc[1] = (float)((xy >> 32) & 0xFFFF) / 16.0f;
        sVu4Const.u.misc[2] = z == 0 ? 4294967295.0f : z == 1 ? 16777215.0f : 65535.0f;
        sVu4Const.xy = xy;
        sVu4Const.z = z;
        sVu4Const.valido = 1;
        sVu4Const.versao++;
    }
    return &sVu4Const.u;
}
/* A igualdade só é conhecida para esta cópia, neste quadro e antes de a entrada mudar. */
static void vu4_uniforme_gravou(const void *a, unsigned quadro) {
    sVu4Const.gravado = a;
    sVu4Const.quadro = quadro;
    sVu4Const.gravadaVersao = sVu4Const.versao;
}
static int vu4_uniforme_conhecido(const void *a, const void *b, unsigned quadro) {
    return sVu4Const.gravado != NULL && a == sVu4Const.gravado && b == &sVu4Const.u &&
           quadro == sVu4Const.quadro && sVu4Const.versao == sVu4Const.gravadaVersao;
}
#if defined(BT3_GPU_FALSO) || defined(BT3_TESTE_VU_CORTES)
/* Oráculo do caminho completo, também nos acertos: não consulta a chave nem a vaga lembrada. */
static int vu4_uniforme_confere(const Vu0Uniform *u, const float *consts, uint64_t xy, uint64_t zbuf) {
    Vu0Uniform ref;
    memset(&ref, 0, sizeof(ref));
    memcpy(ref.screen, consts, 64);
    ref.misc[0] = (float)(xy & 0xFFFF) / 16.0f;
    ref.misc[1] = (float)((xy >> 32) & 0xFFFF) / 16.0f;
    ref.misc[2] = ((zbuf >> 24) & 15) == 0 ? 4294967295.0f : ((zbuf >> 24) & 15) == 1 ? 16777215.0f : 65535.0f;
    return memcmp(u, &ref, sizeof(ref)) == 0;
}
#endif
