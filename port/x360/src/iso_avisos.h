/* Textos públicos, compartilhados com a conferência local das caixas. */
#ifndef X360_ISO_AVISOS_H
#define X360_ISO_AVISOS_H
#include <wchar.h>
#include "iso360.h"
#define ISO_LARGO_(s) L##s
#define ISO_LARGO(s) ISO_LARGO_(s)
/* Propostas provisórias para aprovação do Lucas. Toda frase da instalação fica aqui. */
static const wchar_t *const ISO_CONV_TITULO = L"Prepare ISO";
static const wchar_t *const ISO_CONV_PERGUNTA = L"Prepare your original USA ISO for Xbox 360? This requires 3 GB of free space. Your original will be kept.";
static const wchar_t *const ISO_CONV_SIM = L"Convert";
static const wchar_t *const ISO_CONV_NAO = L"Cancel";
static const wchar_t *const ISO_CONV_OK = L"OK";
static const wchar_t *const ISO_CONV_SUCESSO = L"ISO ready";
static const wchar_t *const ISO_CONV_PRONTA = L"Conversion complete. The game will restart.";
static const wchar_t *const ISO_CONV_ESPACO_TITULO = L"Not enough space";
static const wchar_t *const ISO_CONV_ESPACO = L"Free at least 3 GB in the game drive and try again.";
static const wchar_t *const ISO_CONV_FALHA_TITULO = L"Conversion failed";
static const wchar_t *const ISO_CONV_FALHA = L"The ISO could not be prepared. Check your original USA disc image and storage, then try again.";
static const wchar_t *const ISO_CONV_DESTINO = L"Keep your original as bt3.iso. Conversion needs the name bt3-x360.iso to be available. Existing files were kept.";
static const char *const ISO_CONV_PROGRESSO[] = { "Checking ISO...", "Copying ISO...", "Converting ISO...", "Verifying ISO...", "Finishing..." };
static const char *const ISO_CONV_AGUARDE = "Please wait. Do not turn off the console.";
static void Iso_Aviso(int motivo, const wchar_t **titulo, const wchar_t **texto) {
    *titulo = L"Cannot read ISO";
    *texto = L"The ISO is unreadable, incomplete or damaged. Copy " ISO_LARGO(ISO_NOME)
             L" to the root of the game folder again, or create a new image from your disc and run convert_iso.py on your PC.";
    if (motivo == ISO_ORIGINAL) {
        *titulo = L"ISO conversion required";
        *texto = L"Convert your original USA SLUS-21678 ISO on your PC with convert_iso.py. "
                 L"Copy the result to the root of the game folder as " ISO_LARGO(ISO_NOME) L", next to default.xex.";
    } else if (motivo == ISO_NAO_SUPORTADA) {
        *titulo = L"Unsupported game version";
        *texto = L"Only the USA SLUS-21678 is supported. Convert your original USA ISO on your PC with convert_iso.py. "
                 L"Copy the result to the root of the game folder as " ISO_LARGO(ISO_NOME) L", next to default.xex.";
    } else if (motivo == ISO_AUSENTE) {
        *titulo = L"ISO not found";
        *texto = L"The ISO was not found in the root of the game folder with the expected name: " ISO_LARGO(ISO_NOME) L".";
    }
}
#endif
