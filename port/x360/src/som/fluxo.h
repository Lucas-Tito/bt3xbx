/*
 * F6, etapa 5: a leitura dos fluxos ADX em blocos, fora da thread do jogo (decisão 25.4b, plano/F6/05.md).
 *
 * Uma thread de leitura só (no 360, a thread de hardware 1) atende todos os fluxos abertos. Cada fluxo tem dois
 * buffers de um bloco (64 KiB por padrão, BT3_FLUXO_BLOCO no ambiente muda), com 64 bytes a mais no fim de cada um:
 * um quadro ADX (18 bytes por canal) que começa dentro do bloco cabe inteiro nele. Quem toca pede os bytes de uma
 * posição (Fluxo_Pegar) e nunca espera: se ainda não chegaram, recebe NULL e o pedido fica feito. O bloco seguinte
 * (ou, depois do fim do laço, o do começo dele) é pedido adiantado no outro buffer assim que o atual começa a ser usado.
 *
 * Nenhuma função daqui faz E/S na thread de quem chama: abrir, ler e fechar o arquivo são da thread de leitura. C89
 * puro com a trava e a thread do 360 (_XBOX) ou do pthreads (os testes do WSL).
 */
#ifndef X360_FLUXO_H
#define X360_FLUXO_H
#ifdef __cplusplus
extern "C" {
#endif

#define FLUXO_SOBRA 64 /* bytes lidos além do bloco: o maior quadro ADX (estéreo, 36) cabe inteiro */

typedef struct Fluxo Fluxo;

enum { FLUXO_PENDENTE = 0, FLUXO_PRONTO = 1, FLUXO_ERRO = 2 };

/* Pede `caminho` (o mesmo que o fopen do jogo abre): a thread de leitura o abre e lê o primeiro bloco, de `primeiro`
   bytes (0: um bloco inteiro; só o cabeçalho pede menos). NULL se não há thread ou lugar: quem chama lê do jeito antigo. */
Fluxo *Fluxo_Abrir(const char *caminho, long primeiro);
/* FLUXO_PENDENTE (o primeiro bloco não chegou), FLUXO_PRONTO (chegou: *tamanho é o do arquivo) ou FLUXO_ERRO. */
int Fluxo_Estado(Fluxo *f, long *tamanho);
/* Os n bytes a partir de pos, se já estão na memória; NULL se ainda não (o bloco fica pedido). Com `adianta`, pede o
   bloco seguinte no outro buffer. O ponteiro vale até a próxima chamada de Fluxo_Pegar ou Fluxo_Fechar do mesmo fluxo.
   Quem chama não pede além do fim do arquivo. */
const unsigned char *Fluxo_Pegar(Fluxo *f, long pos, int n, int adianta);
/* O laço em bytes: [ini, fim). Depois do bloco que chega a `fim`, o adiantado é o de `ini`. */
void Fluxo_Laco(Fluxo *f, long ini, long fim);
/* Solta o fluxo (a thread de leitura fecha o arquivo depois). O ponteiro não vale mais. */
void Fluxo_Fechar(Fluxo *f);
/* Para a thread de leitura (a saída do jogo). Os fluxos abertos depois disso voltam NULL. */
void Fluxo_Parar(void);

typedef struct FluxoContadores {
    unsigned abertos, abertosMax;   /* fluxos abertos agora e o máximo ao mesmo tempo */
    unsigned aberturas;             /* Fluxo_Abrir que deram certo */
    unsigned leituras, erros;       /* blocos lidos; aberturas ou leituras que falharam */
    unsigned bytes;                 /* bytes lidos (em KiB: kib) */
    unsigned kib;
    unsigned faltas;                /* Fluxo_Pegar sem os dados na memória (quem toca deu silêncio ou esperou) */
    unsigned adiantados;            /* blocos pedidos adiantados */
    unsigned memoria;               /* bytes dos buffers reservados agora */
    unsigned msPior;                /* a leitura de bloco mais lenta, em ms, e a soma das leituras */
    unsigned msSoma;
    unsigned hist[8];               /* leituras de bloco por tempo: <2, <5, <10, <20, <50, <100, <250, >=250 ms */
} FluxoContadores;
void Fluxo_Contadores(FluxoContadores *c);

#ifdef __cplusplus
}
#endif
#endif
