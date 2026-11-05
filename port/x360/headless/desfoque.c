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
        const char *v = getenv("BT3_FX_OFF");
        int fx = v != NULL ? atoi(v) & 255 : 0;
        if (fx & 32) desligados |= 1 << 1;
        if (fx & 64) desligados |= 1 << 6;
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

/* BT3_DISTANCIA_FORCA: o peso do desfoque da distância (StgFog_BuildClut), em porcento de 0 a 100. Padrão no 360: 25
   (decisão 42, escolhida pelo Lucas no console); nas builds de teste, 100, o original. Só a mistura muda: o passe é
   desenhado igual, e o custo na GPU não muda. */
#ifdef PORT_X360
#define FORCA_PADRAO 25
#else
#define FORCA_PADRAO 100
#endif
int Port_DistanciaForca(void) {
    static int iniciou, forca = FORCA_PADRAO;
    if (!iniciou) {
        const char *v = getenv("BT3_DISTANCIA_FORCA");
        if (v != NULL && *v != '\0') {
            forca = atoi(v);
            if (forca < 0) forca = 0;
            if (forca > 100) forca = 100;
        }
        iniciou = 1;
        printf("desfoque: força da distância %d%% (BT3_DISTANCIA_FORCA)\n", forca);
    }
    return forca;
}

/* Só muda a quantidade desenhada; movimento e aleatórios continuam iguais. */
int Port_ClimaQuantidade(int quantidade) {
    const char *v = getenv("BT3_FX_OFF");
    return v != NULL && (atoi(v) & 128) ? (quantidade + 1) / 2 : quantidade;
}
