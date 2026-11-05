/*
 * O amostrador de perfil da thread do jogo (F8, docs/x360/plano/F8/08-explosao-planeta.md): a parte que não depende do
 * console, em C89, para o teste no host (port/x360/tools/amostra_confere.sh) compilar o mesmo código.
 *   - AmostraHist_LerPilha: com a thread do jogo suspensa, acha na pilha dela o quadro de armadilha (KTRAP_FRAME) que o
 *     núcleo gravou ao interromper a thread, e dele o Iar, o Lr e o r1; depois os endereços de retorno da pilha do jogo.
 *     Só lê memória pela função dada (nada de trava, nada de alocar: a thread do jogo está parada);
 *   - AmostraHist_Resumir: com a thread já retomada, troca cada endereço pela função que o contém (o .pdata da imagem);
 *   - o resto: guarda as amostras do quadro corrente e, quando o quadro do jogo termina, soma cada uma no histograma da
 *     classe do quadro (normal, lento, longo, pela duração), e despeja os histogramas no diário.
 */
#ifndef AMOSTRA_HIST_H
#define AMOSTRA_HIST_H

#ifdef __cplusplus
extern "C" {
#endif

#define AMOSTRA_RET 64  /* endereços de retorno do jogo guardados por amostra */
#define AMOSTRA_FUNCS 48 /* funções distintas por amostra (a folha e as da pilha) */

enum { AMOSTRA_NORMAL, AMOSTRA_LENTO, AMOSTRA_LONGO, AMOSTRA_CLASSES };
/* nada: a thread não parou ou a pilha não deu nada; armadilha: o quadro de armadilha achado pelo encadeamento da pilha;
   varredura: achado varrendo a pilha (o encadeamento não chegou nele); sem Iar: nenhum quadro de armadilha (a thread
   estava numa espera do núcleo), só os retornos da pilha */
enum { AMOSTRA_NADA, AMOSTRA_ARMADILHA, AMOSTRA_VARREDURA, AMOSTRA_SEM_IAR, AMOSTRA_TIPOS };
#define AMOSTRA_FOLHA_ESPERA 0u /* a folha de uma amostra sem Iar */
#define AMOSTRA_FOLHA_NUCLEO 1u /* a folha de um Iar fora da imagem (no núcleo) */

/* o quadro de armadilha do núcleo do 360 (xkelib, kePrivateStructs.h: KTRAP_FRAME, 0x1C0 bytes) */
#define AMOSTRA_TF_TAM 0x1C0u
#define AMOSTRA_TF_R1 0x58u /* Gpr1, 64 bits: a palavra alta em 0x58, a baixa em 0x5C */
#define AMOSTRA_TF_LR 0x1A8u
#define AMOSTRA_TF_IAR 0x1ACu

typedef int (*AmostraLer)(unsigned endereco, unsigned *valor); /* 0 se não pôde ler */

typedef struct {
    unsigned textoIni, textoFim; /* o código da imagem (a seção executável) */
    const unsigned *pdata;       /* pares (início, dados) do .pdata, em ordem; NULL sem ele */
    unsigned nPdata;
} AmostraImagem;

typedef struct {
    unsigned tipo, iar, lr, r1, armadilha; /* armadilha: onde o quadro de armadilha estava */
    unsigned nRet;
    unsigned ret[AMOSTRA_RET]; /* os retornos que caem na imagem, do mais fundo para o mais raso */
} AmostraCrua;

typedef struct {
    unsigned tipo;
    unsigned folha;    /* a função do Iar; AMOSTRA_FOLHA_ESPERA ou AMOSTRA_FOLHA_NUCLEO */
    unsigned chamador; /* a função que chamou a folha; 0 sem */
    unsigned n;
    unsigned funcs[AMOSTRA_FUNCS]; /* as funções distintas na pilha, a folha inclusive (o tempo inclusivo) */
} AmostraUma;

unsigned AmostraHist_Funcao(const AmostraImagem *img, unsigned endereco); /* o início da função, ou o próprio endereço */

/* pilhaNucleo: o r1 salvo na troca de contexto (KTHREAD.KernelStack); [limite, base): a pilha da thread */
void AmostraHist_LerPilha(const AmostraImagem *img, AmostraLer ler, unsigned pilhaNucleo, unsigned limite,
                          unsigned base, AmostraCrua *saida);
void AmostraHist_Resumir(const AmostraImagem *img, const AmostraCrua *c, AmostraUma *u);

unsigned AmostraHist_Tamanho(void); /* bytes que AmostraHist_Iniciar pede */
int AmostraHist_Iniciar(void *memoria, unsigned tamanho, unsigned limiarMs, unsigned longoMs);
void AmostraHist_Guardar(const AmostraUma *u);           /* no quadro corrente */
void AmostraHist_FimDoQuadro(unsigned quadros, double ms); /* o quadro do jogo terminou: as amostras vão para a classe */
unsigned AmostraHist_Quadros(int classe);
unsigned AmostraHist_Amostras(int classe);
/* As linhas "amostra: dN ..." no diário (port/x360/tools/amostra_perfil.py as lê). cabecalho: o que identifica a build
   e o estado do amostrador. Chamar fora da suspensão (o diário tem trava). */
void AmostraHist_Despejar(const char *motivo, const char *cabecalho);

#ifdef __cplusplus
}
#endif

#endif
