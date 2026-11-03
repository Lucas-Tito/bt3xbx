/*
 * Importação e exportação de saves do BT3 (.psu do PS2) pela pasta do jogo. A lógica fica aqui, sem E/S direta: o
 * sistema de arquivos entra por SaveFs (uma tabela de funções), que o 360 preenche com a API do Win32
 * (port/x360/src/f2/save_importa_360.c) e o teste do Linux com a libc (port/x360/tools/teste_save_importa_fs.c).
 * Documento: docs/x360/formatos/save-importacao.md. Mora em src e não em
 * conv porque o selo da ISO convertida conta todo arquivo .c e .h de conv (convert_iso.py): um arquivo novo lá invalida as ISOs já
 * convertidas. C89, sem alocação: os buffers são estáticos do módulo (uns 370 KB), então uma chamada por vez (a importação roda antes do jogo; a exportação, na thread do jogo).
 *
 * Importação: a pasta import tem os .psu soltos (as subpastas não contam). SaveImp_Procurar lista e confere cada um
 * (SaveConv_Ps2Para360); SaveImp_Importar grava o escolhido (com cópia do save atual em backup\NNN antes) e move
 * os .psu para done, skipped e failed; SaveImp_Pular só move. Nada é apagado: só movido, e nome já existente no
 * destino ganha o sufixo _2, _3...
 *
 * Exportação: SaveImp_Exportar gera export\BASLUS-21678DBZT3.psu a partir dos três arquivos atuais, só se a pasta
 * export existe. Nunca lança nada: devolve um código, e quem chama decide (o gancho do jogo só registra no log).
 */
#ifndef SAVE_IMPORTA_H
#define SAVE_IMPORTA_H

#include "save_conv.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SI_NOME 64      /* nome de arquivo na lista (com o zero); nomes maiores nem entram (o FATX só tem até 42) */
#define SI_MAX 64       /* arquivos .psu da lista; os que passam disso ficam na pasta para a próxima abertura */
#define SI_CAMINHO 512

/* a pasta e os três arquivos do save, como no cartão do jogo */
#define SI_PASTA_SAVE "BASLUS-21678DBZT3"
#define SI_PSU_EXPORTA "BASLUS-21678DBZT3.psu"

/* códigos devolvidos pelas funções do SaveFs: 0 = ok; SI_FS_GRANDE = o arquivo não cabe no buffer; o resto é o
 * código do sistema (GetLastError, errno), sempre diferente de 0 */
#define SI_FS_GRANDE (-2)

typedef struct SaveFs {
    void *ctx;
    char sep; /* '\\' no 360, '/' no Linux */
    /* o arquivo inteiro em buf (capacidade cap); *lidos recebe o tamanho */
    int (*ler)(void *ctx, const char *cam, unsigned char *buf, unsigned long cap, unsigned long *lidos);
    /* cria ou substitui o arquivo, com n bytes, e garante que foi para o disco */
    int (*gravar)(void *ctx, const char *cam, const unsigned char *buf, unsigned long n);
    /* renomeia/move; FALHA se o destino já existe (como o MoveFile do 360) */
    int (*mover)(void *ctx, const char *de, const char *para);
    int (*apagar)(void *ctx, const char *cam);
    /* 0 = não existe, 1 = arquivo, 2 = pasta */
    int (*tipo)(void *ctx, const char *cam);
    /* cria uma pasta (o pai tem de existir); 0 também se já existia */
    int (*criar_pasta)(void *ctx, const char *cam);
    /* os nomes dos ARQUIVOS (não pastas) de uma pasta, até max; *n recebe quantos */
    int (*listar)(void *ctx, const char *pasta, char nomes[][SI_NOME], int max, int *n);
    /* uma linha para o log do jogo (em português); pode ser NULL */
    void (*log)(void *ctx, const char *linha);
} SaveFs;

typedef struct SiPastas {
    const char *import;    /* game:\import */
    const char *backup;    /* game:\backup */
    const char *save;      /* a pasta do save: <saves>\card1\BASLUS-21678DBZT3 */
    const char *exportar;  /* game:\export */
} SiPastas;

typedef struct SiItem {
    char nome[SI_NOME];
    int erro; /* 0 = pode importar; senão o código de Psu_TextoErro/SaveConv_TextoErro ou SI_E_* */
} SiItem;

typedef struct SiLista {
    SiItem item[SI_MAX];
    int n;
    int ha_save; /* 1 se já existe o arquivo de dados do save na pasta do save */
} SiLista;

/* erros próprios (os de PSU_E_* e SAVECONV_E_* passam como estão) */
#define SI_E_GRANDE 200     /* "file is too big" */
#define SI_E_LER 201        /* "cannot read file" */
#define SI_E_RECUSADO 202   /* "file cannot be imported" (o item da lista tem erro) */
#define SI_E_CAMINHO 203    /* "path is too long" */
#define SI_E_BACKUP 204     /* "backup failed" */
#define SI_E_GRAVAR 205     /* "could not write the save" */
#define SI_E_TROCA 206      /* "could not replace the save" */
#define SI_E_MOVER 207      /* "could not move the .psu" */
#define SI_E_PASTA 208      /* "could not create a folder" */
#define SI_E_ESCOLHA 209    /* "bad choice" */
#define SI_E_EXPORTA 210    /* "export failed" (o passo diz onde) */

/* onde a importação parou (SiFalha.passo) */
#define SI_P_LER 1       /* lendo ou conferindo o .psu: nada foi tocado */
#define SI_P_BACKUP 2    /* copiando o save atual para backup\NNN: nada foi tocado no save */
#define SI_P_GRAVAR 3    /* gravando os temporários na pasta do save: o save atual segue intacto */
#define SI_P_TROCA 4     /* trocando os arquivos: o save atual é recolocado a partir da memória (restaurado) */
#define SI_P_MOVER 5     /* o save foi importado, mas um .psu não pôde ser movido */
#define SI_P_EXPORTA_LER 6   /* exportação: lendo os três arquivos */
#define SI_P_EXPORTA_CONV 7  /* exportação: conferindo/convertendo */
#define SI_P_EXPORTA_GRAVAR 8 /* exportação: gravando o .psu */

typedef struct SiFalha {
    int passo;      /* SI_P_* */
    int codigo;     /* o erro (conferência do .psu) ou 0 */
    int sistema;    /* o código do sistema do passo que falhou, ou 0 */
    int restaurado; /* SI_P_TROCA: 1 = o save de antes foi recolocado, 0 = não deu (o backup tem a cópia) */
    int importado;  /* 1 se o save novo já está no lugar (SI_P_MOVER) */
    int movidos_mal; /* SI_P_MOVER: quantos .psu ficaram sem mover */
    int backup;     /* o número NNN do backup feito (0 se não houve) */
} SiFalha;

/* data fixa dos .psu exportados (o console não tem relógio confiável): 2007-11-13 12:00:00, o lançamento do jogo nos EUA */
#define SI_DATA_EXPORTA_SEG 0
#define SI_DATA_EXPORTA_MIN 0
#define SI_DATA_EXPORTA_HORA 12
#define SI_DATA_EXPORTA_DIA 13
#define SI_DATA_EXPORTA_MES 11
#define SI_DATA_EXPORTA_ANO 2007

/* Lista os *.psu soltos de pastas->import (sem distinguir maiúsculas), em ordem de nome, cada um conferido. Devolve
 * quantos (0 se a pasta não existe ou não tem nenhum, que é o caso comum: custa uma procura). */
int SaveImp_Procurar(const SaveFs *fs, const SiPastas *pastas, SiLista *lista);

/* Importa o item 'indice' (que tem de ter erro 0): cópia do save atual, gravação segura, e depois os movimentos de
 * todos os .psu. Devolve 0 ou o SI_E_*; 'falha' diz o passo e o código do sistema. */
int SaveImp_Importar(const SaveFs *fs, const SiPastas *pastas, const SiLista *lista, int indice, SiFalha *falha);

/* SKIP: só os movimentos (skipped e failed). Devolve quantos .psu não puderam ser movidos. */
int SaveImp_Pular(const SaveFs *fs, const SiPastas *pastas, const SiLista *lista, SiFalha *falha);

/* Exporta. 1 = não há pasta export (nada foi feito); 0 = gravou export\BASLUS-21678DBZT3.psu; senão SI_E_* ou o
 * erro da conversão, com o passo e o código do sistema em 'falha' (pode ser NULL). */
int SaveImp_Exportar(const SaveFs *fs, const SiPastas *pastas, SiFalha *falha);

/* O gancho da gravação do jogo: 'gravado' é o caminho do arquivo que o jogo acabou de gravar com sucesso (com '/' ou
 * com a barra do sistema). Se for o arquivo de dados do save (BASLUS-21678DBZT3, barra, BASLUS-21678DBZT3), exporta (só com
 * a pasta export). SEMPRE devolve 0: uma falha da exportação só vai ao log e nunca ao jogo. */
int SaveImp_AposGravar(const SaveFs *fs, const SiPastas *pastas, const char *gravado);

/* Texto curto em inglês (o da tela) para qualquer código, e o nome do passo. */
const char *SaveImp_TextoErro(int codigo);
const char *SaveImp_TextoPasso(int passo);

#ifdef __cplusplus
}
#endif

#endif
