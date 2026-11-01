/* Executa o adaptador do console e o mesmo núcleo C sobre a ISO fornecida. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "iso_instalar.h"
#include "iso_conv.h"
void Diario_Escrever(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    puts("");
    fflush(stdout);
}
/* Cada troca dos contadores é conferida: a fase e a porcentagem só avançam. */
static int sPc = -1, sFase = -1;
static unsigned sRetrocessos, sDistintos, sPrimeiro;
static unsigned char sVisto[101];
void Teste_Progresso(void) {
    int fase;
    unsigned pc;
    IsoInstalar_Progresso(&fase, &pc);
    if (sPc < 0)
        sPrimeiro = pc;
    if (pc > 100 || (int)pc < sPc || fase < sFase)
        sRetrocessos++;
    if (pc <= 100 && !sVisto[pc]) {
        sVisto[pc] = 1;
        sDistintos++;
    }
    sPc = (int)pc;
    sFase = fase;
}
int main(int argc, char **argv) {
    int r;
    if (argc == 2 && !strcmp(argv[1], "--sha")) {
        IsoSha s;
        unsigned char b[4096], hash[32];
        size_t n;
        unsigned i;
        IsoSha_Iniciar(&s);
        while ((n = fread(b, 1, sizeof(b), stdin)) != 0)
            IsoSha_Atualizar(&s, b, (unsigned)n);
        if (ferror(stdin))
            return 1;
        IsoSha_Final(&s, hash);
        for (i = 0; i < 32; i++)
            printf("%02x", hash[i]);
        puts("");
        return 0;
    }
    if (argc != 2)
        return 2;
    if (!IsoInstalar_Limpar()) {
        puts("FALHA: limpeza");
        return 10;
    }
    r = IsoInstalar_Executar(argv[1]);
    printf("progresso: %u valores distintos, de %u a %d, fase final %d, retrocessos=%u\n", sDistintos, sPrimeiro, sPc,
           sFase, sRetrocessos);
    if (r == 0) {
        IcContas c;
        unsigned i, feitos = 0, totais = 0;
        IsoInstalar_Contas(&c);
        for (i = 0; i < IC_NCATS; i++) {
            feitos += c.feito[i];
            totais += c.total[i];
        }
        printf("categorias: personagens %u/%u cenarios %u/%u menus %u/%u batalha %u/%u sons %u/%u batidas=%u\n",
               (unsigned)c.feito[0], (unsigned)c.total[0], (unsigned)c.feito[1], (unsigned)c.total[1],
               (unsigned)c.feito[2], (unsigned)c.total[2], (unsigned)c.feito[3], (unsigned)c.total[3],
               (unsigned)c.feito[4], (unsigned)c.total[4], (unsigned)c.batidas);
        printf("categorias: feitos %u de %u\n", feitos, totais);
    }
    printf("resultado: %d\n", r);
    return r;
}
