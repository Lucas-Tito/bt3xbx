/* Contêiner .psu (exportação de save do PS2: EMS, uLaunchELF, mymcplus).
 *
 * Só trata o contêiner: o conteúdo dos arquivos entra e sai como bytes. Sem E/S de
 * arquivo e sem alocação: trabalha sobre buffers que quem chama lê, grava e aloca.
 * Formato em docs/x360/formatos/psu.md. C89, números little-endian byte a byte. */
#ifndef PSU_H
#define PSU_H

#define PSU_ENTRADA 512UL     /* tamanho de cada entrada de diretório */
#define PSU_ALINHA 1024UL     /* dados completados até múltiplo disto */
#define PSU_NOME 32           /* campo do nome (com o zero final) */

#define PSU_MODO_ARQUIVO 0x8497U /* arquivo comum do cartão */
#define PSU_MODO_PASTA 0x8427U   /* pasta comum do cartão */

/* códigos de erro (0 = ok) */
#define PSU_OK 0
#define PSU_E_ARGS 1       /* "bad arguments" */
#define PSU_E_NAO_PSU 2    /* "not a PS2 save (.psu)" */
#define PSU_E_TRUNCADO 3   /* "truncated file" */
#define PSU_E_ENTRADA 4    /* "bad directory entry" */
#define PSU_E_NOME 5       /* "bad name" */
#define PSU_E_SOBRA 6      /* "trailing data" */
#define PSU_E_MUITOS 7     /* "too many files" */
#define PSU_E_ESPACO 8     /* "output buffer too small" */
#define PSU_E_BT3_PASTA 9 /* "not a Budokai Tenkaichi 3 USA save" */
#define PSU_E_BT3_FALTA 10 /* "save is missing a file" */
#define PSU_E_BT3_TAMANHO 11 /* "save file has the wrong size" */
#define PSU_E_BT3_EXTRA 12 /* "save has unexpected files" */

/* data/hora "tod" do PS2 (8 bytes: reservado, segundo, minuto, hora, dia, mês, ano u16) */
typedef struct {
    unsigned char seg, min, hora, dia, mes;
    unsigned short ano;
} PsuData;

typedef struct {
    char nome[PSU_NOME + 1];    /* terminado em zero */
    unsigned modo;
    PsuData criado, modificado;
    const unsigned char *dados; /* aponta para dentro do buffer lido (leitura) ou dos dados de quem chama (escrita) */
    unsigned long tamanho;
} PsuArquivo;

typedef struct {
    char pasta[PSU_NOME + 1];
    unsigned modo;              /* modo da pasta; na escrita 0 vale PSU_MODO_PASTA */
    PsuData criada, modificada;
    unsigned nArquivos;
    PsuArquivo *arquivos;       /* vetor de quem chama (capacidade em Psu_Ler) */
} PsuSave;

/* Lê e valida um .psu inteiro em memória. 'vet' tem capacidade para 'max' arquivos.
 * Preenche 'out' (arquivos = vet); os dados apontam para dentro de 'buf'.
 * Devolve PSU_OK ou um código de erro. */
int Psu_Ler(const unsigned char *buf, unsigned long tam, PsuSave *out, PsuArquivo *vet, unsigned max);

/* Confere que o save lido é o do BT3 USA (BASLUS-21678DBZT3, três arquivos de tamanho certo). */
int Psu_ConferirBT3(const PsuSave *s);

/* Texto curto em inglês para um código de erro. */
const char *Psu_TextoErro(int codigo);

/* Tamanho do .psu que Psu_Escrever produz para 's' (0 se 's' for inválido). */
unsigned long Psu_TamanhoEscrita(const PsuSave *s);

/* Escreve o .psu em 'buf' (capacidade 'cap'); 'escrito' recebe o tamanho. */
int Psu_Escrever(const PsuSave *s, unsigned char *buf, unsigned long cap, unsigned long *escrito);

#endif
