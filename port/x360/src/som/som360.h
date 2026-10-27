/*
 * F6, etapa 3: a camada de áudio do 360 sobre o XAudio2 (som360.cpp, plano/F6/03.md). Chamado só pelo main360.cpp.
 */
#ifndef X360_SOM360_H
#define X360_SOM360_H
#ifdef __cplusplus
extern "C" {
#endif
/* Lê BT3_SOM e BT3_SOM_TESTE do ambiente; com uma das duas, abre o XAudio2 (48 kHz estéreo, uma voz de fonte nas
   threads de hardware 4 e 5), o misturador (thread 5) e o vigia (thread 1), que toca o tom de teste e escreve os
   contadores no diário.
   Sem nenhuma, não faz nada: o jogo fica como antes da etapa 3 (o SDL_InitSubSystem recusa). */
void Som360_Iniciar(void);
/* A saída (Port_Sair, a volta ao dashboard): para o vigia, StopEngine, solta as vozes e o motor. Pode ser chamada
   mais de uma vez. Nunca de dentro de um retorno do XAudio2. */
void Som360_Parar(void);
/* A queda (AoMorrer): StopEngine já, sem soltar nada; a mistura passa a dar silêncio. */
void Som360_Queda(void);

/* Etapa 7: só a thread de desenho consulta, nunca a DPC. A posição volta ao instante do vblank pelo mftb;
   as faltas da voz invalidam a extrapolação. Copia um anel por versão, sem tomar nem acrescentar trava no mixer. */
typedef struct SomFilmeAmostra {
    unsigned valido, geracao, posicao, faltas, travas, silencios, erro;
    unsigned janelaUs;
} SomFilmeAmostra;
void Som360_FilmeAmostra(unsigned long long tb, double tbPorMs, SomFilmeAmostra *a);
#ifdef __cplusplus
}
#endif
#endif
