/* Núcleos da VU0: só ponteiros e inteiros cruzam a fronteira de ABI.
 * No 360 a thread já está em truncamento; a variante de teste restaura o
 * arredondamento do restante da build PPC, que usa float por software.
 */
#include "../../include/vu0_nativo.h"
#ifdef BT3_TESTE_VU0_NATIVO
#include <fenv.h>
#define VN_DECLARA_MODO int vn_modo
#define VN_TRUNCA vn_modo = fegetround(); fesetround(FE_TOWARDZERO)
#define VN_RESTAURA fesetround(vn_modo)
#else
#define VN_DECLARA_MODO
#define VN_TRUNCA
#define VN_RESTAURA
#endif

/* Fronteira de chamada obrigatória, inclusive no cl do XDK: esta função só
 * multiplica, o chamador só soma. Nem /fp:fast pode fundir as duas operações.
 * Os temporários passam como floats, sem stw/lfs de bits intermediários.
 * Não habilitar otimização de programa inteiro nesta fronteira.
 */
static VN_SEM_INLINE void Vn_Produtos(float *p, const float *a, const float *b, int n) {
    int i;
    for (i = 0; i < n; ++i) p[i] = a[i] * b[i];
}

int Vn_Op4(const VnBits *a, const VnBits *b,
                         const VnBits *acc, VnBits *out, int op) {
    const float *fa = (const float *)a, *fb = (const float *)b;
    const float *fc = (const float *)acc;
    float p[4], r[4];
    int i;
    VN_DECLARA_MODO;
    if (!Vn_Entradas(a, 4) || !Vn_Entradas(b, 4) ||
        (acc && !Vn_Entradas(acc, 4))) return 0;
    VN_TRUNCA;
    if (op) Vn_Produtos(p, fa, fb, 4);
    for (i = 0; i < 4; ++i) {
        if (!op) r[i] = fa[i] + fb[i];
        else if (acc) r[i] = fc[i] + p[i];
        else r[i] = p[i];
    }
    VN_RESTAURA;
    memcpy(out, r, 16);
    return 1;
}

int Vn_Bc4(const VnBits *a, VnBits b,
                         const VnBits *acc, VnBits *out, int op) {
    VnBits bb[4];
    bb[0] = bb[1] = bb[2] = bb[3] = b;
    return Vn_Op4(a, bb, acc, out, op);
}

/* Carrega os operandos como floats direto da origem, sem cópia inteira na pilha. */
static VN_SEM_INLINE void Vn_ProdutosMatriz(float *p, const float *r, const float *v) {
    int i;
    for (i = 0; i < 16; ++i) p[i] = r[i] * v[i / 4];
}

int Vn_MtxApply(const VnBits *r, const VnBits *v, VnBits *out) {
    float p[16], o[4];
    int c;
    VN_DECLARA_MODO;
    if (!Vn_Entradas(r, 16) || !Vn_Entradas(v, 4)) return 0;
    VN_TRUNCA;
    Vn_ProdutosMatriz(p, (const float *)r, (const float *)v);
    for (c = 0; c < 4; ++c) {
        float a = p[c] + p[4 + c];
        a = a + p[8 + c];
        o[c] = a + p[12 + c];
    }
    VN_RESTAURA;
    memcpy(out, o, 16);
    return 1;
}
