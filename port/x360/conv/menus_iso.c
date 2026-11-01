/* ISO v4: converte o conteúdo dos menus antes da carga, mantendo cada entrada AFS.
   BPE usa blocos independentes, tabela de pares e tamanho do fluxo em big-endian.
   O cabeçalho do contêiner e a tabela externa já foram convertidos pelo formato. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "conv.h"

static conv_u32 be32(const conv_u8 *p) {
    return (conv_u32)p[0] << 24 | (conv_u32)p[1] << 16 | (conv_u32)p[2] << 8 | p[3];
}
static void gravar32(conv_u8 *p, conv_u32 v) {
    p[0] = (conv_u8)(v >> 24); p[1] = (conv_u8)(v >> 16);
    p[2] = (conv_u8)(v >> 8); p[3] = (conv_u8)v;
}

/* Limites de saída, fluxo, pilha e expansão cíclica conferidos mesmo em entrada ruim. */
int Conv_BpeAbrir(const conv_u8 *p, conv_ofs n, conv_u8 *out, conv_ofs tam) {
    conv_ofs in = 0, o = 0, passos = 0;
    conv_u8 esq[256], dir[256], pilha[256];
    while (in < n) {
        unsigned c = 0, cont, i, sp = 0, bloco;
        for (i = 0; i < 256; i++) esq[i] = (conv_u8)i;
        for (;;) {
            if (in == n) return 0;
            cont = p[in++];
            if (cont > 127) { c += cont - 127; cont = 0; }
            if (c > 256) return 0;
            if (c == 256) break;
            for (i = 0; i <= cont; i++, c++) {
                if (c >= 256 || in == n) return 0;
                esq[c] = p[in++];
                if (esq[c] != c) {
                    if (in == n) return 0;
                    dir[c] = p[in++];
                }
            }
            if (c == 256) break;
        }
        if (n - in < 2) return 0;
        bloco = (unsigned)p[in] << 8 | p[in + 1]; in += 2;
        if (bloco > 32767 || bloco > n - in) return 0;
        while (bloco || sp) {
            unsigned ch;
            if (++passos > tam * 2UL + n) return 0;
            if (sp) ch = pilha[--sp];
            else { ch = p[in++]; bloco--; }
            if (esq[ch] == ch) {
                if (o == tam) return 0;
                out[o++] = (conv_u8)ch;
            } else {
                if (sp > 254) return 0;
                pilha[sp++] = dir[ch]; pilha[sp++] = esq[ch];
            }
        }
    }
    return o == tam;
}

/* Substituição gulosa de pares por códigos ausentes do bloco. Empates escolhem o
   primeiro par do fluxo: resultado determinístico em x86, PPC e XDK C89. */
static conv_ofs comprimir(const conv_u8 *src, conv_ofs tam, conv_u8 *dst,
                         conv_ofs cap, unsigned limite, unsigned distintos) {
    conv_u8 bloco[32767], esq[256], dir[256], usado[256];
    unsigned *freq = (unsigned *)malloc(65536UL * sizeof(unsigned));
    conv_ofs in = 0, out = 0;
    if (!freq) return 0;
#define EMITIR(v) do { if (out == cap) goto cheio; dst[out++] = (conv_u8)(v); } while (0)
    while (in < tam) {
        unsigned n = 0, unicos = 0, i, j, codigo, par, melhor, maximo;
        memset(usado, 0, sizeof(usado));
        while (in < tam && n < limite) {
            unsigned ch = src[in];
            if (!usado[ch]) {
                if (unicos == distintos) break;
                usado[ch] = 1; unicos++;
            }
            bloco[n++] = src[in++];
        }
        for (i = 0; i < 256; i++) esq[i] = (conv_u8)i;
        for (;;) {
            for (codigo = 256; codigo && usado[codigo - 1]; codigo--) {}
            if (!codigo) break;
            codigo--;
            memset(freq, 0, 65536UL * sizeof(unsigned));
            maximo = 2; melhor = 0;
            for (i = 1; i < n; i++) {
                par = (unsigned)bloco[i - 1] * 256 + bloco[i];
                freq[par]++;
                if (freq[par] > maximo) { melhor = par; maximo = freq[par]; }
            }
            if (maximo <= 2) break;
            usado[codigo] = 1;
            esq[codigo] = (conv_u8)(melhor >> 8); dir[codigo] = (conv_u8)melhor;
            for (i = 0, j = 0; i < n; i++) {
                if (i + 1 < n && bloco[i] == esq[codigo] && bloco[i + 1] == dir[codigo]) {
                    bloco[j++] = (conv_u8)codigo; i++;
                } else bloco[j++] = bloco[i];
            }
            n = j;
        }
        /* Tabela: salta até 128 literais; após salto não final vem uma entrada.
           Grupos de pares têm até 128 entradas. */
        for (i = 0; i < 256;) {
            unsigned inicio = i, cont;
            if (esq[i] == i) {
                while (i < 256 && esq[i] == i && i - inicio < 128) i++;
                EMITIR(127 + i - inicio);
                if (i == 256) break;
                EMITIR(esq[i]); if (esq[i] != i) EMITIR(dir[i]); i++;
            } else {
                while (i < 256 && esq[i] != i && i - inicio < 128) i++;
                cont = i - inicio;
                EMITIR(cont - 1);
                for (j = inicio; j < i; j++) { EMITIR(esq[j]); EMITIR(dir[j]); }
            }
        }
        EMITIR(n >> 8); EMITIR(n);
        for (i = 0; i < n; i++) EMITIR(bloco[i]);
    }
    free(freq);
    return out;
cheio:
    free(freq);
    return 0;
#undef EMITIR
}

/* Conserva blocos cujo conteúdo é só bytes. Nos demais, tenta a gramática
   original por programação dinâmica e uma nova gramática gulosa. Assim a arte
   não perde compressão por causa de poucos números do cabeçalho. */
static unsigned expandir(unsigned ch, const conv_u8 *esq, const conv_u8 *dir,
                         conv_u8 *out, unsigned cap, unsigned nivel) {
    unsigned a, b;
    if (!cap || nivel > 256) return 0;
    if (esq[ch] == ch) { *out = (conv_u8)ch; return 1; }
    a = expandir(esq[ch], esq, dir, out, cap, nivel + 1);
    if (!a || a == cap) return 0;
    b = expandir(dir[ch], esq, dir, out + a, cap - a, nivel + 1);
    return b ? a + b : 0;
}

static conv_ofs preservar(const conv_u8 *src, conv_ofs pack, const conv_u8 *antes,
                           const conv_u8 *raw, conv_ofs tam, conv_u8 *dst, conv_ofs cap) {
    conv_ofs in = 0, out = 0, o = 0;
    conv_u8 *dicionario = (conv_u8 *)malloc(256UL * 32768UL);
    conv_u8 *tentativa = (conv_u8 *)malloc(100000UL);
    conv_u8 *melhor = (conv_u8 *)malloc(100000UL);
    unsigned *custo = (unsigned *)malloc(32769UL * sizeof(unsigned));
    conv_u8 *escolha = (conv_u8 *)malloc(32768UL);
    if (!dicionario || !tentativa || !melhor || !custo || !escolha) goto erro;
    while (in < pack) {
        conv_u8 esq[256], dir[256];
        unsigned lens[256], c = 0, cont, k, j, nb, bruto = 0, tokens;
        conv_ofs inicio = in, tabela, fim, menor, novo;
        for (k = 0; k < 256; k++) esq[k] = (conv_u8)k;
        do {
            cont = src[in++];
            if (cont > 127) { c += cont - 127; cont = 0; }
            if (c == 256) break;
            for (k = 0; k <= cont; k++, c++) {
                esq[c] = src[in++];
                if (esq[c] != c) dir[c] = src[in++];
            }
        } while (c < 256);
        tabela = in - inicio;
        nb = (unsigned)src[in] << 8 | src[in + 1]; in += 2;
        fim = in + nb;
        for (k = 0; k < 256; k++)
            lens[k] = expandir(k, esq, dir, dicionario + k * 32768UL, 32768, 0);
        for (k = 0; k < nb; k++) {
            unsigned l = lens[src[in + k]];
            if (!l || l > 32768 - bruto) goto erro;
            bruto += l;
        }
        if (!bruto || bruto > tam - o) goto erro;
        if (!memcmp(antes + o, raw + o, bruto)) {
            menor = fim - inicio;
            if (menor > cap - out) goto erro;
            memcpy(dst + out, src + inicio, menor);
        } else {
            menor = 0;
            /* Custo mínimo em tokens com a tabela original, inclusive os
               literais que a tabela ainda permite representar diretamente. */
            custo[bruto] = 0;
            for (j = bruto; j-- > 0;) {
                custo[j] = 1000000;
                for (k = 0; k < 256; k++) {
                    unsigned l = lens[k];
                    const conv_u8 *palavra = dicionario + k * 32768UL;
                    if (l && l <= bruto - j && palavra[0] == raw[o + j] &&
                        custo[j + l] + 1 < custo[j] && !memcmp(palavra, raw + o + j, l)) {
                        custo[j] = custo[j + l] + 1; escolha[j] = (conv_u8)k;
                    }
                }
            }
            tokens = custo[0];
            if (tokens <= 32767) {
                memcpy(melhor, src + inicio, tabela);
                menor = tabela;
                melhor[menor++] = (conv_u8)(tokens >> 8); melhor[menor++] = (conv_u8)tokens;
                for (j = 0; j < bruto; j += lens[escolha[j]]) melhor[menor++] = escolha[j];
            }
            for (k = 0; k < 3; k++) {
                novo = comprimir(raw + o, bruto, tentativa, 100000, 32760, 128 + k * 40);
                if (novo && (!menor || novo < menor)) {
                    menor = novo; memcpy(melhor, tentativa, novo);
                }
            }
            if (!menor || menor > cap - out) goto erro;
            memcpy(dst + out, melhor, menor);
        }
        out += menor; in = fim; o += bruto;
    }
    if (o != tam) goto erro;
    free(dicionario); free(tentativa); free(melhor); free(custo); free(escolha);
    return out;
erro:
    free(dicionario); free(tentativa); free(melhor); free(custo); free(escolha);
    return 0;
}

static int conteudo(const char *formato, conv_u8 *p, conv_ofs cap, char *msg, int nmsg) {
    conv_ofs tam, pack, novo = 0;
    conv_u8 *raw = NULL, *bits = NULL, *fluxo = NULL, *prova = NULL;
    int r = -1;
    static const unsigned limites[] = {10000, 8192, 16384, 4096, 32760};
    unsigned i;
    if (nmsg) msg[0] = 0;
    if (cap < 8) goto fim;
    tam = be32(p); pack = be32(p + 4);
    if (!tam || tam > CONV_JOGO_MAX || pack > cap - 8) goto fim;
    raw = (conv_u8 *)malloc(tam); bits = (conv_u8 *)calloc((tam + 7) / 8, 1);
    fluxo = (conv_u8 *)malloc(cap - 8); prova = (conv_u8 *)malloc(tam);
    if (!raw || !bits || !fluxo || !prova) goto fim;
    if (!Conv_BpeAbrir(p + 8, pack, raw, tam)) goto fim;
    memcpy(prova, raw, tam);
    if (Conv_FormatoBits(formato, formato, raw, tam, bits, msg, nmsg) != 1) goto fim;
    for (i = 0; i < tam; i++) if (!(bits[i >> 3] & (1u << (i & 7)))) goto fim;
    novo = preservar(p + 8, pack, prova, raw, tam, fluxo, cap - 8);
    for (i = 0; i < sizeof(limites) / sizeof(limites[0]) && !novo; i++)
        novo = comprimir(raw, tam, fluxo, cap - 8, limites[i], 200);
    if (!novo) {
        if (nmsg) { strncpy(msg, "menu recomprimido não cabe na entrada original", nmsg - 1); msg[nmsg - 1] = 0; }
        goto fim;
    }
    if (!Conv_BpeAbrir(fluxo, novo, prova, tam) || memcmp(raw, prova, tam)) goto fim;
    gravar32(p + 4, novo);
    memcpy(p + 8, fluxo, novo);
    r = 1;
fim:
    if (r < 0 && nmsg && !msg[0]) {
        strncpy(msg, "menu ISO: BPE, cobertura ou memória inválidos", nmsg - 1); msg[nmsg - 1] = 0;
    }
    free(raw); free(bits); free(fluxo); free(prova);
    return r;
}

int Conv_MenusISO(const char *nome, conv_u8 *buf, conv_ofs tam, char *msg, int nmsg) {
    const char *arquivo = Conv_FormatoDe(nome), *f;
    conv_u32 n, i;
    if (!arquivo) return 0;
    if (!strcmp(arquivo, "menu_bpe")) {
        f = Conv_FormatoSecao(nome, 0);
        return f ? conteudo(f, buf, tam, msg, nmsg) : 0;
    }
    if (strcmp(arquivo, "menu_pacote")) return 0;
    if (tam < 8) return -1;
    n = be32(buf);
    if (n > 64 || (n + 2) * 4 > tam) return -1;
    for (i = 1; i <= n; i++) {
        conv_ofs a = be32(buf + i * 4), b = be32(buf + (i + 1) * 4);
        if (a > b || b > tam) return -1;
        if (a == b) continue;
        f = Conv_FormatoSecao(nome, (int)i);
        if (!f) {
            if (nmsg) { strncpy(msg, "menu ISO: seção sem formato", nmsg - 1); msg[nmsg - 1] = 0; }
            return -1;
        }
        if (conteudo(f, buf + a, b - a, msg, nmsg) < 0) return -1;
    }
    return 1;
}
