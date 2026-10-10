/*
 * F1: registro da sequência de funções do jogo chamadas, para achar onde a build PowerPC se separa da x86.
 * O código do jogo é compilado com -finstrument-functions (headless.py --chamadas); cada entrada de função
 * grava o endereço dela em BT3_CHAMADAS=<arquivo> (4 bytes, ordem do processador que roda). Depois,
 * port/x360/tools/chamadas.py traduz os endereços em nomes e compara as duas sequências.
 */
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#define SEM __attribute__((no_instrument_function))
#define BUF 4096
static uint32_t sBuf[BUF];
static int sN, sFd = -2;

SEM static void despejar(void) {
    if (sFd >= 0 && sN > 0) {
        ssize_t r = write(sFd, sBuf, (size_t)sN * 4);
        (void)r;
    }
    sN = 0;
}

SEM void __cyg_profile_func_enter(void *fn, void *site) {
    (void)site;
    if (sFd == -2) {
        const char *c = getenv("BT3_CHAMADAS");
        sFd = c != NULL ? open(c, O_WRONLY | O_CREAT | O_TRUNC, 0644) : -1;
        if (sFd >= 0) {
            atexit(despejar);
        }
    }
    if (sFd < 0) {
        return;
    }
    sBuf[sN++] = (uint32_t)(uintptr_t)fn;
    if (sN == BUF) {
        despejar();
    }
}

SEM void __cyg_profile_func_exit(void *fn, void *site) {
    (void)fn;
    (void)site;
}
