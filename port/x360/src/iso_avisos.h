/* Textos públicos, compartilhados com a conferência local das caixas. */
#ifndef X360_ISO_AVISOS_H
#define X360_ISO_AVISOS_H
#include <wchar.h>
#include "iso360.h"
#define ISO_LARGO_(s) L##s
#define ISO_LARGO(s) ISO_LARGO_(s)
/* Textos aprovados pelo Lucas em 29/10/2026. Toda frase da instalação fica aqui. */
static const wchar_t *const ISO_CONV_TITULO = L"Convert ISO";
static const wchar_t *const ISO_CONV_PERGUNTA = L"Convert your original USA ISO for Xbox 360? It needs 3 GB of free space. Your original will be kept.";
static const wchar_t *const ISO_CONV_SIM = L"Convert";
static const wchar_t *const ISO_CONV_NAO = L"Cancel";
static const wchar_t *const ISO_CONV_OK = L"OK";
static const wchar_t *const ISO_CONV_SUCESSO = L"ISO ready";
static const wchar_t *const ISO_CONV_PRONTA = L"Conversion complete. The game will restart.";
static const wchar_t *const ISO_CONV_ESPACO_TITULO = L"Not enough space";
static const wchar_t *const ISO_CONV_ESPACO = L"Free at least 3 GB in the game drive and try again.";
static const wchar_t *const ISO_CONV_FALHA_TITULO = L"Conversion failed";
static const wchar_t *const ISO_CONV_FALHA = L"The ISO could not be converted. Check your original USA disc image and storage, then try again.";
static const wchar_t *const ISO_CONV_DESTINO = L"Rename your original ISO to bt3.iso and try again. The name bt3-x360.iso is reserved for the converted ISO.";
static const char *const ISO_CONV_PROGRESSO[] = { "Checking ISO...", "Copying ISO...", "Converting ISO...", "Verifying ISO...", "Finishing..." };
static const char *const ISO_CONV_AGUARDE = "Please wait. Do not turn off the console.";
/* Tela de progresso em estilo de terminal (só maiúsculas: a fonte 5x7 do port não tem minúsculas).
   Aprovadas pelo Lucas no console em 1/11/2026. A versão entra no %s do título. */
static const char *const ISO_CONV_T_TITULO = "BT3 X360 %s  ISO CONVERSION";
/* A conversão inteira levou 9 min 21 s no HD interno do console do Lucas (1/11/2026); 15 dá folga para mídia lenta. */
static const char *const ISO_CONV_T_PRAZO = "THIS CAN TAKE UP TO 15 MINUTES.";
static const char *const ISO_CONV_T_AVISO = "DO NOT TURN OFF THE CONSOLE.";
static const char *const ISO_CONV_T_CONFERINDO = "CHECKING ORIGINAL ISO";
static const char *const ISO_CONV_T_COPIANDO = "COPYING GAME DATA";
/* Na ordem de IC_CAT_* (personagens, cenários, menus, batalha, sons). */
static const char *const ISO_CONV_T_CATEGORIAS[5] = {"CONVERTING CHARACTERS", "CONVERTING STAGES", "CONVERTING MENUS",
                                                     "CONVERTING BATTLE DATA", "CONVERTING SOUNDS"};
static const char *const ISO_CONV_T_VERIFICANDO = "VERIFYING CONVERTED ISO";
static const char *const ISO_CONV_T_DECORRIDO = "ELAPSED";
static const char *const ISO_CONV_T_OK = "OK";
static const char *const ISO_CONV_T_FALHOU = "FAILED";
/* Linha de ajuda da pergunta própria (substitui as caixas do sistema), aprovada pelo Lucas no console em 1/11/2026. */
static const char *const ISO_CONV_T_AJUDA_DOIS = "A: SELECT    LEFT/RIGHT: CHOOSE    B: CANCEL";
static const char *const ISO_CONV_T_AJUDA_UM = "A: OK";
/* Tela SAVE IMPORT (importação de saves do PS2 pela pasta import): TODOS os textos abaixo estão pendentes de aprovação
   do Lucas. Só maiúsculas (a fonte 5x7 não tem minúsculas). O %s do título é a versão. */
static const char *const ISO_IMP_T_TITULO = "BT3 X360 %s  SAVE IMPORT";
static const char *const ISO_IMP_T_ACHADOS = "FOUND IN IMPORT FOLDER:";
static const char *const ISO_IMP_T_AVISO = "IF YOU IMPORT, YOUR CURRENT SAVE GOES TO SAVES\\BACKUP.";
static const char *const ISO_IMP_T_IMPORTAR = "IMPORT";
static const char *const ISO_IMP_T_PULAR = "SKIP";
static const char *const ISO_IMP_T_AJUDA = "A: SELECT    UP/DOWN: FILE    LEFT/RIGHT: ACTION    B: SKIP";
static const char *const ISO_IMP_T_RECUSADO = "THIS FILE CANNOT BE IMPORTED. CHOOSE AN OK FILE OR SKIP.";
static const char *const ISO_IMP_T_OK = "OK";
static const char *const ISO_IMP_T_TRABALHANDO = "IMPORTING THE SAVE. DO NOT TURN OFF THE CONSOLE.";
static const char *const ISO_IMP_T_TRABALHANDO_TITULO = "IMPORTING";
static const char *const ISO_IMP_T_AJUDA_OK = "A: OK";
static const char *const ISO_IMP_T_FEITO_TITULO = "SAVE IMPORTED";
static const char *const ISO_IMP_T_FEITO = "THE SAVE WAS IMPORTED.";
static const char *const ISO_IMP_T_FEITO_BACKUP = "YOUR PREVIOUS SAVE IS IN SAVES\\BACKUP\\%03d.";
static const char *const ISO_IMP_T_FEITO_ARQUIVOS = "THE IMPORTED FILE WENT TO SAVES\\IMPORT\\DONE.";
static const char *const ISO_IMP_T_FALHA_TITULO = "IMPORT FAILED";
static const char *const ISO_IMP_T_PASSO = "STEP: %s";
static const char *const ISO_IMP_T_MOTIVO = "REASON: %s";
static const char *const ISO_IMP_T_SISTEMA = "SYSTEM CODE: %d";
static const char *const ISO_IMP_T_INTACTO = "YOUR CURRENT SAVE WAS NOT CHANGED.";
static const char *const ISO_IMP_T_RECOLOCADO = "YOUR PREVIOUS SAVE WAS PUT BACK.";
static const char *const ISO_IMP_T_NAO_RECOLOCADO = "YOUR PREVIOUS SAVE COULD NOT BE PUT BACK.";
static const char *const ISO_IMP_T_COPIA = "A COPY OF IT IS IN SAVES\\BACKUP\\%03d.";
static const char *const ISO_IMP_T_MOVER = "THE SAVE WAS IMPORTED, BUT %d FILE(S) COULD NOT BE MOVED.";
static const char *const ISO_IMP_T_MOVER_DE_NOVO = "THIS SCREEN MAY APPEAR AGAIN NEXT TIME.";
static const char *const ISO_IMP_T_PULAR_TITULO = "SKIPPED";
static const char *const ISO_IMP_T_PULAR_MOVER = "%d FILE(S) COULD NOT BE MOVED OUT OF SAVES\\IMPORT,";
static const char *const ISO_IMP_T_PULAR_DE_NOVO = "SO THIS SCREEN WILL APPEAR AGAIN NEXT TIME.";
static const char *const ISO_IMP_T_REMOVER = "TO STOP IT, REMOVE THEM FROM SAVES\\IMPORT.";
static const char *const ISO_IMP_T_FEITO_PULADOS = "%d FILE(S) NOT IMPORTED WENT TO SAVES\\IMPORT\\SKIPPED.";
static const char *const ISO_IMP_T_FEITO_RECUSADOS = "%d REFUSED FILE(S) WENT TO SAVES\\IMPORT\\FAILED.";
static void Iso_Aviso(int motivo, const wchar_t **titulo, const wchar_t **texto) {
    *titulo = L"Cannot read ISO";
    /* Textos da ausente e da ilegível revistos pelo Lucas em 1/11/2026: sem citar o nome reservado da convertida,
       que levava quem tem só a original a dar a ela o nome bt3-x360.iso. */
    *texto = L"The ISO in the game folder is unreadable, incomplete or damaged.";
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
        *texto = L"No ISO in the game folder. Copy your original USA ISO next to default.xex.";
    }
}
#endif
