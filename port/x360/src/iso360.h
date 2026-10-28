/* Motivos da recusa, compartilhados pela leitura da ISO e pela caixa do sistema. */
#ifndef X360_ISO360_H
#define X360_ISO360_H
#ifdef __cplusplus
extern "C" {
#endif

enum {
    ISO_OK = 0,
    ISO_ORIGINAL = 1,
    ISO_AUSENTE = 2,
    ISO_NAO_SUPORTADA = 3,
    ISO_ILEGIVEL = 4
};
#define ISO_NOME "bt3-x360.iso"
#define ISO_MARCA_VERSAO 2
void Iso_Iniciar(void);
int Iso_Recusada(void);
const char *Iso_Pedida(void); /* caminho escolhido, inclusive nas recusas da busca automática */

#ifdef __cplusplus
}
#endif
#endif
