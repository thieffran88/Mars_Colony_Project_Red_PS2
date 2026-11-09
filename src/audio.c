/* Ponte entre o SDL2 (thread de audio) e o sintetizador em synth.c.
 *
 * O SPU2 do PS2 trabalha a 48 kHz. Qualquer outra taxa e convertida pelo
 * audsrv (IOP) com um conversor simples que gera chiado. Por isso pedimos
 * 48 kHz e, para poupar a CPU do EE, o sintetizador roda a 24 kHz e aqui
 * dobramos a taxa com interpolacao linear + suavizacao.
 * Se o audio nao puder ser aberto, o jogo segue em silencio. */
#include "audio.h"
#include <SDL.h>
#include <string.h>

static SDL_AudioDeviceID dev;
static int upsample;               /* 1 = sintetiza na metade da taxa */
static int16_t half[4096 * 2];
static int prevL, prevR;

static void callback(void *ud, Uint8 *stream, int len) {
    (void)ud;
    int16_t *out = (int16_t *)stream;
    int frames = len / 4;
    if (!upsample) { synth_render(out, frames); return; }
    while (frames > 0) {
        int n = frames / 2;
        if (n > 4096) n = 4096;
        if (n < 1) n = 1;
        synth_render(half, n);
        for (int i = 0; i < n && frames > 0; i++) {
            int L = half[2 * i], R = half[2 * i + 1];
            /* amostra intermediaria (interpolada) e depois a original */
            out[0] = (int16_t)((prevL + L) >> 1);
            out[1] = (int16_t)((prevR + R) >> 1);
            out += 2; frames--;
            if (frames > 0) { out[0] = (int16_t)L; out[1] = (int16_t)R; out += 2; frames--; }
            prevL = L; prevR = R;
        }
    }
}

void audio_init(void) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return;
    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 2048;
    want.callback = callback;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have,
                              SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (!dev) return;
    upsample = have.freq >= 44100;
    synth_init(upsample ? have.freq / 2 : have.freq);
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
