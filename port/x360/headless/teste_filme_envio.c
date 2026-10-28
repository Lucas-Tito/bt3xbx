/* Sem jogo nem GPU: o DMA real de plat_gs.c e o sceGsSyncPath extraído de plat_stub.c. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include "captura_pasta.h"

extern volatile uint32_t *Port_DmaChcr(int channel);
extern int sceGsSyncPath(void);
static unsigned imagem, envios, esperas;

void Port_GsVif1Chain(uint32_t tadr, int tte) { (void)tadr; (void)tte; assert(0); }
void Port_GsGifChannel(uint32_t addr, uint32_t qwc, int chain) {
    assert(addr == 0x12340000u && qwc == 0 && chain == 1);
    assert(imagem == envios + 1);
    envios++;
}
void Port_GsEspera(void) {
    /* A espera pela gravação tem de vir depois da execução da GIF. */
    assert(envios == imagem);
    esperas++;
}
static void envia(unsigned numero) {
    imagem = numero;
    *(volatile uint32_t *)(uintptr_t)0x1000A030u = 0x12340000u;
    *(volatile uint32_t *)(uintptr_t)0x1000A020u = 0;
    *Port_DmaChcr(2) = 0x105;
    sceGsSyncPath();
    assert(envios == numero);
    assert((*Port_DmaChcr(2) & 0x100u) == 0);
    sceGsSyncPath(); /* Não repetir o envio na segunda espera. */
    assert(envios == numero);
}
int main(void) {
    char pasta[40];
    void *p = mmap((void *)(uintptr_t)0x10008000u, 0x8000, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    assert(p == (void *)(uintptr_t)0x10008000u);
    envia(1); /* Primeira imagem depois do boot, sem DMA anterior pendente. */
    envia(2);
    sceGsSyncPath(); /* Fechamento, sem novos envios. */
    envia(3); /* Primeiro envio do segundo filme. */
    assert(esperas == 7);
    assert(munmap(p, 0x8000) == 0);
    assert(captura_pasta(NULL, pasta) && strcmp(pasta, "game:\\shots") == 0);
    assert(captura_pasta("f6e7b_cap_op", pasta) && strcmp(pasta, "game:\\f6e7b_cap_op") == 0);
    assert(captura_pasta("game:\\f6e7b_cap_op", pasta) && strcmp(pasta, "game:\\f6e7b_cap_op") == 0);
    assert(captura_pasta("123456789012345678901234567890", pasta));
    assert(!captura_pasta("1234567890123456789012345678901", pasta));
    assert(!captura_pasta("", pasta));
    assert(!captura_pasta("game:\\", pasta));
    assert(!captura_pasta("..", pasta));
    assert(!captura_pasta("a\\b", pasta));
    assert(!captura_pasta("game:\\game:\\a", pasta));
    assert(!captura_pasta("a/b", pasta));
    puts("tudo certo: primeiro envio, filme seguinte, espera sem duplicação e pastas");
    return 0;
}
