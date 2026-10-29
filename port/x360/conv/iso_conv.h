/* Instalação da ISO: núcleo C89 comum ao console e ao ensaio nativo. */
#ifndef ISO_CONV_H
#define ISO_CONV_H
#include "conv.h"
#define IC_TAMANHO 3020816384U
#define IC_MARCA_POS 0x3df400U
#define IC_MARCA_TAM 512
#define IC_TEMP "bt3-x360.tmp"
enum { IC_OK, IC_ORIGEM, IC_ESPACO, IC_IO, IC_FORMATO, IC_CONFERE, IC_DESTINO };
enum { IC_IDENTIFICAR, IC_COPIAR, IC_CONVERTER, IC_VERIFICAR, IC_PUBLICAR };
/* Posições e tamanhos cabem em u32, inclusive no Xenon. Nenhum FILE/long de 32 bits.
   ler: 0 = original somente leitura, 1 = temporário reaberto somente leitura.
   iniciar cria exclusivamente; reabrir sincroniza, fecha e reabre; publicar não substitui.
   descartar fecha e remove somente o temporário desta operação. */
typedef struct IsoConvIO {
    void *ctx;
    int (*tamanho)(void *, int, conv_u32 *);
    int (*ler)(void *, int, conv_u32, void *, unsigned);
    /* Um pedido de leitura e um de escrita podem coexistir. O buffer pertence
       ao pedido até esperar_* devolver; mesmo na falha, esperar drena o pedido.
       antecipar lê da original (0) ou do temporário reaberto (1), como ler. */
    int (*antecipar)(void *, int, conv_u32, void *, unsigned);
    int (*esperar_leitura)(void *);
    int (*enviar)(void *, conv_u32, const void *, unsigned);
    int (*esperar_gravacao)(void *);
    void (*hash_saida)(void *, conv_u8 *);
    void *(*alocar)(unsigned); /* buffers de E/S alinhados a 128 bytes */
    void (*liberar)(void *);
    conv_u8 sha_saida[32];
    conv_u32 relidos, hash_origem_ms, conversao_ms, hash_releitura_ms, lotes;
    conv_u32 (*relogio)(void);
    int (*espaco)(void *); /* pelo menos 3 GiB livres, além da original */
    int (*iniciar)(void *);
    int (*reabrir)(void *);
    int (*conferir_indices)(void *, conv_u32 *);
    int (*publicar)(void *);
    void (*descartar)(void *);
    /* Progresso do processo inteiro, em KiB lidos ou gravados: a passagem lê e
       grava a ISO (2 partes de 3) e a releitura a lê de novo (1 parte). feito
       nunca diminui e chega a total na publicação; a fase só avança. */
    void (*progresso)(void *, int, conv_u32 feito, conv_u32 total);
} IsoConvIO;
int IsoConv_Executar(IsoConvIO *io, const conv_u8 selo[IC_MARCA_TAM], char *erro, unsigned n);
/* SHA256 portátil, sem acesso desalinhado nem dependência da ordem do host. */
typedef struct IsoSha {
    conv_u32 h[8], baixo, alto;
    unsigned usado;
    conv_u8 bloco[64];
} IsoSha;
void IsoSha_Iniciar(IsoSha *s);
void IsoSha_Atualizar(IsoSha *s, const void *buf, unsigned n);
void IsoSha_Final(IsoSha *s, conv_u8 hash[32]);
#endif
