/*
 * F6, etapa 2 (redes 3 e 4): o gs/snd_adx.c do port inteiro, sem mudar uma linha, mais o que o teste das amostras
 * precisa dele. Na build headless.py --gravador este objeto entra no lugar do snd_adx.c; no teste_som (rede 3), também.
 *   Som_DecodificaAdx: um arquivo ADX pelo start() e pelo decode() do port, como um player o toca, para a referência
 *     independente em Python (port/x360/tools/som_ref.py) comparar.
 */
#include "../../src/gs/snd_adx.c"
#ifdef PORT_FLUXO_BLOCOS
#include <unistd.h>
extern void Port_AdxTick(void); /* plat_sndstate.c */
#endif

/* Toca `path` num player do port e grava a saída estéreo (s16 little-endian, esquerdo e direito) em `saida`: o arquivo
   inteiro; com laço, até a primeira volta e mais `depois` amostras a partir dela. info[]: canais, taxa, amostras,
   laço, início e fim do laço, c1, c2, amostras antes da volta, amostras depois. `estraga_coef`: soma 1 ao c1 (o
   arredondamento do ffmpeg num coeficiente; controle negativo). Devolve 0, ou -1 se o port recusou o arquivo.
   (F6, etapa 5) Com -DPORT_FLUXO_BLOCOS, o caminho do 360: start_fluxo e view_fluxo, K vblanks contados antes de olhar o
   cabeçalho (o estado que o jogo vê enquanto o bloco não chegou), e o decode() pelos blocos da thread de leitura, que
   aqui se espera (o console dá silêncio). info[10..12]: K, e o stat e o left da vista depois do cabeçalho. */
int Som_DecodificaAdx(const char *path, long depois, FILE *saida, int estraga_coef, long info[13]) {
    Player *p = &sPlayers[0];
    int16_t buf[32 * 2];
    long antes = 0, apos = 0;
    int voltou = 0;

    stop(p);
    memset(p, 0, sizeof(*p));
    p->pan[0] = p->pan[1] = PAN_AUTO;
#ifdef PORT_FLUXO_BLOCOS
    {
        int b0 = adx_blanks(path); /* (o jeito do PC, só para escolher K: dos dois lados do fim) */
        long k, K = b0 > 0 ? ((b0 & 1) ? b0 + 2 : b0 / 2) : 7, espera = 0;
        memset(&gPortAdxView[0], 0, sizeof(gPortAdxView[0]));
        if (gPortAdxCount < 1) {
            gPortAdxCount = 1;
        }
        if (!start_fluxo(p, path)) {
            return -1;
        }
        view_fluxo(p, path);
        for (k = 0; k < K; k++) {
            Port_AdxTick();
        }
        while (p->pendente) {
            fluxo_resolve(p);
            if (p->pendente) {
                if (++espera > 200000) {
                    fprintf(stderr, "teste_som: o cabeçalho de %s não chegou\n", path);
                    return -1;
                }
                usleep(50);
            }
        }
        info[10] = K;
        info[11] = gPortAdxView[0].stat;
        info[12] = gPortAdxView[0].left;
    }
    if (p->fluxo == NULL || p->stream == NULL) {
        return -1;
    }
#else
    start(p, path);
    if (p->file == NULL || p->stream == NULL) {
        return -1;
    }
#endif
    if (estraga_coef) {
        p->c1 += 1;
    }
    for (;;) {
        uint32_t era = p->pos;
        int n = decode(p, buf, 1), k;
#ifdef PORT_FLUXO_BLOCOS
        static long espera;
        if (p->falta) { /* o bloco ainda não chegou: aqui se espera */
            p->falta = 0;
            if (n == 0) {
                if (++espera > 200000) {
                    fprintf(stderr, "teste_som: o bloco de %s em %u não chegou\n", path, p->pos);
                    return -1;
                }
                usleep(50);
                continue;
            }
        }
        espera = 0;
#endif
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
