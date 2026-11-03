/* Desenho da tela SAVE IMPORT: o mesmo terminal preto, a mesma fonte, margem e passo da conversão da ISO
   (iso_tela_desenho.h, que este arquivo inclui: a fonte não se duplica). Separado de importa_tela.cpp para a prova
   host (port/x360/tools/prova_importa.py) compilar este mesmo desenho com o D3D falso. Não apresenta: quem chama faz o
   Present. Textos em iso_avisos.h (pendentes de aprovação do Lucas). */
#ifndef IMPORTA_TELA_DESENHO_H
#define IMPORTA_TELA_DESENHO_H
#include <string.h>
#include "iso_tela_desenho.h"
#include "save_importa.h"

#define IMP_SEL 0xffffffffu  /* a linha selecionada */
#define IMP_JANELA 8         /* arquivos visíveis ao mesmo tempo (a lista rola) */
#define IMP_NOME 28          /* colunas do nome; o resto vira reticências */
#define IMP_STATUS 34        /* colunas do motivo (o texto mais longo, "not a Budokai Tenkaichi 3 USA save", cabe) */
#define IMP_COLUNAS 66       /* o que cabe em 1280 px a 18 px por letra com a margem de 80 */

/* A janela da lista que acompanha o selecionado. */
static int impInicio(int n, int sel) {
    int ini = sel - IMP_JANELA + 1;
    if (n <= IMP_JANELA)
        return 0;
    if (ini < 0)
        ini = 0;
    if (ini > n - IMP_JANELA)
        ini = n - IMP_JANELA;
    return ini;
}

/* Nome em IMP_NOME colunas: o que passa vira "..." no fim. */
static void impNome(char *out, const char *nome) {
    size_t n = strlen(nome);
    if (n <= IMP_NOME) {
        strcpy(out, nome);
    } else {
        memcpy(out, nome, IMP_NOME - 3);
        strcpy(out + IMP_NOME - 3, "...");
    }
}

/* A tela de escolha. arq = arquivo selecionado, acao = 0 IMPORT / 1 SKIP, aviso = o recusado foi mandado importar. */
static void telaImportar(D3DDevice *dev, const char *versao, const SiLista &l, int arq, int acao, bool aviso) {
    char buf[160], nome[SI_NOME + 4];
    int i, ini = impInicio(l.n, arq), linha = 0;
    dev->Clear(0, NULL, D3DCLEAR_TARGET, TELA_FUNDO, 1, 0);
    sprintf(buf, ISO_IMP_T_TITULO, versao);
    texto(dev, buf, TELA_X, TELA_Y + (linha++) * TELA_PASSO, TELA_ESCALA, TELA_TEXTO);
    linha++;
    if (l.n > IMP_JANELA)
        sprintf(buf, "%s %d/%d", ISO_IMP_T_ACHADOS, arq + 1, l.n);
    else
        strcpy(buf, ISO_IMP_T_ACHADOS);
    texto(dev, buf, TELA_X, TELA_Y + (linha++) * TELA_PASSO, TELA_ESCALA, TELA_TEXTO);
    for (i = ini; i < l.n && i < ini + IMP_JANELA; i++, linha++) {
        unsigned cor = i == arq ? IMP_SEL : TELA_TEXTO;
        const char *st = l.item[i].erro ? SaveImp_TextoErro(l.item[i].erro) : ISO_IMP_T_OK;
        char motivo[IMP_STATUS + 1];
        int y = TELA_Y + linha * TELA_PASSO, x = TELA_X + (2 + IMP_NOME + 1) * 6 * TELA_ESCALA;
        impNome(nome, l.item[i].nome);
        sprintf(buf, "%c %s", i == arq ? '>' : ' ', nome);
        texto(dev, buf, TELA_X, y, TELA_ESCALA, cor);
        strncpy(motivo, st, IMP_STATUS);
        motivo[IMP_STATUS] = 0;
        texto(dev, motivo, x, y, TELA_ESCALA, l.item[i].erro ? TELA_FALHA : TELA_OK);
    }
    linha += IMP_JANELA - (l.n < IMP_JANELA ? l.n : IMP_JANELA); /* a lista ocupa sempre o mesmo espaço */
    linha++;
    if (l.ha_save)
        texto(dev, ISO_IMP_T_AVISO, TELA_X, TELA_Y + linha * TELA_PASSO, TELA_ESCALA, TELA_TEXTO);
    linha += 2;
    telaBotoes(buf, ISO_IMP_T_IMPORTAR, ISO_IMP_T_PULAR, acao);
    texto(dev, buf, TELA_X, TELA_Y + linha * TELA_PASSO, TELA_ESCALA, TELA_TEXTO);
    linha += 2;
    if (aviso)
        texto(dev, ISO_IMP_T_RECUSADO, TELA_X, TELA_Y + linha * TELA_PASSO, TELA_ESCALA, TELA_FALHA);
    linha += 2;
    texto(dev, ISO_IMP_T_AJUDA, TELA_X, TELA_Y + linha * TELA_PASSO, TELA_ESCALA, TELA_TEXTO);
}

/* Uma tela de mensagem: título colorido, linhas quebradas em TELA_P_COLUNAS, e a ajuda (NULL = sem ajuda). */
#define IMP_LINHAS 10
struct ImpMsg {
    const char *titulo;
    unsigned cor;
    int n;
    char l[IMP_LINHAS][TELA_P_COLUNAS * 2 + 1];
    const char *ajuda;
};

static void impAdd(ImpMsg &m, const char *texto_) {
    char quebra[4][TELA_P_COLUNAS + 1];
    int k = telaQuebrar(quebra, 4, texto_);
    for (int i = 0; i < k && m.n < IMP_LINHAS; i++) {
        char *d = m.l[m.n++];
        strcpy(d, quebra[i]);
        for (; *d; d++) /* os motivos vêm em minúsculas: a tela é de maiúsculas */
            if (*d >= 'a' && *d <= 'z')
                *d -= 32;
    }
}

/* O resultado de SaveImp_Importar (r, f) ou de SaveImp_Pular (skip = 1, r = nº de movidos mal). */
static void impResultado(ImpMsg &m, int r, const SiFalha &f, bool skip) {
    char b[200];
    m.n = 0;
    m.ajuda = ISO_IMP_T_AJUDA_OK;
    if (skip) {
        m.titulo = ISO_IMP_T_PULAR_TITULO;
        m.cor = TELA_FALHA;
        impAdd(m, ISO_IMP_T_INTACTO);
        sprintf(b, ISO_IMP_T_PULAR_MOVER, r);
        impAdd(m, b);
        impAdd(m, ISO_IMP_T_PULAR_DE_NOVO);
        if (f.sistema) {
            sprintf(b, ISO_IMP_T_SISTEMA, f.sistema);
            impAdd(m, b);
        }
        return;
    }
    if (r == 0) {
        m.titulo = ISO_IMP_T_FEITO_TITULO;
        m.cor = TELA_OK;
        impAdd(m, ISO_IMP_T_FEITO);
        if (f.backup) {
            sprintf(b, ISO_IMP_T_FEITO_BACKUP, f.backup);
            impAdd(m, b);
        }
        impAdd(m, ISO_IMP_T_FEITO_ARQUIVOS);
        return;
    }
    if (f.passo == SI_P_MOVER && f.importado) {
        m.titulo = ISO_IMP_T_FEITO_TITULO;
        m.cor = TELA_FALHA;
        sprintf(b, ISO_IMP_T_MOVER, f.movidos_mal);
        impAdd(m, b);
        if (f.sistema) {
            sprintf(b, ISO_IMP_T_SISTEMA, f.sistema);
            impAdd(m, b);
        }
        impAdd(m, ISO_IMP_T_MOVER_DE_NOVO);
        return;
    }
    m.titulo = ISO_IMP_T_FALHA_TITULO;
    m.cor = TELA_FALHA;
    sprintf(b, ISO_IMP_T_PASSO, SaveImp_TextoPasso(f.passo));
    impAdd(m, b);
    sprintf(b, ISO_IMP_T_MOTIVO, SaveImp_TextoErro(f.codigo ? f.codigo : r));
    impAdd(m, b);
    if (f.sistema) {
        sprintf(b, ISO_IMP_T_SISTEMA, f.sistema);
        impAdd(m, b);
    }
    if (f.passo == SI_P_TROCA && !f.restaurado) {
        impAdd(m, ISO_IMP_T_NAO_RECOLOCADO);
        if (f.backup) {
            sprintf(b, ISO_IMP_T_COPIA, f.backup);
            impAdd(m, b);
        }
    } else {
        impAdd(m, f.passo == SI_P_TROCA ? ISO_IMP_T_RECOLOCADO : ISO_IMP_T_INTACTO);
    }
}

static void telaImportaMsg(D3DDevice *dev, const char *versao, const ImpMsg &m) {
    char buf[160];
    int i, linha = 0;
    dev->Clear(0, NULL, D3DCLEAR_TARGET, TELA_FUNDO, 1, 0);
    sprintf(buf, ISO_IMP_T_TITULO, versao);
    texto(dev, buf, TELA_X, TELA_Y + (linha++) * TELA_PASSO, TELA_ESCALA, TELA_TEXTO);
    linha++;
    texto(dev, m.titulo, TELA_X, TELA_Y + (linha++) * TELA_PASSO, TELA_ESCALA, m.cor);
    for (i = 0; i < m.n; i++)
        texto(dev, m.l[i], TELA_X, TELA_Y + (linha++) * TELA_PASSO, TELA_ESCALA, TELA_TEXTO);
    linha++;
    if (m.ajuda)
        texto(dev, m.ajuda, TELA_X, TELA_Y + linha * TELA_PASSO, TELA_ESCALA, TELA_TEXTO);
}
#endif
