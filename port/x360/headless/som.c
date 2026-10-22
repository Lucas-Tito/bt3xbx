/*
 * F6, etapa 2, rede 2: o rastro de som (plano/F6/00-escopo.md, seção 2). Tudo o que o jogo manda ao som e o que ele
 * lê de volta, por vblank, nas builds sem janela (x86 e PPC), para comparar as duas e duas rodadas da x86 byte a byte.
 *
 * Entra por --wrap (headless.py) nas chamadas do jogo à CRI e ao SOUNDS.IRX que o port atende em gs/snd_adx.c e
 * gs/snd_se.c, sem mudar uma linha deles nem do jogo:
 *   ADXT_Create, ADXT_StartAfs, ADXT_StartFname, ADXT_Stop, ADXT_Pause, ADXT_SetOutVol, ADXT_SetOutPan,
 *   ADXT_SetOutputMono, ADXT_GetStat (o estado devolvido), sceSifCallRpc (cada RPC do SOUNDS.IRX e cada comando da
 *   fila de efeitos: banco, id, volume, pan, pitch, handle), sceSifSetDma (as tabelas do banco) e func_002967C0
 *   (sceSdRemote: as amostras do banco que vão à memória do chip, com o CRC32 dos bytes).
 * O vblank é o gPortVBlanks do plat_stub.c; o fim de cada vblank é o Port_AdxTick (também por --wrap).
 *
 * BT3_SOM=arquivo liga. Uma linha por evento, "V evento", na ordem das chamadas; as leituras de estado seguidas viram
 * uma linha só ("V stat p0=3 p1=0 ..."), e vblanks seguidos que só leram os mesmos estados viram "V-W stat ...".
 * Os players são numerados pela ordem do ADXT_Create (o ponteiro não se compara entre builds). Os endereços que
 * aparecem (tabelas do banco, memória do chip) são os do PS2, iguais nas duas builds. Sem a variável, nada se grava.
 *
 * Com -DBT3_GRAVADOR (a build --gravador, rede 4) passa por aqui também o relógio do dispositivo gravador
 * (gravador.c, a cada vblank) e os estragos de propósito do controle negativo (BT3_SABOTA): pitch (+100 cents em todo
 * efeito), pan (127 - pan) e banco (um byte da amostra do primeiro efeito tocado, grava_se.c). Os estragos agem
 * depois do registro: o rastro continua o do jogo, e só o PCM gravado muda.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern unsigned gPortVBlanks; /* plat_stub.c */

static FILE *sSom;
static int sTentou;
static unsigned sV = 0xFFFFFFFFu; /* o vblank em montagem */

/* as linhas do vblank em montagem, sem o número do vblank na frente */
static char sBuf[1 << 16];
static size_t sLen;
static int sLinhas;      /* quantas linhas */
static int sUltStat;     /* a última linha é de estado (recebe as leituras seguintes) */
static int sSoStat;      /* todas as linhas são de estado (então é uma só) */
/* o último trecho só de estado ainda não escrito: do vblank sAntIni ao sAntFim, o texto em sAnt */
static char sAnt[1 << 12];
static unsigned sAntIni, sAntFim;
static int sTemAnt;

static int ligado(void) {
    if (!sTentou) {
        const char *n = getenv("BT3_SOM");
        sTentou = 1;
        sSom = n != NULL && n[0] != '\0' ? fopen(n, "w") : NULL;
    }
    return sSom != NULL;
}

static void escreve_ant(void) {
    if (sTemAnt) {
        if (sAntIni == sAntFim) {
            fprintf(sSom, "%u %s", sAntIni, sAnt);
        } else {
            fprintf(sSom, "%u-%u %s", sAntIni, sAntFim, sAnt);
        }
        sTemAnt = 0;
    }
}

/* escreve as linhas em montagem, cada uma com o vblank na frente */
static void escreve_linhas(void) {
    char *l = sBuf, *n;
    escreve_ant();
    while (*l != '\0' && (n = strchr(l, '\n')) != NULL) {
        fprintf(sSom, "%u %.*s\n", sV, (int)(n - l + 0), l);
        l = n + 1;
    }
    sLen = 0;
    sBuf[0] = '\0';
}

/* fecha o vblank em montagem: só estado, junta com o trecho anterior se for o vblank seguinte com o mesmo texto */
static void fecha_vblank(void) {
    if (sV != 0xFFFFFFFFu && sLen > 0) {
        if (sSoStat && sLinhas == 1 && sLen < sizeof(sAnt)) {
            if (sTemAnt && sAntFim + 1 == sV && strcmp(sAnt, sBuf) == 0) {
                sAntFim = sV;
            } else {
                escreve_ant();
                memcpy(sAnt, sBuf, sLen + 1);
                sAntIni = sAntFim = sV;
                sTemAnt = 1;
            }
            sLen = 0;
            sBuf[0] = '\0';
        } else {
            escreve_linhas();
            fflush(sSom);
        }
    }
    sLen = 0;
    sBuf[0] = '\0';
}

static void vblank(void) {
    if (gPortVBlanks != sV) {
        fecha_vblank();
        sV = gPortVBlanks;
        sLinhas = 0;
        sUltStat = 0;
        sSoStat = 1;
    }
}

static void poe(const char *s) {
    size_t n = strlen(s);
    if (sLen + n + 2 >= sizeof(sBuf)) { /* um vblank enorme: escreve o que tem e segue (e ele deixa de ser só estado) */
        if (sLen > 0 && sBuf[sLen - 1] != '\n') { /* (a linha de estado aberta sai inteira) */
            sBuf[sLen++] = '\n';
            sBuf[sLen] = '\0';
        }
        escreve_linhas();
        sSoStat = 0;
        sUltStat = 0;
    }
    memcpy(sBuf + sLen, s, n + 1);
    sLen += n;
}

/* um evento (não estado) no vblank de agora */
static void evento(const char *s) {
    vblank();
    poe(s);
    poe("\n");
    sLinhas++;
    sUltStat = 0;
    sSoStat = 0;
}

/* uma leitura de estado: junta-se à linha de estado aberta, ou abre uma */
static void estado(int p, int st) {
    char t[32];
    vblank();
    if (sUltStat && sLen > 0) {
        sLen--; /* tira o \n da linha aberta */
        sBuf[sLen] = '\0';
        sprintf(t, " p%d=%d\n", p, st);
        poe(t);
    } else {
        sprintf(t, "stat p%d=%d\n", p, st);
        poe(t);
        sLinhas++;
        sUltStat = 1;
    }
}

/* ------------------------------------------------------------------------------------------------ os players */

#define MAXP 32
static void *sPlayers[MAXP];
static int sNPlayers;

static int player(void *p) {
    int i;
    for (i = 0; i < sNPlayers; i++) {
        if (sPlayers[i] == p) {
            return i;
        }
    }
    return p == NULL ? -1 : 99;
}

void *__real_ADXT_Create(int maxnch, void *work, int worksize);
void __real_ADXT_StartAfs(void *adxt, int patid, int fid);
void __real_ADXT_StartFname(void *adxt, char *fname);
void __real_ADXT_Stop(void *adxt);
void __real_ADXT_Pause(void *adxt, int sw);
void __real_ADXT_SetOutVol(void *adxt, int vol);
void __real_ADXT_SetOutPan(void *adxt, int chan, int pan);
void __real_ADXT_SetOutputMono(int flag);
int __real_ADXT_GetStat(void *adxt);
int __real_sceSifCallRpc(void *client, int fno, int mode, void *send, int sendSize, void *recv, int recvSize, void *cb, void *arg);
uint32_t __real_sceSifSetDma(void *d, int count);
int __real_func_002967C0(int block, int cmd, int core, int mode, void *iopAddr, uint32_t spuAddr, uint32_t size);
void __real_Port_AdxTick(void);

void *__wrap_ADXT_Create(int maxnch, void *work, int worksize) {
    void *p = __real_ADXT_Create(maxnch, work, worksize);
    if (ligado()) {
        char t[96];
        int i = player(p);
        if (i == 99 && sNPlayers < MAXP) {
            sPlayers[sNPlayers] = p;
            i = sNPlayers++;
        }
        sprintf(t, "create p%d maxnch %d worksize %d", i, maxnch, worksize);
        evento(t);
    }
    return p;
}

void __wrap_ADXT_StartAfs(void *adxt, int patid, int fid) {
    if (ligado()) {
        char t[96];
        sprintf(t, "start p%d afs %d %d", player(adxt), patid, fid);
        evento(t);
    }
    __real_ADXT_StartAfs(adxt, patid, fid);
}

void __wrap_ADXT_StartFname(void *adxt, char *fname) {
    if (ligado()) {
        char t[600];
        snprintf(t, sizeof(t), "start p%d fname %s", player(adxt), fname != NULL ? fname : "(null)");
        evento(t);
    }
    __real_ADXT_StartFname(adxt, fname);
}

void __wrap_ADXT_Stop(void *adxt) {
    if (ligado()) {
        char t[64];
        sprintf(t, "stop p%d", player(adxt));
        evento(t);
    }
    __real_ADXT_Stop(adxt);
}

void __wrap_ADXT_Pause(void *adxt, int sw) {
    if (ligado()) {
        char t[64];
        sprintf(t, "pause p%d %d", player(adxt), sw);
        evento(t);
    }
    __real_ADXT_Pause(adxt, sw);
}

void __wrap_ADXT_SetOutVol(void *adxt, int vol) {
    if (ligado()) {
        char t[64];
        sprintf(t, "vol p%d %d", player(adxt), vol);
        evento(t);
    }
    __real_ADXT_SetOutVol(adxt, vol);
}

void __wrap_ADXT_SetOutPan(void *adxt, int chan, int pan) {
    if (ligado()) {
        char t[64];
        sprintf(t, "pan p%d %d %d", player(adxt), chan, pan);
        evento(t);
    }
    __real_ADXT_SetOutPan(adxt, chan, pan);
}

void __wrap_ADXT_SetOutputMono(int flag) {
    if (ligado()) {
        char t[64];
        sprintf(t, "mono %d", flag);
        evento(t);
    }
    __real_ADXT_SetOutputMono(flag);
}

int __wrap_ADXT_GetStat(void *adxt) {
    int st = __real_ADXT_GetStat(adxt);
    if (ligado()) {
        estado(player(adxt), st);
    }
    return st;
}

/* ------------------------------------------------------------------------------------------------ os efeitos */

static uint32_t crc32(const uint8_t *p, uint32_t n) {
    uint32_t c = 0xFFFFFFFFu, i;
    int k;
    for (i = 0; i < n; i++) {
        c ^= p[i];
        for (k = 0; k < 8; k++) {
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
    }
    return ~c;
}

#ifdef BT3_GRAVADOR
extern void Gravador_Vblank(void);                  /* gravador.c */
extern void Som_SabotaBanco(int mask, int id);      /* grava_se.c */
static const char *sabota(void) {
    static const char *s;
    static int lido;
    if (!lido) {
        lido = 1;
        s = getenv("BT3_SABOTA");
        if (s != NULL && s[0] != '\0') {
            fprintf(stderr, "som: ESTRAGO DE PROPÓSITO: %s (controle negativo da rede 4)\n", s);
        } else {
            s = NULL;
        }
    }
    return s;
}
#endif

int __wrap_sceSifCallRpc(void *client, int fno, int mode, void *send, int sendSize, void *recv, int recvSize, void *cb, void *arg) {
    if (ligado()) {
        char t[160];
        const int32_t *w = send;
        if (fno == 0xD && send != NULL) {
            const uint8_t *q = send;
            uint32_t n, i;
            memcpy(&n, q, 4);
            sprintf(t, "rpc 0xd n %u", n);
            evento(t);
            for (i = 0; i < n && i < 32; i++) {
                const uint8_t *c = q + 4 + i * 12;
                uint16_t h;
                int32_t pitch;
                memcpy(&h, c + 2, 2);
                memcpy(&pitch, c + 8, 4);
                sprintf(t, "se tipo %u laco %u handle %u banco %u id %u vol %u pan %u pitch %d", c[0], c[1], h, c[4], c[5],
                        c[6], c[7], pitch);
                evento(t);
            }
        } else {
            int k, n = send != NULL ? sendSize / 4 : 0;
            int len = sprintf(t, "rpc 0x%x tam %d", fno, sendSize);
            for (k = 0; k < n && k < 6; k++) {
                len += sprintf(t + len, " 0x%x", (unsigned)w[k]);
            }
            evento(t);
        }
    }
#ifdef BT3_GRAVADOR
    if (fno == 0xD && send != NULL && sabota() != NULL) {
        uint8_t *q = send;
        uint32_t n, i;
        memcpy(&n, q, 4);
        for (i = 0; i < n && i < 32; i++) {
            uint8_t *c = q + 4 + i * 12;
            if (c[0] != 0) {
                continue;
            }
            if (strcmp(sabota(), "pitch") == 0) {
                int32_t p;
                memcpy(&p, c + 8, 4);
                p += 100;
                memcpy(c + 8, &p, 4);
            } else if (strcmp(sabota(), "pan") == 0) {
                c[7] = (uint8_t)(127 - c[7]);
            } else if (strcmp(sabota(), "banco") == 0) {
                Som_SabotaBanco(c[4], c[5]);
            }
        }
    }
#endif
    return __real_sceSifCallRpc(client, fno, mode, send, sendSize, recv, recvSize, cb, arg);
}

/* as tabelas do banco (main -> IOP): endereços e tamanhos (o conteúdo de duas palavras muda de ordem na PPC, saída A) */
uint32_t __wrap_sceSifSetDma(void *d, int count) {
    if (ligado()) {
        const uint32_t *r = d;
        int i;
        for (i = 0; i < count && i < 8; i++) {
            char t[96];
            sprintf(t, "dma 0x%x -> 0x%x tam %d modo %d", r[i * 4], r[i * 4 + 1], (int)r[i * 4 + 2], (int)r[i * 4 + 3]);
            evento(t);
        }
    }
    return __real_sceSifSetDma(d, count);
}

/* sceSdRemote: 0x80D0 leva as amostras à memória do chip (o conteúdo é ADPCM, igual nas duas ordens de bytes) */
int __wrap_func_002967C0(int block, int cmd, int core, int mode, void *iopAddr, uint32_t spuAddr, uint32_t size) {
    if (ligado() && cmd == 0x80D0) {
        char t[96];
        sprintf(t, "spu 0x%x tam 0x%x crc %08x", spuAddr, size, iopAddr != NULL ? crc32(iopAddr, size) : 0u);
        evento(t);
    }
    return __real_func_002967C0(block, cmd, core, mode, iopAddr, spuAddr, size);
}

/* o fim de um vblank (Port_VBlank, depois do gPortVBlanks++ e da contagem do estado) */
void __wrap_Port_AdxTick(void) {
    __real_Port_AdxTick();
#ifdef BT3_GRAVADOR
    Gravador_Vblank();
#endif
}

/* no fim do programa: o vblank em montagem e o último trecho de estado */
static void __attribute__((destructor)) som_fim(void) {
    if (sSom != NULL) {
        fecha_vblank();
        escreve_ant();
        fclose(sSom);
        sSom = NULL;
    }
}
