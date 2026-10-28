/* Estatística exata: -1 não participa, 0/1 participa sem/com alfa alto.
   O oráculo conserva a ordem e os predicados da varredura D3D original. */
#ifndef BT3_GS_ALFA_H
#define BT3_GS_ALFA_H
static int gs_alfa_referencia(const GsQuadro *q, const GsDraw *d, unsigned mascara) {
    uint32_t k;
    int alto = 0;
    if (d->native != 0 || d->count == 0 || !(mascara & GS_CC_A)) return -1;
    if (d->vu) {
        const Vu0Uniform *u = &q->vuUni[d->uniform];
        alto = u->color0[3] > 128.0f;
        if (u->misc[3] == 3.0f) {
            for (k = 0; k < d->count && !alto; k++) {
                uint32_t v = q->vuIdx[d->first + k];
                alto = v < q->vuVertCount && q->vuVerts[v * 12 + 7] > 128.0f;
            }
        }
    } else {
        for (k = 0; k < d->count && !alto; k++) alto = q->verts[d->first + k].a > 0x80;
    }
    return alto;
}
static int gs_alfa_metadado(const GsQuadro *q, unsigned n, unsigned mascara) {
    const GsDraw *d = &q->draws[n];
    if (d->native != 0 || d->count == 0 || !(mascara & GS_CC_A)) return -1;
    return q->alfa[n] != 0;
}
/* Usa a origem normal enquanto os três índices são escritos, nunca o buffer GPU. */
static int gs_alfa_triangulo(const float *v, unsigned i) {
    return v[i * 12 + 7] > 128.0f || v[(i + 1) * 12 + 7] > 128.0f || v[(i + 2) * 12 + 7] > 128.0f;
}
static int gs_alfa_vu(const Vu0Uniform *u, int indicesAltos, unsigned count) {
    return count != 0 && (u->color0[3] > 128.0f || (u->misc[3] == 3.0f && indicesAltos));
}
/* Somente trechos com índices entram na união. Uma fusão não muda o uniforme. */
static void gs_alfa_junta(unsigned char *saida, unsigned n, int alto, int junta) {
    saida[n] = (unsigned char)((junta ? saida[n] : 0) | (alto != 0));
}
#endif
