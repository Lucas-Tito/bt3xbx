/*
 * F4, tarefa 9c-2: o pow de double do código do jogo nas builds sem janela (x86, x86r e PowerPC).
 *
 * O código do jogo se compila com float em software, e o port_libm.h só desvia para as pontes os nomes de float
 * (o powf, entre eles); o pow de double do ReplayMenu_Draw (src/menu/menu_za_d.c:409, num / (s32)pow(10.0, i)) vai
 * direto ao da libm, que é de float em hardware. Na x86 o jogo lê o resultado em eax:edx e a libm o devolve no st0
 * (visto no gdb: eax:edx = 0x3800, quase zero, com st0 = 10), o (s32) dá 0 e a divisão cai com SIGFPE; na PowerPC o
 * jogo passa os argumentos em r3..r6 e lê r3:r4, e a libm lê f1 e f2 e devolve em f1 (a divw por zero não cai: os
 * dígitos sairiam errados em silêncio). No 360 tudo é float em hardware, e o pow chega certo.
 *
 * O headless.py liga uma cópia do objeto do menu_za_d.c com a chamada renomeada para Port_PowDuplo (objcopy
 * --redefine-sym); o objeto compilado do jogo não muda, e os outros que chamam pow (o som do GS, de float em
 * hardware) continuam com o da libm. Esta ponte se compila com float em hardware e só passa inteiros, como as do
 * port/src/plat_libm.c: o double do float em software é o padrão de 64 bits, que as duas convenções passam como um
 * inteiro de 64 bits (na pilha e em eax:edx na x86; em r3:r4 e r5:r6 na PowerPC).
 */
#include <math.h>
#include <stdint.h>
#include <string.h>

uint64_t Port_PowDuplo(uint64_t a, uint64_t b) {
    double x, y, r;
    uint64_t v;

    memcpy(&x, &a, 8);
    memcpy(&y, &b, 8);
    r = pow(x, y);
    memcpy(&v, &r, 8);
    return v;
}
