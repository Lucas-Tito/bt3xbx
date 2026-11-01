/* Instalação síncrona, chamada somente pela thread de hardware 2, antes do jogo. */
#ifndef ISO_INSTALAR_H
#define ISO_INSTALAR_H
#ifdef __cplusplus
extern "C" {
#endif
int IsoInstalar_Limpar(void);
int IsoInstalar_Executar(const char *origem);
void IsoInstalar_Progresso(int *fase, unsigned *porcento);
#ifdef __cplusplus
}
#endif
#endif
