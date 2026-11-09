/* Ponte entre o SDL2 (thread de audio) e o sintetizador em synth.c.
 * Se o audio nao puder ser aberto, o jogo segue em silencio. */
#include "audio.h"
#include <SDL.h>
#include <string.h>

static SDL_AudioDeviceID dev;

static void callback(void *ud, Uint8 *stream, int len) {
    (void)ud;
    synth_render((int16_t *)stream, len / 4);
}

void audio_init(void) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return;
    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq = 22050;              /* metade da carga de CPU no EE do PS2 */
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = callback;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!dev) return;
    synth_init(have.freq);
    SDL_PauseAudioDevice(dev, 0);
}

void audio_sfx(int id) {
    if (!dev) return;
    SDL_LockAudioDevice(dev);
    synth_sfx(id);
    SDL_UnlockAudioDevice(dev);
}

void audio_music(int track) {
    if (!dev) return;
    SDL_LockAudioDevice(dev);
    synth_music(track);
    SDL_UnlockAudioDevice(dev);
}

void audio_volume(int music, int sfx) {
    if (!dev) return;
    SDL_LockAudioDevice(dev);
    synth_volume(music, sfx);
    SDL_UnlockAudioDevice(dev);
}

void audio_quit(void) {
    if (dev) SDL_CloseAudioDevice(dev);
    dev = 0;
}
