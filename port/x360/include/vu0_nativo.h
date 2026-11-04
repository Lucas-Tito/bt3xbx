#ifndef BT3_VU0_NATIVO_H
#define BT3_VU0_NATIVO_H
/* FPU em truncamento, sem fast-math. A faixa conservadora evita todo limite
 * diferente entre o modelo inteiro e a FPU: zero ou 2^-30 <= |x| <= 2^30.
 * Produtos ficam entre 2^-60 e 2^60; a menor parcela tem passo 2^-83.
 * Mesmo as três somas da matriz não podem produzir denormal nem estouro.
 * A classificação usa os bits de entrada, inclusive sob flush-to-zero.
 * Compilar com -fno-strict-aliasing (ou o modelo de alias do cl do XDK).
 * Saída e estado só são publicados depois de aceitar todos os operandos.
 */
#include <string.h>
#ifdef _MSC_VER
/* A CRT do XDK não fornece stdint.h nem a palavra inline em C. */
typedef unsigned int VnBits;
#define VN_INLINE __inline
#define VN_SEM_INLINE __declspec(noinline)
#else
#include <stdint.h>
typedef uint32_t VnBits;
#define VN_INLINE inline
#define VN_SEM_INLINE __attribute__((noinline, noclone))
#endif

static VN_INLINE int Vn_Aceita(VnBits u) {
    VnBits a = u & 0x7fffffffu;
    return a == 0 || (a >= 0x30800000u && a <= 0x4e800000u);
}

static VN_INLINE int Vn_Entradas(const VnBits *u, int n) {
    int i;
    for (i = 0; i < n; ++i) {
        if (!Vn_Aceita(u[i])) return 0;
    }
    return 1;
}

/* A implementação só recebe ponteiros e inteiros, inclusive na build de teste
 * PPC que chama estes núcleos de hardware a partir da ABI soft-float. */
int Vn_Op4(const VnBits *a, const VnBits *b, const VnBits *acc, VnBits *out, int op);
int Vn_Bc4(const VnBits *a, VnBits b, const VnBits *acc, VnBits *out, int op);
int Vn_MtxApply(const VnBits *r, const VnBits *v, VnBits *out);
#endif
