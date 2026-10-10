/*
 * F1: rastreio de leituras de dados do disco.
 *
 * Pergunta: de cada arquivo do jogo, quais bytes o código lê como número de 2, 4 ou 8 bytes? É isso que
 * precisa ter a ordem invertida para a build big-endian (PowerPC, Xbox 360). A decomp ainda não catalogou
 * os formatos; em vez de adivinhar, o jogo rodando diz.
 *
 * Como: a build x86 de 32 bits sem janela é compilada com -fsanitize=kernel-address em modo de chamada, e o
 * compilador põe uma chamada __asan_loadN/__asan_storeN antes de cada leitura e escrita de memória. Aqui:
 *   - cada byte lido do disco (Port_FileReadAt) ganha uma origem: (arquivo, deslocamento no arquivo);
 *   - memcpy/memmove levam a origem junto; memset e escrita de outro dado a apagam (o heap é reaproveitado
 *     depois que o arquivo sai, e o dado novo não é do disco); reescrever o próprio campo (deslocamento do
 *     arquivo virando ponteiro) a mantém;
 *   - cópia feita em laço pelo próprio jogo (lê N bytes, grava N bytes) também leva a origem: a leitura
 *     conta como cópia, não como uso;
 *   - a saída do Bpe_Decode (descompressão) vira um arquivo virtual "<origem>#bpe";
 *   - cada leitura de um byte com origem é anotada: (arquivo, deslocamento) -> tamanhos usados.
 * No fim (atexit), BT3_RASTREIO=<arquivo> recebe uma linha por (arquivo, deslocamento):
 *     arquivo  deslocamento  tamanhos  usos  escritas  pcs  cópias  ímpar  desalinhada
 * tamanhos é uma máscara: 1, 2, 4, 8, 16 bytes. port/x360/tools/rastreio.py resume e gera o mapa.
 *
 * Não é usado em nenhuma build que vá para o console.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NAO_RASTREAR __attribute__((no_sanitize("kernel-address"), noinline))

/* ------------------------------------------------------------------ arquivos */

#define MAX_ARQ 16384
static char *sNome[MAX_ARQ];
static uint32_t sArqs = 1; /* 0 = sem origem */

static uint32_t arquivo(const char *nome) {
    uint32_t i;
    for (i = 1; i < sArqs; i++) {
        if (strcmp(sNome[i], nome) == 0) {
            return i;
        }
    }
    if (sArqs == MAX_ARQ) {
        fprintf(stderr, "rastreio: mais de %d arquivos; os seguintes ficam sem origem\n", MAX_ARQ);
        return 0;
    }
    sNome[sArqs] = strdup(nome);
    return sArqs++;
}

/* ------------------------------------------------------------------ sombra: origem de cada byte */

/* origem = arquivo << 40 | deslocamento. Sombra preguiçosa em blocos de 64 KB do espaço de 4 GB. */
#define BLOCO 16
static uint64_t *sSombra[1u << (32 - BLOCO)];
static int sAtivo;

static uint64_t *sombra(uint32_t a, int criar) {
    uint64_t **b = &sSombra[a >> BLOCO];
    if (*b == NULL) {
        if (!criar) {
            return NULL;
        }
        *b = calloc(1u << BLOCO, sizeof(uint64_t));
        if (*b == NULL) {
            return NULL;
        }
    }
    return &(*b)[a & ((1u << BLOCO) - 1)];
}

static uint64_t origem(uint32_t a) {
    uint64_t *s = sombra(a, 0);
    return s != NULL ? *s : 0;
}

static void marcar(uint32_t a, uint64_t o) {
    uint64_t *s = sombra(a, o != 0);
    if (s != NULL) {
        *s = o;
    }
}

/* ------------------------------------------------------------------ anotações */

typedef struct Uso {
    uint64_t chave; /* origem do primeiro byte lido */
    uint32_t usos, escritas;
    uint32_t pcs[4];  /* até 4 funções diferentes que leram (o catálogo precisa de todas, não só da primeira) */
    uint8_t tamanhos; /* máscara das larguras 1, 2, 4, 8, 16 lidas aqui */
    uint8_t copias;   /* larguras com que a posição foi copiada (vai para o relatório, não decide largura) */
    uint8_t impar;    /* lida num tamanho que não é largura de número (3, 5, 6, 12...): trecho, não campo */
    uint8_t desal;    /* lida como número num endereço de memória desalinhado para a largura */
} Uso;

#define TAB (1u << 23)
static Uso *sTab;
static uint32_t sTabN, sPerdidos;

static Uso *uso(uint64_t chave) {
    uint32_t h = (uint32_t)((chave * 0x9E3779B97F4A7C15ull) >> 40) & (TAB - 1);
    if (sTab == NULL) {
        sTab = calloc(TAB, sizeof(Uso));
    }
    while (sTab[h].chave != 0 && sTab[h].chave != chave) {
        h = (h + 1) & (TAB - 1);
    }
    if (sTab[h].chave == 0) {
        if (sTabN >= TAB - TAB / 8) {
            return NULL;
        }
        sTab[h].chave = chave;
        sTabN++;
    }
    return &sTab[h];
}

/* Uma leitura com origem, guardada por um instante: se uma escrita do mesmo tamanho vier logo depois
   gravando o MESMO valor em outro lugar, foi uma cópia (laço do jogo), e a origem passa para o destino.
   A escrita ainda não aconteceu quando o callback roda; a conferência do valor fica para o callback
   seguinte (pendente). Valor diferente: era uma conta, e a leitura é uso de verdade. */
#define ANEL 8
static struct { uint32_t addr, size, pc; int vivo; } sAnel[ANEL];
static int sAnelPos;
static struct { uint32_t dst, src, size, pc; int vivo; } sPend;

static int largura(uint32_t size) {
    return size == 1 || size == 2 || size == 4 || size == 8 || size == 16;
}

static void anotar_pc(Uso *u, uint32_t pc) {
    int k;
    for (k = 0; k < 4; k++) {
        if (u->pcs[k] == pc) {
            return;
        }
        if (u->pcs[k] == 0) {
            u->pcs[k] = pc;
            return;
        }
    }
}

static void anotar_uso(uint32_t addr, uint32_t size, uint32_t pc) {
    uint64_t o = origem(addr);
    Uso *u;
    if (o == 0) {
        return;
    }
    u = uso(o);
    if (u == NULL) {
        sPerdidos++;
        return;
    }
    u->usos++;
    if (!largura(size)) {
        u->impar = 1;
    } else {
        u->tamanhos |= (uint8_t)size;
        if (size > 1 && size <= 8 && (addr & (size - 1)) != 0) {
            u->desal = 1;
        }
    }
    anotar_pc(u, pc);
}

static void anotar_escrita(uint32_t addr, uint32_t size, uint32_t pc) {
    Uso *u;
    if (origem(addr) == 0) {
        return;
    }
    u = uso(origem(addr));
    if (u != NULL) {
        u->escritas++;
        if (largura(size)) {
            u->tamanhos |= (uint8_t)size;
        }
        anotar_pc(u, pc);
    }
}

NAO_RASTREAR static void resolver(void) {
    uint32_t b;
    if (!sPend.vivo) {
        return;
    }
    sPend.vivo = 0;
    if (memcmp((const void *)(uintptr_t)sPend.dst, (const void *)(uintptr_t)sPend.src, sPend.size) == 0) {
        Uso *u = origem(sPend.src) != 0 ? uso(origem(sPend.src)) : NULL;
        if (u != NULL) {
            u->copias |= (uint8_t)sPend.size;
        }
        for (b = 0; b < sPend.size; b++) {
            marcar(sPend.dst + b, origem(sPend.src + b));
        }
    } else {
        anotar_uso(sPend.src, sPend.size, sPend.pc);
        for (b = 0; b < sPend.size; b++) {
            marcar(sPend.dst + b, 0); /* gravou uma conta, não o dado do disco */
        }
    }
}

NAO_RASTREAR static void ler(uint32_t addr, uint32_t size, uint32_t pc) {
    int i;
    resolver();
    if (!sAtivo || origem(addr) == 0) {
        return;
    }
    /* a leitura mais velha do anel que não virou cópia é uso de verdade */
    i = sAnelPos;
    if (sAnel[i].vivo) {
        anotar_uso(sAnel[i].addr, sAnel[i].size, sAnel[i].pc);
    }
    sAnel[i].addr = addr;
    sAnel[i].size = size;
    sAnel[i].pc = pc;
    sAnel[i].vivo = 1;
    sAnelPos = (sAnelPos + 1) % ANEL;
}

NAO_RASTREAR static void gravar(uint32_t addr, uint32_t size, uint32_t pc) {
    int k, i;
    resolver();
    if (!sAtivo) {
        return;
    }
    /* no mesmo lugar: o jogo reescrevendo o campo (deslocamento do arquivo virando ponteiro) */
    for (k = 0; k < ANEL; k++) {
        if (sAnel[k].vivo && sAnel[k].addr == addr) {
            anotar_uso(sAnel[k].addr, sAnel[k].size, sAnel[k].pc);
            sAnel[k].vivo = 0;
            anotar_escrita(addr, size, pc);
            return;
        }
    }
    /* candidata a cópia: a leitura mais velha do mesmo tamanho; confere o valor no próximo callback */
    for (k = 0; k < ANEL; k++) {
        i = (sAnelPos + k) % ANEL;
        if (sAnel[i].vivo && sAnel[i].size == size) {
            sPend.dst = addr;
            sPend.src = sAnel[i].addr;
            sPend.size = size;
            sPend.pc = sAnel[i].pc;
            sPend.vivo = 1;
            sAnel[i].vivo = 0;
            return;
        }
    }
    /* escrita de outro dado: a memória deixa de ser do disco (o heap é reaproveitado depois que o arquivo sai) */
    for (k = 0; k < (int)size; k++) {
        marcar(addr + (uint32_t)k, 0);
    }
}

#define PC ((uint32_t)(uintptr_t)__builtin_return_address(0))
#define LOAD(n) NAO_RASTREAR void __asan_load##n##_noabort(uintptr_t a) { ler((uint32_t)a, n, PC); } \
                NAO_RASTREAR void __asan_store##n##_noabort(uintptr_t a) { gravar((uint32_t)a, n, PC); }
LOAD(1) LOAD(2) LOAD(4) LOAD(8) LOAD(16)
NAO_RASTREAR void __asan_loadN_noabort(uintptr_t a, size_t n) { ler((uint32_t)a, (uint32_t)n, PC); }
NAO_RASTREAR void __asan_storeN_noabort(uintptr_t a, size_t n) {
    size_t i;
    resolver();
    for (i = 0; i < n; i++) {
        marcar((uint32_t)a + (uint32_t)i, 0);
    }
}
NAO_RASTREAR void __asan_handle_no_return(void) {}
NAO_RASTREAR void __asan_init(void) {}
NAO_RASTREAR void __asan_version_mismatch_check_v8(void) {}
NAO_RASTREAR void __asan_register_globals(void *g, size_t n) { (void)g; (void)n; }
NAO_RASTREAR void __asan_unregister_globals(void *g, size_t n) { (void)g; (void)n; }

/* ------------------------------------------------------------------ cópias e origens */

extern void *__real_memcpy(void *, const void *, size_t);
extern void *__real_memmove(void *, const void *, size_t);
extern void *__real_memset(void *, int, size_t);

NAO_RASTREAR static void copiar_origem(uint32_t dst, uint32_t src, size_t n) {
    size_t i;
    resolver();
    if (!sAtivo) {
        return;
    }
    if (dst > src && dst < src + n) {
        for (i = n; i-- > 0;) {
            marcar(dst + (uint32_t)i, origem(src + (uint32_t)i));
        }
    } else {
        for (i = 0; i < n; i++) {
            marcar(dst + (uint32_t)i, origem(src + (uint32_t)i));
        }
    }
}

NAO_RASTREAR void *__wrap_memcpy(void *d, const void *s, size_t n) {
    copiar_origem((uint32_t)(uintptr_t)d, (uint32_t)(uintptr_t)s, n);
    return __real_memcpy(d, s, n);
}
NAO_RASTREAR void *__wrap_memmove(void *d, const void *s, size_t n) {
    copiar_origem((uint32_t)(uintptr_t)d, (uint32_t)(uintptr_t)s, n);
    return __real_memmove(d, s, n);
}
NAO_RASTREAR void *__wrap_memset(void *d, int c, size_t n) {
    size_t i;
    resolver();
    if (sAtivo) {
        for (i = 0; i < n; i++) {
            marcar((uint32_t)(uintptr_t)d + (uint32_t)i, 0);
        }
    }
    return __real_memset(d, c, n);
}
NAO_RASTREAR void *__asan_memcpy(void *d, const void *s, size_t n) { return __wrap_memcpy(d, s, n); }
NAO_RASTREAR void *__asan_memmove(void *d, const void *s, size_t n) { return __wrap_memmove(d, s, n); }
NAO_RASTREAR void *__asan_memset(void *d, int c, size_t n) { return __wrap_memset(d, c, n); }

extern size_t __real_Port_FileReadAt(const char *rel, long offset, void *buf, size_t bytes);
NAO_RASTREAR size_t __wrap_Port_FileReadAt(const char *rel, long offset, void *buf, size_t bytes) {
    size_t got, i;
    uint64_t f;
    resolver();
    got = __real_Port_FileReadAt(rel, offset, buf, bytes);
    if (sAtivo && got > 0) {
        f = (uint64_t)arquivo(rel) << 40;
        for (i = 0; i < got; i++) {
            marcar((uint32_t)(uintptr_t)buf + (uint32_t)i, f | (uint64_t)(offset + (long)i));
        }
    }
    return got;
}

extern void *__real_Bpe_Decode(void *src, void *dst, int32_t *rawSize);
NAO_RASTREAR void *__wrap_Bpe_Decode(void *src, void *dst, int32_t *rawSize) {
    uint64_t o = origem((uint32_t)(uintptr_t)src);
    int32_t tam;
    void *out;
    if (rawSize == NULL) {
        rawSize = &tam; /* quem chama nem sempre pede o tamanho; aqui ele é preciso */
    }
    out = __real_Bpe_Decode(src, dst, rawSize);
    if (sAtivo && o != 0 && out != NULL && rawSize != NULL && *rawSize > 0) {
        char nome[256];
        uint64_t f;
        int32_t i;
        snprintf(nome, sizeof(nome), "%s@0x%llX#bpe", sNome[o >> 40], (unsigned long long)(o & 0xFFFFFFFFFFull));
        f = (uint64_t)arquivo(nome) << 40;
        for (i = 0; i < *rawSize; i++) {
            marcar((uint32_t)(uintptr_t)out + (uint32_t)i, f | (uint64_t)i);
        }
    }
    return out;
}

/* ------------------------------------------------------------------ saída */

NAO_RASTREAR static void despejar(void) {
    const char *caminho = getenv("BT3_RASTREIO");
    FILE *fp;
    uint32_t i;
    int k;
    if (caminho == NULL || sTab == NULL) {
        return;
    }
    resolver();
    sAtivo = 0;
    for (k = 0; k < ANEL; k++) {
        if (sAnel[k].vivo) {
            anotar_uso(sAnel[k].addr, sAnel[k].size, sAnel[k].pc);
        }
    }
    fp = fopen(caminho, "w");
    if (fp == NULL) {
        return;
    }
    for (i = 0; i < TAB; i++) {
        Uso *u = &sTab[i];
        if (u->chave != 0) {
            fprintf(fp, "%s\t%llu\t%u\t%u\t%u\t%08x,%08x,%08x,%08x\t%u\t%u\t%u\n", sNome[u->chave >> 40],
                    (unsigned long long)(u->chave & 0xFFFFFFFFFFull), u->tamanhos, u->usos, u->escritas, u->pcs[0], u->pcs[1],
                    u->pcs[2], u->pcs[3], u->copias, u->impar, u->desal);
        }
    }
    fclose(fp);
    fprintf(stderr, "rastreio: %u posições de %u arquivos em %s%s\n", sTabN, sArqs - 1, caminho,
            sPerdidos || sArqs == MAX_ARQ ? " (TABELA CHEIA: usos ou arquivos perdidos)" : "");
}

/* As tabelas de dados do executável (vindas do disco do usuário): origem "ELF:<objeto>.<seção>", com as faixas
   que headless.py tirou do mapa do linker para <programa>.elfdata. */
NAO_RASTREAR static void marcar_elf(void) {
    char exe[512], linha[300], nome[256];
    unsigned long a, n, i;
    ssize_t k = readlink("/proc/self/exe", exe, sizeof(exe) - 16);
    FILE *fp;
    if (k <= 0) {
        return;
    }
    exe[k] = '\0';
    strcat(exe, ".elfdata");
    fp = fopen(exe, "r");
    if (fp == NULL) {
        fprintf(stderr, "rastreio: sem %s (tabelas do executável sem origem)\n", exe);
        return;
    }
    while (fgets(linha, sizeof(linha), fp) != NULL) {
        if (sscanf(linha, "%250s %lx %lx", nome, &a, &n) == 3) {
            char rotulo[300];
            uint64_t f;
            snprintf(rotulo, sizeof(rotulo), "ELF:%s", nome);
            f = (uint64_t)arquivo(rotulo) << 40;
            for (i = 0; i < n; i++) {
                marcar((uint32_t)(a + i), f | i);
            }
        }
    }
    fclose(fp);
}

__attribute__((constructor)) NAO_RASTREAR static void iniciar(void) {
    sAtivo = getenv("BT3_RASTREIO") != NULL;
    if (sAtivo) {
        marcar_elf();
        atexit(despejar);
    }
}
