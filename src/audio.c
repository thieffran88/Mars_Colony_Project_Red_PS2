/* Saida de audio.
 *
 * PS2: o jogo alimenta o audsrv (IOP) diretamente a partir do loop principal
 * (audio_pump), sem a thread de audio do SDL. No PS2 as threads nao tem
 * fatiamento de tempo: a thread do SDL so rodava quando o loop principal
 * cedia a CPU, e a IA calculando por centenas de ms causava estalos ou
 * silencio. Agora a fila de ~100 ms do audsrv e completada a cada quadro e
 * tambem durante a busca da IA (gancho ai_idle). Saida a 48 kHz, a taxa
 * nativa do SPU2 (sem o conversor de taxa do audsrv); o sintetizador roda a
 * 24 kHz e e interpolado 2x.
 *
 * PC: thread de audio do SDL normalmente.
 * Se o audio nao puder ser aberto, o jogo segue em silencio. */
#include "audio.h"
#include <SDL.h>
#include <string.h>

#define CHUNK 512                     /* quadros de saida por bloco (48 kHz) */
static int16_t half[CHUNK];           /* CHUNK/2 quadros estereo a 24 kHz    */
static int prevL, prevR;

/* gera 'frames' quadros estereo a partir do sintetizador a meia taxa */
static void render_up(int16_t *out, int frames) {
    while (frames > 0) {
        int n = frames / 2;
        if (n > CHUNK / 2) n = CHUNK / 2;
        if (n < 1) n = 1;
        synth_render(half, n);
        for (int i = 0; i < n && frames > 0; i++) {
            int L = half[2 * i], R = half[2 * i + 1];
            out[0] = (int16_t)((prevL + L) >> 1);
            out[1] = (int16_t)((prevR + R) >> 1);
            out += 2; frames--;
            if (frames > 0) { out[0] = (int16_t)L; out[1] = (int16_t)R; out += 2; frames--; }
            prevL = L; prevR = R;
        }
    }
}

#ifdef PS2
/* ------------------------------------------------------------------ PS2 */
#include <audsrv.h>
#include <ps2_audio_driver.h>

static int active;
static int16_t outbuf[CHUNK * 2] __attribute__((aligned(64)));

void audio_init(void) {
    if (init_audio_driver() < 0) return;
    struct audsrv_fmt_t fmt;
    fmt.freq = 48000; fmt.bits = 16; fmt.channels = 2;
    if (audsrv_set_format(&fmt) != 0) {
        fmt.freq = 44100;
        if (audsrv_set_format(&fmt) != 0) return;
    }
    audsrv_set_volume(MAX_VOLUME);
    synth_init(fmt.freq / 2);
    active = 1;
    audio_pump();
}

void audio_pump(void) {
    if (!active) return;
    int avail = audsrv_available();
    int guard = 12;                     /* no maximo ~128 ms por chamada */
    while (avail >= CHUNK * 4 && guard-- > 0) {
        render_up(outbuf, CHUNK);
        audsrv_play_audio((const char *)outbuf, CHUNK * 4);
        avail -= CHUNK * 4;
    }
}

void audio_sfx(int id) { if (active) synth_sfx(id); }
void audio_music(int track) { if (active) synth_music(track); }
void audio_volume(int music, int sfx) { if (active) synth_volume(music, sfx); }
void audio_quit(void) {
    if (active) audsrv_stop_audio();
    active = 0;
}

#else
/* ------------------------------------------------------------------- PC */
static SDL_AudioDeviceID dev;
static int upsample;

static void callback(void *ud, Uint8 *stream, int len) {
    (void)ud;
    if (upsample) render_up((int16_t *)stream, len / 4);
    else synth_render((int16_t *)stream, len / 4);
}

void audio_init(void) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return;
    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = callback;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have,
                              SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (!dev) return;
    upsample = have.freq >= 44100;
    synth_init(upsample ? have.freq / 2 : have.freq);
    SDL_PauseAudioDevice(dev, 0);
}

void audio_pump(void) { }

#define LOCKED(x) do { if (!dev) return; SDL_LockAudioDevice(dev); x; SDL_UnlockAudioDevice(dev); } while (0)
void audio_sfx(int id) { LOCKED(synth_sfx(id)); }
void audio_music(int track) { LOCKED(synth_music(track)); }
void audio_volume(int music, int sfx) { LOCKED(synth_volume(music, sfx)); }
void audio_quit(void) {
    if (dev) SDL_CloseAudioDevice(dev);
    dev = 0;
}
#endif
