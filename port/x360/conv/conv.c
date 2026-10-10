/* Base da conversão por formato: acesso com limite, troca com marcação e o registro dos formatos (conv.h). */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conv.h"

void Conv_Iniciar(ConvCtx *c, const char *nome, conv_u8 *buf, conv_ofs tam, conv_u8 *feito) {
    c->buf = buf;
    c->tam = tam;
    c->nome = nome;
    c->feito = feito;
    c->erro = 0;
    c->msg[0] = '\0';
}

void Conv_Erro(ConvCtx *c, const char *fmt, ...) {
    va_list ap;
    if (c->erro) {
        return; /* fica a primeira mensagem: é ela que explica */
    }
    c->erro = 1;
    va_start(ap, fmt);
    vsprintf(c->msg, fmt, ap); /* C89: sem vsnprintf no cl -TC; as mensagens são curtas */
    va_end(ap);
}

int Conv_Cabe(ConvCtx *c, conv_ofs ofs, conv_ofs n, const char *oque) {
    if (c->erro) {
        return 0;
    }
    if (ofs > c->tam || n > c->tam - ofs) {
        Conv_Erro(c, "%s: 0x%lX+0x%lX passa do fim (0x%lX)", oque, (unsigned long)ofs, (unsigned long)n,
                  (unsigned long)c->tam);
        return 0;
    }
    return 1;
}

conv_u32 Conv_Ler32(ConvCtx *c, conv_ofs ofs) {
    const conv_u8 *p;
    if (!Conv_Cabe(c, ofs, 4, "leitura u32")) {
        return 0;
    }
    p = c->buf + ofs;
    return (conv_u32)p[0] | (conv_u32)p[1] << 8 | (conv_u32)p[2] << 16 | (conv_u32)p[3] << 24;
}

conv_u16 Conv_Ler16(ConvCtx *c, conv_ofs ofs) {
    const conv_u8 *p;
    if (!Conv_Cabe(c, ofs, 2, "leitura u16")) {
        return 0;
    }
    p = c->buf + ofs;
    return (conv_u16)(p[0] | p[1] << 8);
}

/* marca [ofs, ofs+w) como um número de largura w (ou bytes, w = CONV_BYTE com n bytes); 0 se já estava marcado */
static int marcar(ConvCtx *c, conv_ofs ofs, conv_ofs n, conv_u8 w) {
    conv_ofs i;
    for (i = 0; i < n; i++) {
        if (c->feito[ofs + i] != CONV_LIVRE) {
            Conv_Erro(c, "0x%lX convertido duas vezes (marca %u, agora %u)", (unsigned long)(ofs + i),
                      (unsigned)c->feito[ofs + i], (unsigned)w);
            return 0;
        }
    }
    if (w == CONV_BYTE) {
        memset(c->feito + ofs, CONV_BYTE, n);
    } else {
        c->feito[ofs] = w;
        memset(c->feito + ofs + 1, CONV_CONT, n - 1);
    }
    return 1;
}

static void trocar(conv_u8 *p, int w) {
    int i;
    for (i = 0; i < w / 2; i++) {
        conv_u8 t = p[i];
        p[i] = p[w - 1 - i];
        p[w - 1 - i] = t;
    }
}

conv_u16 Conv_U16(ConvCtx *c, conv_ofs ofs) {
    conv_u16 v = Conv_Ler16(c, ofs);
    if (!c->erro && marcar(c, ofs, 2, 2)) {
        trocar(c->buf + ofs, 2);
    }
    return v;
}

conv_u32 Conv_U32(ConvCtx *c, conv_ofs ofs) {
    conv_u32 v = Conv_Ler32(c, ofs);
    if (!c->erro && marcar(c, ofs, 4, 4)) {
        trocar(c->buf + ofs, 4);
    }
    return v;
}

void Conv_U64(ConvCtx *c, conv_ofs ofs) {
    if (Conv_Cabe(c, ofs, 8, "u64") && marcar(c, ofs, 8, 8)) {
        trocar(c->buf + ofs, 8);
    }
}

void Conv_U16s(ConvCtx *c, conv_ofs ofs, conv_ofs n) {
    conv_ofs i;
    if (!Conv_Cabe(c, ofs, n * 2, "trecho u16")) {
        return;
    }
    for (i = 0; i < n && !c->erro; i++) {
        Conv_U16(c, ofs + i * 2);
    }
}

void Conv_U32s(ConvCtx *c, conv_ofs ofs, conv_ofs n) {
    conv_ofs i;
    if (!Conv_Cabe(c, ofs, n * 4, "trecho u32")) {
        return;
    }
    for (i = 0; i < n && !c->erro; i++) {
        Conv_U32(c, ofs + i * 4);
    }
}

void Conv_Bytes(ConvCtx *c, conv_ofs ofs, conv_ofs n) {
    if (Conv_Cabe(c, ofs, n, "bytes")) {
        marcar(c, ofs, n, CONV_BYTE);
    }
}

conv_u8 Conv_Marca(ConvCtx *c, conv_ofs ofs) {
    return ofs < c->tam ? c->feito[ofs] : CONV_LIVRE;
}

void Conv_Zeros(ConvCtx *c, conv_ofs ini, conv_ofs fim, const char *oque) {
    conv_ofs i;
    if (!Conv_Cabe(c, ini, fim - ini, oque)) {
        return;
    }
    for (i = ini; i < fim; i++) {
        if (c->feito[i] == CONV_LIVRE) {
            if (c->buf[i] != 0) {
                Conv_Erro(c, "%s: byte 0x%lX não reconhecido (não é enchimento)", oque, (unsigned long)i);
                return;
            }
            c->feito[i] = CONV_BYTE;
        }
    }
}

/* ------------------------------------------------------------------ registro */

/* os formatos (um arquivo .c cada) */
void Conv_Personagem(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_PersonagemAnim(ConvCtx *c, conv_ofs base, conv_ofs tam);
void Conv_BobjAnim(ConvCtx *c, conv_ofs base, conv_ofs tam);

typedef struct Formato { const char *nome; ConvFn fn; } Formato;
static const Formato sFormatos[] = {
    {"personagem", Conv_Personagem},           /* file[0] do slot: modelo, texturas, parâmetros, lábio */
    {"personagem_anim", Conv_PersonagemAnim},  /* file[1]: animações em BPE (só a tabela e os cabeçalhos) */
    {"animacao", Conv_BobjAnim},               /* uma animação descomprimida (Conv_Animacao, em tempo de jogo) */
};

/* Que formato tem cada arquivo do disco. Os arquivos não dizem o próprio formato: a lista vem das tabelas do
   jogo que escolhem o arquivo de cada coisa (ver docs/x360/formatos/). */
typedef struct Arquivo { const char *nome; const char *formato; } Arquivo;
static const Arquivo sArquivos[] = {
    {"pzs3us1/01473.bin", "personagem"},
    {"pzs3us1/01481.bin", "personagem_anim"},
    {"pzs3us1/01793.bin", "personagem"},
    {"pzs3us1/01801.bin", "personagem_anim"},
};

static int converter(ConvFn fn, const char *nome, conv_u8 *buf, conv_ofs tam, conv_u8 *feito, char *msg, int nmsg) {
    ConvCtx c;
    Conv_Iniciar(&c, nome, buf, tam, feito);
    fn(&c, 0, tam);
    if (c.erro) {
        if (msg != NULL && nmsg > 0) {
            strncpy(msg, c.msg, (size_t)nmsg - 1);
            msg[nmsg - 1] = '\0';
        }
        return -1;
    }
    return 1;
}

int Conv_Formato(const char *formato, const char *nome, conv_u8 *buf, conv_ofs tam, conv_u8 *feito, char *msg,
                 int nmsg) {
    unsigned i;
    for (i = 0; i < sizeof(sFormatos) / sizeof(sFormatos[0]); i++) {
        if (strcmp(sFormatos[i].nome, formato) == 0) {
            return converter(sFormatos[i].fn, nome, buf, tam, feito, msg, nmsg);
        }
    }
    return 0;
}

int Conv_Arquivo(const char *nome, conv_u8 *buf, conv_ofs tam, conv_u8 *feito, char *msg, int nmsg) {
    unsigned i;
    for (i = 0; i < sizeof(sArquivos) / sizeof(sArquivos[0]); i++) {
        if (strcmp(sArquivos[i].nome, nome) == 0) {
            return Conv_Formato(sArquivos[i].formato, nome, buf, tam, feito, msg, nmsg);
        }
    }
    return 0;
}

const char *Conv_Registrado(int i) {
    return i >= 0 && (unsigned)i < sizeof(sArquivos) / sizeof(sArquivos[0]) ? sArquivos[i].nome : NULL;
}

int Conv_Animacao(conv_u8 *buf, conv_ofs tam, char *msg, int nmsg) {
    conv_u8 *feito = (conv_u8 *)calloc(tam ? tam : 1, 1);
    int r;
    if (feito == NULL) {
        if (msg != NULL && nmsg > 0) {
            strncpy(msg, "sem memória para a marcação", (size_t)nmsg - 1);
            msg[nmsg - 1] = '\0';
        }
        return -1;
    }
    r = Conv_Formato("animacao", "animação", buf, tam, feito, msg, nmsg);
    free(feito);
    return r;
}
