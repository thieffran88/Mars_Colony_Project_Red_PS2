#ifndef AUDIO_H
#define AUDIO_H
#include "synth.h"   /* SFX_* e TRACK_* */

void audio_init(void);
void audio_sfx(int id);
void audio_music(int track);          /* TRACK_NONE / TITLE / GAME / VICTORY */
void audio_volume(int music, int sfx); /* 0..10 */
void audio_quit(void);

#endif
