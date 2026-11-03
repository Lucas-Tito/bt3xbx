/*
 * Mapa de larguras do save (decisão 22 de docs/x360/decisoes.md; documento: docs/x360/formatos/save-larguras.md).
 *
 * O jogo grava o save "no formato nativo do host": o do PS2 (e de um .psu do PCSX2) é little-endian, o do 360 é
 * big-endian. Um conversor que troque a ordem dos bytes de cada número precisa da largura (1, 2, 4 ou 8 bytes) de
 * cada número em cada posição. Esta é a fonte única dessa informação, para os três arquivos da pasta
 * BASLUS-21678DBZT3: o de dados (0x4000 bytes, o SaveData de include/sys/save.h), icon.sys (964, McCardIconSys) e
 * dbzsm.ico (40.090, copiado do disco sem mexer).
 *
 * Cada arquivo é coberto por faixas, sem buraco nem sobreposição: a faixa {desl, qtd, larg} tem `qtd` números de
 * `larg` bytes a partir de `desl`, cada um alinhado ao próprio tamanho (desl e a largura são múltiplos de `larg`).
 * Largura 1 não troca nada.
 *
 * C89 (cl.exe -TC e gcc -std=c89): o mesmo arquivo compila no console e no PC.
 */
#ifndef SAVE_LARGURAS_H
#define SAVE_LARGURAS_H

#define SAVELARG_ARQ_DADOS 0   /* BASLUS-21678DBZT3 (o SaveData) */
#define SAVELARG_ARQ_ICONSYS 1 /* icon.sys */
#define SAVELARG_ARQ_ICO 2     /* dbzsm.ico */
#define SAVELARG_ARQ_NUM 3

#define SAVELARG_TAM_DADOS 0x4000UL
#define SAVELARG_TAM_ICONSYS 964UL
#define SAVELARG_TAM_ICO 40090UL

/* de onde vem a largura da faixa */
#define SAVELARG_TIPADO 0    /* campo com tipo em include/sys/save.h (ou McCardIconSys): largura declarada */
#define SAVELARG_RESOLVIDO 1 /* bloco opaco do save.h, largura achada pelo código que o acessa (a varredura) */
#define SAVELARG_SEM_ACESSO 2 /* nenhum código acessa: largura 1, não troca */
#define SAVELARG_DISCO 3     /* bytes copiados do disco sem mexer (o ícone): largura 1 */

typedef struct SaveFaixa {
    unsigned long desl; /* deslocamento do primeiro número */
    unsigned long qtd;  /* quantos números */
    int larg;           /* 1, 2, 4 ou 8 */
    int cat;            /* SAVELARG_* */
    const char *nome;   /* o campo, como no save.h ou nos views do menu */
} SaveFaixa;

/* o nome do arquivo (o do cartão) e o tamanho; NULL / 0 para um número de arquivo inválido */
const char *SaveLarg_NomeArquivo(int arquivo);
unsigned long SaveLarg_Tamanho(int arquivo);

/* As faixas do arquivo em ordem de deslocamento. *n recebe quantas são; NULL se o arquivo não existe. O vetor é
   estático (montado na primeira chamada), não se altera nem se libera. */
const SaveFaixa *SaveLarg_Faixas(int arquivo, int *n);

/* Troca a ordem dos bytes de cada número de buf (um arquivo inteiro, tam bytes), no lugar, na largura do mapa:
   serve nos dois sentidos (little para big e big para little). 0 se trocou; -1 se o arquivo não existe ou tam não é
   o do arquivo (nada muda). Atenção: a soma dos dois primeiros números do arquivo de dados (McCard_Checksum) é
   calculada sobre os bytes, então muda com a troca: quem converte a recalcula depois (docs, seção "A soma"). */
int SaveLarg_Inverte(unsigned char *buf, unsigned long tam, int arquivo);

#endif
