/*
 * O amostrador de perfil da thread do jogo, a parte portável (amostra_hist.h). C89: o cl do 360 em -TC e o gcc do
 * teste no host (port/x360/tools/teste_amostra/teste.c) compilam este mesmo arquivo.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amostra_hist.h"

void Diario_Escrever(const char *formato, ...);

/* ---- a pilha da thread suspensa ---------------------------------------------------------------------------------- */

#define CODIGO_INI 0x80000000u /* o Iar de um quadro de armadilha: código do núcleo, do xam ou da imagem */
#define CODIGO_FIM 0x90000000u
#define QUADROS_MAX 64          /* quadros seguidos pelo encadeamento */
#define QUADRO_NUCLEO_MAX 0x10000u /* maior quadro do núcleo aceito no encadeamento */
#define QUADRO_JOGO_MAX 0x100000u  /* maior quadro do jogo (a pilha do jogo tem 16 MB) */
#define VARREDURA_MAX 0x4000u      /* bytes varridos acima do r1 salvo, sem o encadeamento */

static int NaImagem(const AmostraImagem *img, unsigned e) {
    return e >= img->textoIni && e < img->textoFim;
}

unsigned AmostraHist_Funcao(const AmostraImagem *img, unsigned e) {
    unsigned lo = 0, hi = img->nPdata, meio, ini, tam;
    if (img->pdata == NULL || hi == 0) {
        return e;
    }
    while (hi - lo > 1) { /* o maior início que não passa de e */
        meio = (lo + hi) / 2;
        if (img->pdata[2 * meio] <= e) {
            lo = meio;
        } else {
            hi = meio;
        }
    }
    ini = img->pdata[2 * lo];
    tam = ((img->pdata[2 * lo + 1] >> 8) & 0x3FFFFFu) * 4; /* FunctionLength, em instruções */
    return e >= ini && e - ini < tam ? ini : e;
}

/* Um quadro de armadilha em o: r1 de 64 bits com a parte alta zero e alinhado, o Iar em código. elo != 0: o r1 tem de
   ser o próximo elo do encadeamento (o quadro do manipulador aponta para o quadro interrompido); elo == 0: só acima de
   o e dentro da pilha. */
static int Armadilha(AmostraLer ler, unsigned o, unsigned elo, unsigned base, unsigned *r1, unsigned *iar) {
    unsigned alto;
    if (!ler(o + AMOSTRA_TF_R1, &alto) || alto != 0 || !ler(o + AMOSTRA_TF_R1 + 4, r1) || (*r1 & 3) != 0) {
        return 0;
    }
    if (elo != 0 ? *r1 != elo : (*r1 <= o || *r1 > base)) {
        return 0;
    }
    return ler(o + AMOSTRA_TF_IAR, iar) && *iar >= CODIGO_INI && *iar < CODIGO_FIM && (*iar & 3) == 0;
}

/* Os retornos que caem na imagem, de f para cima: no 360 o prólogo guarda o Lr em -8 do r1 de quem chamou (stw r12,-8(r1)
   antes do stwu, ou o __savegprlr_N), então o retorno da função dona do quadro f está em -8 do elo seguinte. */
static void Retornos(const AmostraImagem *img, AmostraLer ler, unsigned f, unsigned limite, unsigned base,
                     unsigned maxQuadro, AmostraCrua *c) {
    unsigned i, b, ra;
    for (i = 0; i < QUADROS_MAX && c->nRet < AMOSTRA_RET; i++) {
        if (f < limite || f >= base || !ler(f, &b) || (b & 3) != 0 || b <= f || b > base || b - f > maxQuadro) {
            return;
        }
        if (ler(b - 8, &ra) && NaImagem(img, ra)) {
            c->ret[c->nRet++] = ra;
        }
        f = b;
    }
}

void AmostraHist_LerPilha(const AmostraImagem *img, AmostraLer ler, unsigned ks, unsigned limite, unsigned base,
                          AmostraCrua *c) {
    unsigned i, f, b, o, ultimo, r1 = 0, iar = 0, lr = 0, bOk;
    c->tipo = AMOSTRA_NADA;
    c->iar = c->lr = c->r1 = c->armadilha = 0;
    c->nRet = 0;
    if (ks < limite || ks >= base || (ks & 3) != 0) {
        return;
    }
    /* 1. o encadeamento a partir do r1 salvo: em cada quadro, um quadro de armadilha cujo r1 seja o próximo elo */
    f = ks;
    for (i = 0; i < QUADROS_MAX; i++) {
        if (!ler(f, &b)) {
            break;
        }
        bOk = (b & 3) == 0 && b > f && b <= base && b - f <= QUADRO_NUCLEO_MAX;
        ultimo = bOk ? (b - f >= AMOSTRA_TF_TAM ? b - AMOSTRA_TF_TAM : f - 8) : f;
        for (o = f; o <= ultimo && o + AMOSTRA_TF_TAM <= base; o += 8) {
            if (Armadilha(ler, o, bOk ? b : 0, base, &r1, &iar)) {
                c->tipo = AMOSTRA_ARMADILHA;
                c->armadilha = o;
                goto achou;
            }
        }
        if (!bOk) {
            break;
        }
        f = b;
    }
    /* 2. sem o encadeamento: a varredura acima do r1 salvo, só com o Iar na imagem e o r1 com um elo válido */
    for (o = ks; o + AMOSTRA_TF_TAM <= base && o - ks < VARREDURA_MAX; o += 8) {
        if (Armadilha(ler, o, 0, base, &r1, &iar) && NaImagem(img, iar) && r1 >= o + AMOSTRA_TF_TAM &&
            ler(r1, &b) && b > r1 && b <= base) {
            c->tipo = AMOSTRA_VARREDURA;
            c->armadilha = o;
            goto achou;
        }
    }
    /* 3. nenhum quadro de armadilha: uma espera do núcleo; só os retornos que caem na imagem */
    Retornos(img, ler, ks, limite, base, QUADRO_JOGO_MAX, c);
    if (c->nRet != 0) {
        c->tipo = AMOSTRA_SEM_IAR;
    }
    return;
achou:
    if (!ler(c->armadilha + AMOSTRA_TF_LR, &lr)) {
        lr = 0;
    }
    c->iar = iar;
    c->lr = lr;
    c->r1 = r1;
    Retornos(img, ler, r1, limite, base, QUADRO_JOGO_MAX, c);
}

static unsigned Acrescentar(AmostraUma *u, unsigned f) {
    unsigned i;
    for (i = 0; i < u->n; i++) {
        if (u->funcs[i] == f) {
            return 0;
        }
    }
    if (u->n < AMOSTRA_FUNCS) {
        u->funcs[u->n++] = f;
    }
    return 1;
}

/* A folha e o chamador: um Lr numa função que não a da folha é o chamador (a folha sem quadro, que não guarda o Lr, ou
   que ainda não chamou ninguém); um Lr na própria folha (ela já chamou outra) deixa o chamador para o primeiro retorno
   da pilha, que é o da folha. O inclusivo: a folha, a função do Lr e as dos retornos, cada uma uma vez. */
void AmostraHist_Resumir(const AmostraImagem *img, const AmostraCrua *c, AmostraUma *u) {
    unsigned i, fl = 0, primeiro;
    u->tipo = c->tipo;
    u->folha = AMOSTRA_FOLHA_ESPERA;
    u->chamador = 0;
    u->n = 0;
    if (c->tipo == AMOSTRA_NADA) {
        return;
    }
    primeiro = c->nRet != 0 ? AmostraHist_Funcao(img, c->ret[0]) : 0;
    if (c->tipo == AMOSTRA_ARMADILHA || c->tipo == AMOSTRA_VARREDURA) {
        u->folha = NaImagem(img, c->iar) ? AmostraHist_Funcao(img, c->iar) : AMOSTRA_FOLHA_NUCLEO;
        fl = NaImagem(img, c->lr) ? AmostraHist_Funcao(img, c->lr) : 0;
        u->chamador = fl != 0 && fl != u->folha ? fl : primeiro;
        if (u->folha != AMOSTRA_FOLHA_NUCLEO) {
            Acrescentar(u, u->folha);
        }
        if (fl != 0) {
            Acrescentar(u, fl);
        }
    } else {
        u->chamador = primeiro;
    }
    for (i = 0; i < c->nRet; i++) {
        Acrescentar(u, AmostraHist_Funcao(img, c->ret[i]));
    }
}

/* ---- os histogramas ---------------------------------------------------------------------------------------------- */

#define FOLHAS 8192u   /* potências de 2 */
#define PARES 16384u
#define INCLUSIVAS 8192u
#define BUFFER 2048u   /* amostras de um quadro do jogo */
#define SONDAS 256u
#define TOPO 200u      /* entradas despejadas por tabela */
#define POR_LINHA 8u
#define FAIXA_MS 5u    /* as durações dos quadros, em faixas de 5 ms */
#define FAIXAS 41u     /* a última: 200 ms ou mais */

typedef struct {
    unsigned a, b, n;
} Entrada;

typedef struct {
    Entrada folhas[FOLHAS], pares[PARES], inclusivas[INCLUSIVAS];
    unsigned quadros, amostras, tipos[AMOSTRA_TIPOS], descartadas, fora;
    double ms;
} Classe;

typedef struct {
    Classe classes[AMOSTRA_CLASSES];
    AmostraUma buffer[BUFFER];
    unsigned ordem[PARES];
    unsigned nBuffer, descartadas, despejos, faixas[AMOSTRA_CLASSES][FAIXAS];
    unsigned limiarMs, longoMs;
} Estado;

static Estado *s;
static const char *const kClasses[AMOSTRA_CLASSES] = {"normal", "lento", "longo"};

unsigned AmostraHist_Tamanho(void) {
    return (unsigned)sizeof(Estado);
}

int AmostraHist_Iniciar(void *memoria, unsigned tamanho, unsigned limiarMs, unsigned longoMs) {
    if (memoria == NULL || tamanho < sizeof(Estado)) {
        return 0;
    }
    memset(memoria, 0, sizeof(Estado));
    s = (Estado *)memoria;
    s->limiarMs = limiarMs;
    s->longoMs = longoMs;
    return 1;
}

void AmostraHist_Guardar(const AmostraUma *u) {
    if (s == NULL) {
        return;
    }
    if (s->nBuffer < BUFFER) {
        s->buffer[s->nBuffer++] = *u;
    } else {
        s->descartadas++;
    }
}

static void Somar(Entrada *t, unsigned mascara, unsigned a, unsigned b, unsigned *fora) {
    unsigned h = a * 2654435761u ^ b * 2246822519u, k, i;
    h ^= h >> 15;
    for (k = 0; k < SONDAS; k++) {
        i = (h + k) & mascara;
        if (t[i].n == 0) {
            t[i].a = a;
            t[i].b = b;
            t[i].n = 1;
            return;
        }
        if (t[i].a == a && t[i].b == b) {
            t[i].n++;
            return;
        }
    }
    (*fora)++;
}

void AmostraHist_FimDoQuadro(unsigned quadros, double ms) {
    unsigned i, j, c, faixa;
    double porQuadro;
    Classe *k;
    const AmostraUma *u;
    if (s == NULL) {
        return;
    }
    if (quadros == 0) {
        quadros = 1;
    }
    porQuadro = ms / quadros;
    c = porQuadro < s->limiarMs ? AMOSTRA_NORMAL : porQuadro < s->longoMs ? AMOSTRA_LENTO : AMOSTRA_LONGO;
    faixa = (unsigned)(porQuadro / FAIXA_MS);
    s->faixas[c][faixa < FAIXAS ? faixa : FAIXAS - 1] += quadros;
    k = &s->classes[c];
    k->quadros += quadros;
    k->ms += ms;
    k->descartadas += s->descartadas;
    for (i = 0; i < s->nBuffer; i++) {
        u = &s->buffer[i];
        k->amostras++;
        k->tipos[u->tipo < AMOSTRA_TIPOS ? u->tipo : AMOSTRA_NADA]++;
        if (u->tipo == AMOSTRA_NADA) {
            continue;
        }
        Somar(k->folhas, FOLHAS - 1, u->folha, 0, &k->fora);
        Somar(k->pares, PARES - 1, u->folha, u->chamador, &k->fora);
        for (j = 0; j < u->n; j++) {
            Somar(k->inclusivas, INCLUSIVAS - 1, u->funcs[j], 0, &k->fora);
        }
    }
    s->nBuffer = 0;
    s->descartadas = 0;
}

unsigned AmostraHist_Quadros(int classe) {
    return s != NULL && classe >= 0 && classe < AMOSTRA_CLASSES ? s->classes[classe].quadros : 0;
}

unsigned AmostraHist_Amostras(int classe) {
    return s != NULL && classe >= 0 && classe < AMOSTRA_CLASSES ? s->classes[classe].amostras : 0;
}

static const Entrada *sOrdenando;

/* a contagem maior primeiro; no empate, as chaves (o despejo sai igual para a mesma entrada) */
static int Comparar(const void *x, const void *y) {
    const Entrada *a = &sOrdenando[*(const unsigned *)x], *b = &sOrdenando[*(const unsigned *)y];
    if (a->n != b->n) {
        return a->n > b->n ? -1 : 1;
    }
    if (a->a != b->a) {
        return a->a < b->a ? -1 : 1;
    }
    return a->b < b->b ? -1 : a->b > b->b ? 1 : 0;
}

static void Tabela(unsigned d, const char *classe, const char *nome, const Entrada *t, unsigned tam, int par) {
    char linha[512];
    unsigned i, n = 0, m, p = 0;
    for (i = 0; i < tam; i++) {
        if (t[i].n != 0) {
            s->ordem[n++] = i;
        }
    }
    sOrdenando = t;
    qsort(s->ordem, n, sizeof(unsigned), Comparar);
    m = n < TOPO ? n : TOPO;
    for (i = 0; i < m; i++) {
        const Entrada *e = &t[s->ordem[i]];
        if (par) {
            sprintf(linha + p, " %08X/%08X:%u", e->a, e->b, e->n);
        } else {
            sprintf(linha + p, " %08X:%u", e->a, e->n);
        }
        p += (unsigned)strlen(linha + p);
        if ((i + 1) % POR_LINHA == 0 || i + 1 == m) {
            Diario_Escrever("amostra: d%u %s %s%s", d, classe, nome, linha);
            p = 0;
        }
    }
    Diario_Escrever("amostra: d%u %s %s: %u entradas, %u despejadas", d, classe, nome, n, m);
}

void AmostraHist_Despejar(const char *motivo, const char *cabecalho) {
    char linha[512];
    unsigned d, c, f, p;
    const Classe *k;
    if (s == NULL) {
        return;
    }
    d = ++s->despejos;
    Diario_Escrever("amostra: d%u começo (%s): %s; limiar %u ms, longo %u ms", d, motivo, cabecalho, s->limiarMs,
                    s->longoMs);
    for (c = 0; c < AMOSTRA_CLASSES; c++) {
        k = &s->classes[c];
        Diario_Escrever("amostra: d%u %s: %u quadros, %.1f ms, %u amostras (armadilha %u, varredura %u, sem Iar %u, "
                        "nada %u), %u descartadas, %u fora das tabelas",
                        d, kClasses[c], k->quadros, k->ms, k->amostras, k->tipos[AMOSTRA_ARMADILHA],
                        k->tipos[AMOSTRA_VARREDURA], k->tipos[AMOSTRA_SEM_IAR], k->tipos[AMOSTRA_NADA], k->descartadas,
                        k->fora);
        p = 0;
        linha[0] = 0;
        for (f = 0; f < FAIXAS; f++) {
            if (s->faixas[c][f] != 0) {
                sprintf(linha + p, " %u:%u", f * FAIXA_MS, s->faixas[c][f]);
                p += (unsigned)strlen(linha + p);
            }
        }
        Diario_Escrever("amostra: d%u %s durações (ms:quadros)%s", d, kClasses[c], linha);
        if (k->amostras == 0) {
            continue;
        }
        Tabela(d, kClasses[c], "folha", k->folhas, FOLHAS, 0);
        Tabela(d, kClasses[c], "par", k->pares, PARES, 1);
        Tabela(d, kClasses[c], "pilha", k->inclusivas, INCLUSIVAS, 0);
    }
    Diario_Escrever("amostra: d%u fim", d);
}
