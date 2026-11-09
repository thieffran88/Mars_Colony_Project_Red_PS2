/* Sintetizador minimo: efeitos sonoros e uma trilha ambiente procedural.
 * Se o audio nao puder ser aberto, o jogo segue em silencio. */
#include "audio.h"
#include <SDL.h>
#include <math.h>
#include <string.h>

#define RATE 44100
#define NVOICE 6

typedef struct { float freq, freq2, t, dur, vol; int wave; float phase; int active; } Voice;

static SDL_AudioDeviceID dev;
static Voice voices[NVOICE];
static int music_on;
static float mus_t, pad_phase[3], arp_phase;
static int mus_step;

/* escala menor (La menor) para a trilha */
static const float SCALE[] = { 220.00f, 261.63f, 293.66f, 329.63f, 392.00f, 440.00f, 523.25f, 587.33f };
static const int CHORDS[4][3] = { {0, 2, 4}, {5, 0, 3}, {3, 5, 0}, {4, 1, 3} };

static float osc(int wave, float ph) {
    float x = ph - floorf(ph);
    switch (wave) {
    case 0: return sinf(x * 6.2831853f);
    case 1: return x < 0.5f ? 0.6f : -0.6f;
    case 2: return 1.6f * (x < 0.5f ? 4 * x - 1 : 3 - 4 * x) * 0.5f;
    default: return 0;
    }
}

static void callback(void *ud, Uint8 *stream, int len) {
    (void)ud;
    int16_t *out = (int16_t *)stream;
    int n = len / 4;
    for (int i = 0; i < n; i++) {
        float s = 0;
        for (int v = 0; v < NVOICE; v++) {
            Voice *V = &voices[v];
            if (!V->active) continue;
            float k = V->t / V->dur;
            float f = V->freq + (V->freq2 - V->freq) * k;
            float env = (1 - k) * (V->t < 0.005f ? V->t / 0.005f : 1);
            s += osc(V->wave, V->phase) * env * V->vol;
            V->phase += f / RATE;
            V->t += 1.0f / RATE;
            if (V->t >= V->dur) V->active = 0;
        }
        if (music_on) {
            int chord = (mus_step / 8) % 4;
            float pad = 0;
            for (int k = 0; k < 3; k++) {
                float f = SCALE[CHORDS[chord][k]] * 0.5f;
                pad += sinf(pad_phase[k] * 6.2831853f);
                pad_phase[k] += f / RATE;
                if (pad_phase[k] > 1) pad_phase[k] -= 1;
            }
            float beat = mus_t;
            float aenv = expf(-beat * 5.0f);
            int note = CHORDS[chord][mus_step % 3] + ((mus_step / 3) % 2 ? 2 : 0);
            float af = SCALE[note % 8] * 2.0f;
            float arp = sinf(arp_phase * 6.2831853f) * aenv;
            arp_phase += af / RATE;
            if (arp_phase > 1) arp_phase -= 1;
            s += pad * 0.035f + arp * 0.05f;
            mus_t += 1.0f / RATE;
            if (mus_t >= 0.42f) { mus_t = 0; mus_step++; }
        }
        if (s > 1) s = 1; if (s < -1) s = -1;
        int16_t o = (int16_t)(s * 30000);
        out[i * 2] = o; out[i * 2 + 1] = o;
    }
}

void audio_init(void) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return;
    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq = RATE; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024;
    want.callback = callback;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev) SDL_PauseAudioDevice(dev, 0);
}

static void play(float f1, float f2, float dur, float vol, int wave) {
    if (!dev) return;
    SDL_LockAudioDevice(dev);
    int best = 0; float bt = -1;
    for (int v = 0; v < NVOICE; v++) {
        if (!voices[v].active) { best = v; bt = 1e9f; break; }
        if (voices[v].t > bt) { bt = voices[v].t; best = v; }
    }
    Voice *V = &voices[best];
    V->freq = f1; V->freq2 = f2; V->dur = dur; V->vol = vol; V->wave = wave;
    V->t = 0; V->phase = 0; V->active = 1;
    SDL_UnlockAudioDevice(dev);
}

void audio_sfx(int id) {
    switch (id) {
    case SFX_MOVE:   play(880, 880, 0.04f, 0.15f, 1); break;
    case SFX_OK:     play(660, 990, 0.10f, 0.20f, 2); break;
    case SFX_BACK:   play(500, 300, 0.10f, 0.20f, 2); break;
    case SFX_CARD:   play(1200, 400, 0.08f, 0.15f, 1); break;
    case SFX_LAND:   play(300, 120, 0.25f, 0.30f, 0); play(600, 900, 0.12f, 0.12f, 2); break;
    case SFX_ENERGY: play(400, 1600, 0.18f, 0.15f, 2); break;
    case SFX_WIN:    play(523, 523, 0.3f, 0.2f, 2); play(659, 659, 0.45f, 0.2f, 2); play(784, 1046, 0.7f, 0.2f, 2); break;
    case SFX_ERROR:  play(160, 140, 0.15f, 0.25f, 1); break;
    }
}

void audio_music(int on) {
    if (!dev) return;
    SDL_LockAudioDevice(dev);
    music_on = on;
    SDL_UnlockAudioDevice(dev);
}

void audio_quit(void) {
    if (dev) SDL_CloseAudioDevice(dev);
    dev = 0;
}
