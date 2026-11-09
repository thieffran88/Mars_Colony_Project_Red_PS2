#ifndef AUDIO_H
#define AUDIO_H

enum { SFX_MOVE = 0, SFX_OK, SFX_BACK, SFX_CARD, SFX_LAND, SFX_ENERGY, SFX_WIN, SFX_ERROR };

void audio_init(void);
void audio_sfx(int id);
void audio_music(int on);
void audio_quit(void);

#endif
