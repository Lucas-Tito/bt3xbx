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
    printf("resultado: %d\n", r);
    return r;
}
