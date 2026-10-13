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
 *     arquivo  deslocamento  tamanhos  usos  escritas  pcs  cópias  ímpar  desalinhada  pcs-de-byte
 * tamanhos é uma máscara: 1, 2, 4, 8, 16 bytes. port/x360/tools/rastreio.py resume e gera o mapa.
 *
 * Não é usado em nenhuma build que vá para o console.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
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
    uint32_t pcb[4];  /* até 4 que leram como byte (rastreio.py vê se são campos de structs já corrigidas) */
    uint8_t tamanhos; /* máscara das larguras 1, 2, 4, 8, 16 lidas aqui */
    uint8_t copias;   /* larguras com que a posição foi copiada (vai para o relatório, não decide largura) */
    uint8_t impar;    /* lida num tamanho que não é largura de número (3, 5, 6, 12...): trecho, não campo */
    uint8_t desal;    /* lida como número num endereço de memória desalinhado para a largura */
} Uso;

/* 2^23 posições por padrão (uns 470 MB); BT3_RASTREIO_BITS=24 ou 25 para rodadas que leem mais (os roteiros de
   controle dos menus, F4 etapa 4, enchem a de 2^23 antes da luta) */
static uint32_t TAB = 1u << 23, sTabBits = 23;
static Uso *sTab;
static uint32_t sTabN, sPerdidos;

static Uso *uso(uint64_t chave) {
    uint32_t h;
    if (sTab == NULL) {
        const char *e = getenv("BT3_RASTREIO_BITS");
        if (e != NULL && atoi(e) >= 16 && atoi(e) <= 28) {
            sTabBits = (uint32_t)atoi(e);
            TAB = 1u << sTabBits;
        }
        sTab = calloc(TAB, sizeof(Uso));
    }
    /* (até 24 bits, o mesmo espalhamento de sempre: a ordem do rastreio.tsv da F1 não muda) */
    h = (uint32_t)((chave * 0x9E3779B97F4A7C15ull) >> (sTabBits <= 24 ? 40 : 64 - sTabBits)) & (TAB - 1);
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

/* ------------------------------------------------------------------ detector de visões (BT3_VISOES)

   Acha a classe 3 da decisão 7 sem esperar crash: memória gravada com uma largura e lida com outra (s32 gravado,
   u8 lido no mesmo byte; dois u16 gravados, um u32 lido). Num host big-endian o resultado muda. Para cada byte,
   a "forma" da última gravação (largura e posição dentro dela), o valor gravado e a função que gravou; cópias
   levam os três junto; dado do disco, das tabelas do executável e memset ficam neutros (são tratados por
   conversão). Byte cujo valor atual não é o gravado foi escrito por código sem instrumentação (struct passada por
   valor, libc, port): também neutro. Cada leitura com forma diferente da gravação é anotada por (função que
   gravou, função que leu). */
static int sVisoes;
static uint16_t *sForma[1u << (32 - BLOCO)]; /* valor gravado << 8 | forma */
static uint32_t *sQuem[1u << (32 - BLOCO)];

static uint8_t forma(uint32_t a) {
    uint16_t *b = sForma[a >> BLOCO];
    uint16_t f;
    if (b == NULL) {
        return 0;
    }
    f = b[a & ((1u << BLOCO) - 1)];
    if ((f & 0xFF) == 0 || (f >> 8) != *(volatile uint8_t *)(uintptr_t)a) {
        return 0;
    }
    return (uint8_t)f;
}

/* v = forma; o valor é o byte que está (ou vai estar, numa cópia) na memória: fixado depois (fixar_valor). */
static void por_forma(uint32_t a, uint8_t v, uint8_t valor) {
    uint16_t **b = &sForma[a >> BLOCO];
    if (*b == NULL) {
        if (v == 0) {
            return;
        }
        *b = calloc(1u << BLOCO, sizeof(uint16_t));
        if (*b == NULL) {
            return;
        }
    }
    (*b)[a & ((1u << BLOCO) - 1)] = (uint16_t)(valor << 8 | v);
}

static uint32_t quem(uint32_t a) {
    uint32_t *b = sQuem[a >> BLOCO];
    return b != NULL ? b[a & ((1u << BLOCO) - 1)] : 0;
}

static void por_quem(uint32_t a, uint32_t pc) {
    uint32_t **b = &sQuem[a >> BLOCO];
    if (*b == NULL) {
        if (pc == 0) {
            return;
        }
        *b = calloc(1u << BLOCO, sizeof(uint32_t));
        if (*b == NULL) {
            return;
        }
    }
    (*b)[a & ((1u << BLOCO) - 1)] = pc;
}

/* A gravação ainda não aconteceu quando o callback roda: o valor de cada byte é lido no callback seguinte. */
static struct { uint32_t addr, size; int vivo; } sGravada;

NAO_RASTREAR static void fixar_valor(void) {
    uint32_t k;
    if (!sGravada.vivo) {
        return;
    }
    sGravada.vivo = 0;
    for (k = 0; k < sGravada.size; k++) {
        uint32_t a = sGravada.addr + k;
        uint16_t *b = sForma[a >> BLOCO];
        if (b != NULL && (b[a & ((1u << BLOCO) - 1)] & 0xFF) != 0) {
            b[a & ((1u << BLOCO) - 1)] = (uint16_t)(*(volatile uint8_t *)(uintptr_t)a << 8 | (b[a & ((1u << BLOCO) - 1)] & 0xFF));
        }
    }
}

static int log2w(uint32_t n) {
    return n == 1 ? 0 : n == 2 ? 1 : n == 4 ? 2 : n == 8 ? 3 : n == 16 ? 4 : -1;
}

/* forma = 0x80 | log2(largura) << 4 | posição */
static void forma_gravada(uint32_t addr, uint32_t size, uint32_t pc) {
    int l = log2w(size);
    uint32_t k;
    if (!sVisoes) {
        return;
    }
    fixar_valor();
    for (k = 0; k < size; k++) {
        por_forma(addr + k, l < 0 ? 0 : (uint8_t)(0x80 | l << 4 | k), 0);
        por_quem(addr + k, l < 0 ? 0 : pc);
    }
    sGravada.addr = addr;
    sGravada.size = size;
    sGravada.vivo = l >= 0;
}

typedef struct Visao { uint32_t gravou, leu; uint8_t lw, gw, pos; uint32_t n; } Visao;
#define VTAB (1u << 16)
static Visao *sVis;

/* 1 se a leitura não tem a forma da gravação (preenche *v); 0 se está certa ou não dá para julgar. */
NAO_RASTREAR static int forma_lida(uint32_t addr, uint32_t size, uint32_t pc, Visao *v) {
    uint8_t f0, f;
    uint32_t k, gw, base;
    int igual = 1, simetrico = 1;
    if (!sVisoes || log2w(size) < 0) {
        return 0;
    }
    f0 = forma(addr);
    if (f0 == 0) {
        return 0;
    }
    for (k = 0; k < size; k++) {
        f = forma(addr + k);
        if (f == 0) {
            return 0; /* parte neutra: não dá para julgar */
        }
        if (f != (uint8_t)(0x80 | log2w(size) << 4 | k)) {
            igual = 0;
        }
    }
    if (igual) {
        return 0;
    }
    gw = 1u << ((f0 >> 4) & 7);
    if (gw > size) {
        /* leitura de s bytes na posição p de uma gravação de gw: no big-endian ela vale o número que o
           little-endian monta com os bytes [gw-p-s, gw-p) da gravação; se forem os mesmos bytes de [p, p+s),
           na mesma ordem (0, 0xFFFFFFFF...), não muda nada */
        uint32_t p = f0 & 15;
        base = addr - p;
        if (p + size <= gw) {
            for (k = 0; k < size; k++) {
                if (*(uint8_t *)(uintptr_t)(base + p + k) != *(uint8_t *)(uintptr_t)(base + gw - p - size + k)) {
                    simetrico = 0;
                }
            }
            if (simetrico) {
                return 0;
            }
        }
    } else if ((f0 & 15) == 0 && size % gw == 0) {
        /* leitura larga sobre gravações inteiras de gw (dois u16 lidos como u32): no big-endian os pedaços
           aparecem em ordem inversa; se o pedaço i for igual ao n-1-i, o número é o mesmo */
        uint32_t n = size / gw, i;
        for (i = 0; i < n; i++) {
            if (forma(addr + i * gw) != (uint8_t)(f0 & 0xF0)) {
                simetrico = 0; /* não são gravações inteiras da mesma largura: não dá para comparar pedaços */
            }
        }
        for (i = 0; simetrico && i < n / 2; i++) {
            if (memcmp((const void *)(uintptr_t)(addr + i * gw), (const void *)(uintptr_t)(addr + (n - 1 - i) * gw), gw) != 0) {
                simetrico = 0;
            }
        }
        if (simetrico) {
            return 0;
        }
    }
    v->gravou = quem(addr);
    v->leu = pc;
    v->lw = (uint8_t)size;
    v->gw = (uint8_t)gw;
    v->pos = f0 & 15;
    return 1;
}

NAO_RASTREAR static void anotar_visao(const Visao *v) {
    uint32_t h;
    if (sVis == NULL) {
        sVis = calloc(VTAB, sizeof(Visao));
    }
    h = ((v->gravou * 2654435761u) ^ (v->leu * 40503u) ^ v->lw) & (VTAB - 1);
    while (sVis[h].n != 0 && !(sVis[h].gravou == v->gravou && sVis[h].leu == v->leu && sVis[h].lw == v->lw)) {
        h = (h + 1) & (VTAB - 1);
    }
    {
        uint32_t n = sVis[h].n;
        sVis[h] = *v;
        sVis[h].n = n + 1;
    }
}

/* Antes da cópia (memcpy) ou depois dela (cópia em laço já gravada): o valor de cada byte é o da origem. */
NAO_RASTREAR static void copiar_um(uint32_t d, uint32_t s) {
    uint8_t f = forma(s);
    por_forma(d, f, *(volatile uint8_t *)(uintptr_t)s);
    por_quem(d, f ? quem(s) : 0);
}

NAO_RASTREAR static void copiar_forma(uint32_t dst, uint32_t src, size_t n) {
    size_t i;
    if (!sVisoes) {
        return;
    }
    fixar_valor();
    if (dst > src && dst < src + n) {
        for (i = n; i-- > 0;) {
            copiar_um(dst + (uint32_t)i, src + (uint32_t)i);
        }
    } else {
        for (i = 0; i < n; i++) {
            copiar_um(dst + (uint32_t)i, src + (uint32_t)i);
        }
    }
}

NAO_RASTREAR static void neutro(uint32_t dst, size_t n) {
    size_t i;
    if (!sVisoes) {
        return;
    }
    fixar_valor();
    for (i = 0; i < n; i++) {
        por_forma(dst + (uint32_t)i, 0, 0);
    }
}

/* Uma leitura com origem, guardada por um instante: se uma escrita do mesmo tamanho vier logo depois
   gravando o MESMO valor em outro lugar, foi uma cópia (laço do jogo), e a origem passa para o destino.
   A escrita ainda não aconteceu quando o callback roda; a conferência do valor fica para o callback
   seguinte (pendente). Valor diferente: era uma conta, e a leitura é uso de verdade. */
#define ANEL 32
static struct { uint32_t addr, size, pc; int vivo, sus; Visao v; } sAnel[ANEL];
static int sAnelPos;
static struct { uint32_t dst, size, pc; int vivo; } sPend;

static int largura(uint32_t size) {
    return size == 1 || size == 2 || size == 4 || size == 8 || size == 16;
}

static void anotar_em(uint32_t *pcs, uint32_t pc) {
    int k;
    for (k = 0; k < 4; k++) {
        if (pcs[k] == pc) {
            return;
        }
        if (pcs[k] == 0) {
            pcs[k] = pc;
            return;
        }
    }
}

static void anotar_pc(Uso *u, uint32_t pc) {
    anotar_em(u->pcs, pc);
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
        if (size == 1) {
            anotar_em(u->pcb, pc);
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
    int k, i;
    fixar_valor();
    if (!sPend.vivo) {
        return;
    }
    sPend.vivo = 0;
    /* a gravação já aconteceu: procura no anel, da mais nova para a mais velha, a leitura do mesmo tamanho cujo
       valor é o que foi gravado; achou, é cópia (a origem e a forma passam para o destino) */
    for (k = 1; k <= ANEL; k++) {
        i = (sAnelPos - k + ANEL) % ANEL;
        if (sAnel[i].vivo && sAnel[i].size == sPend.size &&
            memcmp((const void *)(uintptr_t)sPend.dst, (const void *)(uintptr_t)sAnel[i].addr, sPend.size) == 0) {
            Uso *u = origem(sAnel[i].addr) != 0 ? uso(origem(sAnel[i].addr)) : NULL;
            if (u != NULL) {
                u->copias |= (uint8_t)sPend.size;
            }
            for (b = 0; b < sPend.size; b++) {
                marcar(sPend.dst + b, origem(sAnel[i].addr + b));
            }
            if (sAnel[i].sus && sAnel[i].v.gw > sAnel[i].v.lw) {
                /* copiou um valor lido mais estreito que a gravação (u8 de dentro de um s32): no big-endian a
                   leitura já pega outro byte; o destino fica com a forma da própria gravação */
                anotar_visao(&sAnel[i].v);
            } else {
                /* cópia de bytes, inclusive de uma leitura mais larga que as gravações (struct de dois floats
                   copiada como 8 bytes): não muda valor nenhum, a forma vai junto */
                copiar_forma(sPend.dst, sAnel[i].addr, sPend.size);
            }
            sAnel[i].vivo = 0;
            return;
        }
    }
    /* gravou uma conta, não um dado lido: a memória deixa de ser do disco */
    for (b = 0; b < sPend.size; b++) {
        marcar(sPend.dst + b, 0);
    }
}

NAO_RASTREAR static void talvez_parar(void);

NAO_RASTREAR static void ler(uint32_t addr, uint32_t size, uint32_t pc) {
    int i, sus;
    Visao v;
    talvez_parar();
    resolver();
    sus = forma_lida(addr, size, pc, &v);
    /* com o detector ligado, toda leitura entra no anel: é assim que uma cópia (struct inteira copiada palavra
       a palavra) leva a forma da origem, inclusive a parte neutra, em vez de virar "gravação de 4" */
    if ((!sAtivo || origem(addr) == 0) && !sus && !sVisoes) {
        return;
    }
    /* a leitura mais velha do anel que não virou cópia é uso de verdade */
    i = sAnelPos;
    if (sAnel[i].vivo) {
        anotar_uso(sAnel[i].addr, sAnel[i].size, sAnel[i].pc);
        if (sAnel[i].sus) {
            anotar_visao(&sAnel[i].v);
        }
    }
    sAnel[i].addr = addr;
    sAnel[i].size = size;
    sAnel[i].pc = pc;
    sAnel[i].vivo = 1;
    sAnel[i].sus = sus;
    sAnel[i].v = v;
    sAnelPos = (sAnelPos + 1) % ANEL;
}

NAO_RASTREAR static void gravar(uint32_t addr, uint32_t size, uint32_t pc) {
    int k;
    talvez_parar();
    resolver();
    forma_gravada(addr, size, pc);
    if (!sAtivo && !sVisoes) {
        return;
    }
    /* no mesmo lugar: o jogo reescrevendo o campo (deslocamento do arquivo virando ponteiro) */
    for (k = 0; k < ANEL; k++) {
        if (sAnel[k].vivo && sAnel[k].addr == addr) {
            anotar_uso(sAnel[k].addr, sAnel[k].size, sAnel[k].pc);
            if (sAnel[k].sus) {
                anotar_visao(&sAnel[k].v);
            }
            sAnel[k].vivo = 0;
            anotar_escrita(addr, size, pc);
            return;
        }
    }
    /* candidata a cópia: decide no próximo callback, quando o valor gravado já está na memória (resolver) */
    sPend.dst = addr;
    sPend.size = size;
    sPend.pc = pc;
    sPend.vivo = 1;
}

#define PC ((uint32_t)(uintptr_t)__builtin_return_address(0))
#define LOAD(n) NAO_RASTREAR void __asan_load##n##_noabort(uintptr_t a) { ler((uint32_t)a, n, PC); } \
                NAO_RASTREAR void __asan_store##n##_noabort(uintptr_t a) { gravar((uint32_t)a, n, PC); }
LOAD(1) LOAD(2) LOAD(4) LOAD(8) LOAD(16)
NAO_RASTREAR void __asan_loadN_noabort(uintptr_t a, size_t n) { ler((uint32_t)a, (uint32_t)n, PC); }
NAO_RASTREAR void __asan_storeN_noabort(uintptr_t a, size_t n) {
    size_t i;
    resolver();
    neutro((uint32_t)a, n);
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
    copiar_forma((uint32_t)(uintptr_t)d, (uint32_t)(uintptr_t)s, n);
    return __real_memcpy(d, s, n);
}
NAO_RASTREAR void *__wrap_memmove(void *d, const void *s, size_t n) {
    copiar_origem((uint32_t)(uintptr_t)d, (uint32_t)(uintptr_t)s, n);
    copiar_forma((uint32_t)(uintptr_t)d, (uint32_t)(uintptr_t)s, n);
    return __real_memmove(d, s, n);
}
NAO_RASTREAR void *__wrap_memset(void *d, int c, size_t n) {
    size_t i;
    resolver();
    neutro((uint32_t)(uintptr_t)d, n);
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
    neutro((uint32_t)(uintptr_t)buf, got);
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
    if (out != NULL && *rawSize > 0) {
        neutro((uint32_t)(uintptr_t)out, (size_t)*rawSize);
    }
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
            fprintf(fp, "%s\t%llu\t%u\t%u\t%u\t%08x,%08x,%08x,%08x\t%u\t%u\t%u\t%08x,%08x,%08x,%08x\n", sNome[u->chave >> 40],
                    (unsigned long long)(u->chave & 0xFFFFFFFFFFull), u->tamanhos, u->usos, u->escritas, u->pcs[0], u->pcs[1],
                    u->pcs[2], u->pcs[3], u->copias, u->impar, u->desal, u->pcb[0], u->pcb[1], u->pcb[2], u->pcb[3]);
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

NAO_RASTREAR static void despejar_visoes(void) {
    const char *caminho = getenv("BT3_VISOES");
    FILE *fp;
    uint32_t i, n = 0;
    resolver();
    for (i = 0; i < ANEL; i++) {
        if (sAnel[i].vivo && sAnel[i].sus) {
            anotar_visao(&sAnel[i].v);
        }
    }
    sVisoes = 0;
    if (caminho == NULL || (fp = fopen(caminho, "w")) == NULL) {
        return;
    }
    for (i = 0; sVis != NULL && i < VTAB; i++) {
        if (sVis[i].n) {
            fprintf(fp, "%08x\t%08x\t%u\t%u\t%u\t%u\n", sVis[i].gravou, sVis[i].leu, sVis[i].gw, sVis[i].lw, sVis[i].pos, sVis[i].n);
            n++;
        }
    }
    fclose(fp);
    fprintf(stderr, "visões: %u pares (função que gravou, função que leu) em %s\n", n, caminho);
}

/* `timeout` encerra com SIGTERM, que não passa pelo atexit: sem isto um rastreio limitado por tempo (os menus
   rodam em ciclo, sem fim próprio) perderia tudo. O tratador só marca: exit() dentro do sinal podia travar (o sinal
   cai no meio de um malloc, e o despejo faz fopen) ou reentrar. Quem sai é ler()/gravar(), que só rodam a partir do
   código instrumentado do jogo, nunca de dentro da libc. O alarme é a rede: se o jogo não ler nem gravar em 10 s
   (parado numa chamada de sistema), o SIGALRM encerra sem despejo em vez de deixar o timeout esperando. */
static volatile sig_atomic_t sParar;

NAO_RASTREAR static void ao_terminar(int sinal) {
    (void)sinal;
    sParar = 1;
    alarm(10);
}

NAO_RASTREAR static void talvez_parar(void) {
    if (sParar == 1) {
        sParar = 2; /* uma vez só: o exit não volta aqui */
        exit(0);
    }
}

__attribute__((constructor)) NAO_RASTREAR static void iniciar(void) {
    sAtivo = getenv("BT3_RASTREIO") != NULL;
    sVisoes = getenv("BT3_VISOES") != NULL;
    if (sAtivo || sVisoes) {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = ao_terminar;
        sa.sa_flags = SA_RESETHAND; /* um segundo SIGTERM encerra na hora */
        sigaction(SIGTERM, &sa, NULL);
    }
    if (sAtivo) {
        marcar_elf();
        atexit(despejar);
    }
    if (sVisoes) {
        atexit(despejar_visoes);
    }
}
