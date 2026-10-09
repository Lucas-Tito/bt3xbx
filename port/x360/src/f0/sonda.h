/* F0: programa de medições no console (sonda). Ver docs/x360/plano.md. */
#ifndef X360_SONDA_H
#define X360_SONDA_H

#ifdef __cplusplus
extern "C" {
#endif

/* Devolvem o número de falhas (0 = tudo certo). */
int Sonda_Float(void);
int Sonda_Memoria(void);

#ifdef __cplusplus
}
#endif

#endif
