/* F5: testemunho das subalocações, independente em cada arquitetura, no mesmo vblank do dump.
   Só código de teste: a arena do jogo não recebe cabeçalhos nem muda de endereço. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#define XXH_INLINE_ALL
#include "../../third_party/xxhash/xxhash.h"

/* BT3_MEMORIAS: uma linha "vblank caminho_do_hash" por ponto, sem encerrar o roteiro.
   As regiões e o formato são os do state.c. Só memória de teste, fora do heap do jogo. */
static struct { unsigned vb; char caminho[1000]; int feita; } pontos[128];
static int npontos = -1;
static void iniciar_pontos(void) {
    const char *lista = getenv("BT3_MEMORIAS");
    FILE *fp;
    char linha[1200], extra;
    npontos = 0;
    if (!lista) return;
    fp = fopen(lista, "r");
    if (!fp) { perror(lista); exit(2); }
    while (fgets(linha, sizeof(linha), fp)) {
        if (npontos == 128 || sscanf(linha, "%u %999s %c", &pontos[npontos].vb,
                                    pontos[npontos].caminho, &extra) != 2 || !pontos[npontos].vb) {
            fprintf(stderr, "memorias: FALHA: lista inválida ou mais de 128 pontos\n"); exit(2);
        }
        npontos++;
    }
    if (ferror(fp) || fclose(fp) || !npontos) { fprintf(stderr, "memorias: FALHA: lista vazia ou ilegível\n"); exit(2); }
}
static void regiao(FILE *dump, FILE *pages, const char *nome, const uint8_t *p, size_t n) {
    size_t off;
    if (fprintf(dump, "%s %llx %llx\n", nome, (unsigned long long)(uintptr_t)p,
                (unsigned long long)n) < 0 || fwrite(p, 1, n, dump) != n) {
        fprintf(stderr, "memorias: FALHA: escrita do dump\n"); exit(2);
    }
    for (off = 0; off < n; off += 4096) {
        size_t tam = n - off < 4096 ? n - off : 4096;
        if (fprintf(pages, "%s %llx %016llx\n", nome, (unsigned long long)(uintptr_t)(p + off),
                    (unsigned long long)XXH3_64bits(p + off, tam)) < 0) exit(2);
    }
}
static void dump_ponto(const char *destino) {
    char exe[1024], caminho[1100];
    long n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    FILE *mapa, *dump, *pages;
    unsigned long long a, tam;
    uint8_t *heap;
    uint32_t tamanho;
    unsigned regioes = 0;
    extern void Port_HeapRegion(uint8_t **base, uint32_t *size);
    if (n <= 0 || n >= (long)sizeof(exe) - 1) exit(2);
    exe[n] = 0;
    snprintf(caminho, sizeof(caminho), "%s.mem", exe);
    mapa = fopen(caminho, "r");
    snprintf(caminho, sizeof(caminho), "%s.dump", destino);
    dump = fopen(caminho, "wb");
    snprintf(caminho, sizeof(caminho), "%s.pages", destino);
    pages = fopen(caminho, "w");
    if (!mapa || !dump || !pages) { fprintf(stderr, "memorias: FALHA: abertura das regiões/saídas\n"); exit(2); }
    while (fscanf(mapa, "%llx %llx", &a, &tam) == 2) {
        if (++regioes > 62) exit(2);
        regiao(dump, pages, "globals", (const uint8_t *)(uintptr_t)a, (size_t)tam);
    }
    if (!regioes || !feof(mapa) || fclose(mapa)) exit(2);
    Port_HeapRegion(&heap, &tamanho);
    regiao(dump, pages, "heap", heap, tamanho);
    regiao(dump, pages, "scratchpad", (const uint8_t *)(uintptr_t)0x70000000u, 0x4000);
    if (fclose(dump) || fclose(pages)) exit(2);
}

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
    if (ativo < 0) {
        iniciar_pontos();
        ativo = npontos > 0 || (getenv("BT3_HASH_AT") != NULL && getenv("BT3_HASH") != NULL);
    }
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
static void grava_pool(unsigned vb, const char *destino) {
        char path[1200];
        FILE *fp;
        unsigned i;
        snprintf(path, sizeof(path), "%s.pool", destino);
        fp = fopen(path, "w");
        if (fp) {
            fprintf(fp, "POOL 1 %u %u %u\n", vb, (unsigned)(uintptr_t)gBtlPool, (unsigned)falha);
            for (i = 0; i < 9; i++) fprintf(fp, "S %u %u %u\n", i, geracao[i], ordem[i]);
            for (i = 0; i < total; i++) {
                Registro *r = &registros[i];
                fprintf(fp, "A %u %u %u %u %u %u\n", r->slot, r->geracao, r->ordem,
                        r->endereco, r->tamanho, r->dono);
            }
            if (ferror(fp) || fclose(fp)) exit(2);
        } else exit(2);
}
void __wrap_Port_StateLog(unsigned vb, const void *fight, unsigned size) {
    int i;
    if (ligado()) {
        for (i = 0; i < npontos; i++) if (vb == pontos[i].vb && !pontos[i].feita) {
            grava_pool(vb, pontos[i].caminho);
            dump_ponto(pontos[i].caminho);
            pontos[i].feita = 1;
        }
        if (getenv("BT3_HASH_AT") && vb == (unsigned)atoi(getenv("BT3_HASH_AT")))
            grava_pool(vb, getenv("BT3_HASH"));
    }
    __real_Port_StateLog(vb, fight, size);
    /* As rodadas próprias de memória param depois do dump; a rodada que prova o roteiro inteiro continua. */
    if (ligado() && getenv("BT3_HASH_AT") && vb == (unsigned)atoi(getenv("BT3_HASH_AT")) && getenv("BT3_MEMORIA_SAIR")) exit(0);
}
