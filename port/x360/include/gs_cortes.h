/* F5: rateio exclusivo, sob BT3_CORTES=A,B, desligado por padrão. Só a gravação escreve.
   A pilha separa inclusive o GIF chamado de dentro da VU1, sem contar o envio duas vezes. */
#ifndef GS_CORTES_H
#define GS_CORTES_H
extern int gGsCortesAtivo;
enum { COR_DMA, COR_VIF, COR_GIF, COR_CPU, COR_VU1, COR_UNPACK, COR_ENVIO, COR_ENTREGA, COR_HASH, COR_CONST, COR_FECHO, COR_N };
void Gs_CortesEntra(int etapa);
void Gs_CortesSai(void);
void Gs_CortesInicio(unsigned quadro);
void Gs_CortesFim(void);
#define GS_CORTES_ENTRA(e) do { if (gGsCortesAtivo) Gs_CortesEntra(e); } while (0)
#define GS_CORTES_SAI() do { if (gGsCortesAtivo) Gs_CortesSai(); } while (0)
#endif
