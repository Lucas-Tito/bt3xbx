/* Caso isolado: os mesmos leitores de EftBurst_Start, sem menus, GPU ou substituição da relocação. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "battle/eft_d.h"
#include "sys/gfxm_b_b.h"

static unsigned char comum[0x40];
void *gCommonRes = comum;
EftBurstRes gEftBurstRes;
extern void EftBurst_Start(void);
void Port_UnpackCheck(const char *onde) { (void)onde; }

#define EXIGE(c) do { if (!(c)) { fprintf(stderr, "FALHA transição: %s\n", #c); return 1; } } while (0)
int main(int argc, char **argv) {
    FILE *f;
    static unsigned char dados[300000];
    unsigned char *b = dados;
    long tam;
    EftBurstModel *m;
    TexFile *t;
    void *grupos, *texturas;
    if (argc != 2) return 2;
    f = fopen(argv[1], "rb");
    if (!f) return 2;
    fseek(f, 0, SEEK_END); tam = ftell(f); rewind(f);
    EXIGE(tam > 0 && tam <= (long)sizeof(dados));
    EXIGE(b && fread(b, 1, tam, f) == (size_t)tam);
    fclose(f);
    memcpy(comum + 0x34, &b, sizeof(b));
    printf("LEU %s: %ld bytes; EftBurst_Start\n", argv[1], tam);
    fflush(stdout);
    EftBurst_Start();
    m = gEftBurstRes.model;
    t = (TexFile *)gEftBurstRes.texPack;
    EXIGE((unsigned char *)m == b + 0x280 && (unsigned char *)t == b + 0x2FC00);
    EXIGE(m->groupCount == 2 && m->texCount == 2 && m->relocated == 1);
    EXIGE((unsigned char *)m->groups == (unsigned char *)m + 0x22340);
    EXIGE((unsigned char *)m->tex == (unsigned char *)m + 0x40);
    EXIGE(t->count == 7 && t->ent == (TexEntry *)((unsigned char *)t + 0x20));
    EXIGE((unsigned char *)gEftBurstRes.camAnim == b + 0x40);
    EXIGE(gEftBurstRes.startReq == 1 && gEftBurstRes.endReq == 0);
    grupos = m->groups; texturas = m->tex;
    EftBurst_Start();
    EXIGE(m->groups == grupos && m->tex == texturas);
    EXIGE(t->ent == (TexEntry *)((unsigned char *)t + 0x20));
    printf("transição: modelo %d grupos, %d texturas; partículas %u texturas; duas relocações: tudo certo\n",
           m->groupCount, m->texCount, t->count);

    return 0;
}
