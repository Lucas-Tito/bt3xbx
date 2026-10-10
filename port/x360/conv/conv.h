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
    conv_u8 *feito; /* tam bytes, zerados pelo chamador */
    int erro;
    char msg[240];
} ConvCtx;

void Conv_Iniciar(ConvCtx *c, const char *nome, conv_u8 *buf, conv_ofs tam, conv_u8 *feito);

/* 1 se [ofs, ofs+n) cabe no buffer; senão anota o erro (com o contexto) e devolve 0 */
int Conv_Cabe(ConvCtx *c, conv_ofs ofs, conv_ofs n, const char *oque);
void Conv_Erro(ConvCtx *c, const char *fmt, ...);

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

/* Uma animação de personagem descomprimida (BobjAnim), em tempo de jogo: chamada por BtlObjAnim_Load logo depois
   de Bpe_Decode, só em host big-endian. Aloca a marcação. 1: convertida; -1: erro (msg). */
int Conv_Animacao(conv_u8 *buf, conv_ofs tam, char *msg, int nmsg);

/* i-ésimo arquivo do registro (NULL depois do último) */
const char *Conv_Registrado(int i);

/* um conversor de formato: converte [base, base+tam) de c */
typedef void (*ConvFn)(ConvCtx *c, conv_ofs base, conv_ofs tam);

#endif
