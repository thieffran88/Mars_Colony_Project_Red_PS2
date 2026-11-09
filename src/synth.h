/* Sintetizador de musica e efeitos com tema espacial (C puro, sem SDL).
 * Tudo e gerado em tempo real: osciladores por tabela, envelopes,
 * filtro passa-baixa, FM para sinos e eco estereo "ping-pong". */
#ifndef SYNTH_H
#define SYNTH_H
#include <stdint.h>

enum {
    SFX_MOVE = 0,   /* cursor: bip de radio            */
    SFX_OK,         /* confirmar: dois tons subindo     */
    SFX_BACK,       /* voltar: dois tons descendo       */
    SFX_CARD,       /* carta: sopro + chirp             */
    SFX_LAND,       /* colono pousa: retrofoguete + farol */
    SFX_ENERGY,     /* energia: zap de laser subindo    */
    SFX_WIN,        /* vitoria (toca a fanfarra)        */
    SFX_ERROR,      /* erro: zumbido                    */
    SFX_LAUNCH,     /* colono sai da Terra: decolagem   */
    SFX_ALERT,      /* sua vez: ping de sonar           */
    SFX_REVEAL,     /* carta revelada: brilho cristalino */
    SFX_PROMOTE,    /* subir para **: arpejo de estrelas */
    SFX_DRAW,       /* comprar cartas: bips de dados    */
    SFX_COUNT
};

enum { TRACK_NONE = 0, TRACK_TITLE, TRACK_GAME, TRACK_VICTORY, TRACK_COUNT };

void synth_init(int rate);
void synth_render(int16_t *out, int frames);      /* estereo intercalado */
void synth_sfx(int id);
void synth_music(int track);                      /* troca com fade */
void synth_volume(int music, int sfx);            /* 0..10 */
int  synth_track(void);

#endif
