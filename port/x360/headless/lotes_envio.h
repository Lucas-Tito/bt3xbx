/* Oráculo do consumidor real de faixas, chamado pela mesma thread de desenho
   que o D3D. O hash de cada comando e seus dados é relido no fecho e comparado
   com a execução integral, inclusive os comandos da cauda. Não é raster D3D. */
#include "../include/gs_lotes.h"
static unsigned sEnvN, sEnvQuadro, sEnvFaixas;
static uint64_t sEnvH[MAX_DRAWS], sEnvHash;
static int fake_lotes_z(const GsDraw *d) {
    uint32_t k = sPipeKey[d->pipeline];
    return ((k >> 12) & 1) || ((k >> 10) & 3) >= 2;
}
static uint32_t fake_lotes_prontos(const GsQuadro *q, uint32_t de) {
    const char *e = getenv("BT3_LOTES");
    if (e && !atoi(e)) return de;
    e = getenv("BT3_LOTES_ENVIO");
    if (e && !atoi(e)) return de;
    return gs_lotes_limite(q, de, fake_lotes_z);
}
static void fake_lotes_prepara(const GsQuadro *q) { (void)q; }
static void fake_lotes_desenha(const GsQuadro *q, uint32_t ate) {
    unsigned i;
    const char *a = getenv("BT3_LOTES_ATRASO"), *m = getenv("BT3_LOTES_MUTANTE");
    if (!sEnvN) {
        sEnvQuadro = q->quadro;
        sEnvHash = 1469598103934665603ull;
        if (a) usleep(atoi(a) * 1000);
    }
    px_exige(q->quadro == sEnvQuadro && ate > sEnvN && ate <= q->drawCount, "envio ordem/quadro");
    for (i = sEnvN; i < ate; i++) {
        uint64_t h = px_hash(q, &q->draws[i]);
        sEnvH[i] = h;
        if (m && !strcmp(m, "dados") && !i) sEnvH[i] ^= 1;
        sEnvHash = (sEnvHash ^ h) * 1099511628211ull;
    }
    sEnvN = ate;
    if (m && !strcmp(m, "ordem")) sEnvN++;
    sEnvFaixas++;
}
static void fake_lotes_fim(const GsQuadro *q) {
    unsigned i;
    uint64_t integral = 1469598103934665603ull, executado = sEnvN ? sEnvHash : integral;
    px_exige(!sEnvN || (q->quadro == sEnvQuadro && sEnvN <= q->drawCount), "envio fecho/quadro");
    for (i = 0; i < q->drawCount; i++) {
        uint64_t h = px_hash(q, &q->draws[i]);
        integral = (integral ^ h) * 1099511628211ull;
        if (i < sEnvN) px_exige(sEnvH[i] == h, "envio dados imutáveis");
        else executado = (executado ^ h) * 1099511628211ull;
    }
    px_exige(integral == executado, "envio comandos e cauda");
    fprintf(stderr, "lotes: quadro %u faixas %u antes %u cauda %u comandos %016llx iguais\n",
            q->quadro, sEnvFaixas, sEnvN, q->drawCount - sEnvN, (unsigned long long)executado);
    sEnvN = sEnvFaixas = 0;
}
