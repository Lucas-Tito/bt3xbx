/*
 * (F6, etapa 6, docs/x360/plano/F6/06.md, seção 5) Um arquivo lido em blocos grandes, com o bloco seguinte já pedido
 * a uma thread de leitura enquanto quem lê consome o atual. Feito para o filme: o decodificador (port/src/plat_mpeg2.c)
 * lê o .PSS em pedaços de 4, 10, 2 e um pacote por vez (36,7 leituras por imagem, cada uma uma ida à ISO com a trava);
 * com os blocos, ele lê da memória e a ISO vê uma leitura de 256 KB a cada seis imagens, fora da thread do jogo.
 *
 * C89 sem nada do 360 além da thread (Win32 com _XBOX, pthreads fora dele), para o mesmo arquivo rodar no teste do
 * WSL (port/x360/tools/teste_blocos.sh) e no console (iso360.c). Um leitor só, que lê para a frente (pode voltar: o
 * bloco é pedido de novo). Posições e tamanhos em long (32 bits no cl, 64 no gcc; o filme tem menos de 2 GB).
 */
#ifndef BLOCOS_H
#define BLOCOS_H

/* lê n bytes da posição pos (do começo do arquivo); 1 se leu todos */
typedef int (*BlocosLer)(void *ctx, long pos, void *buf, long n);

typedef struct Blocos Blocos;

typedef struct BlocosConta {
    unsigned blocos;     /* blocos lidos pela thread de leitura (ou na hora, sem ela) */
    unsigned faltas;     /* blocos pedidos na hora: o que se queria não estava adiantado (o primeiro, ou um salto) */
    unsigned esperas;    /* vezes que quem lê esperou um bloco */
    double msLeitura;    /* tempo das leituras dos blocos, somado, e o pior bloco */
    double msLeituraPior;
    double msEspera;     /* tempo de quem lê esperando, somado, e a pior espera */
    double msEsperaPior;
} BlocosConta;

/* tam: o tamanho do arquivo; bloco: bytes por bloco (dois buffers deste tamanho); hw: com thread de leitura, a
   thread de hardware dela no 360 (ignorada fora dele); -1: sem thread, cada bloco lido na hora em que falta.
   NULL sem memória. */
Blocos *Blocos_Abrir(BlocosLer ler, void *ctx, long tam, long bloco, int hw);
/* até n bytes da posição pos para dst: os bytes entregues (menos que n só no fim do arquivo), -1 se a leitura falhou */
long Blocos_Ler(Blocos *b, long pos, void *dst, long n);
/* os contadores até aqui (os da espera são de quem lê; chamar da mesma thread que lê) */
void Blocos_Conta(Blocos *b, BlocosConta *c);
/* para a thread (esperando a leitura em curso) e solta tudo */
void Blocos_Fechar(Blocos *b);

#endif
