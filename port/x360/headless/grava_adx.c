/*
 * F6, etapa 2 (redes 3 e 4): o gs/snd_adx.c do port inteiro, sem mudar uma linha, mais o que o teste das amostras
 * precisa dele. Na build headless.py --gravador este objeto entra no lugar do snd_adx.c; no teste_som (rede 3), também.
 *   Som_DecodificaAdx: um arquivo ADX pelo start() e pelo decode() do port, como um player o toca, para a referência
 *     independente em Python (port/x360/tools/som_ref.py) comparar.
 */
#include "../../src/gs/snd_adx.c"

/* Toca `path` num player do port e grava a saída estéreo (s16 little-endian, esquerdo e direito) em `saida`: o arquivo
   inteiro; com laço, até a primeira volta e mais `depois` amostras a partir dela. info[]: canais, taxa, amostras,
   laço, início e fim do laço, c1, c2, amostras antes da volta, amostras depois. `estraga_coef`: soma 1 ao c1 (o
   arredondamento do ffmpeg num coeficiente; controle negativo). Devolve 0, ou -1 se o port recusou o arquivo. */
int Som_DecodificaAdx(const char *path, long depois, FILE *saida, int estraga_coef, long info[10]) {
    Player *p = &sPlayers[0];
    int16_t buf[32 * 2];
    long antes = 0, apos = 0;
    int voltou = 0;

    stop(p);
    memset(p, 0, sizeof(*p));
    p->pan[0] = p->pan[1] = PAN_AUTO;
    start(p, path);
    if (p->file == NULL || p->stream == NULL) {
        return -1;
    }
    if (estraga_coef) {
        p->c1 += 1;
    }
    for (;;) {
        uint32_t era = p->pos;
        int n = decode(p, buf, 1), k;
        if (n == 0) {
            if (p->loop && p->pos < era && !voltou) {
                voltou = 1; /* a volta do laço: a próxima chamada decodifica o começo do laço */
                continue;
            }
            break;
        }
        if (voltou) {
            if (apos >= depois) {
                break;
            }
            if (apos + n > depois) {
                n = (int)(depois - apos);
            }
        }
        for (k = 0; k < n * 2; k++) {
            uint8_t b[2];
            b[0] = (uint8_t)buf[k];
            b[1] = (uint8_t)((uint16_t)buf[k] >> 8);
            fwrite(b, 1, 2, saida);
        }
        if (voltou) {
            apos += n;
        } else {
            antes += n;
        }
    }
    info[0] = (long)p->channels;
    info[1] = (long)p->rate;
    info[2] = (long)p->total;
    info[3] = p->loop;
    info[4] = (long)p->loopStart;
    info[5] = (long)p->loopEnd;
    info[6] = p->c1;
    info[7] = p->c2;
    info[8] = antes;
    info[9] = apos;
    stop(p);
    return 0;
}
