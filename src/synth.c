/*
 * Mars Colony: Project Red PS2 - sintetizador espacial.
 *
 * Trilhas (todas originais, geradas nota a nota):
 *   TITLE   "Horizonte Vermelho" - synthwave heroico em Do menor, 104 BPM
 *   GAME    "Colônia à Deriva"   - ambiente calmo em La menor, 76 BPM
 *   VICTORY fanfarra curta em Do maior; depois volta ao tema do titulo
 *
 * Feito para rodar no EE do PS2: osciladores por tabela, sem sinf/expf por
 * amostra; custo proporcional ao numero de vozes ativas.
 */
#include "synth.h"
#include <math.h>
#include <string.h>

#define TAB      1024
#define NV_MUS   22
#define NV_SFX   14
#define ECHO_MAX 24000

enum { W_SINE = 0, W_TRI, W_SQUARE, W_SAW, W_NOISE };
enum { BUS_MUS = 0, BUS_SFX };

typedef struct {
    int wave;
    float f0, f1, sweepT;       /* frequencia inicial/final e tempo da varredura */
    float atk, hold, rel;       /* envelope (segundos)                            */
    float vol, pan;             /* volume, pan -1..1                              */
    float lpc, lpc1;            /* passa-baixa (0 = desligado), alvo da varredura */
    float det;                  /* 2o oscilador desafinado (razao), 0 = nao       */
    float vibRate, vibDepth;
    float fmRatio, fmIndex;     /* FM (sinos)                                     */
    float delay;                /* atraso para comecar (segundos)                 */
} NoteP;

typedef struct {
    uint8_t active, wave, stage;
    int delay, hold, sweep;
    float ph, ph2, inc, incMul, det;
    float env, atk, rel;
    float vol, panL, panR;
    float lp, lpc, lpcMul;
    float vibPh, vibInc, vibDepth;
    float fmPh, fmRatio, fmIndex, fmDecay;
    uint32_t nz;
} Voice;

static float sintab[TAB + 1];
static float midi[128];
static float RATE = 22050.f;
static Voice vm[NV_MUS], vs[NV_SFX];
static float echoL[ECHO_MAX], echoR[ECHO_MAX];
static int echoPos, echoDL, echoDR;
static float echoDampL, echoDampR;
static float gainMus = 0.6f, gainSfx = 0.8f;

static int curTrack = TRACK_NONE, nextTrack = TRACK_NONE;
static float fade = 0.f; static int fadingOut = 0;
static int stepLeft, stepSamples, step;     /* step = semicolcheia global */
static int sparkleStep;
static uint32_t rng = 0x1234567u;

static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static float frnd(void) { return (float)(rnd() & 0xFFFF) / 65535.f; }

static inline float tsin(float ph) {
    float x = (ph - (float)(int)ph) * TAB;
    if (x < 0) x += TAB;
    int i = (int)x;
    float f = x - i;
    return sintab[i] + (sintab[i + 1] - sintab[i]) * f;
}

/* ------------------------------------------------------------------ */

static void start_voice(Voice *pool, int n, const NoteP *p) {
    int best = 0; float be = 1e9f;
    for (int i = 0; i < n; i++) {
        if (!pool[i].active) { best = i; be = -1; break; }
        float e = pool[i].stage == 2 ? pool[i].env * 0.5f : pool[i].env + 1.f;
        if (e < be) { be = e; best = i; }
    }
    Voice *v = &pool[best];
    memset(v, 0, sizeof(*v));
    v->active = 1;
    v->wave = (uint8_t)p->wave;
    v->delay = (int)(p->delay * RATE);
    v->inc = p->f0 / RATE;
    if (p->f1 > 0 && p->sweepT > 0) {
        v->sweep = (int)(p->sweepT * RATE);
        v->incMul = powf(p->f1 / p->f0, 1.f / (float)v->sweep);
    } else v->incMul = 1.f;
    v->det = p->det;
    v->atk = p->atk > 0.0005f ? 1.f / (p->atk * RATE) : 1.f;
    v->hold = (int)(p->hold * RATE);
    v->rel = powf(0.001f, 1.f / ((p->rel > 0.005f ? p->rel : 0.005f) * RATE));
    v->vol = p->vol;
    float pan = p->pan < -1 ? -1 : p->pan > 1 ? 1 : p->pan;
    v->panL = 0.5f * (1.f - pan) + 0.25f; v->panR = 0.5f * (1.f + pan) + 0.25f;
    v->lpc = p->lpc;
    v->lpcMul = 1.f;
    if (p->lpc > 0 && p->lpc1 > 0) {
        int n2 = v->sweep ? v->sweep : (int)((p->atk + p->hold + p->rel) * RATE);
        if (n2 < 1) n2 = 1;
        v->lpcMul = powf(p->lpc1 / p->lpc, 1.f / (float)n2);
    }
    v->vibInc = p->vibRate / RATE; v->vibDepth = p->vibDepth;
    v->fmRatio = p->fmRatio; v->fmIndex = p->fmIndex;
    v->fmDecay = p->fmIndex > 0 ? powf(0.05f, 1.f / ((p->hold + p->rel * 0.6f + 0.05f) * RATE)) : 1.f;
    v->nz = rnd() | 1;
}

static void play(int bus, NoteP p) {
    if (bus == BUS_MUS) start_voice(vm, NV_MUS, &p);
    else start_voice(vs, NV_SFX, &p);
}

static inline float osc(Voice *v, float ph) {
    float x = ph - (float)(int)ph;
    switch (v->wave) {
    case W_SINE: return tsin(ph);
    case W_TRI: return x < 0.5f ? 4.f * x - 1.f : 3.f - 4.f * x;
    case W_SQUARE: return x < 0.5f ? 0.7f : -0.7f;
    case W_SAW: return 2.f * x - 1.f;
    default:
        v->nz ^= v->nz << 13; v->nz ^= v->nz >> 17; v->nz ^= v->nz << 5;
        return (float)(int32_t)v->nz * (1.f / 2147483648.f);
    }
}

/* processa uma voz; acumula em L/R */
static inline void run_voice(Voice *v, float *L, float *R) {
    if (v->delay > 0) { v->delay--; return; }
    /* envelope */
    if (v->stage == 0) { v->env += v->atk; if (v->env >= 1.f) { v->env = 1.f; v->stage = 1; } }
    else if (v->stage == 1) { if (--v->hold <= 0) v->stage = 2; }
    else { v->env *= v->rel; if (v->env < 0.0008f) { v->active = 0; return; } }
    float inc = v->inc;
    if (v->vibDepth > 0) { inc *= 1.f + v->vibDepth * tsin(v->vibPh); v->vibPh += v->vibInc; }
    float ph = v->ph;
    if (v->fmIndex > 0.001f) {
        ph += v->fmIndex * tsin(v->fmPh) * 0.16f;
        v->fmPh += inc * v->fmRatio;
        if (v->fmPh > 1024.f) v->fmPh -= 1024.f;
        v->fmIndex *= v->fmDecay;
    }
    float s = osc(v, ph);
    if (v->det > 0) {
        s = 0.5f * (s + osc(v, v->ph2));
        v->ph2 += inc * v->det;
        if (v->ph2 > 1024.f) v->ph2 -= 1024.f;
    }
    v->ph += inc;
    if (v->ph > 1024.f) v->ph -= 1024.f;
    if (v->sweep > 0) { v->inc *= v->incMul; v->lpc *= v->lpcMul; v->sweep--; }
    else if (v->lpcMul != 1.f) { v->lpc *= v->lpcMul; if (v->lpc > 1.f) v->lpc = 1.f; }
    if (v->lpc > 0) { v->lp += v->lpc * (s - v->lp); s = v->lp; }
    s *= v->env * v->vol;
    *L += s * v->panL; *R += s * v->panR;
}

/* ------------------------------------------------------------------ */
/* musica                                                               */

static const int MINOR[3] = { 0, 3, 7 }, MAJOR[3] = { 0, 4, 7 };

typedef struct { int root; int minor; } Chord;

static int chord_note(Chord c, int k, int lo, int hi) {
    int n = c.root + (c.minor ? MINOR[k % 3] : MAJOR[k % 3]) + 12 * (k / 3);
    while (n < lo) n += 12;
    while (n > hi) n -= 12;
    return n;
}

static float bar_secs(int bpm) { return 240.f / bpm; }

/* --- "Colônia à Deriva" (partida) --- */
static const Chord GAME_A[4] = { {57, 1}, {53, 0}, {48, 0}, {55, 0} };
static const Chord GAME_B[4] = { {50, 1}, {57, 1}, {53, 0}, {52, 0} };
#define GAME_BPM 76

static void seq_game(int st) {
    int bar = (st / 16) % 16, s = st % 16;
    int sec = bar / 4;                  /* A A B A */
    int isB = sec == 2;
    Chord c = isB ? GAME_B[bar % 4] : GAME_A[bar % 4];
    float bs = bar_secs(GAME_BPM);
    if (s == 0) {
        for (int k = 0; k < 3; k++)
            play(BUS_MUS, (NoteP){ .wave = W_TRI, .f0 = midi[chord_note(c, k, 57, 69)], .atk = 1.1f, .hold = bs * 0.85f,
                                   .rel = 1.8f, .vol = 0.050f, .pan = -0.6f + 0.6f * k, .lpc = 0.10f, .det = 1.004f });
        play(BUS_MUS, (NoteP){ .wave = W_SINE, .f0 = midi[chord_note(c, 0, 45, 56)], .atk = 1.4f, .hold = bs * 0.8f,
                               .rel = 1.5f, .vol = 0.050f });
        sparkleStep = (int)(rnd() % 16);
        if (bar % 4 == 0)   /* vento solar */
            play(BUS_MUS, (NoteP){ .wave = W_NOISE, .f0 = 100, .atk = 2.5f, .hold = bs * 2.f, .rel = 3.f,
                                   .vol = 0.06f, .pan = frnd() * 1.6f - 0.8f, .lpc = 0.012f, .lpc1 = 0.05f });
    }
    /* baixo */
    if ((!isB && (s == 0 || s == 10)) || (isB && (s == 0 || s == 6 || s == 12)))
        play(BUS_MUS, (NoteP){ .wave = W_TRI, .f0 = midi[chord_note(c, 0, 36, 47)], .atk = 0.005f, .hold = 0.08f,
                               .rel = 0.6f, .vol = 0.16f, .lpc = 0.22f });
    /* arpejo com eco */
    static const int UPDN[8] = { 0, 1, 2, 3, 4, 3, 2, 1 };
    if ((!isB && s % 2 == 0) || (isB && s % 3 == 0)) {
        int k = UPDN[(isB ? s / 3 : s / 2) % 8];
        play(BUS_MUS, (NoteP){ .wave = W_TRI, .f0 = midi[chord_note(c, k, 64, 88)], .atk = 0.003f, .hold = 0.02f,
                               .rel = 0.30f, .vol = 0.040f, .pan = (s & 2) ? 0.6f : -0.6f, .lpc = 0.35f });
    }
    /* estrelas (sino FM) */
    if (s == sparkleStep)
        play(BUS_MUS, (NoteP){ .wave = W_SINE, .f0 = midi[chord_note(c, (int)(rnd() % 3), 81, 93)], .atk = 0.002f,
                               .hold = 0.01f, .rel = 1.9f, .vol = 0.030f, .pan = frnd() * 2 - 1, .fmRatio = 3.5f, .fmIndex = 1.6f });
    /* percussao suave */
    if (s == 4 || s == 12 || (isB && s == 14))
        play(BUS_MUS, (NoteP){ .wave = W_NOISE, .f0 = 100, .atk = 0.001f, .hold = 0.004f, .rel = 0.05f,
                               .vol = s == 14 ? 0.018f : 0.03f, .pan = 0.3f, .lpc = 0.85f });
    if (isB && (s == 0 || s == 8))
        play(BUS_MUS, (NoteP){ .wave = W_SINE, .f0 = 110, .f1 = 42, .sweepT = 0.18f, .atk = 0.001f, .hold = 0.02f,
                               .rel = 0.25f, .vol = 0.30f });
}

/* --- "Horizonte Vermelho" (titulo) --- */
static const Chord TITLE_C[8] = { {48, 1}, {44, 0}, {51, 0}, {46, 0}, {48, 1}, {44, 0}, {41, 1}, {43, 0} };
static const int8_t LEAD[8][8] = {
    { 67, -1, 70, -1, 72, -1, 70, 67 },
    { 68, -1, 72, -1, 75, -1, 72, -1 },
    { 70, -1, 67, -1, 63, -1, 67, 70 },
    { 74, -1, -1, -1, 72, -1, 70, -1 },
    { 67, -1, 70, -1, 72, -1, 75, -1 },
    { 77, -1, 75, -1, 72, -1, 68, -1 },
    { 72, -1, 77, -1, 75, 72, 70, -1 },
    { 71, -1, -1, -1, 74, -1, 79, -1 },
};
#define TITLE_BPM 104

static void drums(int s, int full) {
    if (s == 0 || s == 8 || (full && s == 10))
        play(BUS_MUS, (NoteP){ .wave = W_SINE, .f0 = 130, .f1 = 45, .sweepT = 0.16f, .atk = 0.001f, .hold = 0.02f,
                               .rel = 0.22f, .vol = 0.34f });
    if (s == 4 || s == 12) {
        play(BUS_MUS, (NoteP){ .wave = W_NOISE, .f0 = 100, .atk = 0.001f, .hold = 0.01f, .rel = 0.14f,
                               .vol = 0.10f, .lpc = 0.45f });
        play(BUS_MUS, (NoteP){ .wave = W_SINE, .f0 = 220, .f1 = 150, .sweepT = 0.08f, .atk = 0.001f, .hold = 0.01f,
                               .rel = 0.08f, .vol = 0.08f });
    }
    if (s % 4 == 2)
        play(BUS_MUS, (NoteP){ .wave = W_NOISE, .f0 = 100, .atk = 0.001f, .hold = 0.003f, .rel = 0.035f,
                               .vol = 0.03f, .pan = -0.3f, .lpc = 0.95f });
}

static void seq_title(int st) {
    int bar = (st / 16) % 16, s = st % 16;
    Chord c = TITLE_C[bar % 8];
    float bs = bar_secs(TITLE_BPM), sixteenth = bs / 16.f;
    int withLead = bar >= 8;
    if (s == 0)
        for (int k = 0; k < 3; k++)
            play(BUS_MUS, (NoteP){ .wave = W_SAW, .f0 = midi[chord_note(c, k, 55, 67)], .atk = 0.25f, .hold = bs * 0.9f,
                                   .rel = 0.6f, .vol = 0.032f, .pan = -0.5f + 0.5f * k, .lpc = 0.07f, .det = 1.006f });
    if (s % 2 == 0)
        play(BUS_MUS, (NoteP){ .wave = W_SAW, .f0 = midi[chord_note(c, 0, 36, 47)], .atk = 0.002f, .hold = 0.07f,
                               .rel = 0.12f, .vol = s == 0 ? 0.13f : 0.09f, .lpc = 0.13f });
    static const int ARP[4] = { 0, 1, 2, 3 };
    play(BUS_MUS, (NoteP){ .wave = W_SQUARE, .f0 = midi[chord_note(c, ARP[s % 4] + (s >= 8 ? 1 : 0), 67, 84)],
                           .atk = 0.002f, .hold = 0.02f, .rel = 0.09f, .vol = 0.020f, .pan = (s & 1) ? 0.55f : -0.55f, .lpc = 0.22f });
    drums(s, withLead);
    if (withLead && s % 2 == 0) {
        int slot = s / 2, n = LEAD[bar % 8][slot];
        if (n > 0) {
            int len = 1;
            while (slot + len < 8 && LEAD[bar % 8][slot + len] == -1) len++;
            play(BUS_MUS, (NoteP){ .wave = W_TRI, .f0 = midi[n], .atk = 0.02f, .hold = len * 2 * sixteenth * 0.85f,
                                   .rel = 0.25f, .vol = 0.075f, .lpc = 0.45f, .det = 1.003f, .vibRate = 5.5f, .vibDepth = 0.006f });
            play(BUS_MUS, (NoteP){ .wave = W_SINE, .f0 = midi[n + 12], .atk = 0.02f, .hold = len * 2 * sixteenth * 0.8f,
                                   .rel = 0.2f, .vol = 0.020f });
        }
    }
}

/* --- fanfarra de vitoria (uma vez, depois tema do titulo) --- */
#define VICTORY_BPM 120
static void brass(int root, int minor, float hold, float vol) {
    Chord c = { root, minor };
    for (int k = 0; k < 4; k++)
        play(BUS_MUS, (NoteP){ .wave = W_SAW, .f0 = midi[chord_note(c, k, 60, 76)], .atk = 0.04f, .hold = hold,
                               .rel = 1.0f, .vol = vol, .pan = -0.6f + 0.4f * k, .lpc = 0.10f, .lpc1 = 0.35f, .det = 1.005f });
}

static void seq_victory(int st) {
    int bar = st / 16, s = st % 16;
    if (bar == 0) {
        static const int UP[4] = { 72, 76, 79, 84 };
        if (s % 2 == 0 && s < 8) {
            play(BUS_MUS, (NoteP){ .wave = W_SQUARE, .f0 = midi[UP[s / 2]], .atk = 0.005f, .hold = 0.09f, .rel = 0.2f,
                                   .vol = 0.06f, .lpc = 0.35f });
            play(BUS_MUS, (NoteP){ .wave = W_SINE, .f0 = midi[UP[s / 2] + 12], .atk = 0.002f, .hold = 0.01f, .rel = 0.8f,
                                   .vol = 0.03f, .fmRatio = 3.5f, .fmIndex = 1.2f });
        }
        if (s == 8) { brass(48, 0, 0.5f, 0.045f); drums(0, 0); }
        if (s == 12) { brass(55, 0, 0.3f, 0.04f); drums(8, 0); }
    } else if (bar == 1) {
        if (s == 0) {
            brass(48, 0, 1.6f, 0.055f);
            play(BUS_MUS, (NoteP){ .wave = W_SINE, .f0 = 90, .f1 = 40, .sweepT = 0.5f, .atk = 0.002f, .hold = 0.1f,
                                   .rel = 1.2f, .vol = 0.35f });
            play(BUS_MUS, (NoteP){ .wave = W_NOISE, .f0 = 100, .atk = 0.002f, .hold = 0.2f, .rel = 1.5f, .vol = 0.08f,
                                   .lpc = 0.6f, .lpc1 = 0.05f });
        }
        static const int SPARK[8] = { 84, 88, 91, 96, 91, 96, 100, 103 };
        if (s % 2 == 0)
            play(BUS_MUS, (NoteP){ .wave = W_SINE, .f0 = midi[SPARK[s / 2]], .atk = 0.002f, .hold = 0.01f, .rel = 0.7f,
                                   .vol = 0.028f, .pan = (s & 2) ? 0.7f : -0.7f, .fmRatio = 2.0f, .fmIndex = 1.0f });
    } else if (bar >= 3) {
        nextTrack = TRACK_TITLE;     /* emenda com o tema do titulo */
        curTrack = TRACK_TITLE; step = -1; fade = 0.f; fadingOut = 0;
    }
}

static int track_bpm(int t) {
    return t == TRACK_TITLE ? TITLE_BPM : t == TRACK_GAME ? GAME_BPM : VICTORY_BPM;
}

static void seq_step(void) {
    switch (curTrack) {
    case TRACK_TITLE: seq_title(step); break;
    case TRACK_GAME: seq_game(step); break;
    case TRACK_VICTORY: seq_victory(step); break;
    default: return;
    }
    step++;
    stepSamples = (int)(RATE * 60.f / track_bpm(curTrack) / 4.f);
}

/* ------------------------------------------------------------------ */
/* efeitos                                                              */

void synth_sfx(int id) {
    switch (id) {
    case SFX_MOVE:
        play(BUS_SFX, (NoteP){ .wave = W_SINE, .f0 = 1320, .f1 = 1560, .sweepT = 0.03f, .atk = 0.001f, .hold = 0.02f,
                               .rel = 0.03f, .vol = 0.13f });
        break;
    case SFX_OK:
        play(BUS_SFX, (NoteP){ .wave = W_TRI, .f0 = 880, .atk = 0.002f, .hold = 0.04f, .rel = 0.06f, .vol = 0.2f });
        play(BUS_SFX, (NoteP){ .wave = W_TRI, .f0 = 1320, .atk = 0.002f, .hold = 0.05f, .rel = 0.15f, .vol = 0.2f,
                               .delay = 0.06f, .fmRatio = 2.f, .fmIndex = 0.5f });
        break;
    case SFX_BACK:
        play(BUS_SFX, (NoteP){ .wave = W_TRI, .f0 = 1100, .atk = 0.002f, .hold = 0.04f, .rel = 0.05f, .vol = 0.18f });
        play(BUS_SFX, (NoteP){ .wave = W_TRI, .f0 = 620, .atk = 0.002f, .hold = 0.05f, .rel = 0.12f, .vol = 0.18f, .delay = 0.06f });
        break;
    case SFX_CARD:
        play(BUS_SFX, (NoteP){ .wave = W_NOISE, .f0 = 100, .sweepT = 0.15f, .f1 = 100, .atk = 0.02f, .hold = 0.05f,
                               .rel = 0.08f, .vol = 0.22f, .lpc = 0.05f, .lpc1 = 0.6f });
        play(BUS_SFX, (NoteP){ .wave = W_SINE, .f0 = 600, .f1 = 2400, .sweepT = 0.08f, .atk = 0.002f, .hold = 0.05f,
                               .rel = 0.05f, .vol = 0.08f, .delay = 0.04f });
        break;
    case SFX_LAND:
        play(BUS_SFX, (NoteP){ .wave = W_NOISE, .f0 = 100, .atk = 0.01f, .hold = 0.12f, .rel = 0.35f, .vol = 0.30f,
                               .lpc = 0.25f, .lpc1 = 0.02f });
        play(BUS_SFX, (NoteP){ .wave = W_SINE, .f0 = 170, .f1 = 50, .sweepT = 0.25f, .atk = 0.002f, .hold = 0.05f,
                               .rel = 0.25f, .vol = 0.30f, .delay = 0.10f });
        play(BUS_SFX, (NoteP){ .wave = W_SINE, .f0 = 1760, .atk = 0.002f, .hold = 0.02f, .rel = 0.35f, .vol = 0.10f,
                               .delay = 0.22f, .fmRatio = 3.f, .fmIndex = 0.8f });
        break;
    case SFX_LAUNCH:
        play(BUS_SFX, (NoteP){ .wave = W_NOISE, .f0 = 100, .sweepT = 0.7f, .f1 = 100, .atk = 0.08f, .hold = 0.45f,
                               .rel = 0.35f, .vol = 0.30f, .lpc = 0.02f, .lpc1 = 0.35f });
        play(BUS_SFX, (NoteP){ .wave = W_SAW, .f0 = 70, .f1 = 420, .sweepT = 0.7f, .atk = 0.05f, .hold = 0.5f,
                               .rel = 0.25f, .vol = 0.08f, .lpc = 0.08f });
        break;
    case SFX_ENERGY:
        play(BUS_SFX, (NoteP){ .wave = W_SQUARE, .f0 = 300, .f1 = 1800, .sweepT = 0.2f, .atk = 0.002f, .hold = 0.16f,
                               .rel = 0.08f, .vol = 0.18f, .lpc = 0.3f });
        play(BUS_SFX, (NoteP){ .wave = W_SINE, .f0 = 2400, .atk = 0.002f, .hold = 0.01f, .rel = 0.4f, .vol = 0.08f,
                               .delay = 0.18f, .fmRatio = 2.5f, .fmIndex = 1.0f });
        break;
    case SFX_ERROR:
        play(BUS_SFX, (NoteP){ .wave = W_SQUARE, .f0 = 140, .atk = 0.002f, .hold = 0.16f, .rel = 0.05f, .vol = 0.16f,
                               .lpc = 0.3f, .vibRate = 30.f, .vibDepth = 0.06f });
        break;
    case SFX_ALERT:
        play(BUS_SFX, (NoteP){ .wave = W_SINE, .f0 = 1046.5f, .atk = 0.003f, .hold = 0.03f, .rel = 0.9f, .vol = 0.16f,
                               .fmRatio = 1.5f, .fmIndex = 0.6f });
        play(BUS_SFX, (NoteP){ .wave = W_SINE, .f0 = 1046.5f, .atk = 0.003f, .hold = 0.02f, .rel = 0.6f, .vol = 0.06f,
                               .delay = 0.32f });
        break;
    case SFX_REVEAL: {
        static const float N[4] = { 1318.5f, 1568.f, 1975.5f, 2637.f };
        for (int i = 0; i < 4; i++)
            play(BUS_SFX, (NoteP){ .wave = W_SINE, .f0 = N[i], .atk = 0.002f, .hold = 0.01f, .rel = 0.5f, .vol = 0.08f,
                                   .pan = -0.6f + 0.4f * i, .delay = 0.045f * i, .fmRatio = 3.5f, .fmIndex = 1.2f });
        break; }
    case SFX_PROMOTE: {
        static const float N[4] = { 1046.5f, 1318.5f, 1568.f, 2093.f };
        for (int i = 0; i < 4; i++)
            play(BUS_SFX, (NoteP){ .wave = W_TRI, .f0 = N[i], .atk = 0.002f, .hold = 0.03f, .rel = 0.25f, .vol = 0.12f,
                                   .delay = 0.06f * i });
        play(BUS_SFX, (NoteP){ .wave = W_SINE, .f0 = 3136.f, .atk = 0.002f, .hold = 0.01f, .rel = 0.8f, .vol = 0.05f,
                               .delay = 0.26f, .fmRatio = 3.f, .fmIndex = 1.f });
        break; }
    case SFX_DRAW:
        for (int i = 0; i < 3; i++)
            play(BUS_SFX, (NoteP){ .wave = W_SQUARE, .f0 = 1000.f + (rnd() % 800), .atk = 0.001f, .hold = 0.025f,
                                   .rel = 0.02f, .vol = 0.14f, .lpc = 0.4f, .delay = 0.05f * i, .pan = (i - 1) * 0.5f });
        break;
    case SFX_WIN:
        synth_music(TRACK_VICTORY);
        break;
    }
}

/* ------------------------------------------------------------------ */

void synth_init(int rate) {
    RATE = (float)(rate > 8000 ? rate : 22050);
    for (int i = 0; i <= TAB; i++) sintab[i] = sinf(6.28318531f * i / TAB);
    for (int n = 0; n < 128; n++) midi[n] = 440.f * powf(2.f, (n - 69) / 12.f);
    memset(vm, 0, sizeof vm); memset(vs, 0, sizeof vs);
    memset(echoL, 0, sizeof echoL); memset(echoR, 0, sizeof echoR);
    echoDL = (int)(0.36f * RATE); echoDR = (int)(0.27f * RATE);
    if (echoDL >= ECHO_MAX) echoDL = ECHO_MAX - 1;
    if (echoDR >= ECHO_MAX) echoDR = ECHO_MAX - 1;
    echoPos = 0; echoDampL = echoDampR = 0;
    curTrack = nextTrack = TRACK_NONE; fade = 0; fadingOut = 0;
}

void synth_music(int track) {
    if (track == nextTrack && (track == curTrack || fadingOut)) return;
    nextTrack = track;
    if (curTrack == TRACK_NONE) { curTrack = track; step = 0; stepLeft = 0; fade = 0.f; fadingOut = 0; }
    else fadingOut = 1;
}

int synth_track(void) { return nextTrack; }

void synth_volume(int music, int sfx) {
    gainMus = music < 0 ? 0 : music > 10 ? 1.f : music / 10.f;
    gainSfx = sfx < 0 ? 0 : sfx > 10 ? 1.f : sfx / 10.f;
}

void synth_render(int16_t *out, int frames) {
    const float fb = 0.40f, wet = 0.32f;
    float fadeStep = 1.f / (0.6f * RATE);
    for (int i = 0; i < frames; i++) {
        /* sequenciador */
        if (fadingOut) {
            fade -= fadeStep * 1.5f;
            if (fade <= 0.f) {
                fade = 0.f; fadingOut = 0;
                for (int k = 0; k < NV_MUS; k++) vm[k].active = 0;
                curTrack = nextTrack; step = 0; stepLeft = 0;
            }
        } else if (fade < 1.f && curTrack != TRACK_NONE) {
            fade += fadeStep; if (fade > 1.f) fade = 1.f;
        }
        if (curTrack != TRACK_NONE && !fadingOut) {
            if (--stepLeft <= 0) {
                if (step < 0) step = 0;
                seq_step();
                stepLeft = stepSamples;
            }
        }
        float mL = 0, mR = 0, sL = 0, sR = 0;
        for (int k = 0; k < NV_MUS; k++) if (vm[k].active) run_voice(&vm[k], &mL, &mR);
        for (int k = 0; k < NV_SFX; k++) if (vs[k].active) run_voice(&vs[k], &sL, &sR);
        float g = gainMus * fade;
        mL *= g; mR *= g; sL *= gainSfx; sR *= gainSfx;
        /* eco estereo ping-pong (com amortecimento) */
        int rl = echoPos - echoDL; if (rl < 0) rl += ECHO_MAX;
        int rr = echoPos - echoDR; if (rr < 0) rr += ECHO_MAX;
        float eL = echoL[rl], eR = echoR[rr];
        echoDampL += 0.45f * (eL - echoDampL);
        echoDampR += 0.45f * (eR - echoDampR);
        echoL[echoPos] = (mL * 0.55f + sL * 0.30f) + echoDampR * fb;
        echoR[echoPos] = (mR * 0.55f + sR * 0.30f) + echoDampL * fb;
        if (++echoPos >= ECHO_MAX) echoPos = 0;
        float L = mL + sL + wet * eL, Rr = mR + sR + wet * eR;
        L *= 3.0f; Rr *= 3.0f;   /* volume mestre */
        /* saturacao suave */
        L = L * (27.f + L * L) / (27.f + 9.f * L * L);
        Rr = Rr * (27.f + Rr * Rr) / (27.f + 9.f * Rr * Rr);
        L = L > 1.f ? 1.f : L < -1.f ? -1.f : L;
        Rr = Rr > 1.f ? 1.f : Rr < -1.f ? -1.f : Rr;
        out[2 * i] = (int16_t)(L * 30000.f);
        out[2 * i + 1] = (int16_t)(Rr * 30000.f);
    }
}
