/* Relógio do fluxo e correspondência da fila PCM da etapa 7. Sem dispositivo, sem dados do jogo. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "som_nucleo.h"
#include "filme_tempo.h"
void som_travar(void) {}
void som_destravar(void) {}

int main(void) {
    SDL_AudioSpec spec;
    SDL_AudioStream *s, *outro;
    SomFilmeRelogio r, antes;
    FilmeTempo b;
    unsigned pos, g;
    int user, user2, taxa;
    short dados[4096], saida[1024];
    assert(filme_tempo_consistente(0, 0) && filme_tempo_consistente(2, 2));
    assert(!filme_tempo_consistente(1, 1) && !filme_tempo_consistente(2, 4));
    memset(dados, 0, sizeof(dados)); /* silêncio codificado é dado válido, falta de dado é outra coisa */
    spec.format = SOM_AUDIO_S16; spec.channels = 2;
    for (taxa = 24000; taxa <= 48000; taxa += 24000) {
        spec.freq = taxa;
        s = SDL_CreateAudioStream(&spec, NULL);
        outro = SDL_CreateAudioStream(&spec, NULL);
        assert(s && outro);
        SDL_SetAudioStreamGetCallback(s, NULL, &user);
        SDL_SetAudioStreamGetCallback(outro, NULL, &user2);
        assert(SDL_BindAudioStream(1, s));
        assert(SomNucleo_FilmeRelogio(&user, &r) && r.geracao && !r.quadros);
        g = r.geracao;
        assert(!SomNucleo_FilmeRelogio(&user2, &r)); /* existe, mas não está ligado */
        SomNucleo_Misturar(saida, 512);
        assert(SomNucleo_FilmeRelogio(&user, &r) && !r.quadros);
        assert(SDL_PutAudioStreamData(s, dados, sizeof(dados)));
        SomNucleo_Misturar(saida, 512);
        assert(SomNucleo_FilmeRelogio(&user, &r) && r.quadros == 512 && r.geracao == g);
        antes = r;
        SomNucleo_Misturar(saida, 512);
        assert(SomNucleo_FilmeRelogio(&user, &r) && r.quadros == 1024);
        b.saida = 4096; b.inicio = antes.quadros; b.dados = r.quadros - antes.quadros; b.geracao = g;
        assert(filme_tempo_posicao(&b, 4096, &pos) && pos == 512); /* a fila adiantada não foi tocada */
        assert(filme_tempo_posicao(&b, 4352, &pos) && pos == 768);
        assert(!filme_tempo_posicao(&b, 4095, &pos));
        assert(!filme_tempo_posicao(&b, 4608, &pos));
        b.dados = 123;
        assert(filme_tempo_posicao(&b, 4352, &pos) && pos == 635); /* falta no fim: não conta o silêncio */
        b.geracao = 0;
        assert(!filme_tempo_posicao(&b, 4352, &pos));
        b.geracao = g; b.saida = ~0u - 99; b.dados = 512;
        assert(filme_tempo_posicao(&b, 0, &pos) && pos == 612); /* volta do contador de saída */
        SDL_DestroyAudioStream(s);
        assert(!SomNucleo_FilmeRelogio(&user, &r));
        s = SDL_CreateAudioStream(&spec, NULL);
        SDL_SetAudioStreamGetCallback(s, NULL, &user);
        SDL_BindAudioStream(1, s);
        assert(SomNucleo_FilmeRelogio(&user, &r) && r.geracao != g && r.quadros == 0);
        SDL_DestroyAudioStream(s); SDL_DestroyAudioStream(outro);
    }
    puts("tudo certo: posição PCM, falta, fila adiantada, identidade, geração, 24/48 kHz e volta do contador");
    return 0;
}
