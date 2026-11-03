/*
 * Conversão do save do BT3 entre o PS2 (.psu, little-endian) e o 360 (big-endian). Documento: docs/x360/formatos/
 * save-conversao.md. Usa o contêiner (psu.h) e o mapa de larguras (save_larguras.h).
 *
 * O save é a pasta BASLUS-21678DBZT3 com três arquivos: icon.sys (964), dbzsm.ico (40.090, igual nas duas ordens) e
 * BASLUS-21678DBZT3 (16.384, os dados). A conversão troca a ordem de cada número pela largura do mapa e RECALCULA a
 * soma do bloco de dados (McCard_Checksum, src/battle/stgm_a_b.c): a soma é sobre os bytes, então muda com a troca.
 *
 * Sem E/S de arquivo e sem alocação: os buffers são de quem chama. C89, byte a byte, sem supor alinhamento.
 *
 * Datas: o console não tem relógio confiável e o save do 360 não guarda datas. A política de datas do .psu que sai
 * (pasta, arquivos) é de quem chama: 360 para PS2 usa as duas datas passadas (criação e modificação) em todas as
 * entradas, como o mymcplus. PS2 para 360 descarta as datas.
 */
#ifndef SAVE_CONV_H
#define SAVE_CONV_H

#include "psu.h"
#include "save_larguras.h"

/* ordem dos números do save */
#define SAVECONV_PS2 0 /* little-endian: palavras da soma em little-endian */
#define SAVECONV_360 1 /* big-endian */

/* códigos de erro (0 = ok); os do contêiner (PSU_E_*) passam como estão */
#define SAVECONV_OK 0
#define SAVECONV_E_ARGS 100       /* "bad arguments" */
#define SAVECONV_E_SOMA_PS2 101   /* "PS2 save is damaged (checksum)" */
#define SAVECONV_E_SOMA 102       /* "save is damaged (checksum)" */
#define SAVECONV_E_TAMANHO 103    /* "save file has the wrong size" */

/* os três arquivos, na ordem de um dos lados; o buffer é de quem chama (uns 57 KB: estático, não na pilha) */
typedef struct {
    unsigned char iconsys[SAVELARG_TAM_ICONSYS];
    unsigned char ico[SAVELARG_TAM_ICO];
    unsigned char dados[SAVELARG_TAM_DADOS];
} SaveConvTres;

/* A soma do jogo sobre um bloco de dados (SAVELARG_TAM_DADOS bytes): os bytes depois dos 8 primeiros, o de posição par
 * uma vez e o de posição ímpar vezes 256, em 16 bits. */
unsigned SaveConv_Soma(const unsigned char *dados);

/* Recalcula a soma e grava as duas palavras (alto = soma >> 8, baixo = soma & 0xFF) nos 8 primeiros bytes, como u32 na
 * ordem dada (SAVECONV_PS2 ou SAVECONV_360). Zera as palavras antes de somar. Devolve a soma. */
unsigned SaveConv_GravaSoma(unsigned char *dados, int ordem);

/* 1 se as DUAS palavras (u32 na ordem dada) são a soma do bloco; 0 se não. Mais estrito que o jogo (que aceita se uma das
 * duas confere): um save gravado pelo jogo sempre tem as duas. */
int SaveConv_ConferirSoma(const unsigned char *dados, int ordem);

/* PS2 para 360. 's' lido por Psu_Ler. Confere (Psu_ConferirBT3 e a soma na ordem do PS2), copia, inverte a ordem dos
 * dados e do icon.sys, copia o .ico e recalcula a soma em big-endian. Devolve SAVECONV_OK ou um código. */
int SaveConv_Ps2Para360(const PsuSave *s, SaveConvTres *saida);

/* 360 para PS2. 'entrada' tem os três arquivos na ordem do 360. Confere a soma na ordem do 360, inverte, recalcula a soma
 * em little-endian e preenche 's' (com 'vet', capacidade 3) pronto para Psu_Escrever: pasta BASLUS-21678DBZT3, modos
 * PSU_MODO_*, os arquivos na ordem icon.sys, dbzsm.ico, dados, apontando para 'tmp' (de quem chama, vive até o
 * Psu_Escrever). 'entrada' e 'tmp' não podem ser o mesmo buffer. Datas: ver acima. */
int SaveConv_360ParaPs2(const SaveConvTres *entrada, SaveConvTres *tmp, PsuArquivo *vet, PsuSave *s,
                        const PsuData *criada, const PsuData *modificada);

/* Texto curto em inglês para um código (os de PSU_E_* também). */
const char *SaveConv_TextoErro(int codigo);

#endif
