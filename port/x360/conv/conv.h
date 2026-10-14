/*
 * Conversão dos dados do jogo para big-endian, por formato (decisão 8).
 *
 * Os arquivos do disco do PS2 são little-endian. Cada conversor conhece um formato pelas structs da decomp e
 * troca a ordem dos bytes de cada número dele, no lugar: nada muda de tamanho nem de posição, então
 * deslocamentos internos continuam valendo. O mesmo código roda no instalador do 360 (C89, cl -TC) e na
 * ferramenta de PC (conv_tool.c).
 *
 * Disciplina:
 *   - todo acesso passa pelas funções daqui, que conferem o limite do buffer; um formato que não bate com o
 *     esperado (deslocamento fora, contagem absurda) vira erro com mensagem, nunca acesso fora;
 *   - um número se lê E se troca numa operação só (Conv_U32 devolve o valor já em ordem nativa); para olhar
 *     sem trocar, Conv_Ler32 (lê little-endian);
 *   - cada byte é marcado (largura trocada, ou "byte" para dado de 1 byte conhecido): trocar duas vezes é erro,
 *     e a marcação é o que a validação compara com o rastreio e com a cobertura.
 */
#ifndef CONV_H
#define CONV_H

typedef unsigned char conv_u8;
typedef unsigned short conv_u16;
typedef unsigned int conv_u32;
typedef unsigned long conv_ofs;

/* marcação por byte (ConvCtx.feito) */
#define CONV_LIVRE 0      /* não visto por conversor nenhum */
#define CONV_BYTE 1       /* dado de 1 byte (ou opaco sem número: imagem, texto) */
#define CONV_CONT 0x80    /* continuação de um número que começou antes */
/* 2, 4, 8: começo de um número trocado com essa largura */

typedef struct ConvCtx {
    conv_u8 *buf;
    conv_ofs tam;
    const char *nome;
    conv_u8 *feito; /* tam bytes, zerados pelo chamador; com bits, (tam + 7) / 8 bytes, um bit por byte */
    int bits;       /* 1: feito é um mapa de bits (Conv_FormatoBits): só "marcado" ou não, sem a largura */
    int erro;
    char msg[240];
} ConvCtx;

void Conv_Iniciar(ConvCtx *c, const char *nome, conv_u8 *buf, conv_ofs tam, conv_u8 *feito);

/* 1 se [ofs, ofs+n) cabe no buffer; senão anota o erro (com o contexto) e devolve 0 */
int Conv_Cabe(ConvCtx *c, conv_ofs ofs, conv_ofs n, const char *oque);
void Conv_Erro(ConvCtx *c, const char *fmt, ...);
/* acrescenta onde o erro (se houve) aconteceu: "... [entrada 3]" */
void Conv_Onde(ConvCtx *c, const char *fmt, unsigned long n);

/* lê sem trocar (little-endian, como está no disco) */
conv_u32 Conv_Ler32(ConvCtx *c, conv_ofs ofs);
conv_u16 Conv_Ler16(ConvCtx *c, conv_ofs ofs);

/* troca e devolve o valor */
conv_u16 Conv_U16(ConvCtx *c, conv_ofs ofs);
conv_u32 Conv_U32(ConvCtx *c, conv_ofs ofs);
void Conv_U64(ConvCtx *c, conv_ofs ofs);

/* marca atual de um byte (CONV_LIVRE, CONV_BYTE, CONV_CONT ou a largura) */
conv_u8 Conv_Marca(ConvCtx *c, conv_ofs ofs);

/* o que sobrou sem marca em [ini, fim) tem de ser zero (enchimento): marca como bytes; senão é erro (dado que
   nenhum conversor reconhece) */
void Conv_Zeros(ConvCtx *c, conv_ofs ini, conv_ofs fim, const char *oque);

/* trechos */
void Conv_U16s(ConvCtx *c, conv_ofs ofs, conv_ofs n);
void Conv_U32s(ConvCtx *c, conv_ofs ofs, conv_ofs n);
void Conv_Bytes(ConvCtx *c, conv_ofs ofs, conv_ofs n);

/* Converte um arquivo pelo nome (caminho relativo como o jogo pede, "pzs3us1/01473.bin").
   1: convertido; 0: nenhum conversor conhece esse arquivo; -1: erro (msg). */
int Conv_Arquivo(const char *nome, conv_u8 *buf, conv_ofs tam, conv_u8 *feito, char *msg, int nmsg);

/* Converte com um formato pedido pelo nome do formato ("personagem"...); mesmos retornos. */
int Conv_Formato(const char *formato, const char *nome, conv_u8 *buf, conv_ofs tam, conv_u8 *feito, char *msg,
                 int nmsg);

/* O mesmo, com a marcação num mapa de bits (bits: (tam + 7) / 8 bytes zerados pelo chamador), 8 vezes menor: a
   conversão em tempo de jogo (conv_jogo.c), que só precisa de "já convertido" e não da largura. Converte igual ao
   Conv_Formato (o conv_tool --disco confere, conteúdo a conteúdo: os mesmos bytes, o mesmo erro, a mesma cobertura).
   Conv_Marca devolve CONV_BYTE para todo byte marcado. */
int Conv_FormatoBits(const char *formato, const char *nome, conv_u8 *buf, conv_ofs tam, conv_u8 *bits, char *msg,
                     int nmsg);

/* O maior conteúdo descomprimido que o jogo converte em tempo de jogo (as seções dos menus, as folhas, os retratos,
   estágios e páginas do Treino): o mapa de bits da conversão em jogo se reserva uma vez com este tamanho
   (conv_jogo.c). O maior do disco USA tem 0x81C580 bytes (o tabuleiro do Sim Dragon, 00474 inteiro; antes dele, a
   seleção, 448 s2 e 461 s2, com 0x6BF440); o conv_tool --disco falha se algum passar daqui. 9 MB de conteúdo, 1,125 MB
   de mapa */
#define CONV_JOGO_MAX 0x900000UL

/* Uma animação de personagem descomprimida (BobjAnim), em tempo de jogo: chamada por BtlObjAnim_Load logo depois
   de Bpe_Decode, só em host big-endian. Aloca a marcação. 1: convertida; -1: erro (msg). */
int Conv_Animacao(conv_u8 *buf, conv_ofs tam, char *msg, int nmsg);

/* Os blocos de um contêiner BPE (src/sys/bpe.c, Bpe_Decode; o mesmo percurso da descompressão da conv_tool), sem
   descomprimir: dados = o que vem depois do cabeçalho de 8 bytes, comp = o packSize dele. 1 se o último bloco termina
   exatamente em comp. Só os bytes dos blocos, que não têm ordem: vale nos dois hosts. */
int Conv_BpeFecha(const conv_u8 *dados, conv_ofs comp);

/* o formato de um arquivo do disco pelo nome, ou NULL se nenhum conversor o conhece */
const char *Conv_FormatoDe(const char *nome);

/* A tabela única de formatos (conv.c): faixa de arquivos da partição 1 (índice = id - 1) e seção para formato.
   secao: CONV_ARQ = o arquivo como está no disco; 0 = o conteúdo do contêiner BPE no começo do arquivo; n >= 1 = o
   conteúdo da seção n da tabela do arquivo (menus.md 2.1). */
#define CONV_ARQ (-1)
typedef struct ConvFaixa { int ini, fim, secao; const char *formato; } ConvFaixa;
const ConvFaixa *Conv_Faixas(int *n);

/* o formato do conteúdo descomprimido da seção `secao` (0: o contêiner no começo) do arquivo `nome`, como o
   chamador do jogo o converte depois do Sprite_Unpack; NULL se o jogo ainda não o converte */
const char *Conv_FormatoSecao(const char *nome, int secao);

/* um conversor de formato: converte [base, base+tam) de c */
typedef void (*ConvFn)(ConvCtx *c, conv_ofs base, conv_ofs tam);

#endif
