/*
 * Receitas de cartões avançados nativos (F7, etapa 2; docs/x360/plano/F7/02.md). Código de teste das builds sem janela
 * (x86 e PPC no qemu), nunca do 360 nem do produto: o roteiro (roteiro.c) chama daqui os passos "receita aplica" e
 * "receita confere".
 *
 * Uma receita (port/x360/receitas/NOME.txt) parte do save do New Game (Save_SetDefaults, com o cartão vazio no começo
 * da rodada) e escreve só campos identificados, os da tabela port/x360/receitas/campos.txt, que este arquivo e
 * port/x360/tools/f7_receitas.py leem igual. Ela põe o estado imediatamente anterior a um marco; o jogo joga o marco,
 * dá a recompensa e grava pelo McFlow. Depois da carga normal (Continue), "receita confere" diz se o save na memória
 * tem o que a receita espera ("depois") e o que ela escreveu e o jogo não devia mudar.
 *
 * Linhas da receita (palavras separadas por espaço, "#" comenta):
 *   marco NOME              o nome do arquivo, sem .txt
 *   evento TEXTO...         o que o roteiro joga (só documentação)
 *   exige CAMPO[I..J] = V   antes de escrever: o save do New Game tem V ali (senão a receita para)
 *   escreve CAMPO[I..J] = V
 *   ou CAMPO[I] = MASCARA   s32: liga os bits
 *   liga CAMPO[I..J]        campo de bits (b32/b64): liga
 *   chama Save_AddItem N | Save_AddMoney N    os setters do próprio jogo (src/sys/save.c)
 *   depois CAMPO[I..J] = V | tem MASCARA | sem MASCARA | >= V | <= V
 *   arquivo NOME TAMANHO    um arquivo do cartão depois do marco (só o verificador confere)
 * Sem "[...]" o campo tem de ter um elemento só. O que se escreveu (escreve, ou, liga, Save_AddItem) vale também
 * como "depois", a menos que um "depois" fale do mesmo elemento.
 *
 * Valores no log ("receita: ..."): a assinatura FNV-1a de 64 bits do texto da receita (o verificador a recalcula do
 * arquivo versionado) e do bloco de 0x4000 bytes antes e depois das escritas, e a semente (BT3_SEED, plat_sys.c).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sys/save.h" /* SAVE_SIZE, Save_AddItem, Save_AddMoney */

enum { T_U8, T_U16, T_S32, T_B32, T_B64 };
enum { O_EXIGE, O_ESCREVE, O_OU, O_LIGA, O_ITEM, O_DINHEIRO, O_ARQUIVO, O_IGUAL, O_TEM, O_SEM, O_MAIOR, O_MENOR };

typedef struct {
    char nome[40];
    int tipo;
    long desl;
    int qtd, passo;
} RCampo;

typedef struct {
    int op, campo, i0, i1;
    long valor;
    int linha;
} ROp;

#define R_MAX_CAMPOS 64
#define R_MAX_OPS 256
#define R_MAX_RECEITAS 4

typedef struct {
    char nome[48];
    unsigned long long fonte; /* FNV do texto */
    ROp op[R_MAX_OPS];
    int nop;
} Receita;

static RCampo sCampos[R_MAX_CAMPOS];
static int sNCampos = -1;
static Receita sRec[R_MAX_RECEITAS];
static int sNRec;

static unsigned long long fnv(const unsigned char *p, size_t n) {
    unsigned long long h = 0xcbf29ce484222325ULL;
    size_t i;
    for (i = 0; i < n; i++) {
        h ^= p[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

/* as palavras de uma linha, sem o comentário */
static int palavras(char *buf, char **pal, int max) {
    char *c = strchr(buf, '#'), *s = buf;
    int n = 0;
    if (c != NULL) *c = '\0';
    for (;;) {
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') *s++ = '\0';
        if (*s == '\0' || n == max) break;
        pal[n++] = s;
        while (*s != '\0' && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n') s++;
    }
    return n;
}

static int num(const char *p, long *v) {
    char *fim;
    if (p == NULL || *p == '\0') return 0;
    *v = strtol(p, &fim, 0);
    return *fim == '\0';
}

static int tamanho(int tipo) {
    return tipo == T_U8 ? 1 : tipo == T_U16 ? 2 : tipo == T_B64 ? 8 : 4;
}

/* port/x360/receitas/campos.txt: nome tipo deslocamento quantidade passo fonte... */
static int ler_campos(const char *caminho, char *erro, size_t tam) {
    FILE *fp = fopen(caminho, "r");
    char buf[400], *pal[16];
    int n, linha = 0;
    if (fp == NULL) {
        snprintf(erro, tam, "não abre %.200s", caminho);
        return -1;
    }
    sNCampos = 0;
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        RCampo *c;
        long d, q, p;
        linha++;
        n = palavras(buf, pal, 16);
        if (n == 0) continue;
        if (n < 6 || sNCampos == R_MAX_CAMPOS || strlen(pal[0]) >= sizeof(c->nome) || !num(pal[2], &d) ||
            !num(pal[3], &q) || !num(pal[4], &p)) {
            snprintf(erro, tam, "campos.txt:%d: nome tipo desl qtd passo fonte", linha);
            fclose(fp);
            return -1;
        }
        c = &sCampos[sNCampos++];
        strcpy(c->nome, pal[0]);
        c->tipo = !strcmp(pal[1], "u8") ? T_U8 : !strcmp(pal[1], "u16") ? T_U16 : !strcmp(pal[1], "s32") ? T_S32 :
                  !strcmp(pal[1], "b32") ? T_B32 : !strcmp(pal[1], "b64") ? T_B64 : -1;
        c->desl = d;
        c->qtd = (int)q;
        c->passo = (int)p;
        if (c->tipo < 0 || q < 1 || d < 0 || d % tamanho(c->tipo) != 0 ||
            ((c->tipo == T_B32 || c->tipo == T_B64) ? d + (q + tamanho(c->tipo) * 8 - 1) / (tamanho(c->tipo) * 8) *
                                                                tamanho(c->tipo) > SAVE_SIZE
                                                    : (p < tamanho(c->tipo) && q > 1) || d + (q - 1) * p + tamanho(c->tipo) > SAVE_SIZE)) {
            snprintf(erro, tam, "campos.txt:%d: %s: tipo, alinhamento ou faixa fora do save", linha, pal[0]);
            fclose(fp);
            return -1;
        }
    }
    fclose(fp);
    return 0;
}

static int achar_campo(const char *nome) {
    int k;
    for (k = 0; k < sNCampos; k++)
        if (!strcmp(sCampos[k].nome, nome)) return k;
    return -1;
}

/* "nome", "nome[3]", "nome[0..98]" */
static int alvo(char *texto, ROp *o) {
    char *a = strchr(texto, '[');
    long i0 = 0, i1 = 0;
    if (a != NULL) {
        char *b = strchr(a, ']'), *pontos;
        if (b == NULL || b[1] != '\0') return -1;
        *a = '\0';
        *b = '\0';
        pontos = strstr(a + 1, "..");
        if (pontos != NULL) {
            *pontos = '\0';
            if (!num(a + 1, &i0) || !num(pontos + 2, &i1)) return -1;
        } else {
            if (!num(a + 1, &i0)) return -1;
            i1 = i0;
        }
    }
    o->campo = achar_campo(texto);
    if (o->campo < 0) return -1;
    if (a == NULL && sCampos[o->campo].qtd != 1) return -1;
    if (i0 < 0 || i1 < i0 || i1 >= sCampos[o->campo].qtd) return -1;
    o->i0 = (int)i0;
    o->i1 = (int)i1;
    return 0;
}

static unsigned char *elemento(unsigned char *save, const RCampo *c, int i, unsigned long long *mascara) {
    if (c->tipo == T_B64) {
        *mascara = 1ULL << (i % 64);
        return save + c->desl + (i / 64) * 8;
    }
    if (c->tipo == T_B32) {
        *mascara = 1ULL << (i % 32);
        return save + c->desl + (i / 32) * 4;
    }
    *mascara = 0;
    return save + c->desl + (long)i * c->passo;
}

static long ler_valor(const unsigned char *save, const RCampo *c, int i) {
    unsigned long long m;
    const unsigned char *p = elemento((unsigned char *)save, c, i, &m);
    switch (c->tipo) {
    case T_U8: return *p;
    case T_U16: return *(const unsigned short *)p;
    case T_S32: return *(const int *)p;
    case T_B32: return (*(const unsigned *)p & (unsigned)m) != 0;
    default: return (*(const unsigned long long *)p & m) != 0;
    }
}

static void gravar_valor(unsigned char *save, const RCampo *c, int i, long v) {
    unsigned long long m;
    unsigned char *p = elemento(save, c, i, &m);
    switch (c->tipo) {
    case T_U8: *p = (unsigned char)v; break;
    case T_U16: *(unsigned short *)p = (unsigned short)v; break;
    case T_S32: *(int *)p = (int)v; break;
    case T_B32: if (v) *(unsigned *)p |= (unsigned)m; else *(unsigned *)p &= ~(unsigned)m; break;
    default: if (v) *(unsigned long long *)p |= m; else *(unsigned long long *)p &= ~m; break;
    }
}

static int cabe(const RCampo *c, long v) {
    switch (c->tipo) {
    case T_U8: return v >= 0 && v <= 0xFF;
    case T_U16: return v >= 0 && v <= 0xFFFF;
    case T_S32: return v >= -2147483647L - 1 && v <= 0xFFFFFFFFL;
    default: return v == 0 || v == 1;
    }
}

/* Lê e valida a receita; devolve o índice (o mesmo para o mesmo caminho) ou -1 com o erro. */
int Receita_Ler(const char *caminho, char *erro, size_t tam) {
    FILE *fp;
    char campos[400], buf[400], copia[400], *pal[16], *barra;
    unsigned char texto[32768];
    size_t ntexto;
    Receita *r;
    const char *base;
    int n, linha = 0, k, marco = 0, depois = 0;
    if (sNCampos < 0) {
        barra = strrchr(caminho, '/');
        snprintf(campos, sizeof(campos), "%.*scampos.txt", barra != NULL ? (int)(barra - caminho + 1) : 0, caminho);
        if (ler_campos(campos, erro, tam) < 0) return -1;
    }
    base = strrchr(caminho, '/') != NULL ? strrchr(caminho, '/') + 1 : caminho;
    for (k = 0; k < sNRec; k++)
        if (!strncmp(sRec[k].nome, base, strlen(sRec[k].nome)) && !strcmp(base + strlen(sRec[k].nome), ".txt")) return k;
    if (sNRec == R_MAX_RECEITAS) {
        snprintf(erro, tam, "receitas demais numa rodada");
        return -1;
    }
    fp = fopen(caminho, "rb");
    if (fp == NULL) {
        snprintf(erro, tam, "não abre %.200s", caminho);
        return -1;
    }
    ntexto = fread(texto, 1, sizeof(texto), fp);
    fclose(fp);
    if (ntexto == sizeof(texto)) {
        snprintf(erro, tam, "%.100s: receita grande demais", base);
        return -1;
    }
    r = &sRec[sNRec];
    memset(r, 0, sizeof(*r));
    r->fonte = fnv(texto, ntexto);
    if (strlen(base) < 5 || strlen(base) - 4 >= sizeof(r->nome) || strcmp(base + strlen(base) - 4, ".txt")) {
        snprintf(erro, tam, "%.100s: nome de receita inválido", base);
        return -1;
    }
    memcpy(r->nome, base, strlen(base) - 4);
    {
        /* linha a linha sobre a cópia em memória (o mesmo texto da assinatura) */
        size_t i = 0;
        while (i < ntexto) {
            size_t j = 0;
            ROp *o;
            const char *erro_op = NULL;
            long v;
            while (i < ntexto && texto[i] != '\n' && j < sizeof(buf) - 1) buf[j++] = (char)texto[i++];
            if (i < ntexto && texto[i] != '\n') {
                snprintf(erro, tam, "%.60s:%d: linha longa demais", base, linha + 1);
                return -1;
            }
            i++;
            buf[j] = '\0';
            linha++;
            strcpy(copia, buf);
            n = palavras(buf, pal, 16);
            if (n == 0) continue;
            if (!strcmp(pal[0], "marco")) {
                if (n != 2 || strcmp(pal[1], r->nome)) erro_op = "marco NOME (o nome do arquivo)";
                marco = 1;
            } else if (!strcmp(pal[0], "evento")) {
                if (n < 2) erro_op = "evento TEXTO";
            } else {
                if (r->nop == R_MAX_OPS) {
                    snprintf(erro, tam, "%.60s:%d: operações demais", base, linha);
                    return -1;
                }
                o = &r->op[r->nop];
                memset(o, 0, sizeof(*o));
                o->linha = linha;
                if (!strcmp(pal[0], "arquivo")) {
                    /* arquivo NOME TAMANHO: um arquivo do cartão depois do marco (os replays não moram no save); só o
                       verificador o confere, aqui é só a forma */
                    o->op = O_ARQUIVO;
                    if (n != 3 || !num(pal[2], &v) || v <= 0 || strchr(pal[1], '/') != NULL) erro_op = "arquivo NOME TAMANHO";
                    depois = 1;
                } else if (!strcmp(pal[0], "chama")) {
                    if (n != 3 || !num(pal[2], &v)) {
                        erro_op = "chama Save_AddItem N | Save_AddMoney N";
                    } else if (!strcmp(pal[1], "Save_AddItem")) {
                        o->op = O_ITEM;
                        o->campo = achar_campo("item");
                        if (o->campo < 0 || v < 0 || v >= sCampos[o->campo].qtd) erro_op = "Save_AddItem: item fora da tabela";
                        o->i0 = o->i1 = (int)v;
                    } else if (!strcmp(pal[1], "Save_AddMoney")) {
                        o->op = O_DINHEIRO;
                        o->campo = achar_campo("dinheiro");
                        if (o->campo < 0 || v <= 0) erro_op = "Save_AddMoney: valor positivo e campo dinheiro";
                    } else {
                        erro_op = "setter desconhecido";
                    }
                    o->valor = v;
                } else if (!strcmp(pal[0], "liga")) {
                    o->op = O_LIGA;
                    o->valor = 1;
                    if (n != 2 || alvo(pal[1], o) < 0) erro_op = "liga CAMPO[I..J]";
                    else if (sCampos[o->campo].tipo != T_B32 && sCampos[o->campo].tipo != T_B64) erro_op = "liga só em campo de bits";
                } else if (!strcmp(pal[0], "exige") || !strcmp(pal[0], "escreve") || !strcmp(pal[0], "ou") ||
                           !strcmp(pal[0], "depois")) {
                    o->op = !strcmp(pal[0], "exige") ? O_EXIGE : !strcmp(pal[0], "escreve") ? O_ESCREVE :
                            !strcmp(pal[0], "ou") ? O_OU : O_IGUAL;
                    if (n != 4 || alvo(pal[1], o) < 0 || !num(pal[3], &o->valor)) {
                        erro_op = "OP CAMPO[I..J] = V (depois: =, tem, sem, >=, <=), campo identificado e índice na faixa";
                    } else if (o->op == O_IGUAL && strcmp(pal[2], "=")) {
                        o->op = !strcmp(pal[2], "tem") ? O_TEM : !strcmp(pal[2], "sem") ? O_SEM :
                                !strcmp(pal[2], ">=") ? O_MAIOR : !strcmp(pal[2], "<=") ? O_MENOR : -1;
                        if (o->op < 0) erro_op = "depois CAMPO = | tem | sem | >= | <= V";
                    } else if (o->op != O_IGUAL && strcmp(pal[2], "=")) {
                        erro_op = "OP CAMPO = V";
                    }
                    if (erro_op == NULL && (o->op == O_OU || o->op == O_TEM || o->op == O_SEM) &&
                        (sCampos[o->campo].tipo == T_B32 || sCampos[o->campo].tipo == T_B64 ||
                         !cabe(&sCampos[o->campo], o->valor)))
                        erro_op = "máscara só em campo u8, u16 ou s32, e do tamanho dele";
                    if (erro_op == NULL && o->op != O_TEM && o->op != O_SEM && o->op != O_OU && o->op != O_MAIOR &&
                        o->op != O_MENOR &&
                        !cabe(&sCampos[o->campo], o->valor))
                        erro_op = "valor não cabe no tipo do campo";
                } else {
                    erro_op = "palavra desconhecida";
                }
                if (o->op >= O_IGUAL) depois = 1;
                r->nop++;
            }
            if (erro_op != NULL) {
                snprintf(erro, tam, "%.60s:%d: %s: %.120s", base, linha, erro_op, copia);
                return -1;
            }
        }
    }
    if (!marco || !depois) {
        snprintf(erro, tam, "%.60s: sem a linha marco ou sem nenhum depois", base);
        return -1;
    }
    return sNRec++;
}

/* o elemento (campo, i) tem um "depois" explícito? */
static int tem_depois(const Receita *r, int campo, int i) {
    int k;
    for (k = 0; k < r->nop; k++)
        if (r->op[k].op >= O_IGUAL && r->op[k].campo == campo && i >= r->op[k].i0 && i <= r->op[k].i1) return 1;
    return 0;
}

/* Confere o save: só o estado "antes" (o que a receita escreveu) ou o marco inteiro ("depois" e o que ela escreveu
   sem "depois" próprio). Devolve quantos elementos conferiu, ou -1 com o erro. */
static int conferir(const Receita *r, const unsigned char *save, int so_antes, char *erro, size_t tam) {
    int k, i, n = 0;
    for (k = 0; k < r->nop; k++) {
        const ROp *o = &r->op[k];
        const RCampo *c = &sCampos[o->campo];
        if (o->op == O_EXIGE || o->op == O_DINHEIRO || o->op == O_ARQUIVO) continue;
        if (o->op >= O_IGUAL && so_antes) continue;
        for (i = o->i0; i <= o->i1; i++) {
            long v = ler_valor(save, c, i);
            int bom;
            if (o->op < O_IGUAL && !so_antes && tem_depois(r, o->campo, i)) continue;
            switch (o->op) {
            case O_ESCREVE: case O_LIGA: case O_IGUAL: bom = v == o->valor || (c->tipo == T_S32 && (int)v == (int)o->valor); break;
            case O_OU: case O_TEM: bom = ((unsigned long)v & (unsigned long)o->valor & 0xFFFFFFFFUL) == ((unsigned long)o->valor & 0xFFFFFFFFUL); break;
            case O_ITEM: bom = (v & 1) != 0; break;
            case O_SEM: bom = ((unsigned long)v & (unsigned long)o->valor & 0xFFFFFFFFUL) == 0; break;
            case O_MENOR: bom = v <= o->valor; break;
            default: bom = v >= o->valor; break;
            }
            if (!bom) {
                snprintf(erro, tam, "%s:%d: %s[%d] = %ld (0x%lx), fora do esperado", r->nome, o->linha, c->nome, i, v,
                         (unsigned long)v & 0xFFFFFFFFUL);
                return -1;
            }
            n++;
        }
    }
    return n;
}

static const char *semente(void) {
    const char *s = getenv("BT3_SEED");
    return s != NULL ? s : "0x12345678";
}

/* Aplica a receita ao save em memória (o do New Game). 0, ou -1 com o erro (nada escrito se um "exige" falhar). */
int Receita_Aplica(int id, unsigned char *save, char *erro, size_t tam, void (*diz)(const char *)) {
    const Receita *r = &sRec[id];
    char s[400];
    unsigned long long antes = fnv(save, SAVE_SIZE);
    int k, i, escritas = 0, exigidos = 0;
    for (k = 0; k < r->nop; k++) {
        const ROp *o = &r->op[k];
        if (o->op != O_EXIGE) continue;
        for (i = o->i0; i <= o->i1; i++, exigidos++) {
            long v = ler_valor(save, &sCampos[o->campo], i);
            if (v != o->valor && !(sCampos[o->campo].tipo == T_S32 && (int)v == (int)o->valor)) {
                snprintf(erro, tam, "%s:%d: exige %s[%d] = %ld, o save tem %ld: não é o estado do New Game", r->nome,
                         o->linha, sCampos[o->campo].nome, i, o->valor, v);
                return -1;
            }
        }
    }
    for (k = 0; k < r->nop; k++) {
        const ROp *o = &r->op[k];
        const RCampo *c = &sCampos[o->campo];
        switch (o->op) {
        case O_ESCREVE: case O_LIGA:
            for (i = o->i0; i <= o->i1; i++, escritas++) gravar_valor(save, c, i, o->valor);
            break;
        case O_OU:
            for (i = o->i0; i <= o->i1; i++, escritas++) gravar_valor(save, c, i, ler_valor(save, c, i) | o->valor);
            break;
        case O_ITEM:
            Save_AddItem((int)o->valor);
            escritas++;
            break;
        case O_DINHEIRO:
            Save_AddMoney((int)o->valor);
            escritas++;
            break;
        default:
            break;
        }
    }
    k = conferir(r, save, 1, erro, tam);
    if (k < 0) return -1;
    snprintf(s, sizeof(s), "receita: %s aplicada fonte %016llx exigidos %d escritas %d antes %016llx depois %016llx "
             "semente %s", r->nome, r->fonte, exigidos, escritas, antes, fnv(save, SAVE_SIZE), semente());
    diz(s);
    return 0;
}

/* Confere o save em memória: so_antes 1, o estado que a receita põe; 0, o marco cumprido. */
int Receita_Confere(int id, const unsigned char *save, int so_antes, char *erro, size_t tam, void (*diz)(const char *)) {
    const Receita *r = &sRec[id];
    char s[300];
    int n = conferir(r, save, so_antes, erro, tam);
    if (n < 0) return -1;
    snprintf(s, sizeof(s), "receita: %s confere %s ok %d elementos fonte %016llx save %016llx", r->nome,
             so_antes ? "antes" : "depois", n, r->fonte, fnv(save, SAVE_SIZE));
    diz(s);
    return 0;
}
