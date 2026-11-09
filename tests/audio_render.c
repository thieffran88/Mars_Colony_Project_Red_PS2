/* Renderiza as trilhas e efeitos em WAV (sem SDL) e mede niveis/CPU.
 * uso: ./audio_render [pasta]   */
#include "../src/synth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define RATE 22050

static void wav(const char *path, const int16_t *d, int frames) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    int bytes = frames * 4;
    int hdr[11];
    fwrite("RIFF", 1, 4, f); hdr[0] = 36 + bytes; fwrite(hdr, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    int fmt[4] = { 16, 0x00020001, RATE, RATE * 4 }; fwrite(fmt, 4, 4, f);
    short ba[2] = { 4, 16 }; fwrite(ba, 2, 2, f);
    fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
    fwrite(d, 4, frames, f);
    fclose(f);
}

static int stats(const char *name, const int16_t *d, int frames) {
    double sum = 0; int peak = 0, clip = 0;
    for (int i = 0; i < frames * 2; i++) {
        int v = abs(d[i]); if (v > peak) peak = v; if (v >= 29990) clip++;
        sum += (double)d[i] * d[i];
    }
    double rms = sqrt(sum / (frames * 2)) / 32768.0;
    printf("%-14s pico %5.1f%%  RMS %5.1f%%  amostras no limite %d\n", name, peak / 327.68, rms * 100, clip);
    return peak > 0;
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    int ok = 1;
    char path[512];
    const char *tn[] = { "", "titulo", "partida", "vitoria" };
    int secs[] = { 0, 40, 60, 10 };
    for (int t = 1; t < TRACK_COUNT; t++) {
        synth_init(RATE);
        synth_music(t);
        int frames = RATE * secs[t];
        int16_t *buf = malloc(frames * 4);
        clock_t c0 = clock();
        for (int o = 0; o < frames; o += 512) synth_render(buf + o * 2, frames - o < 512 ? frames - o : 512);
        double cpu = (double)(clock() - c0) / CLOCKS_PER_SEC;
        char nm[64]; snprintf(nm, sizeof nm, "musica_%s", tn[t]);
        ok &= stats(nm, buf, frames);
        printf("               %.1fx mais rápido que tempo real (PC)\n", secs[t] / (cpu > 1e-6 ? cpu : 1e-6));
        snprintf(path, sizeof path, "%s/%s.wav", dir, nm); wav(path, buf, frames);
        free(buf);
    }
    const char *sn[] = { "move", "ok", "back", "card", "land", "energy", "win", "error", "launch", "alert",
                         "reveal", "promote", "draw" };
    synth_init(RATE);
    int frames = RATE * 2 * (SFX_COUNT - 1) + RATE * 2;
    int16_t *all = calloc(frames, 4);
    int pos = 0;
    for (int s = 0; s < SFX_COUNT; s++) {
        if (s == SFX_WIN) continue;
        synth_sfx(s);
        int n = RATE * 2;
        synth_render(all + pos * 2, n);
        char nm[64]; snprintf(nm, sizeof nm, "sfx_%s", sn[s]);
        ok &= stats(nm, all + pos * 2, n);
        pos += n;
    }
    snprintf(path, sizeof path, "%s/efeitos.wav", dir); wav(path, all, pos);
    free(all);
    printf(ok ? "OK\n" : "FALHOU (silencio)\n");
    return ok ? 0 : 1;
}
