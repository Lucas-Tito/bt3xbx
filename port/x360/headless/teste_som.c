/*
 * F6, etapa 2, rede 3: as amostras que saem dos decodificadores do port (gs/snd_adx.c e gs/snd_se.c, compilados sem
 * mudança dentro de grava_adx.c e grava_se.c, com o dispositivo gravador.c), para comparar com a referência independente
 * em Python (port/x360/tools/som_ref.py) e entre a x86 e a PPC. Sem o jogo: só o que os dois arquivos pedem, em stubs.
 * Construído por headless.py --alvo x86|ppc --teste-som (port/build/<alvo>t/teste_som); usado por som.py amostras.
 *
 * Um trabalho por linha da entrada padrão:
 *   adx ARQUIVO SAIDA DEPOIS ESTRAGA   o ADX inteiro (com laço: até a volta e DEPOIS amostras dela) em SAIDA;
 *                                      ESTRAGA 1 soma 1 ao c1 (controle negativo)
 *   vag ARQUIVO PARTE TAM SAIDA ESTRAGA INI...   as amostras do banco (a parte das amostras em PARTE, TAM bytes), cada
 *                                      uma de INI (deslocamento na parte) até a bandeira de fim, todas seguidas em SAIDA;
 *                                      ESTRAGA >= 0 troca esse byte da parte antes (controle negativo)
 * Responde uma linha por trabalho: "adx ARQUIVO c TAXA AMOSTRAS LACO INI FIM C1 C2 ANTES DEPOIS" (ou "recusado") e
 * "vag ARQUIVO N1 N2 ..." (amostras de cada uma; -1 se o port recusou).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* o que gs/snd_adx.c e gs/snd_se.c pedem do resto do port */
unsigned gGsFrame, gPortVBlanks;
int gPortResim;
int GsGpu_Enabled(void) { return 0; }
const char *Port_GetEnv(const char *name) { return getenv(name); } /* (o getenv do gs_internal.h) */
int Port_NetSession(void) { return 0; }
int Port_NetVoiceAlt(void) { return 0; }
int PortSongs_IsFile(const char *path) { (void)path; return 0; }
int Port_FilePath(int ptid, int flid, const char *fname, char *out, int size) {
    (void)ptid; (void)flid;
    if (fname == NULL) {
        return 0;
    }
    snprintf(out, (size_t)size, "%s", fname);
    return 1;
}

extern int Som_DecodificaAdx(const char *path, long depois, FILE *saida, int estraga_coef, long info[10]);
extern long Som_DecodificaVag(const uint8_t *smp, uint32_t tam, uint32_t ini, FILE *saida, long estraga);

static char sLinha[1 << 20];

int main(void) {
    setenv("BT3_SOUND", "1", 1); /* o dispositivo (o gravador) abre sem janela */
    while (fgets(sLinha, sizeof(sLinha), stdin) != NULL) {
        char *tok[8192];
        int n = 0;
        char *s = strtok(sLinha, " \t\r\n");
        while (s != NULL && n < 8192) {
            tok[n++] = s;
            s = strtok(NULL, " \t\r\n");
        }
        if (n == 5 && strcmp(tok[0], "adx") == 0) {
            long info[10];
            FILE *o = fopen(tok[2], "wb");
            int r;
            if (o == NULL) {
                printf("erro %s: não abre a saída\n", tok[2]);
                return 1;
            }
            r = Som_DecodificaAdx(tok[1], atol(tok[3]), o, atoi(tok[4]), info);
            fclose(o);
            if (r != 0) {
                printf("adx %s recusado\n", tok[1]);
            } else {
                printf("adx %s %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld\n", tok[1], info[0], info[1], info[2], info[3], info[4],
                       info[5], info[6], info[7], info[8], info[9]);
            }
        } else if (n >= 6 && strcmp(tok[0], "vag") == 0) {
            FILE *f = fopen(tok[1], "rb"), *o = fopen(tok[4], "wb");
            uint32_t parte = (uint32_t)strtoul(tok[2], NULL, 0), tam = (uint32_t)strtoul(tok[3], NULL, 0);
            uint8_t *smp = malloc(tam ? tam : 1);
            int k;
            if (f == NULL || o == NULL || smp == NULL || fseek(f, (long)parte, SEEK_SET) != 0 || fread(smp, 1, tam, f) != tam) {
                printf("erro %s: não lê a parte das amostras\n", tok[1]);
                return 1;
            }
            fclose(f);
            printf("vag %s", tok[1]);
            for (k = 6; k < n; k++) {
                printf(" %ld", Som_DecodificaVag(smp, tam, (uint32_t)strtoul(tok[k], NULL, 0), o, atol(tok[5])));
            }
            printf("\n");
            fclose(o);
            free(smp);
        } else if (n > 0) {
            printf("erro: linha que não entendi: %s\n", tok[0]);
            return 1;
        }
        fflush(stdout);
    }
    return 0;
}
