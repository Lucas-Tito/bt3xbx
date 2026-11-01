/*
 * O mínimo de C que o teste_explosao precisa para ligar como binário de 32 bits sem libc (a máquina de
 * desenvolvimento não tem a libc nem a libgcc de 32 bits): _start, write / exit pelo int 0x80, as funções de
 * memória, o getenv do mode() do softfloat (sem ambiente) e a divisão de 64 bits que o gcc -m32 chama.
 * Usado por port/x360/tools/teste_explosao.py.
 */
#include <stddef.h>
#include <stdint.h>

static long sys3(long n, long a, long b, long c) {
    long r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return r;
}

int write(int fd, const void *p, unsigned n) { return (int)sys3(4, fd, (long)p, (long)n); }
void _exit(int c) {
    for (;;) {
        sys3(1, c, 0, 0);
    }
}
void exit(int c) { _exit(c); }
void abort(void) { _exit(134); }

void *memcpy(void *d, const void *s, size_t n) {
    unsigned char *a = d;
    const unsigned char *b = s;
    while (n--) {
        *a++ = *b++;
    }
    return d;
}
void *memmove(void *d, const void *s, size_t n) {
    unsigned char *a = d;
    const unsigned char *b = s;
    if (a < b) {
        while (n--) {
            *a++ = *b++;
        }
    } else {
        while (n--) {
            a[n] = b[n];
        }
    }
    return d;
}
void *memset(void *d, int c, size_t n) {
    unsigned char *a = d;
    while (n--) {
        *a++ = (unsigned char)c;
    }
    return d;
}
int memcmp(const void *x, const void *y, size_t n) {
    const unsigned char *a = x, *b = y;
    for (; n; n--, a++, b++) {
        if (*a != *b) {
            return *a < *b ? -1 : 1;
        }
    }
    return 0;
}
size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}
char *getenv(const char *n) {
    (void)n;
    return NULL;
}

/* a divisão de 64 bits sem sinal, bit a bit */
static uint64_t udivmod(uint64_t a, uint64_t b, uint64_t *r) {
    uint64_t q = 0, rem = 0;
    int i;
    for (i = 63; i >= 0; i--) {
        rem = (rem << 1) | ((a >> i) & 1);
        if (rem >= b) {
            rem -= b;
            q |= (uint64_t)1 << i;
        }
    }
    if (r) {
        *r = rem;
    }
    return q;
}
uint64_t __udivdi3(uint64_t a, uint64_t b) { return udivmod(a, b, NULL); }
uint64_t __umoddi3(uint64_t a, uint64_t b) {
    uint64_t r;
    udivmod(a, b, &r);
    return r;
}
int64_t __divdi3(int64_t a, int64_t b) {
    int neg = (a < 0) ^ (b < 0);
    uint64_t q = udivmod(a < 0 ? -(uint64_t)a : (uint64_t)a, b < 0 ? -(uint64_t)b : (uint64_t)b, NULL);
    return neg ? -(int64_t)q : (int64_t)q;
}
int64_t __moddi3(int64_t a, int64_t b) {
    uint64_t r;
    udivmod(a < 0 ? -(uint64_t)a : (uint64_t)a, b < 0 ? -(uint64_t)b : (uint64_t)b, &r);
    return a < 0 ? -(int64_t)r : (int64_t)r;
}
uint64_t __udivmoddi4(uint64_t a, uint64_t b, uint64_t *r) { return udivmod(a, b, r); }

int main(int argc, char **argv);

__asm__(".globl _start\n"
        "_start:\n"
        "  xor %ebp, %ebp\n"
        "  mov (%esp), %eax\n"
        "  lea 4(%esp), %edx\n"
        "  and $-16, %esp\n"
        "  sub $8, %esp\n"
        "  push %edx\n"
        "  push %eax\n"
        "  call main\n"
        "  push %eax\n"
        "  call _exit\n");
