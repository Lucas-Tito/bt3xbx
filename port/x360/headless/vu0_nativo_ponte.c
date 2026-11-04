/* Prova do caminho do 360 nos roteiros PPC, conservando as operações escalares
 * e a ABI de bits da build normal. O recuo da matriz continua em vu0_b.c. */
#define Port_FastMtxApply Vn_MatrizReferencia
#include "../../src/plat_fastvec.c"
#undef Port_FastMtxApply
#include "vu0_nativo.h"
int Port_FastMtxApply(const uint32_t *r, const uint32_t *v, uint32_t *out) {
    return Vn_MtxApply(r, v, out);
}
