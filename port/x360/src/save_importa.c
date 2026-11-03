/* Importação e exportação de saves pela pasta do jogo. Ver save_importa.h e docs/x360/formatos/save-importacao.md. */
#include <stdio.h>
#include <string.h>
#include "save_importa.h"

#define CAP_ARQ 65536UL /* o maior arquivo do save que se copia para backup (os de verdade têm até 40.090) */
#define CAP_PSU (3UL * 512 + 3UL * 512 + 16384UL + 40090UL + 964UL + 3UL * 1024 + 4096UL)
#define MAX_NOMES 512

static const char *const kNomes[3] = {"icon.sys", "dbzsm.ico", SI_PASTA_SAVE};

static unsigned char sPsu[CAP_PSU];
static SaveConvTres sTres, sTmp;
static PsuArquivo sVet[16];
static PsuSave sSave;
static unsigned char sAtual[3][CAP_ARQ]; /* o save que estava lá, para o backup e para recolocar */
static unsigned long sTam[3];
static int sExiste[3];
static char sNomes[MAX_NOMES][SI_NOME];

static void registra(const SaveFs *fs, const char *texto, const char *nome, int cod) {
    char linha[SI_NOME + 160];
    if (!fs->log) return;
    sprintf(linha, "importa: %s %.60s (codigo %d)", texto, nome ? nome : "", cod);
    fs->log(fs->ctx, linha);
}

/* a + sep + b em out (capacidade SI_CAMINHO) */
static int junta(const SaveFs *fs, char *out, const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    if (la + lb + 2 > SI_CAMINHO) return SI_E_CAMINHO;
    memcpy(out, a, la);
    out[la] = fs->sep;
    memcpy(out + la + 1, b, lb + 1);
    return 0;
}

static int sufixo(char *out, const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    if (la + lb + 1 > SI_CAMINHO) return SI_E_CAMINHO;
    memcpy(out, a, la);
    memcpy(out + la, b, lb + 1);
    return 0;
}

static int igual_sem_caixa(const char *a, const char *b) {
    for (; *a && *b; a++, b++) {
        int x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return x < y ? -1 : 1;
    }
    return *a ? 1 : (*b ? -1 : 0);
}

static int eh_psu(const char *nome) {
    size_t n = strlen(nome);
    return n > 4 && igual_sem_caixa(nome + n - 4, ".psu") == 0;
}

/* Cria a pasta e as que faltam acima dela (a primeira parte, "game:" ou "/", falha sem problema). */
static int cria_caminho(const SaveFs *fs, const char *caminho) {
    char parcial[SI_CAMINHO];
    size_t i, n = strlen(caminho);
    int r;
    if (n >= SI_CAMINHO) return SI_E_CAMINHO;
    for (i = 1; i < n; i++) {
        if (caminho[i] == fs->sep) {
            memcpy(parcial, caminho, i);
            parcial[i] = 0;
            fs->criar_pasta(fs->ctx, parcial);
        }
    }
    r = fs->criar_pasta(fs->ctx, caminho);
    if (r == 0 && fs->tipo(fs->ctx, caminho) != 2) r = 1;
    return r;
}

/* ---------------------------------------------------------------- a procura */

/* lê o .psu 'cam' e confere até a conversão; deixa o save do 360 em sTres. Devolve 0 ou o erro. */
static int le_converte(const SaveFs *fs, const char *cam) {
    unsigned long lidos = 0;
    int r = fs->ler(fs->ctx, cam, sPsu, CAP_PSU, &lidos);
    if (r == SI_FS_GRANDE) return SI_E_GRANDE;
    if (r != 0) return SI_E_LER;
    r = Psu_Ler(sPsu, lidos, &sSave, sVet, 16);
    if (r != PSU_OK) return r;
    return SaveConv_Ps2Para360(&sSave, &sTres);
}

static int tem_save(const SaveFs *fs, const SiPastas *pastas) {
    char c[SI_CAMINHO];
    if (junta(fs, c, pastas->save, SI_PASTA_SAVE) != 0) return 0;
    return fs->tipo(fs->ctx, c) == 1;
}

int SaveImp_Procurar(const SaveFs *fs, const SiPastas *pastas, SiLista *lista) {
    int total = 0, i, j;
    char cam[SI_CAMINHO];
    lista->n = 0;
    lista->ha_save = 0;
    if (fs->tipo(fs->ctx, pastas->import) != 2) return 0;
    if (fs->listar(fs->ctx, pastas->import, sNomes, MAX_NOMES, &total) != 0) {
        registra(fs, "nao listou a pasta import", "", 0);
        return 0;
    }
    for (i = 0; i < total && lista->n < SI_MAX; i++) {
        if (!eh_psu(sNomes[i])) continue;
        j = lista->n++;
        while (j > 0 && igual_sem_caixa(lista->item[j - 1].nome, sNomes[i]) > 0) { /* inserção em ordem */
            lista->item[j] = lista->item[j - 1];
            j--;
        }
        strcpy(lista->item[j].nome, sNomes[i]);
        lista->item[j].erro = 0;
    }
    if (lista->n == 0) return 0;
    for (i = 0; i < lista->n; i++) {
        if (junta(fs, cam, pastas->import, lista->item[i].nome) != 0) lista->item[i].erro = SI_E_CAMINHO;
        else lista->item[i].erro = le_converte(fs, cam);
        registra(fs, lista->item[i].erro ? "arquivo recusado" : "arquivo valido", lista->item[i].nome,
                 lista->item[i].erro);
    }
    lista->ha_save = tem_save(fs, pastas);
    return lista->n;
}

/* ---------------------------------------------------------------- gravação segura */

/* NOME.tmp com o conteúdo inteiro. Devolve 0 ou o código do sistema. */
static int grava_tmp(const SaveFs *fs, const char *cam, const unsigned char *buf, unsigned long n) {
    char tmp[SI_CAMINHO];
    int r = sufixo(tmp, cam, ".tmp");
    if (r != 0) return 1;
    if (fs->tipo(fs->ctx, tmp) != 0) fs->apagar(fs->ctx, tmp);
    return fs->gravar(fs->ctx, tmp, buf, n);
}

/* A troca do mc_commit (port/src/plat_mc.c): NOME.tmp vira NOME.new, o NOME antigo sai e o .new toma o lugar. */
static int troca(const SaveFs *fs, const char *cam) {
    char tmp[SI_CAMINHO], nova[SI_CAMINHO];
    int r;
    if (sufixo(tmp, cam, ".tmp") != 0 || sufixo(nova, cam, ".new") != 0) return 1;
    if (fs->tipo(fs->ctx, nova) != 0) {
        r = fs->apagar(fs->ctx, nova);
        if (r != 0) return r;
    }
    r = fs->mover(fs->ctx, tmp, nova);
    if (r != 0) return r;
    if (fs->tipo(fs->ctx, cam) != 0) {
        r = fs->apagar(fs->ctx, cam);
        if (r != 0) return r;
    }
    return fs->mover(fs->ctx, nova, cam);
}

static void limpa_temporarios(const SaveFs *fs, const char *cam) {
    char t[SI_CAMINHO];
    if (sufixo(t, cam, ".tmp") == 0 && fs->tipo(fs->ctx, t) != 0) fs->apagar(fs->ctx, t);
    if (sufixo(t, cam, ".new") == 0 && fs->tipo(fs->ctx, t) != 0) fs->apagar(fs->ctx, t);
}

/* ---------------------------------------------------------------- movimentos */

static int move_um(const SaveFs *fs, const SiPastas *pastas, const char *nome, const char *sub, int *sistema) {
    char dir[SI_CAMINHO], de[SI_CAMINHO], para[SI_CAMINHO], cand[SI_NOME * 2 + 16];
    size_t ponto;
    int r, n;
    if (junta(fs, dir, pastas->import, sub) != 0 || junta(fs, de, pastas->import, nome) != 0) return SI_E_CAMINHO;
    r = fs->criar_pasta(fs->ctx, dir);
    if (r != 0) {
        *sistema = r;
        return SI_E_PASTA;
    }
    if (junta(fs, para, dir, nome) != 0) return SI_E_CAMINHO;
    ponto = strlen(nome) - 4; /* só chega aqui com nome de .psu (4 caracteres de extensão) */
    for (n = 2; fs->tipo(fs->ctx, para) != 0; n++) {
        if (n > 9999) return SI_E_MOVER;
        sprintf(cand, "%.*s_%d%s", (int)ponto, nome, n, nome + ponto);
        if (junta(fs, para, dir, cand) != 0) return SI_E_CAMINHO;
    }
    r = fs->mover(fs->ctx, de, para);
    if (r != 0) {
        *sistema = r;
        return SI_E_MOVER;
    }
    return 0;
}

/* O escolhido vai para done, os recusados para failed, os outros para skipped. Devolve quantos não se moveram. */
static int move_todos(const SaveFs *fs, const SiPastas *pastas, const SiLista *lista, int escolhido,
                      SiFalha *falha) {
    int i, mal = 0;
    for (i = 0; i < lista->n; i++) {
        int sis = 0;
        const char *sub = i == escolhido ? "done" : (lista->item[i].erro ? "failed" : "skipped");
        int r = move_um(fs, pastas, lista->item[i].nome, sub, &sis);
        if (r != 0) {
            mal++;
            if (falha->sistema == 0) falha->sistema = sis;
            registra(fs, "nao moveu", lista->item[i].nome, sis);
        } else {
            registra(fs, sub, lista->item[i].nome, 0);
        }
    }
    return mal;
}

int SaveImp_Pular(const SaveFs *fs, const SiPastas *pastas, const SiLista *lista, SiFalha *falha) {
    SiFalha local;
    int mal;
    if (!falha) falha = &local;
    memset(falha, 0, sizeof(*falha));
    mal = move_todos(fs, pastas, lista, -1, falha);
    if (mal) {
        falha->passo = SI_P_MOVER;
        falha->movidos_mal = mal;
    }
    return mal;
}

/* ---------------------------------------------------------------- importação */

/* Lê os três arquivos atuais da pasta do save para sAtual (o que existir). 0 ou o código do sistema. */
static int le_atual(const SaveFs *fs, const SiPastas *pastas, int *sistema) {
    char cam[SI_CAMINHO];
    int i, r;
    for (i = 0; i < 3; i++) {
        sTam[i] = 0;
        sExiste[i] = 0;
        if (junta(fs, cam, pastas->save, kNomes[i]) != 0) return SI_E_CAMINHO;
        if (fs->tipo(fs->ctx, cam) != 1) continue;
        r = fs->ler(fs->ctx, cam, sAtual[i], CAP_ARQ, &sTam[i]);
        if (r != 0) {
            *sistema = r == SI_FS_GRANDE ? 0 : r;
            return r == SI_FS_GRANDE ? SI_E_GRANDE : SI_E_LER;
        }
        sExiste[i] = 1;
    }
    return 0;
}

/* Copia o save atual para backup\NNN (o próximo número livre). Devolve 0 ou o erro; *numero recebe o NNN. */
static int faz_backup(const SaveFs *fs, const SiPastas *pastas, int *numero, int *sistema) {
    char nnn[8], pasta[SI_CAMINHO], cam[SI_CAMINHO];
    int n, i, r;
    r = cria_caminho(fs, pastas->backup);
    if (r != 0) {
        *sistema = r;
        return SI_E_BACKUP;
    }
    for (n = 1; n < 1000; n++) {
        sprintf(nnn, "%03d", n);
        if (junta(fs, pasta, pastas->backup, nnn) != 0) return SI_E_CAMINHO;
        if (fs->tipo(fs->ctx, pasta) == 0) break;
    }
    if (n >= 1000) return SI_E_BACKUP;
    r = fs->criar_pasta(fs->ctx, pasta);
    if (r != 0) {
        *sistema = r;
        return SI_E_BACKUP;
    }
    *numero = n;
    for (i = 0; i < 3; i++) {
        if (!sExiste[i]) continue;
        if (junta(fs, cam, pasta, kNomes[i]) != 0) return SI_E_CAMINHO;
        r = fs->gravar(fs->ctx, cam, sAtual[i], sTam[i]);
        if (r != 0) {
            *sistema = r;
            return SI_E_BACKUP;
        }
    }
    return 0;
}

static const unsigned char *novo_arq(int i, unsigned long *tam) {
    *tam = i == 0 ? SAVELARG_TAM_ICONSYS : (i == 1 ? SAVELARG_TAM_ICO : SAVELARG_TAM_DADOS);
    return i == 0 ? sTres.iconsys : (i == 1 ? sTres.ico : sTres.dados);
}

/* Depois de uma troca que falhou no arquivo 'ate' (inclusive): recoloca o que existia, arquivo por arquivo. */
static int restaura(const SaveFs *fs, const SiPastas *pastas, int ate) {
    char cam[SI_CAMINHO];
    int i, ok = 1;
    for (i = 0; i < 3; i++) {
        if (junta(fs, cam, pastas->save, kNomes[i]) != 0) return 0;
        limpa_temporarios(fs, cam);
        if (i > ate) continue;
        if (sExiste[i]) {
            if (grava_tmp(fs, cam, sAtual[i], sTam[i]) != 0 || troca(fs, cam) != 0) ok = 0;
            limpa_temporarios(fs, cam);
        } else if (fs->tipo(fs->ctx, cam) != 0) {
            if (fs->apagar(fs->ctx, cam) != 0) ok = 0; /* o arquivo novo de um save que não existia */
        }
    }
    return ok;
}

int SaveImp_Importar(const SaveFs *fs, const SiPastas *pastas, const SiLista *lista, int indice, SiFalha *falha) {
    SiFalha local;
    char cam[SI_CAMINHO];
    int i, r, sis = 0, mal, tinha = 0;
    unsigned long tam;
    if (!falha) falha = &local;
    memset(falha, 0, sizeof(*falha));
    falha->passo = SI_P_LER;
    if (indice < 0 || indice >= lista->n) {
        falha->codigo = SI_E_ESCOLHA;
        return SI_E_ESCOLHA;
    }
    if (lista->item[indice].erro != 0) {
        falha->codigo = lista->item[indice].erro;
        return SI_E_RECUSADO;
    }
    /* 1: o .psu de novo, do disco (não se confia na procura) */
    if (junta(fs, cam, pastas->import, lista->item[indice].nome) != 0) {
        falha->codigo = SI_E_CAMINHO;
        return SI_E_CAMINHO;
    }
    r = le_converte(fs, cam);
    if (r != 0) {
        falha->codigo = r;
        registra(fs, "nao leu para importar", lista->item[indice].nome, r);
        return r;
    }
    /* 2: a cópia do que existe. Falhou: nada foi tocado. */
    r = le_atual(fs, pastas, &sis);
    if (r == 0) {
        for (i = 0; i < 3; i++) tinha |= sExiste[i];
        if (tinha) r = faz_backup(fs, pastas, &falha->backup, &sis);
    }
    if (r != 0) {
        falha->passo = SI_P_BACKUP;
        falha->codigo = r;
        falha->sistema = sis;
        registra(fs, "backup falhou", "", sis);
        return r == SI_E_CAMINHO ? r : SI_E_BACKUP;
    }
    if (tinha) registra(fs, "backup feito", "", falha->backup);
    /* 3: a pasta do save e os temporários. Falhou: o save atual segue intacto (só sobram .tmp, que se apagam). */
    r = cria_caminho(fs, pastas->save);
    if (r != 0) {
        falha->passo = SI_P_GRAVAR;
        falha->sistema = r;
        return SI_E_PASTA;
    }
    for (i = 0; i < 3; i++) {
        const unsigned char *dados = novo_arq(i, &tam);
        if (junta(fs, cam, pastas->save, kNomes[i]) != 0) r = 1;
        else r = grava_tmp(fs, cam, dados, tam);
        if (r != 0) {
            int j;
            for (j = 0; j <= i; j++) {
                if (junta(fs, cam, pastas->save, kNomes[j]) == 0) limpa_temporarios(fs, cam);
            }
            falha->passo = SI_P_GRAVAR;
            falha->sistema = r;
            registra(fs, "gravacao do temporario falhou", kNomes[i], r);
            return SI_E_GRAVAR;
        }
    }
    /* 4: a troca, o arquivo de dados por último (é o que o jogo lê). Falhou: recoloca o que havia. */
    for (i = 0; i < 3; i++) {
        if (junta(fs, cam, pastas->save, kNomes[i]) != 0) r = 1;
        else r = troca(fs, cam);
        if (r != 0) {
            falha->passo = SI_P_TROCA;
            falha->sistema = r;
            falha->restaurado = restaura(fs, pastas, i);
            registra(fs, falha->restaurado ? "troca falhou, save anterior recolocado" : "troca falhou, restauracao falhou",
                     kNomes[i], r);
            return SI_E_TROCA;
        }
    }
    registra(fs, "save importado", lista->item[indice].nome, 0);
    /* 5: os movimentos. O save já está no lugar: falhar aqui não o desfaz. */
    mal = move_todos(fs, pastas, lista, indice, falha);
    if (mal) {
        falha->passo = SI_P_MOVER;
        falha->movidos_mal = mal;
        falha->importado = 1;
        return SI_E_MOVER;
    }
    return 0;
}

/* ---------------------------------------------------------------- exportação */

static int exporta(const SaveFs *fs, const SiPastas *pastas, SiFalha *falha) {
    char cam[SI_CAMINHO], dest[SI_CAMINHO];
    PsuData d;
    unsigned long escrito = 0, lidos, tam, n;
    int i, r;
    if (fs->tipo(fs->ctx, pastas->exportar) != 2) return 1; /* sem a pasta: nada */
    for (i = 0; i < 3; i++) {
        unsigned char *buf = i == 0 ? sTres.iconsys : (i == 1 ? sTres.ico : sTres.dados);
        tam = i == 0 ? SAVELARG_TAM_ICONSYS : (i == 1 ? SAVELARG_TAM_ICO : SAVELARG_TAM_DADOS);
        falha->passo = SI_P_EXPORTA_LER;
        if (junta(fs, cam, pastas->save, kNomes[i]) != 0) return SI_E_CAMINHO;
        r = fs->ler(fs->ctx, cam, buf, tam, &lidos);
        if (r != 0) {
            falha->sistema = r == SI_FS_GRANDE ? 0 : r;
            return r == SI_FS_GRANDE ? SAVECONV_E_TAMANHO : SI_E_LER;
        }
        if (lidos != tam) return SAVECONV_E_TAMANHO;
    }
    d.seg = SI_DATA_EXPORTA_SEG;
    d.min = SI_DATA_EXPORTA_MIN;
    d.hora = SI_DATA_EXPORTA_HORA;
    d.dia = SI_DATA_EXPORTA_DIA;
    d.mes = SI_DATA_EXPORTA_MES;
    d.ano = SI_DATA_EXPORTA_ANO;
    falha->passo = SI_P_EXPORTA_CONV;
    r = SaveConv_360ParaPs2(&sTres, &sTmp, sVet, &sSave, &d, &d);
    if (r != 0) return r;
    n = Psu_TamanhoEscrita(&sSave);
    if (n == 0 || n > CAP_PSU) return SI_E_EXPORTA;
    r = Psu_Escrever(&sSave, sPsu, CAP_PSU, &escrito);
    if (r != 0) return r;
    falha->passo = SI_P_EXPORTA_GRAVAR;
    if (junta(fs, dest, pastas->exportar, SI_PSU_EXPORTA) != 0) return SI_E_CAMINHO;
    r = grava_tmp(fs, dest, sPsu, escrito);
    if (r == 0) r = troca(fs, dest);
    if (r != 0) {
        falha->sistema = r;
        limpa_temporarios(fs, dest);
        return SI_E_EXPORTA;
    }
    return 0;
}

int SaveImp_Exportar(const SaveFs *fs, const SiPastas *pastas, SiFalha *falha) {
    SiFalha local;
    if (!falha) falha = &local;
    memset(falha, 0, sizeof(*falha));
    return exporta(fs, pastas, falha);
}

/* o caminho termina em BASLUS-21678DBZT3<barra>BASLUS-21678DBZT3? */
static int eh_arquivo_de_dados(const char *g) {
    size_t n = strlen(g), k = strlen(SI_PASTA_SAVE);
    if (n < 2 * k + 1) return 0;
    if (strcmp(g + n - k, SI_PASTA_SAVE) != 0) return 0;
    if (g[n - k - 1] != '/' && g[n - k - 1] != '\\') return 0;
    if (strncmp(g + n - 2 * k - 1, SI_PASTA_SAVE, k) != 0) return 0;
    return n == 2 * k + 1 || g[n - 2 * k - 2] == '/' || g[n - 2 * k - 2] == '\\';
}

int SaveImp_AposGravar(const SaveFs *fs, const SiPastas *pastas, const char *gravado) {
    SiFalha falha;
    int r;
    if (!gravado || !eh_arquivo_de_dados(gravado)) return 0;
    r = SaveImp_Exportar(fs, pastas, &falha);
    if (r == 1) return 0;
    if (fs->log) {
        char linha[160];
        if (r == 0) sprintf(linha, "exporta: gerou export\\%s", SI_PSU_EXPORTA);
        else sprintf(linha, "exporta: FALHOU no passo %d (%s), codigo %d, sistema %d; o save do jogo nao foi afetado",
                     falha.passo, SaveImp_TextoErro(r), r, falha.sistema);
        fs->log(fs->ctx, linha);
    }
    return 0;
}

/* ---------------------------------------------------------------- textos */

const char *SaveImp_TextoErro(int codigo) {
    switch (codigo) {
    case SI_E_GRANDE: return "file is too big";
    case SI_E_LER: return "cannot read file";
    case SI_E_RECUSADO: return "file cannot be imported";
    case SI_E_CAMINHO: return "path is too long";
    case SI_E_BACKUP: return "backup failed";
    case SI_E_GRAVAR: return "could not write the save";
    case SI_E_TROCA: return "could not replace the save";
    case SI_E_MOVER: return "could not move the file";
    case SI_E_PASTA: return "could not create a folder";
    case SI_E_ESCOLHA: return "bad choice";
    case SI_E_EXPORTA: return "export failed";
    default: return SaveConv_TextoErro(codigo);
    }
}

const char *SaveImp_TextoPasso(int passo) {
    switch (passo) {
    case SI_P_LER: return "reading the file";
    case SI_P_BACKUP: return "copying the current save to backup";
    case SI_P_GRAVAR: return "writing the new save";
    case SI_P_TROCA: return "replacing the save";
    case SI_P_MOVER: return "moving the files";
    default: return "exporting";
    }
}
