/* Diagnóstico da F8: cortes independentes na origem dos efeitos, sem mudar o padrão. */
#include <stdio.h>
#include <stdlib.h>

int Port_DesfoqueOff(int efeito) {
    static const char *nomes[] = {
        "BT3_SEM_DISTANCIA", "BT3_SEM_PANORAMICO", "BT3_SEM_RADIAL",
        "BT3_SEM_BRUMA", "BT3_SEM_NEVOA", "BT3_SEM_BRILHO", "BT3_SEM_CLIMA"
    };
    static int iniciou, desligados, vistos;
    int i, bit = 1 << efeito;
    if (!iniciou) {
        for (i = 0; i < 7; i++) {
            const char *v = getenv(nomes[i]);
            if (v != NULL && atoi(v) != 0) desligados |= 1 << i;
        }
        iniciou = 1;
        printf("desfoque: máscara de cortes %d (distância 1, panorâmico 2, radial 4, bruma 8, névoa 16, brilho 32, clima 64)\n", desligados);
    }
    if ((desligados & bit) && !(vistos & bit)) {
        printf("desfoque: corte alcançado: %s\n", nomes[efeito]);
        vistos |= bit;
    }
    return (desligados & bit) != 0;
}
