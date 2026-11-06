/* Núcleos da VU0: só ponteiros e inteiros cruzam a fronteira de ABI.
 * No 360 a thread já está em truncamento; a variante de teste restaura o
 * arredondamento do restante da build PPC, que usa float por software.
 */
#include "../../include/vu0_nativo.h"
/* Configuração antes de iniciar a thread do jogo, sem getenv nas contas. */
int gVnAtivo = 1;
void Vn_Configurar(const char *valor) {
    gVnAtivo = !(valor && valor[0] == '0' && valor[1] == 0);
}
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

/* O produto arredonda antes da soma, na mesma função. Conservar /fp:precise
 * e conferir o binário depois do link; no GCC usar -ffp-contract=off.
 */
#ifdef _MSC_VER
#pragma fp_contract(off)
#endif
int Vn_Op4(const VnBits *a, const VnBits *b,
                         const VnBits *acc, VnBits *out, int op) {
    const float *fa = (const float *)a, *fb = (const float *)b;
    const float *fc = (const float *)acc;
    float *fo = (float *)out;
    float x, y, z, w;
    VN_DECLARA_MODO;
    if (!gVnAtivo) return 0;
    if (!Vn_Entradas(a, 4) || !Vn_Entradas(b, 4) ||
        (acc && !Vn_Entradas(acc, 4))) return 0;
    VN_TRUNCA;
    /* Todos os resultados precedem a escrita, inclusive com saída sobreposta. */
#define VN_COMPONENTE(i, d) do { \
    if (!op) d = fa[i] + fb[i]; \
    else { float p = fa[i] * fb[i]; d = acc ? fc[i] + p : p; } \
} while (0)
    VN_COMPONENTE(0, x);
    VN_COMPONENTE(1, y);
    VN_COMPONENTE(2, z);
    VN_COMPONENTE(3, w);
#undef VN_COMPONENTE
    fo[0] = x; fo[1] = y; fo[2] = z; fo[3] = w;
    VN_RESTAURA;
    return 1;
}

int Vn_Bc4(const VnBits *a, VnBits b,
                         const VnBits *acc, VnBits *out, int op) {
    VnBits bb[4];
    bb[0] = bb[1] = bb[2] = bb[3] = b;
    return Vn_Op4(a, bb, acc, out, op);
}

int Vn_MtxApply(const VnBits *r, const VnBits *v, VnBits *out) {
    const float *fr = (const float *)r, *fv = (const float *)v;
    float *fo = (float *)out;
    float v0, v1, v2, v3, x, y, z, w;
    VN_DECLARA_MODO;
    if (!gVnAtivo) return 0;
    if (!Vn_Entradas(r, 16) || !Vn_Entradas(v, 4)) return 0;
    VN_TRUNCA;
    v0 = fv[0]; v1 = fv[1]; v2 = fv[2]; v3 = fv[3];
#define VN_COLUNA(c, d) do { \
    float a = fr[c] * v0 + fr[4 + c] * v1; \
    a = a + fr[8 + c] * v2; \
    d = a + fr[12 + c] * v3; \
} while (0)
    VN_COLUNA(0, x);
    VN_COLUNA(1, y);
    VN_COLUNA(2, z);
    VN_COLUNA(3, w);
#undef VN_COLUNA
    fo[0] = x; fo[1] = y; fo[2] = z; fo[3] = w;
    VN_RESTAURA;
    return 1;
}
#ifdef _MSC_VER
#pragma fp_contract(on)
#endif
