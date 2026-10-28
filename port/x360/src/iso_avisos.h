/* Textos públicos, compartilhados com a conferência local das caixas. */
#ifndef X360_ISO_AVISOS_H
#define X360_ISO_AVISOS_H
#include <wchar.h>
#include "iso360.h"
#define ISO_LARGO_(s) L##s
#define ISO_LARGO(s) ISO_LARGO_(s)
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
