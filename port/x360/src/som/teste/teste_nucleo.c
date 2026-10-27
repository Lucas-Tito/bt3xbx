/*
 * F6, etapa 3: o teste do núcleo portátil da camada de áudio (som_nucleo.c), sem emulador nem console:
 *   port/x360/tools/teste_som360.sh      (compila com gcc -std=c89 -pedantic, roda e roda os mutantes)
 * A conversão de taxa é conferida contra a forma fechada, escrita aqui de outro jeito que a conta incremental do
 * núcleo: a saída m de um fluxo de taxa r que começou junto é s[k] + (((s[k+1] - s[k]) * q) >> 15), com
 * k = floor(m * r / 48000) e q = ((m * r mod 48000) << 15) / 48000. A trava daqui só conta a profundidade: toda
 * chamada tem de devolvê-la a zero, e o retorno de chamada tem de rodar com ela pega (o contrato da SDL).
 * Sai com 0 se tudo passou, 1 na primeira diferença (com o caso e a posição).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../som_nucleo.h"

static int sProf, sProfMax;
void som_travar(void) {
    sProf++;
    if (sProf > sProfMax) {
        sProfMax = sProf;
    }
}
void som_destravar(void) {
    sProf--;
}

/* a mistura como a plataforma a chama: com a trava pega */
static void mistura(short *saida, int quadros) {
    som_travar();
    SomNucleo_Misturar(saida, quadros);
    som_destravar();
}

static int sFalhas;
static const char *sCaso = "";
#define CONFERE(cond, ...)                                             \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("FALHA [%s] linha %d: ", sCaso, __LINE__);          \
            printf(__VA_ARGS__);                                       \
            printf("\n");                                              \
            exit(1); /* (um fluxo do caso pode ter ficado ligado) */   \
        }                                                              \
    } while (0)

static unsigned sSemente;
static short aleatorio(void) {
    sSemente = sSemente * 1103515245u + 12345u;
    return (short)(sSemente >> 16);
}

/* a forma fechada (ver o topo); s tem canais amostras por quadro, c o canal (mono: 0) */
static int esperado(const short *s, int canais, int c, long m, int r) {
    long k = m * r / SOM_TAXA;
    long q = ((m * r % SOM_TAXA) << 15) / SOM_TAXA;
    int a = s[k * canais + c], b = s[(k + 1) * canais + c];
    return a + (int)(((long)(b - a) * q) >> 15);
}

static SDL_AudioStream *novo(int taxa, int canais) {
    SDL_AudioSpec e;
    SDL_AudioStream *s;
    e.format = SOM_AUDIO_S16;
    e.channels = canais;
    e.freq = taxa;
    s = SDL_CreateAudioStream(&e, NULL);
    if (s != NULL) {
        SDL_BindAudioStream(1, s);
    }
    return s;
}

static short sSaida[20000 * 2];
static short sEnt[20000 * 2];

/* um fluxo de taxa r e canais, posto inteiro de uma vez, contra a forma fechada */
static void caso_taxa(const char *nome, int r, int canais, int saidas) {
    SDL_AudioStream *s;
    int n = (int)((long)saidas * r / SOM_TAXA) + 2, i, c;
    sCaso = nome;
    for (i = 0; i < n * canais; i++) {
        sEnt[i] = aleatorio();
    }
    s = novo(r, canais);
    CONFERE(s != NULL, "o fluxo não abriu");
    CONFERE(SDL_PutAudioStreamData(s, sEnt, n * canais * 2), "put");
    mistura(sSaida, saidas);
    for (i = 0; i < saidas; i++) {
        for (c = 0; c < 2; c++) {
            int e = esperado(sEnt, canais, canais == 2 ? c : 0, i, r);
            CONFERE(sSaida[i * 2 + c] == e, "saída %d canal %d: %d, esperado %d", i, c, sSaida[i * 2 + c], e);
        }
    }
    SDL_DestroyAudioStream(s);
    CONFERE(sProf == 0, "trava presa: %d", sProf);
}

/* o retorno de chamada: põe os quadros da sequência a partir de onde parou, o tanto que pedirem (ou mais) */
typedef struct Gerador {
    const short *seq;
    int canais, pos, total, chamadas, ultimoAdd, ultimoTotal, sobra, profErrada;
} Gerador;
static void SDLCALL_gera(void *user, SDL_AudioStream *s, int additional, int total) {
    Gerador *g = (Gerador *)user;
    int tam = g->canais * 2, quadros = (additional + tam - 1) / tam + g->sobra;
    if (sProf < 1) {
        g->profErrada++;
    }
    g->chamadas++;
    g->ultimoAdd = additional;
    g->ultimoTotal = total;
    if (g->pos + quadros > g->total) {
        quadros = g->total - g->pos;
    }
    SDL_PutAudioStreamData(s, g->seq + g->pos * g->canais, quadros * tam);
    g->pos += quadros;
}

static void caso_retorno(const char *nome, int r, int canais, int bloco, int blocos, int sobra) {
    Gerador g;
    SDL_AudioStream *s;
    int saidas = bloco * blocos, n = (int)((long)saidas * r / SOM_TAXA) + 2 + sobra * blocos + 2, i, c, b;
    sCaso = nome;
    for (i = 0; i < n * canais; i++) {
        sEnt[i] = aleatorio();
    }
    memset(&g, 0, sizeof(g));
    g.seq = sEnt;
    g.canais = canais;
    g.total = n;
    g.sobra = sobra;
    s = novo(r, canais);
    CONFERE(s != NULL, "o fluxo não abriu");
    CONFERE(SDL_SetAudioStreamGetCallback(s, SDLCALL_gera, &g), "retorno");
    for (b = 0; b < blocos; b++) {
        mistura(sSaida + b * bloco * 2, bloco);
        if (b == 0 && bloco <= 1024) {
            /* o primeiro pedido (um bloco que cabe numa volta da mistura): os dois quadros do começo mais os passos do bloco */
            long precisa = (2L * SOM_TAXA + (long)(bloco - 1) * r) / SOM_TAXA;
            CONFERE(g.ultimoAdd == (int)precisa * canais * 2 && g.ultimoTotal == (int)precisa * canais * 2,
                    "primeiro pedido: additional %d total %d, esperado %ld", g.ultimoAdd, g.ultimoTotal,
                    precisa * canais * 2);
        }
    }
    CONFERE(g.profErrada == 0, "retorno sem a trava pega (%d vezes)", g.profErrada);
    CONFERE(g.chamadas >= (sobra ? 1 : blocos - 1), "só %d retornos em %d blocos", g.chamadas, blocos);
    for (i = 0; i < saidas; i++) {
        for (c = 0; c < 2; c++) {
            int e = esperado(sEnt, canais, canais == 2 ? c : 0, i, r);
            CONFERE(sSaida[i * 2 + c] == e, "saída %d canal %d: %d, esperado %d", i, c, sSaida[i * 2 + c], e);
        }
    }
    SDL_DestroyAudioStream(s);
    CONFERE(sProf == 0, "trava presa: %d", sProf);
}

static void caso_ganho(void) {
    static const float ganhos[] = {0.5f, 0.1f, 0.0f, 1.0f, 2.0f, -1.0f, 0.70794578f};
    SDL_AudioStream *s;
    int i, j, n = 600;
    sCaso = "ganho";
    for (j = 0; j < (int)(sizeof(ganhos) / sizeof(ganhos[0])); j++) {
        float g = ganhos[j];
        double gd = g <= 0.0f ? 0.0 : g >= 1.0f ? 1.0 : (double)g;
        double q = floor(gd * 65536.0 + 0.5);
        for (i = 0; i < (n + 1) * 2; i++) {
            sEnt[i] = aleatorio();
        }
        sEnt[0] = 32767;
        sEnt[1] = -32768;
        sEnt[2] = 3;
        sEnt[3] = -3;
        s = novo(SOM_TAXA, 2);
        CONFERE(SDL_SetAudioStreamGain(s, g), "gain");
        SDL_PutAudioStreamData(s, sEnt, (n + 1) * 4);
        mistura(sSaida, n);
        for (i = 0; i < n * 2; i++) {
            /* o arredondamento: o mais perto, metade para cima */
            int e = (int)floor((double)sEnt[i] * q / 65536.0 + 0.5);
            CONFERE(sSaida[i] == e, "ganho %g, amostra %d: %d de %d, esperado %d", (double)g, i, sSaida[i], sEnt[i], e);
        }
        SDL_DestroyAudioStream(s);
    }
    {   /* NaN */
        float zero = 0.0f;
        s = novo(SOM_TAXA, 2);
        SDL_SetAudioStreamGain(s, zero / zero);
        sEnt[0] = sEnt[1] = sEnt[2] = sEnt[3] = 1000;
        SDL_PutAudioStreamData(s, sEnt, 8);
        mistura(sSaida, 1);
        CONFERE(sSaida[0] == 0 && sSaida[1] == 0, "NaN não calou: %d %d", sSaida[0], sSaida[1]);
        SDL_DestroyAudioStream(s);
    }
    CONFERE(sProf == 0, "trava presa: %d", sProf);
}

static void caso_soma(void) {
    SDL_AudioStream *a, *b, *m;
    SomContadores c0, c1;
    int i, n = 300;
    sCaso = "soma e corte";
    a = novo(SOM_TAXA, 2);
    b = novo(SOM_TAXA, 2);
    m = novo(24000, 1);
    for (i = 0; i < (n + 1) * 2; i++) {
        sEnt[i] = (short)((i & 1) ? -30000 : 30000);
    }
    SDL_PutAudioStreamData(a, sEnt, (n + 1) * 4);
    SDL_PutAudioStreamData(b, sEnt, (n + 1) * 4);
    for (i = 0; i < n / 2 + 2; i++) {
        sEnt[i] = 1000;
    }
    SDL_PutAudioStreamData(m, sEnt, (n / 2 + 2) * 2);
    SomNucleo_Contadores(&c0);
    mistura(sSaida, n);
    SomNucleo_Contadores(&c1);
    for (i = 0; i < n; i++) {
        CONFERE(sSaida[i * 2] == 32767 && sSaida[i * 2 + 1] == -32768, "quadro %d: %d %d", i, sSaida[i * 2],
                sSaida[i * 2 + 1]);
    }
    CONFERE(c1.saturados - c0.saturados == (unsigned)n * 2, "saturados %u, esperado %d", c1.saturados - c0.saturados,
            n * 2);
    /* um pequeno que não satura: a soma é exata */
    for (i = 0; i < (n + 1) * 2; i++) {
        sEnt[i] = (short)(i * 7 - 2000);
    }
    SDL_ClearAudioStream(a);
    SDL_ClearAudioStream(b);
    SDL_ClearAudioStream(m);
    SDL_PutAudioStreamData(a, sEnt, (n + 1) * 4);
    SDL_PutAudioStreamData(b, sEnt, (n + 1) * 4);
    mistura(sSaida, n);
    for (i = 0; i < n * 2; i++) {
        CONFERE(sSaida[i] == 2 * sEnt[i], "soma %d: %d, esperado %d", i, sSaida[i], 2 * sEnt[i]);
    }
    SDL_DestroyAudioStream(a);
    SDL_DestroyAudioStream(b);
    SDL_DestroyAudioStream(m);
    CONFERE(sProf == 0, "trava presa: %d", sProf);
}

static void caso_falta(void) {
    SDL_AudioStream *s;
    SomContadores c0, c1;
    int i;
    sCaso = "falta e volta";
    for (i = 0; i < 150 * 2; i++) {
        sEnt[i] = aleatorio();
    }
    s = novo(SOM_TAXA, 2);
    SDL_PutAudioStreamData(s, sEnt, 100 * 4);
    SomNucleo_Contadores(&c0);
    mistura(sSaida, 512);
    SomNucleo_Contadores(&c1);
    /* sai um quadro a menos que entrou: o último fica como o "b" da interpolação até chegar o seguinte */
    for (i = 0; i < 99 * 2; i++) {
        CONFERE(sSaida[i] == sEnt[i], "amostra %d: %d, esperado %d", i, sSaida[i], sEnt[i]);
    }
    for (i = 99 * 2; i < 512 * 2; i++) {
        CONFERE(sSaida[i] == 0, "amostra %d depois da falta: %d", i, sSaida[i]);
    }
    CONFERE(c1.curtos - c0.curtos == 413, "curtos %u, esperado 413", c1.curtos - c0.curtos);
    SDL_PutAudioStreamData(s, sEnt + 100 * 2, 50 * 4);
    mistura(sSaida, 49);
    for (i = 0; i < 49 * 2; i++) {
        CONFERE(sSaida[i] == sEnt[99 * 2 + i], "volta, amostra %d: %d, esperado %d", i, sSaida[i], sEnt[99 * 2 + i]);
    }
    SomNucleo_Contadores(&c0);
    CONFERE(c0.curtos == c1.curtos, "a volta contou falta: %u", c0.curtos - c1.curtos);
    /* limpar recomeça do zero: a fase e o histórico */
    SDL_ClearAudioStream(s);
    for (i = 0; i < 31 * 2; i++) {
        sEnt[i] = aleatorio();
    }
    SDL_PutAudioStreamData(s, sEnt, 31 * 4);
    mistura(sSaida, 30);
    for (i = 0; i < 30 * 2; i++) {
        CONFERE(sSaida[i] == sEnt[i], "depois de limpar, amostra %d: %d, esperado %d", i, sSaida[i], sEnt[i]);
    }
    SDL_DestroyAudioStream(s);
    CONFERE(sProf == 0, "trava presa: %d", sProf);
}

static void caso_partido_e_fila(void) {
    SDL_AudioStream *s;
    const unsigned char *p = (const unsigned char *)sEnt;
    int i, n = 10000, k;
    sCaso = "dados partidos e fila que cresce";
    for (i = 0; i < (n + 1) * 2; i++) {
        sEnt[i] = aleatorio();
    }
    s = novo(SOM_TAXA, 2);
    /* os primeiros 64 quadros em pedaços de 1, 3 e 5 bytes; o resto de uma vez (passa da fila inicial de 4096) */
    for (i = 0, k = 1; i < 64 * 4; k = k == 1 ? 3 : k == 3 ? 5 : 1) {
        int t = i + k > 64 * 4 ? 64 * 4 - i : k;
        SDL_PutAudioStreamData(s, p + i, t);
        i += t;
    }
    SDL_PutAudioStreamData(s, p + 64 * 4, (n + 1 - 64) * 4);
    mistura(sSaida, n);
    for (i = 0; i < n * 2; i++) {
        CONFERE(sSaida[i] == sEnt[i], "amostra %d: %d, esperado %d", i, sSaida[i], sEnt[i]);
    }
    SDL_DestroyAudioStream(s);
    CONFERE(sProf == 0, "trava presa: %d", sProf);
}

static void caso_vida(void) {
    SDL_AudioSpec e;
    SDL_AudioStream *s, *t;
    SomContadores c;
    int i;
    sCaso = "criar, ligar, soltar, recusar";
    e.format = 0x8008; /* s8 */
    e.channels = 2;
    e.freq = 24000;
    CONFERE(SDL_CreateAudioStream(&e, NULL) == NULL, "aceitou s8");
    e.format = SOM_AUDIO_S16;
    e.channels = 3;
    CONFERE(SDL_CreateAudioStream(&e, NULL) == NULL, "aceitou 3 canais");
    CONFERE(SDL_CreateAudioStream(NULL, NULL) == NULL, "aceitou nulo");
    CONFERE(SomNucleo_Erro()[0] != '\0', "sem mensagem de erro");
    e.channels = 2;
    s = SDL_CreateAudioStream(&e, NULL);
    CONFERE(s != NULL, "não criou");
    CONFERE(!SDL_BindAudioStream(0, s), "ligou no dispositivo 0");
    for (i = 0; i < 200 * 2; i++) {
        sEnt[i] = 1234;
    }
    SDL_PutAudioStreamData(s, sEnt, 200 * 4);
    mistura(sSaida, 100);
    for (i = 0; i < 200; i++) {
        CONFERE(sSaida[i] == 0, "fluxo não ligado tocou: %d", sSaida[i]);
    }
    t = novo(SOM_TAXA, 2);
    SomNucleo_Contadores(&c);
    CONFERE(c.fluxos == 2 && c.ligados == 1, "fluxos %u, ligados %u", c.fluxos, c.ligados);
    SDL_DestroyAudioStream(s);
    SDL_DestroyAudioStream(t);
    SomNucleo_Contadores(&c);
    CONFERE(c.fluxos == 0 && c.ligados == 0, "depois de soltar: fluxos %u, ligados %u", c.fluxos, c.ligados);
    mistura(sSaida, 10);
    for (i = 0; i < 20; i++) {
        CONFERE(sSaida[i] == 0, "sem fluxos, saiu %d", sSaida[i]);
    }
    CONFERE(SDL_LockAudioStream(NULL) == 0 && SDL_UnlockAudioStream(NULL) == 0, "trava de fluxo nulo");
    CONFERE(sProf == 0, "trava presa: %d", sProf);
}

static void caso_guide(void) {
    SDL_AudioStream *s[4];
    Gerador g[4];
    const int taxas[4] = {24000, 44100, 16000, 48000};
    const int blocos[6] = {37, 512, 193, 1500, 71, 512};
    int pos[4] = {0, 0, 0, 0}, j, b, i, c;
    SomRelogio r, antes;
    SomContadores c0, c1;
    sCaso = "Guide: BGM e MAP seguem, vozes e sampler conservam fila e fase";
    for (i = 0; i < 20000; i++) {
        sEnt[i] = (short)((i * 37) % 3000 - 1500);
    }
    for (j = 0; j < 4; j++) {
        memset(&g[j], 0, sizeof(g[j]));
        g[j].seq = sEnt;
        g[j].canais = 1;
        g[j].total = 20000;
        g[j].sobra = 300; /* a Guide também deve conservar a fila já cheia */
        s[j] = novo(taxas[j], 1);
        SDL_SetAudioStreamGetCallback(s[j], SDLCALL_gera, &g[j]);
    }
    /* O sampler real usa user NULL. Aqui, uma fila pronta e sem retorno. */
    SDL_SetAudioStreamGetCallback(s[3], NULL, NULL);
    SDL_PutAudioStreamData(s[3], sEnt, 20000 * 2);
    for (b = 0; b < 6; b++) {
        int aberta = b == 1 || b == 2 || b == 4;
        int chamadas = g[2].chamadas, geradas = g[2].pos;
        SomNucleo_Relogio(&g[2], &antes);
        SomNucleo_Contadores(&c0);
        som_travar();
        SomNucleo_Guide(aberta, &g[0], &g[1]);
        som_destravar();
        mistura(sSaida, blocos[b]);
        for (i = 0; i < blocos[b]; i++) {
            int e = 0;
            for (j = 0; j < 4; j++) {
                if (!aberta || j < 2) {
                    e += esperado(sEnt, 1, 0, pos[j] + i, taxas[j]);
                }
            }
            for (c = 0; c < 2; c++) {
                CONFERE(sSaida[i * 2 + c] == e, "bloco %d, quadro %d: %d, esperado %d", b, i, sSaida[i * 2 + c], e);
            }
        }
        for (j = 0; j < 4; j++) {
            if (!aberta || j < 2) {
                pos[j] += blocos[b];
            }
        }
        SomNucleo_Contadores(&c1);
        CONFERE(c1.curtos == c0.curtos, "a Guide contou falta");
        CONFERE(SomNucleo_Relogio(&g[2], &r) && r.quadros == (unsigned)pos[2], "relógio da voz andou na pausa");
        if (aberta) {
            CONFERE(g[2].chamadas == chamadas && g[2].pos == geradas && r.quadros == antes.quadros,
                    "a voz consumiu dados ou chamou o retorno na Guide");
        }
    }
    for (j = 0; j < 4; j++) {
        SDL_DestroyAudioStream(s[j]);
    }
    CONFERE(!SomNucleo_Relogio(&g[2], &r) && r.geracao == 0, "relógio de fluxo destruído");
    /* Um fluxo que nasce na Guide fica intacto, inclusive com os handles de música ainda nulos. */
    SomNucleo_Guide(1, NULL, NULL);
    s[0] = novo(24000, 1);
    SDL_PutAudioStreamData(s[0], sEnt, 20000 * 2);
    mistura(sSaida, 512);
    for (i = 0; i < 1024; i++) {
        CONFERE(sSaida[i] == 0, "um fluxo novo com user NULL tocou na Guide");
    }
    SomNucleo_Guide(0, NULL, NULL);
    mistura(sSaida, 37);
    for (i = 0; i < 37; i++) {
        CONFERE(sSaida[i * 2] == esperado(sEnt, 1, 0, i, 24000), "o fluxo novo perdeu o começo");
    }
    SDL_DestroyAudioStream(s[0]);
    CONFERE(sProf == 0, "trava presa: %d", sProf);
    printf("Guide: música contínua, pausa e retomada exatas, fluxo novo suspenso\n");
}

int main(void) {
    sSemente = 25;
    caso_taxa("48 kHz estéreo, identidade", 48000, 2, 3000);
    caso_taxa("24 kHz mono, o dobro", 24000, 1, 3000);
    caso_taxa("24 kHz estéreo", 24000, 2, 3000);
    caso_taxa("16 kHz mono, o triplo", 16000, 1, 3000);
    caso_taxa("44,1 kHz estéreo", 44100, 2, 5000);
    caso_taxa("22,05 kHz mono", 22050, 1, 5000);
    caso_taxa("96 kHz estéreo, a metade", 96000, 2, 3000);
    caso_retorno("retorno, 24 kHz estéreo em blocos de 512", 24000, 2, 512, 12, 0);
    caso_retorno("retorno, 24 kHz mono, sobra de 300 quadros", 24000, 1, 480, 10, 300);
    caso_retorno("retorno, 44,1 kHz estéreo em blocos de 480", 44100, 2, 480, 12, 0);
    caso_retorno("retorno, 48 kHz estéreo em blocos de 1500 (dois pedaços)", 48000, 2, 1500, 5, 0);
    caso_ganho();
    caso_soma();
    caso_falta();
    caso_partido_e_fila();
    caso_vida();
    caso_guide();
    printf("trava: profundidade máxima %d\n", sProfMax);
    if (sFalhas) {
        printf(">> FALHA: %d caso(s)\n", sFalhas);
        return 1;
    }
    printf("núcleo do som: todos os casos passaram\n");
    return 0;
}
