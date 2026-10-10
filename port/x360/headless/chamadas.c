/*
 * F1: registro da sequência de funções do jogo chamadas, para achar onde a build PowerPC se separa da x86.
 * O código do jogo é compilado com -finstrument-functions (headless.py --chamadas); cada entrada de função
 * grava o endereço dela em BT3_CHAMADAS=<arquivo> (4 bytes, ordem do processador que roda). Depois,
 * port/x360/tools/chamadas.py traduz os endereços em nomes e compara as duas sequências.
 */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SEM __attribute__((no_instrument_function))
#define BUF 64 /* pequeno: num crash o que fica no buffer se perde, e o fim é o que interessa */
static uint32_t sBuf[BUF];
static int sN, sFd = -2;

/* BT3_VIGIAR=<endereço>: a cada entrada de função confere uma palavra da memória e avisa (stderr) cada vez
   que ela muda, com a função que entrou e a anterior: acha quem escreve num lugar sem watchpoint. */
static volatile uint32_t *sVigia;
static uint32_t sVigiaAntes, sAnterior;

SEM static void despejar(void) {
    if (sFd >= 0 && sN > 0) {
        ssize_t r = write(sFd, sBuf, (size_t)sN * 4);
        (void)r;
    }
    sN = 0;
}

SEM void __cyg_profile_func_enter(void *fn, void *site) {
    (void)site;
    if (sVigia != NULL && *sVigia != sVigiaAntes) {
        fprintf(stderr, "vigia: %08x -> %08x ao entrar em %p (antes: %08x, chamada de %p)\n", sVigiaAntes, *sVigia, fn,
                sAnterior, site);
        sVigiaAntes = *sVigia;
    }
    sAnterior = (uint32_t)(uintptr_t)fn;
    if (sFd == -2) {
        const char *c = getenv("BT3_CHAMADAS");
        const char *v = getenv("BT3_VIGIAR");
        if (v != NULL) {
            sVigia = (volatile uint32_t *)(uintptr_t)strtoul(v, NULL, 0);
            sVigiaAntes = 0;
        }
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
