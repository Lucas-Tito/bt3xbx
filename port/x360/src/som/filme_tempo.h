/* F6, etapa 7: correspondência entre a fila PCM e a posição do BGM. C89, também usada no teste do WSL.
   Cada bloco tem dados num prefixo e silêncio na sobra (um pedido ao retorno, bloco menor que PEDACO).
   O relógio é em quadros de saída de 48 kHz; nunca conta a fila adiantada como áudio já tocado. */
#ifndef FILME_TEMPO_H
#define FILME_TEMPO_H
typedef struct FilmeTempo {
    unsigned saida, geracao, inicio, dados;
} FilmeTempo;

/* Uma cópia do anel só vale se o escritor não estava no meio nem publicou outra versão. */
static int filme_tempo_consistente(unsigned antes, unsigned depois) {
    return antes == depois && !(antes & 1u);
}

static int filme_tempo_posicao(const FilmeTempo *b, unsigned tocadas, unsigned *posicao) {
    unsigned deslocamento = tocadas - b->saida;
    if (b->geracao == 0 || deslocamento >= 512 || b->dados > 512) {
        return 0;
    }
    *posicao = b->inicio + (deslocamento < b->dados ? deslocamento : b->dados);
    return 1;
}
#endif
