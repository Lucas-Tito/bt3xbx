#ifndef VU0_CONTA_H
#define VU0_CONTA_H
extern unsigned gVcChamadas[512];
int Vc_Motivo(const unsigned *a, int na, const unsigned *b, int nb, const unsigned *c, int nc);
int Vc_Resultado(int id, int ok, int motivo);
void Vc_Passo(int combate, unsigned vb);
#endif
