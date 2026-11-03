/* Contêiner .psu do PS2. Ver psu.h e docs/x360/formatos/psu.md. */
#include <string.h>
#include "psu.h"

#define BT3_PASTA "BASLUS-21678DBZT3"
#define MODO_EXISTE 0x8000U
#define MODO_ARQUIVO 0x0010U
#define MODO_DIR 0x0020U

static unsigned lu16(const unsigned char *p) {
    return (unsigned)p[0] | (unsigned)p[1] << 8;
}
static unsigned long lu32(const unsigned char *p) {
    return (unsigned long)p[0] | (unsigned long)p[1] << 8 | (unsigned long)p[2] << 16 | (unsigned long)p[3] << 24;
}
static void eu16(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)(v & 255U);
    p[1] = (unsigned char)((v >> 8) & 255U);
}
static void eu32(unsigned char *p, unsigned long v) {
    p[0] = (unsigned char)(v & 255UL);
    p[1] = (unsigned char)((v >> 8) & 255UL);
    p[2] = (unsigned char)((v >> 16) & 255UL);
    p[3] = (unsigned char)((v >> 24) & 255UL);
}

static int eh_pasta(unsigned modo) {
    return (modo & (MODO_ARQUIVO | MODO_DIR | MODO_EXISTE)) == (MODO_DIR | MODO_EXISTE);
}
static int eh_arquivo(unsigned modo) {
    return (modo & (MODO_ARQUIVO | MODO_DIR | MODO_EXISTE)) == (MODO_ARQUIVO | MODO_EXISTE);
}

static void le_data(const unsigned char *p, PsuData *d) {
    d->seg = p[1];
    d->min = p[2];
    d->hora = p[3];
    d->dia = p[4];
    d->mes = p[5];
    d->ano = (unsigned short)lu16(p + 6);
}
static void es_data(unsigned char *p, const PsuData *d) {
    p[0] = 0;
    p[1] = d->seg;
    p[2] = d->min;
    p[3] = d->hora;
    p[4] = d->dia;
    p[5] = d->mes;
    eu16(p + 6, d->ano);
}

/* nome de 32 bytes: precisa de um zero dentro do campo, não vazio, sem barra */
static int le_nome(const unsigned char *p, char *dest) {
    unsigned i;
    for (i = 0; i < PSU_NOME; i++)
        if (p[i] == 0) break;
    if (i == 0 || i == PSU_NOME) return PSU_E_NOME;
    memcpy(dest, p, i);
    dest[i] = 0;
    for (i = 0; dest[i]; i++)
        if (dest[i] == '/' || dest[i] == '\\') return PSU_E_NOME;
    return PSU_OK;
}

static unsigned long arredonda(unsigned long n) {
    return (n + (PSU_ALINHA - 1UL)) / PSU_ALINHA * PSU_ALINHA;
}

int Psu_Ler(const unsigned char *buf, unsigned long tam, PsuSave *out, PsuArquivo *vet, unsigned max) {
    unsigned long n, i, pos, fl, nfiles;
    const unsigned char *e;
    PsuArquivo *a;
    int r;
    if (!buf || !out || (max && !vet)) return PSU_E_ARGS;
    memset(out, 0, sizeof(*out));
    if (tam < 3 * PSU_ENTRADA) return PSU_E_NAO_PSU;
    e = buf;
    if (!eh_pasta(lu16(e))) return PSU_E_NAO_PSU;
    n = lu32(e + 4);
    if (n < 2) return PSU_E_NAO_PSU;
    if (!eh_pasta(lu16(buf + PSU_ENTRADA)) || !eh_pasta(lu16(buf + 2 * PSU_ENTRADA))) return PSU_E_NAO_PSU;
    if (strncmp((const char *)buf + PSU_ENTRADA + 0x40, ".", 2) != 0 ||
        strncmp((const char *)buf + 2 * PSU_ENTRADA + 0x40, "..", 3) != 0)
        return PSU_E_NAO_PSU;
    r = le_nome(e + 0x40, out->pasta);
    if (r) return r;
    out->modo = lu16(e);
    le_data(e + 8, &out->criada);
    le_data(e + 0x18, &out->modificada);
    nfiles = n - 2;
    if (nfiles > max) return PSU_E_MUITOS;
    out->nArquivos = (unsigned)nfiles;
    out->arquivos = vet;
    pos = 3 * PSU_ENTRADA;
    for (i = 0; i < nfiles; i++) {
        if (tam - pos < PSU_ENTRADA) return PSU_E_TRUNCADO;
        e = buf + pos;
        a = &vet[i];
        memset(a, 0, sizeof(*a));
        a->modo = lu16(e);
        if (!eh_arquivo(a->modo)) return PSU_E_ENTRADA;
        r = le_nome(e + 0x40, a->nome);
        if (r) return r;
        fl = lu32(e + 4);
        le_data(e + 8, &a->criado);
        le_data(e + 0x18, &a->modificado);
        pos += PSU_ENTRADA;
        if (fl > tam - pos) return PSU_E_TRUNCADO;
        if (arredonda(fl) > tam - pos) return PSU_E_TRUNCADO; /* falta o completamento */
        a->dados = buf + pos;
        a->tamanho = fl;
        pos += arredonda(fl);
    }
    if (pos != tam) return PSU_E_SOBRA;
    return PSU_OK;
}

int Psu_ConferirBT3(const PsuSave *s) {
    static const char *const nomes[3] = {"icon.sys", "dbzsm.ico", BT3_PASTA};
    static const unsigned long tams[3] = {964UL, 40090UL, 16384UL};
    unsigned i, j;
    int achou;
    if (!s) return PSU_E_ARGS;
    if (strcmp(s->pasta, BT3_PASTA) != 0) return PSU_E_BT3_PASTA;
    for (i = 0; i < 3; i++) {
        achou = 0;
        for (j = 0; j < s->nArquivos; j++) {
            if (strcmp(s->arquivos[j].nome, nomes[i]) != 0) continue;
            if (s->arquivos[j].tamanho != tams[i]) return PSU_E_BT3_TAMANHO;
            achou = 1;
        }
        if (!achou) return PSU_E_BT3_FALTA;
    }
    if (s->nArquivos != 3) return PSU_E_BT3_EXTRA;
    return PSU_OK;
}

const char *Psu_TextoErro(int c) {
    switch (c) {
    case PSU_OK: return "ok";
    case PSU_E_ARGS: return "bad arguments";
    case PSU_E_NAO_PSU: return "not a PS2 save (.psu)";
    case PSU_E_TRUNCADO: return "truncated file";
    case PSU_E_ENTRADA: return "bad directory entry";
    case PSU_E_NOME: return "bad name";
    case PSU_E_SOBRA: return "trailing data";
    case PSU_E_MUITOS: return "too many files";
    case PSU_E_ESPACO: return "output buffer too small";
    case PSU_E_BT3_PASTA: return "not a Budokai Tenkaichi 3 USA save";
    case PSU_E_BT3_FALTA: return "save is missing a file";
    case PSU_E_BT3_TAMANHO: return "save file has the wrong size";
    case PSU_E_BT3_EXTRA: return "save has unexpected files";
    }
    return "unknown error";
}

static int nome_ok(const char *nome) {
    unsigned i;
    if (!nome || !nome[0]) return 0;
    for (i = 0; nome[i]; i++) {
        if (i >= PSU_NOME - 1U || nome[i] == '/' || nome[i] == '\\') return 0;
    }
    return 1;
}

static int valida_escrita(const PsuSave *s) {
    unsigned i;
    if (!s || !nome_ok(s->pasta) || (s->nArquivos && !s->arquivos)) return PSU_E_ARGS;
    if (s->modo && !eh_pasta(s->modo)) return PSU_E_ARGS;
    for (i = 0; i < s->nArquivos; i++) {
        const PsuArquivo *a = &s->arquivos[i];
        if (!nome_ok(a->nome)) return PSU_E_NOME;
        if (!eh_arquivo(a->modo)) return PSU_E_ARGS;
        if (a->tamanho && !a->dados) return PSU_E_ARGS;
    }
    return PSU_OK;
}

unsigned long Psu_TamanhoEscrita(const PsuSave *s) {
    unsigned long t = 3 * PSU_ENTRADA;
    unsigned i;
    if (valida_escrita(s) != PSU_OK) return 0;
    for (i = 0; i < s->nArquivos; i++) t += PSU_ENTRADA + arredonda(s->arquivos[i].tamanho);
    return t;
}

static void entrada(unsigned char *p, unsigned modo, unsigned long len, const PsuData *c, const PsuData *m,
                    const char *nome) {
    memset(p, 0, PSU_ENTRADA);
    eu16(p, modo);
    eu32(p + 4, len);
    es_data(p + 8, c);
    es_data(p + 0x18, m);
    memcpy(p + 0x40, nome, strlen(nome));
}

int Psu_Escrever(const PsuSave *s, unsigned char *buf, unsigned long cap, unsigned long *escrito) {
    unsigned long t, pos, a;
    unsigned i;
    int r = valida_escrita(s);
    if (r) return r;
    if (!buf) return PSU_E_ARGS;
    t = Psu_TamanhoEscrita(s);
    if (cap < t) return PSU_E_ESPACO;
    entrada(buf, s->modo ? s->modo : PSU_MODO_PASTA, (unsigned long)s->nArquivos + 2UL, &s->criada, &s->modificada,
            s->pasta);
    entrada(buf + PSU_ENTRADA, PSU_MODO_PASTA, 0, &s->criada, &s->criada, ".");
    entrada(buf + 2 * PSU_ENTRADA, PSU_MODO_PASTA, 0, &s->criada, &s->criada, "..");
    pos = 3 * PSU_ENTRADA;
    for (i = 0; i < s->nArquivos; i++) {
        const PsuArquivo *f = &s->arquivos[i];
        entrada(buf + pos, f->modo, f->tamanho, &f->criado, &f->modificado, f->nome);
        pos += PSU_ENTRADA;
        a = arredonda(f->tamanho);
        memset(buf + pos, 0, a);
        if (f->tamanho) memcpy(buf + pos, f->dados, f->tamanho);
        pos += a;
    }
    if (escrito) *escrito = pos;
    return PSU_OK;
}
