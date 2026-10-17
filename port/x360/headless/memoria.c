/* F5: testemunho das subalocações, independente em cada arquitetura, no mesmo vblank do dump.
   Só código de teste: a arena do jogo não recebe cabeçalhos nem muda de endereço. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

extern unsigned gPortVBlanks;
extern void *gBtlPool;
extern void *__real_BtlPool_Alloc(int slot, int size);
extern void __real_BtlPool_Reset(int slot);
extern void __real_BtlPool_Free(int slot, void *ptr);
extern void __real_BtlPool_Init(int arg);
extern void __real_BtlPool_Term(void);
extern void __real_Port_StateLog(unsigned vb, const void *fight, unsigned size);

typedef struct { unsigned slot, geracao, ordem, endereco, tamanho, dono; } Registro;
static Registro registros[2048];
static unsigned total, geracao[9], ordem[9];
static int ativo = -1, falha;
static int ligado(void) {
    if (ativo < 0) ativo = getenv("BT3_HASH_AT") != NULL && getenv("BT3_HASH") != NULL;
    return ativo;
}
void __wrap_BtlPool_Init(int arg) {
    unsigned i;
    __real_BtlPool_Init(arg);
    total = 0;
    for (i = 0; i < 9; i++) { geracao[i] = 0; ordem[i] = 0; }
}
void __wrap_BtlPool_Term(void) { __real_BtlPool_Term(); total = 0; }
void *__wrap_BtlPool_Alloc(int slot, int size) {
    void *p = __real_BtlPool_Alloc(slot, size);
    if (ligado()) {
        if (slot < 0 || slot >= 9 || total >= 2048 || size <= 0) falha = 1;
        else {
            Registro *r = &registros[total++];
            r->slot = slot; r->geracao = geracao[slot]; r->ordem = ordem[slot]++;
            r->endereco = (uintptr_t)p; r->tamanho = size;
            r->dono = (uintptr_t)__builtin_return_address(0);
        }
    }
    return p;
}
void __wrap_BtlPool_Free(int slot, void *ptr) {
    unsigned i, j = 0;
    int arena = gBtlPool && slot >= 0 && slot < 9 && (((unsigned *)gBtlPool)[0x98/4] & (2u << slot));
    __real_BtlPool_Free(slot, ptr);
    if (ligado() && !arena) {
        for (i = 0; i < total; i++) if (registros[i].endereco != (unsigned)(uintptr_t)ptr) registros[j++] = registros[i];
        total = j;
    }
}
void __wrap_BtlPool_Reset(int slot) {
    unsigned i, j = 0;
    __real_BtlPool_Reset(slot);
    if (ligado()) {
        if (slot < 0 || slot >= 9) { falha = 1; return; }
        if (!gBtlPool || !(((unsigned *)gBtlPool)[0x98/4] & (2u << slot))) return;
        geracao[slot]++; ordem[slot] = 0;
        for (i = 0; i < total; i++) if (registros[i].slot != (unsigned)slot) registros[j++] = registros[i];
        total = j;
    }
}
void __wrap_Port_StateLog(unsigned vb, const void *fight, unsigned size) {
    if (ligado() && vb == (unsigned)atoi(getenv("BT3_HASH_AT"))) {
        char path[1200];
        FILE *fp;
        unsigned i;
        snprintf(path, sizeof(path), "%s.pool", getenv("BT3_HASH"));
        fp = fopen(path, "w");
        if (fp) {
            fprintf(fp, "POOL 1 %u %u %u\n", vb, (unsigned)(uintptr_t)gBtlPool, (unsigned)falha);
            for (i = 0; i < 9; i++) fprintf(fp, "S %u %u %u\n", i, geracao[i], ordem[i]);
            for (i = 0; i < total; i++) {
                Registro *r = &registros[i];
                fprintf(fp, "A %u %u %u %u %u %u\n", r->slot, r->geracao, r->ordem,
                        r->endereco, r->tamanho, r->dono);
            }
            fclose(fp);
        }
    }
    __real_Port_StateLog(vb, fight, size);
    /* As rodadas próprias de memória param depois do dump; a rodada que prova o roteiro inteiro continua. */
    if (ligado() && vb == (unsigned)atoi(getenv("BT3_HASH_AT")) && getenv("BT3_MEMORIA_SAIR")) exit(0);
}
