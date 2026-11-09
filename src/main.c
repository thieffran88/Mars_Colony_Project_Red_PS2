/*
 * Mars Colony: Project Red PS2
 * Interface (SDL2): roda no PlayStation 2 (PS2SDK + SDL2) e no PC (para testes).
 */
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "game.h"
#include "ai.h"
#include "render.h"
#include "audio.h"

/* ------------------------------------------------------------------ */
/* entrada                                                              */

enum { B_UP, B_DOWN, B_LEFT, B_RIGHT, B_OK, B_BACK, B_INFO, B_START, B_SELECT, B_L1, B_R1, NB };
/* Entrada por fonte: 0 = controle 1 (+ teclado no PC), 1 = controle 2.
 * Na partida, o controle 1 comanda so o jogador 1 e o controle 2 comanda os
 * jogadores 2 a 4. 'pressed' (uniao das duas fontes) serve aos menus. */
#define NSRC 2
static int held[NSRC][NB], pressedSrc[NSRC][NB], pressed[NB];
static Uint32 repeatAt[NSRC][NB];
static SDL_JoystickID padId[NSRC] = { -1, -1 };
static int quitReq;

static void press(int src, int b, int down) {
    if (b < 0 || src < 0 || src >= NSRC) return;
    if (down && !held[src][b]) { pressedSrc[src][b] = 1; repeatAt[src][b] = SDL_GetTicks() + 320; }
    held[src][b] = down;
}

static int pad_slot(SDL_JoystickID id) {
    for (int i = 0; i < NSRC; i++) if (padId[i] == id) return i;
    return -1;
}

/* abre o controle do indice 'dev' e o associa a uma porta (1 ou 2) */
static void open_pad(int dev) {
    if (!SDL_IsGameController(dev)) return;
    SDL_GameController *gc = SDL_GameControllerOpen(dev);
    if (!gc) return;
    SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gc));
    if (pad_slot(id) >= 0) return;
    int want = SDL_GameControllerGetPlayerIndex(gc);
    if (want < 0 || want >= NSRC) want = dev;
    if (want >= 0 && want < NSRC && padId[want] < 0) { padId[want] = id; return; }
    for (int i = 0; i < NSRC; i++) if (padId[i] < 0) { padId[i] = id; return; }
}

static int pad2_connected(void) { return padId[1] >= 0; }

/* controle que comanda o jogador p (sem controle 2, o controle 1 comanda todos) */
static int src_for_player(int p) { return (p == 0 || !pad2_connected()) ? 0 : 1; }

static int key_button(SDL_Keycode k) {
    switch (k) {
    case SDLK_UP: return B_UP;       case SDLK_DOWN: return B_DOWN;
    case SDLK_LEFT: return B_LEFT;   case SDLK_RIGHT: return B_RIGHT;
    case SDLK_RETURN: case SDLK_z: case SDLK_SPACE: return B_OK;
    case SDLK_ESCAPE: case SDLK_x: case SDLK_BACKSPACE: return B_BACK;
    case SDLK_TAB: case SDLK_i: return B_INFO;
    case SDLK_p: case SDLK_F1: return B_START;
    case SDLK_r: case SDLK_c: return B_SELECT;
    case SDLK_q: return B_L1;        case SDLK_e: return B_R1;
    }
    return -1;
}

static int pad_button(int b) {
    switch (b) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return B_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return B_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return B_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return B_RIGHT;
    case SDL_CONTROLLER_BUTTON_A: return B_OK;      /* X (cruz)   */
    case SDL_CONTROLLER_BUTTON_B: return B_BACK;    /* circulo    */
    case SDL_CONTROLLER_BUTTON_Y: return B_INFO;    /* triangulo  */
    case SDL_CONTROLLER_BUTTON_X: return B_SELECT;  /* quadrado   */
    case SDL_CONTROLLER_BUTTON_START: return B_START;
    case SDL_CONTROLLER_BUTTON_BACK: return B_SELECT;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return B_L1;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return B_R1;
    }
    return -1;
}

static void poll_input(void) {
    memset(pressed, 0, sizeof(pressed));
    memset(pressedSrc, 0, sizeof(pressedSrc));
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT: quitReq = 1; break;
        case SDL_KEYDOWN: if (!e.key.repeat) press(0, key_button(e.key.keysym.sym), 1); break;
        case SDL_KEYUP: press(0, key_button(e.key.keysym.sym), 0); break;
        case SDL_CONTROLLERDEVICEADDED: open_pad(e.cdevice.which); break;
        case SDL_CONTROLLERDEVICEREMOVED: {
            int sl = pad_slot(e.cdevice.which);
            if (sl >= 0) { padId[sl] = -1; memset(held[sl], 0, sizeof(held[sl])); }
            break; }
        case SDL_CONTROLLERBUTTONDOWN: press(pad_slot(e.cbutton.which), pad_button(e.cbutton.button), 1); break;
        case SDL_CONTROLLERBUTTONUP: press(pad_slot(e.cbutton.which), pad_button(e.cbutton.button), 0); break;
        case SDL_CONTROLLERAXISMOTION: {
            int v = e.caxis.value, sl = pad_slot(e.caxis.which);
            if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                press(sl, B_UP, v < -20000); press(sl, B_DOWN, v > 20000);
            } else if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
                press(sl, B_LEFT, v < -20000); press(sl, B_RIGHT, v > 20000);
            }
            break; }
        }
    }
    Uint32 now = SDL_GetTicks();
    for (int sv = 0; sv < NSRC; sv++) {
        for (int b = B_UP; b <= B_RIGHT; b++)
            if (held[sv][b] && now >= repeatAt[sv][b]) { pressedSrc[sv][b] = 1; repeatAt[sv][b] = now + 90; }
        for (int b = 0; b < NB; b++) pressed[b] |= pressedSrc[sv][b];
    }
}

/* ------------------------------------------------------------------ */
/* estado da interface                                                  */

enum { SC_TITLE, SC_SETUP, SC_GAME, SC_OVER, SC_RULES, SC_CREDITS, SC_LANG };
static int scene = SC_LANG, rulesReturn = SC_TITLE;
static int langSel = 0;
static Uint32 frame;

/* configuracao */
static int cfgNp = 2;
static int cfgSlot[MAXP] = { 0, 2, 2, 2 };   /* 0 humano, 1 facil, 2 normal, 3 dificil */
static int cfgDraft = 0, cfgSpeed = 0;
static int cfgMusic = 6, cfgSfx = 8;     /* volumes 0..10 */
static int setupSel = 0, titleSel = 0;
static const char *SLOT_NAMES[LANG_COUNT][4] = {
    { "Humano", "CPU Fácil", "CPU Normal", "CPU Difícil" },
    { "Human", "CPU Easy", "CPU Normal", "CPU Hard" },
    { "Humano", "CPU Fácil", "CPU Normal", "CPU Difícil" },
};
static const char *SPEED_NAMES[LANG_COUNT][3] = {
    { "Normal", "Rápida", "Instantânea" },
    { "Normal", "Fast", "Instant" },
    { "Normal", "Rápida", "Instantánea" },
};
#define SLOT_NAME SLOT_NAMES[g_lang]
#define SPEED_NAME SPEED_NAMES[g_lang]

/* jogo */
static Game G;
static Game undoSnap;
static int undoOk, undoOwner = -1;
static Option opts[MAX_OPTS];
static int nopts, optSel, optScroll, optsDirty = 1;
static int viewer = -1, curtain = 0;
static Uint32 cpuReadyAt;
static int paused, pauseSel;
static int inspect, inspectSel;
static uint32_t aiSeed = 1234567u;
static Uint32 overAt;
static int overSel;
static int rulesPage;
static int flashBld = -1; static Uint32 flashUntil;
static int autoplayHuman; /* testes: CPU joga pelo humano */
static int zoomCard = -1, popupCard = -1, lastCard = -1;
static Uint32 popupUntil;
static char popupText[64];

static int num_humans(void) {
    int n = 0;
    for (int i = 0; i < G.np; i++) n += G.p[i].human;
    return n;
}

static void start_game(void) {
    int human[MAXP], lvl[MAXP];
    for (int i = 0; i < MAXP; i++) {
        human[i] = cfgSlot[i] == 0;
        lvl[i] = cfgSlot[i] == 0 ? 1 : cfgSlot[i] - 1;
    }
    uint32_t seed = (uint32_t)SDL_GetTicks() * 2654435761u ^ (uint32_t)(frame * 40503u) ^ 0x5EEDu;
    game_init(&G, cfgNp, human, lvl, cfgDraft, seed);
    aiSeed = seed ^ 0xABCDEFu;
    optsDirty = 1; optSel = 0; optScroll = 0;
    viewer = -1;
    for (int i = 0; i < G.np; i++) if (G.p[i].human) { viewer = i; break; }
    curtain = 0; paused = 0; inspect = 0; undoOk = 0;
    cpuReadyAt = SDL_GetTicks() + 600;
    scene = SC_GAME;
}

/* ------------------------------------------------------------------ */
/* desenho: fundo                                                       */

#define NSTARS 90
static int16_t starX[NSTARS], starY[NSTARS]; static uint8_t starB[NSTARS];

static void init_stars(void) {
    uint32_t s = 99;
    for (int i = 0; i < NSTARS; i++) {
        s = s * 1103515245u + 12345u; starX[i] = (int16_t)((s >> 8) % SCREEN_W);
        s = s * 1103515245u + 12345u; starY[i] = (int16_t)((s >> 8) % SCREEN_H);
        s = s * 1103515245u + 12345u; starB[i] = (uint8_t)(80 + (s >> 8) % 170);
    }
}

static void draw_space(void) {
    r_vgrad(0, 0, SCREEN_W, SCREEN_H, RGBA(8, 6, 18, 255), RGBA(48, 14, 16, 255));
    for (int i = 0; i < NSTARS; i++) {
        int b = starB[i] + (int)(40 * sinf((frame + i * 37) * 0.05f));
        if (b > 255) b = 255; if (b < 30) b = 30;
        r_rect(starX[i], starY[i], (i % 7 == 0) ? 2 : 1, (i % 7 == 0) ? 2 : 1, RGBA(255, 240, 230, b));
    }
}

static void panel(int x, int y, int w, int h, Color border) {
    r_rrect(x, y, w, h, RGBA(18, 16, 26, 225));
    r_frame(x, y, w, h, border);
}

/* ------------------------------------------------------------------ */
/* desenho: cartas                                                      */

static void draw_card_mini(int x, int y, int w, int h, int card, int hl, int dim) {
    const CardDef *cd = &CARDS[card];
    Color base = COL_BLD[cd->color];
    if (hl) { r_rrect(x - 3, y - 3, w + 6, h + 6, RGBA(255, 255, 255, 120 + (int)(100 * sinf(frame * 0.2f)))); }
    r_rrect(x, y, w, h, RGBA(15, 14, 20, 255));
    r_rrect(x + 1, y + 1, w - 2, h - 2, c_mul(base, dim ? 0.45f : 0.95f));
    char v[4]; snprintf(v, sizeof v, "%d", cd->value);
    int hh = h >= 60 ? 16 : 14;
    r_text(FONT_MED, x + 4, y + 1, v, WHITE, 0);
    if (w >= 40) r_text(FONT_MED, x + w - 4 - r_text_w(FONT_MED, v), y + 1, v, RGBA(255, 255, 255, 170), 0);
    else if (w >= 26) r_icon(IC_LEAF + cd->color, x + w - 14, y + 2, 12, RGBA(255, 255, 255, 220));
    int ay = y + hh;
    if (w >= 50 && h >= 70) {
        int th = r_text_wrap(FONT_SMALL, x + 4, y + hh, w - 8, card_name(card), RGBA(255, 255, 255, dim ? 120 : 255), 11);
        ay = y + hh + (th > 22 ? 22 : th) + 1;
    }
    int ah = y + h - 3 - ay;
    r_rrect(x + 3, ay, w - 6, ah, RGBA(250, 250, 252, dim ? 120 : 235));
    r_art_fit(&ART_CARD[card], x + 4, ay + 1, w - 8, ah - 2, dim ? RGBA(255, 255, 255, 110) : WHITE);
}

/* carta completa, no estilo das cartas impressas (w ~ 200 => h = 1.48 w) */
static void draw_card_full(int x, int y, int w, int card) {
    const CardDef *cd = &CARDS[card];
    Color base = COL_BLD[cd->color];
    float k = w / 200.0f;
    int h = (int)(w * 1.48f);
#define K(v) ((int)((v) * k + 0.5f))
    r_rect(x + K(6), y + K(6), w, h, RGBA(0, 0, 0, 110));
    r_rrect(x, y, w, h, RGBA(22, 22, 30, 255));
    r_rrect(x + K(4), y + K(4), w - K(8), h - K(8), RGBA(246, 246, 249, 255));
    /* cabecalho */
    r_rrect(x + K(9), y + K(9), w - K(18), K(26), base);
    r_rect(x + K(14), y + K(11), w - K(70), K(2), RGBA(255, 255, 255, 120));
    r_icon(IC_LEAF + cd->color, x + K(14), y + K(13), K(18), WHITE);
    {   /* reduz a fonte se o nome do tipo nao couber */
        float ts = k;
        r_text_scale(ts);
        while (r_text_w(FONT_MED, bld_name(cd->color)) > w - K(82) && ts > k * 0.6f) { ts -= 0.05f; r_text_scale(ts); }
        r_text(FONT_MED, x + K(36), y + K(13) + (int)((k - ts) * 8), bld_name(cd->color), WHITE, w - K(80));
    }
    r_rrect(x + w - K(40), y + K(11), K(26), K(22), RGBA(22, 22, 30, 255));
    char v[8]; snprintf(v, sizeof v, "%d", cd->value);
    r_text_scale(k * 1.25f);
    r_text_center(FONT_MED, x + w - K(27), y + K(12), v, WHITE);
    /* nome */
    {
        float ts = k;
        r_text_scale(ts);
        while (r_text_w(FONT_MED, card_name(card)) > w - K(20) && ts > k * 0.55f) { ts -= 0.05f; r_text_scale(ts); }
        r_text_center(FONT_MED, x + w / 2, y + K(42) + (int)((k - ts) * 8), card_name(card), RGBA(25, 25, 35, 255));
    }
    r_text_scale(k);
    r_rect(x + K(16), y + K(61), w - K(32), K(3) > 0 ? K(3) : 1, base);
    /* janela da ilustracao */
    int wx = x + K(12), wy = y + K(68), ww = w - K(24), wh = K(86);
    r_rrect(wx, wy, ww, wh, base);
    r_rrect(wx + K(3), wy + K(3), ww - K(6), wh - K(6), RGBA(255, 255, 255, 255));
    r_art_fit(&ART_CARD[card], wx + K(8), wy + K(14), ww - K(16), wh - K(26), WHITE);
    char tag[16]; snprintf(tag, sizeof tag, "#%02d", card + 1);
    r_rrect(wx + K(6), wy + K(6), K(30), K(14), RGBA(30, 30, 40, 255));
    r_text_scale(k * 0.85f);
    r_text(FONT_SMALL, wx + K(10), wy + K(7), tag, WHITE, 0);
    for (int dx = K(8); dx < ww - K(12); dx += K(12)) r_rect(wx + dx, wy + wh - K(9), K(6), K(2) > 0 ? K(2) : 1, base);
    /* acoes */
    for (int i = 0; i < 2; i++) {
        int by = y + K(159) + i * K(62), bh = K(58);
        r_rrect(x + K(10), by, w - K(20), bh, RGBA(30, 30, 40, 255));
        r_rect(x + K(12), by + K(18), w - K(24), bh - K(20), RGBA(250, 250, 252, 255));
        r_text_scale(k * 0.9f);
        r_text(FONT_MED, x + K(16), by + K(2), i == 0 ? TR("AÇÃO DA MÃO", "HAND ACTION", "ACCIÓN DE MANO") : TR("AÇÃO DO MÓDULO", "MODULE ACTION", "ACCIÓN DE MÓDULO"), WHITE, 0);
        r_rrect(x + w - K(28), by + K(4), K(11), K(11), c_mul(base, 1.2f));
        char buf[200];
        game_fx_text(i == 0 ? &cd->top : &cd->bottom, buf, sizeof buf);
        r_text_scale(k);
        r_text_wrap(FONT_SMALL, x + K(15), by + K(21), w - K(30), buf, RGBA(30, 30, 40, 255), K(13));
    }
    r_text_scale(k * 0.7f);
    r_text(FONT_SMALL, x + K(12), y + h - K(14), "MARS COLONY · PROJECT RED", RGBA(110, 110, 120, 255), 0);
    r_text_scale(1.0f);
#undef K
}

static void draw_card_back(int x, int y, int w, int h, int color, int hl) {
    if (hl) r_rrect(x - 2, y - 2, w + 4, h + 4, RGBA(255, 255, 255, 130 + (int)(100 * sinf(frame * 0.2f))));
    r_rrect(x, y, w, h, c_mul(COL_BLD[color], 0.7f));
    r_frame(x + 1, y + 1, w - 2, h - 2, RGBA(255, 255, 255, 60));
    int is = w - 4 < 16 ? w - 4 : 16;
    r_icon(IC_LEAF + color, x + (w - is) / 2, y + (h - is) / 2, is, RGBA(255, 255, 255, 220));
}

/* ------------------------------------------------------------------ */
/* desenho: tabuleiro                                                   */

static const char *ability_short(int b) {
    const char *a[5] = { TR("Mova {C} {S}<>{D}", "Move {C} {S}<>{D}", "Mueve {C} {S}<>{D}"), TR("Ganhe 1 {E}", "Gain 1 {E}", "Gana 1 {E}"), TR("1 {C} da Terra", "1 {C} from Earth", "1 {C} de la Tierra"), TR("Carta ao fundo", "Card to bottom", "Carta al fondo"), TR("{C} nave > {S}", "{C} ship > {S}", "{C} nave > {S}") };
    return a[b];
}

static int option_building(const Option *o) {
    switch (o->kind) {
    case OPT_SHIP_TO: case OPT_PROMOTE: case OPT_ECO_MOVE: case OPT_SWITCH: case OPT_DEMOTE: case OPT_MOVE_YES:
        return o->a;
    case OPT_PLAY_PREP: {
        int p = G.decider; return CARDS[G.p[p].prep[o->a]].color; }
    case OPT_PLAY_OPP: return CARDS[G.p[o->a].prep[o->b]].color;
    }
    return -1;
}

static void draw_building(int b, int x, int y, int w, int h, int hl) {
    Building *B = &G.b[b];
    Color bc = COL_BLD[b];
    int flash = (b == flashBld && SDL_GetTicks() < flashUntil);
    panel(x, y, w, h, hl ? RGBA(255, 255, 255, 255) : c_mul(bc, 1.2f));
    if (hl || flash) r_frame(x - 1, y - 1, w + 2, h + 2, RGBA(255, 255, 255, 120 + (int)(120 * sinf(frame * 0.25f))));
    r_rect(x + 1, y + 1, w - 2, 18, bc);
    r_icon(IC_LEAF + b, x + 3, y + 2, 16, RGBA(255, 255, 255, 255));
    r_text(FONT_SMALL, x + 21, y + 3, color_name(b), RGBA(255, 255, 255, 255), w - 44);
    /* valor visivel */
    r_rect(x + w - 20, y + 2, 18, 16, RGBA(0, 0, 0, 120));
    char v[4];
    if (b == C_CONSTR) snprintf(v, sizeof v, "-");
    else snprintf(v, sizeof v, "%d", game_top_value(&G, b));
    r_text_center(FONT_MED, x + w - 11, y + 1, v, RGBA(255, 255, 255, 255));

    int cy = y + 22;
    if (b != C_CONSTR) {
        int lw = w * 52 / 100;
        r_rect(x + 3, cy, lw - 4, 70, RGBA(255, 255, 255, 14));
        r_rect(x + lw + 1, cy, w - lw - 4, 70, RGBA(255, 220, 120, 22));
        r_art_fit(&ART_BLD[b], x + 4, cy + 30, lw - 6, 40, RGBA(255, 255, 255, 235));
        r_text(FONT_SMALL, x + 5, cy + 1, "{S} 2pt", RGBA(220, 220, 220, 255), 0);
        r_text(FONT_SMALL, x + lw + 3, cy + 1, "{D}4pt", RGBA(255, 225, 150, 255), 0);
        int row = 0;
        for (int p = 0; p < G.np; p++) {
            if (!B->star1[p]) continue;
            int ry = cy + 16 + row * 13;
            r_icon(IC_COLONIST, x + 6, ry, 13, COL_PLAYER[p]);
            char t[8]; snprintf(t, sizeof t, "x%d", B->star1[p]);
            r_text(FONT_SMALL, x + 21, ry, t, RGBA(255, 255, 255, 255), 0);
            row++;
        }
        int cap = game_star2_cap(&G), slot = 0;
        for (int p = 0; p < G.np; p++)
            for (int k = 0; k < B->star2[p]; k++) {
                int sx = x + lw + 6, sy = cy + 17 + slot * 25;
                r_rrect(sx, sy, w - lw - 12, 22, c_mul(COL_PLAYER[p], 0.45f));
                r_icon(IC_COLONIST, sx + (w - lw - 12) / 2 - 9, sy + 2, 18, COL_PLAYER[p]);
                slot++;
            }
        for (; slot < cap; slot++) {
            int sx = x + lw + 6, sy = cy + 17 + slot * 25;
            r_frame(sx, sy, w - lw - 12, 22, RGBA(255, 220, 120, 90));
        }
    } else {
        r_text_wrap(FONT_SMALL, x + 6, cy + 2, w - 12, TR("Não abriga colonos. Recebe cartas de construção.", "Holds no colonists. Takes construction cards.", "No aloja colonos. Recibe cartas de construcción."), RGBA(190, 190, 200, 255), 13);
        r_art_fit(&ART_BLD[b], x + w - 64, cy + 34, 60, 36, WHITE);
    }
    /* habilidade */
    int ay = y + 95;
    r_text(FONT_SMALL, x + 5, ay, ability_short(b), RGBA(255, 230, 170, 255), w - 10);
    /* pilha */
    int sy = y + 112;
    if (B->nstack == 0) {
        r_frame(x + 6, sy, 30, 42, RGBA(255, 255, 255, 50));
        r_text(FONT_SMALL, x + 42, sy + 6, b == C_CONSTR ? TR("Sem cartas", "No cards", "Sin cartas") : TR("Valor base 3", "Base value 3", "Valor base 3"), RGBA(170, 170, 180, 255), w - 46);
    } else {
        int top = B->stack[B->nstack - 1];
        draw_card_mini(x + 6, sy, 30, 42, top, 0, 0);
        char t[64];
        r_text(FONT_SMALL, x + 42, sy + 2, card_name(top), RGBA(255, 255, 255, 255), w - 46);
        int n = 0, off = snprintf(t, sizeof t, TR("Antes:", "Before:", "Antes:"));
        for (int i = B->nstack - 2; i >= 0 && n < 6; i--, n++) off += snprintf(t + off, sizeof t - off, " %d", CARDS[B->stack[i]].value);
        if (B->nstack > 1) r_text(FONT_SMALL, x + 42, sy + 17, t, RGBA(170, 170, 180, 255), w - 46);
        snprintf(t, sizeof t, TR("%d carta%s", "%d card%s", "%d carta%s"), B->nstack, B->nstack > 1 ? "s" : "");
        r_text(FONT_SMALL, x + 42, sy + 30, t, RGBA(140, 140, 150, 255), w - 46);
    }
}

static void draw_player_strip(int y, int hlPlayer, int hlPrep) {
    int gap = 4, w = (632 - (G.np - 1) * gap) / G.np;
    for (int p = 0; p < G.np; p++) {
        int x = 4 + p * (w + gap);
        Player *P = &G.p[p];
        int turn = (G.phase != PH_OVER && p == G.cur && G.phase != PH_SETUP_PREP && G.phase != PH_DRAFT);
        panel(x, y, w, 52, turn ? COL_PLAYER[p] : RGBA(80, 80, 95, 255));
        if (turn) r_frame(x + 1, y + 1, w - 2, 50, c_alpha(COL_PLAYER[p], 120 + (int)(100 * sinf(frame * 0.15f))));
        r_rect(x + 1, y + 1, 4, 50, COL_PLAYER[p]);
        char t[48];
        int nw = r_chrome(x + 9, y + 4, 12, player_name(p), c_mul(COL_PLAYER[p], 1.15f));
        if (!P->human) r_text(FONT_SMALL, x + 14 + nw, y + 3, "CPU", RGBA(200, 200, 210, 255), 0);
        if (G.start == p) r_text(FONT_SMALL, x + w - 34, y + 4, "1º", RGBA(255, 220, 120, 255), 0);
        int ix = x + 9, iy = y + 20;
        snprintf(t, sizeof t, "{R}%d", P->ship); ix += r_text(FONT_SMALL, ix, iy, t, RGBA(255, 255, 255, 255), 0) + 6;
        snprintf(t, sizeof t, "{W}%d", P->earth); ix += r_text(FONT_SMALL, ix, iy, t, RGBA(255, 255, 255, 255), 0) + 6;
        snprintf(t, sizeof t, "{E}%d", P->energy); ix += r_text(FONT_SMALL, ix, iy, t, RGBA(255, 255, 255, 255), 0) + 6;
        snprintf(t, sizeof t, "{C}%d/7", game_on_mars(&G, p)); r_text(FONT_SMALL, ix, iy, t, RGBA(255, 255, 255, 255), 0);
        /* mao e modulo */
        iy = y + 36;
        snprintf(t, sizeof t, "{H}%d", P->nhand);
        int hx = x + 9 + r_text(FONT_SMALL, x + 9, iy, t, RGBA(220, 220, 220, 255), 0) + 6;
        r_text(FONT_SMALL, hx, iy, "{M}", RGBA(220, 220, 220, 255), 0);
        hx += 16;
        for (int k = 0; k < P->nprep; k++) {
            int c = P->prep[k];
            int hl = (p == hlPlayer && k == hlPrep);
            if (p == viewer && !curtain) {
                draw_card_back(hx + k * 18, iy - 1, 16, 15, CARDS[c].color, hl);
                char v[4]; snprintf(v, sizeof v, "%d", CARDS[c].value);
                r_text(FONT_SMALL, hx + k * 18 + 4, iy, v, RGBA(255, 255, 255, 255), 0);
            } else draw_card_back(hx + k * 18, iy - 1, 16, 15, CARDS[c].color, hl);
        }
        Score s; game_score(&G, p, &s);
        snprintf(t, sizeof t, "%d pts", s.total);
        r_text(FONT_SMALL, x + w - 6 - r_text_w(FONT_SMALL, t), iy, t, RGBA(255, 220, 120, 255), 0);
    }
}

/* qual carta do jogador observador destacar */
static void option_card(const Option *o, int *hand, int *prep) {
    *hand = -1; *prep = -1;
    switch (o->kind) {
    case OPT_PLAY_HAND: case OPT_CARD: *hand = o->a; break;
    case OPT_PLAY_PREP: *prep = o->a; break;
    case OPT_DISCARD_ENERGY: case OPT_TUCK: if (o->a == 0) *hand = o->b; else *prep = o->b; break;
    }
}

static void draw_card_detail(int x, int y, int w, int card, const char *extra) {
    char buf[200];
    if (card >= 0) {
        const CardDef *cd = &CARDS[card];
        r_rect(x, y, 4, 14, COL_BLD[cd->color]);
        snprintf(buf, sizeof buf, TR("%s  {%d} valor %d", "%s  {%d} value %d", "%s  {%d} valor %d"), card_name(card), cd->color, cd->value);
        r_text(FONT_MED, x + 8, y - 1, buf, RGBA(255, 255, 255, 255), w - 8);
        int yy = y + 19;
        game_fx_text(&cd->top, buf, sizeof buf);
        char line[240]; snprintf(line, sizeof line, "{H} %s", buf);
        yy += r_text_wrap(FONT_SMALL, x, yy, w, line, RGBA(230, 230, 230, 255), 13) + 3;
        game_fx_text(&cd->bottom, buf, sizeof buf);
        snprintf(line, sizeof line, "{M} %s", buf);
        yy += r_text_wrap(FONT_SMALL, x, yy, w, line, RGBA(200, 220, 255, 255), 13);
        if (extra) r_text_wrap(FONT_SMALL, x, yy + 3, w, extra, RGBA(255, 220, 140, 255), 13);
    } else if (extra) {
        r_text_wrap(FONT_SMALL, x, y, w, extra, RGBA(230, 230, 230, 255), 13);
    }
}

static void building_info(int b, char *buf, int n) {
    if (b == C_CONSTR) snprintf(buf, n, "%s - %s", bld_name(b), bld_ability(b));
    else snprintf(buf, n, TR("%s (valor visível %d) - %s", "%s (visible value %d) - %s", "%s (valor visible %d) - %s"), bld_name(b), game_top_value(&G, b), bld_ability(b));
}

static void draw_my_cards(int x, int y, int w, int h, int hlHand, int hlPrep) {
    panel(x, y, w, h, RGBA(90, 80, 110, 255));
    if (viewer < 0 || curtain) {
        r_text_center(FONT_MED, x + w / 2, y + h / 2 - 20, TR("Cartas ocultas", "Hidden cards", "Cartas ocultas"), RGBA(160, 160, 170, 255));
        return;
    }
    Player *P = &G.p[viewer];
    char t[64];
    snprintf(t, sizeof t, TR("Cartas de %s", "%s's cards", "Cartas de %s"), player_name(viewer));
    r_text(FONT_SMALL, x + 6, y + 3, t, COL_PLAYER[viewer], 0);
    int n = P->nhand + P->nprep;
    int isDraft = G.phase == PH_DRAFT && G.decider == viewer;
    if (isDraft) n += G.npool[viewer];
    if (n == 0) return;
    int avail = w - 12 - 10;
    int cw = (avail - (n - 1) * 4) / n; if (cw > 58) cw = 58;
    int ch = cw * 4 / 3 + 4; if (ch > 82) ch = 82;
    int cx = x + 6, cy = y + 20;
    r_text(FONT_SMALL, cx, cy + ch + 2, TR("{H} mão", "{H} hand", "{H} mano"), RGBA(170, 170, 180, 255), 0);
    for (int i = 0; i < P->nhand; i++, cx += cw + 4) draw_card_mini(cx, cy, cw, ch, P->hand[i], i == hlHand, 0);
    cx += 6;
    if (P->nprep) r_text(FONT_SMALL, cx, cy + ch + 2, TR("{M} módulo", "{M} module", "{M} módulo"), RGBA(170, 170, 180, 255), 0);
    for (int i = 0; i < P->nprep; i++, cx += cw + 4) draw_card_mini(cx, cy, cw, ch, P->prep[i], i == hlPrep, 0);
    if (isDraft) {
        cx += 6;
        r_text(FONT_SMALL, cx, cy + ch + 2, TR("lote", "pack", "lote"), RGBA(255, 220, 140, 255), 0);
        int sel = (optSel < nopts && opts[optSel].kind == OPT_DRAFT) ? opts[optSel].a : -1;
        for (int i = 0; i < G.npool[viewer]; i++, cx += cw + 4) draw_card_mini(cx, cy, cw, ch, G.pool[viewer][i], i == sel, 0);
    }
}

static const char *phase_prompt(char *buf, int n) {
    int d = G.decider;
    switch (G.phase) {
    case PH_DRAFT: snprintf(buf, n, TR("Draft: fique com 1 carta (%d de 4)", "Draft: keep 1 card (%d of 4)", "Draft: quédate 1 carta (%d de 4)"), G.draftPick + 1); break;
    case PH_SETUP_PREP: snprintf(buf, n, TR("Preparação: carta para o {M} módulo (%d/2)", "Setup: card for your {M} module (%d/2)", "Preparación: carta para el {M} módulo (%d/2)"), G.p[d].nprep + 1); break;
    case PH_ACTION: snprintf(buf, n, TR("Sua vez! Escolha 1 ação", "Your turn! Choose 1 action", "¡Tu turno! Elige 1 acción")); break;
    case PH_FX: snprintf(buf, n, "%s \"%s\"", G.ctxBottom ? "{M}" : "{H}", card_name((int)G.ctxCard)); break;
    case PH_CONNECT_MOVE: {
        int under = G.b[(int)G.ctxBld].nstack >= 2 ? CARDS[G.b[(int)G.ctxBld].stack[G.b[(int)G.ctxBld].nstack - 2]].value : 3;
        snprintf(buf, n, TR("Valor %d supera %d! Pousar colono?", "Value %d beats %d! Land a colonist?", "¡Valor %d supera %d! ¿Aterrizar colono?"), CARDS[(int)G.ctxCard].value, under); break; }
    case PH_ABILITY: snprintf(buf, n, TR("Habilidade {%d} %s", "Ability {%d} %s", "Habilidad {%d} %s"), G.ctxBld, color_name((int)G.ctxBld)); break;
    case PH_CLEANUP:
        if (G.p[d].nhand + G.p[d].nprep > 4) snprintf(buf, n, TR("Regra das 4 cartas: descarte da mão", "4-card rule: discard from hand", "Regla de 4 cartas: descarta de la mano"));
        else snprintf(buf, n, TR("Regra das 4 cartas: escolha p/ o {M}", "4-card rule: choose for {M}", "Regla de 4 cartas: elige para el {M}"));
        break;
    default: buf[0] = 0;
    }
    return buf;
}

static void draw_log(int x, int y, int w, int lines) {
    int start = G.nlog - lines; if (start < 0) start = 0;
    for (int i = start, k = 0; i < G.nlog; i++, k++) {
        int a = (i == G.nlog - 1) ? 255 : 170;
        r_text(FONT_SMALL, x, y + k * 14, G.log[i], RGBA(220, 215, 205, a), w);
    }
}

static int human_turn_now(void) {
    return G.phase != PH_OVER && G.decider >= 0 && G.p[G.decider].human && !autoplayHuman;
}

/* barra de volume 0..10 com setas */
static void volume_bar(int x, int y, int v) {
    r_text(FONT_SMALL, x, y, "<", WHITE, 0);
    for (int i = 0; i < 10; i++)
        r_rect(x + 10 + i * 9, y + 10 - i, 7, 3 + i, i < v ? RGBA(255, 170, 80, 255) : RGBA(80, 70, 90, 255));
    r_text(FONT_SMALL, x + 104, y, ">", WHITE, 0);
}

static int pill(int x, const char *txt, Color c) {
    int w = r_text_w(FONT_SMALL, txt) + 14;
    r_rrect(x, 2, w, 17, RGBA(10, 8, 20, 255));
    r_rrect(x + 1, 3, w - 2, 15, RGBA(42, 34, 66, 255));
    r_text(FONT_SMALL, x + 7, 4, txt, c, 0);
    return w;
}

static void draw_game(void) {
    draw_space();
    /* barra superior */
    r_rect(0, 0, SCREEN_W, 21, RGBA(0, 0, 0, 120));
    char t[160];
    int bx = 4;
    snprintf(t, sizeof t, TR("RODADA %d", "ROUND %d", "RONDA %d"), G.round); bx += pill(bx, t, RGBA(255, 230, 200, 255)) + 4;
    snprintf(t, sizeof t, TR("{K} Baralho %d", "{K} Deck %d", "{K} Mazo %d"), G.ndeck); bx += pill(bx, t, WHITE) + 4;
    snprintf(t, sizeof t, TR("Descarte %d", "Discard %d", "Descarte %d"), G.ndiscard); pill(bx, t, WHITE);
    const char *hint = human_turn_now()
        ? (undoOk && G.phase != PH_ACTION ? TR("{X} OK {O} Desfazer {Q} Carta {T} Info {Z}", "{X} OK {O} Undo {Q} Card {T} Info {Z}", "{X} OK {O} Deshacer {Q} Carta {T} Info {Z}") : TR("{X} OK  {Q} Ver carta  {T} Inspecionar  {Z}", "{X} OK  {Q} View card  {T} Inspect  {Z}", "{X} OK  {Q} Ver carta  {T} Inspeccionar  {Z}"))
        : TR("{Q} Última carta  {T} Inspecionar  {Z}", "{Q} Last card  {T} Inspect  {Z}", "{Q} Última carta  {T} Inspeccionar  {Z}");
    if (inspect) hint = TR("{T}/{O} Sair   < > Navegar", "{T}/{O} Exit   < > Browse", "{T}/{O} Salir   < > Navegar");
    r_text(FONT_SMALL, SCREEN_W - 6 - r_text_w(FONT_SMALL, hint), 3, hint, RGBA(220, 220, 230, 255), 0);

    int hlBld = -1, hlHand = -1, hlPrep = -1, hlOppP = -1, hlOppK = -1;
    Option *fo = (human_turn_now() && !curtain && optSel < nopts) ? &opts[optSel] : NULL;
    if (fo) {
        hlBld = option_building(fo);
        option_card(fo, &hlHand, &hlPrep);
        if (fo->kind == OPT_PLAY_OPP) { hlOppP = fo->a; hlOppK = fo->b; }
    }
    if (G.phase == PH_FX && fo && hlHand < 0 && hlBld < 0) hlBld = -1;
    if (inspect && inspectSel < 5) hlBld = inspectSel;

    for (int b = 0; b < 5; b++) draw_building(b, 4 + b * 127, 23, 123, 158, b == hlBld);
    draw_player_strip(184, hlOppP, hlOppK);
    if (inspect && inspectSel >= 5) {
        int gap = 4, w = (632 - (G.np - 1) * gap) / G.np, p = inspectSel - 5;
        r_frame(4 + p * (w + gap) - 2, 182, w + 4, 56, RGBA(255, 255, 255, 200 + (int)(55 * sinf(frame * 0.3f))));
    }

    /* painel esquerdo: cartas + detalhe */
    draw_my_cards(4, 240, 262, 120, hlHand, hlPrep);
    panel(4, 363, 262, 82, RGBA(90, 80, 110, 255));
    {
        int dx = 10, dy = 369, dw = 250;
        char ex[240];
        if (inspect) {
            if (inspectSel < 5) {
                building_info(inspectSel, ex, sizeof ex);
                r_text_wrap(FONT_SMALL, dx, dy, dw, ex, RGBA(235, 235, 235, 255), 13);
            } else {
                int p = inspectSel - 5; Score s; game_score(&G, p, &s);
                snprintf(ex, sizeof ex, TR("%s: nave %d, Terra %d, energia %d, em Marte %d/7.\nPontos agora: %d (nave %d, {S} %d, {D} %d, bônus %d, energia %d).", "%s: ship %d, Earth %d, energy %d, on Mars %d/7.\nPoints now: %d (ship %d, {S} %d, {D} %d, bonus %d, energy %d).", "%s: nave %d, Tierra %d, energía %d, en Marte %d/7.\nPuntos ahora: %d (nave %d, {S} %d, {D} %d, bonus %d, energía %d)."),
                         player_name(p), G.p[p].ship, G.p[p].earth, G.p[p].energy, s.onMars, s.total, s.ship, s.star1, s.star2,
                         s.allFour + s.fourInOne, s.energy);
                r_text_wrap(FONT_SMALL, dx, dy, dw, ex, RGBA(235, 235, 235, 255), 13);
            }
        } else if (fo) {
            int card = -1;
            Player *P = &G.p[G.decider];
            if (hlHand >= 0 && hlHand < P->nhand) card = P->hand[hlHand];
            else if (hlPrep >= 0 && hlPrep < P->nprep) card = P->prep[hlPrep];
            else if (fo->kind == OPT_DRAFT) card = G.pool[G.decider][fo->a];
            if (card >= 0) {
                const char *extra = NULL;
                if (fo->kind == OPT_PLAY_PREP && CARDS[card].color != C_CONSTR) {
                    int beat = CARDS[card].value > game_top_value(&G, CARDS[card].color);
                    extra = beat ? TR("Supera o valor visível: pode pousar 1 {C}!", "Beats the visible value: you may land 1 {C}!", "Supera el valor visible: ¡puedes aterrizar 1 {C}!") : TR("Não supera o valor visível.", "Does not beat the visible value.", "No supera el valor visible.");
                }
                draw_card_detail(dx, dy, dw, card, extra);
            } else if (fo->kind == OPT_PLAY_OPP) {
                int c = CARDS[G.p[fo->a].prep[fo->b]].color;
                snprintf(ex, sizeof ex, TR("Carta oculta de %s. O dono pode usar a ação {M}; você usa a habilidade: %s", "Hidden card of %s. The owner may use the {M} action; you use the ability: %s", "Carta oculta de %s. El dueño puede usar la acción {M}; tú usas la habilidad: %s"),
                         player_name(fo->a), bld_ability(c));
                r_text_wrap(FONT_SMALL, dx, dy, dw, ex, RGBA(235, 235, 235, 255), 13);
            } else if (G.phase == PH_FX) {
                draw_card_detail(dx, dy, dw, G.ctxCard, NULL);
            } else if (hlBld >= 0) {
                building_info(hlBld, ex, sizeof ex);
                r_text_wrap(FONT_SMALL, dx, dy, dw, ex, RGBA(235, 235, 235, 255), 13);
            } else if (fo->kind == OPT_TAKE_EARTH) {
                r_text_wrap(FONT_SMALL, dx, dy, dw, TR("Pegue 1 colono seu da Terra e coloque-o na sua nave.", "Take 1 of your colonists from Earth and put it on your ship.", "Toma 1 colono tuyo de la Tierra y ponlo en tu nave."), RGBA(235, 235, 235, 255), 13);
            } else {
                r_text_wrap(FONT_SMALL, dx, dy, dw, TR("Use as setas para ver as opções.", "Use the arrows to browse the options.", "Usa las flechas para ver las opciones."), RGBA(180, 180, 190, 255), 13);
            }
        } else {
            draw_log(dx, dy, dw, 5);
        }
    }

    /* painel direito: decisoes */
    int px = 270, py = 240, pw = 366, ph = 205;
    panel(px, py, pw, ph, RGBA(120, 70, 60, 255));
    if (G.phase == PH_OVER) {
        r_text_center(FONT_MED, px + pw / 2, py + 80, TR("Fim de jogo!", "Game over!", "¡Fin del juego!"), RGBA(255, 220, 120, 255));
        r_art(&ART_MASC[MASC_CHEER], px + pw / 2 - 36, py + 110, ART_MASC[MASC_CHEER].w, ART_MASC[MASC_CHEER].h, WHITE);
    } else if (curtain) {
        r_chrome_center(px + pw / 2 - 30, py + 50, 20, player_name(G.decider), c_mul(COL_PLAYER[G.decider], 1.15f));
        if (pad2_connected()) {
            char cm[80];
            snprintf(cm, sizeof cm, TR("Sua vez! Use o controle %d", "Your turn! Use controller %d", "¡Tu turno! Usa el mando %d"), src_for_player(G.decider) + 1);
            r_text_center(FONT_SMALL, px + pw / 2 - 30, py + 84, cm, RGBA(230, 230, 230, 255));
        } else
            r_text_center(FONT_SMALL, px + pw / 2 - 30, py + 84, TR("Sua vez! Passe o controle", "Your turn! Pass the controller", "¡Tu turno! Pasa el mando"), RGBA(230, 230, 230, 255));
        r_text_center(FONT_SMALL, px + pw / 2 - 30, py + 100, TR("e pressione {X}", "and press {X}", "y pulsa {X}"), RGBA(230, 230, 230, 255));
        const SprRect *m = &ART_MASC[MASC_THUMBS];
        r_art(m, px + pw - m->w - 10, py + ph - m->h - 10, m->w, m->h, WHITE);
    } else if (human_turn_now()) {
        const SprRect *m = &ART_MASC[MASC_POINT];
        int mx = px + pw - m->w - 2, my = py + 22;
        char pr[120];
        int cw = 0;
        if (pad2_connected()) {   /* indicador do controle que esta comandando */
            char cm[16];
            snprintf(cm, sizeof cm, TR("CTRL %d", "PAD %d", "MANDO %d"), src_for_player(G.decider) + 1);
            cw = r_text_w(FONT_SMALL, cm) + 12;
            r_rrect(px + pw - cw - 6, py + 5, cw, 15, c_mul(COL_PLAYER[G.decider], 0.8f));
            r_text(FONT_SMALL, px + pw - cw, py + 6, cm, WHITE, 0);
        }
        r_text(FONT_MED, px + 8, py + 5, phase_prompt(pr, sizeof pr), RGBA(255, 210, 160, 255), pw - 22 - cw);
        int rows = 8, rh = 16, ly = py + 26;
        if (optSel < optScroll) optScroll = optSel;
        if (optSel >= optScroll + rows) optScroll = optSel - rows + 1;
        for (int i = optScroll; i < nopts && i < optScroll + rows; i++) {
            int ry = ly + (i - optScroll) * rh;
            int maxw = (ry < my + m->h - 4) ? mx - px - 26 : pw - 32;
            if (i == optSel) {
                r_rect(px + 4, ry - 1, maxw + 22, rh, RGBA(200, 80, 50, 170 + (int)(40 * sinf(frame * 0.2f))));
                r_icon(IC_ARROW, px + 7, ry + 1, 13, WHITE);
            }
            char lb[160]; game_option_label(&G, &opts[i], lb, sizeof lb);
            r_text(FONT_SMALL, px + 24, ry + 1, lb, i == optSel ? WHITE : RGBA(210, 210, 215, 255), maxw);
        }
        if (optScroll > 0) r_text(FONT_SMALL, px + 8, ly - 12, "^", RGBA(255, 255, 255, 200), 0);
        if (optScroll + rows < nopts) r_text(FONT_SMALL, px + 8, ly + rows * rh - 4, "v", RGBA(255, 255, 255, 200), 0);
        /* mascote apontando para as opcoes */
        int bob = (int)(2 * sinf(frame * 0.12f));
        r_art(m, mx + bob, my, m->w, m->h, WHITE);
        r_rect(px + 6, py + ph - 48, pw - 12, 1, RGBA(255, 255, 255, 50));
        const SprRect *rb = &ART_MASC[MASC_ROBOT];
        int rw = rb->w * 3 / 4, rhh = rb->h * 3 / 4;
        r_art(rb, px + pw - rw - 4, py + ph - rhh - 3, rw, rhh, WHITE);
        draw_log(px + 8, py + ph - 45, pw - 22 - rw, 3);
    } else {
        const SprRect *rb = &ART_MASC[MASC_ROBOT];
        if (G.decider >= 0) {
            int dots = (int)(frame / 15) % 4;
            snprintf(t, sizeof t, TR("%s está pensando%.*s", "%s is thinking%.*s", "%s está pensando%.*s"), player_name(G.decider), dots, "...");
            r_icon(IC_CHIP, px + 8, py + 6, 16, COL_PLAYER[G.decider]);
            r_text(FONT_MED, px + 28, py + 5, t, COL_PLAYER[G.decider], pw - 36);
        }
        draw_log(px + 8, py + 30, pw - 24 - rb->w, LOG_LINES);
        int bob = (int)(3 * sinf(frame * 0.15f));
        r_art(rb, px + pw - rb->w - 8, py + ph - rb->h - 8 + bob, rb->w, rb->h, WHITE);
    }

    /* carta revelada pela CPU */
    if (popupCard >= 0 && SDL_GetTicks() < popupUntil && !paused && zoomCard < 0) {
        int w = 150, h = (int)(w * 1.48f);
        int x = 246, y = 22;
        draw_card_full(x, y, w, popupCard);
        r_rrect(x + 10, y + h + 2, w - 20, 16, RGBA(10, 8, 20, 220));
        r_text_center(FONT_SMALL, x + w / 2, y + h + 3, popupText, WHITE);
        (void)h;
    }
    /* zoom da carta */
    if (zoomCard >= 0 && !paused) {
        r_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, 170));
        draw_card_full(220, 6, 200, zoomCard);
        r_text_center(FONT_SMALL, 320, 310, TR("{Q} / {O} Fechar", "{Q} / {O} Close", "{Q} / {O} Cerrar"), WHITE);
    }

    /* pausa */
    if (paused) {
        r_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, 160));
        panel(190, 120, 260, 196, RGBA(255, 120, 80, 255));
        r_chrome_center(320, 130, 18, TR("PAUSA", "PAUSE", "PAUSA"), RGBA(255, 210, 170, 255));
        const char *it[5] = { TR("Continuar", "Continue", "Continuar"), TR("Música", "Music", "Música"), TR("Efeitos", "Effects", "Efectos"), TR("Como jogar", "How to play", "Cómo jugar"), TR("Sair para o título", "Quit to title", "Salir al título") };
        for (int i = 0; i < 5; i++) {
            int yy = 166 + i * 28;
            if (i == pauseSel) r_rect(204, yy - 3, 232, 22, RGBA(200, 80, 50, 200));
            if (i == 1 || i == 2) {
                r_text(FONT_MED, 214, yy, it[i], WHITE, 0);
                volume_bar(310, yy + 2, i == 1 ? cfgMusic : cfgSfx);
            } else r_text_center(FONT_MED, 320, yy, it[i], WHITE);
        }
    }
}

/* ------------------------------------------------------------------ */
/* logica da cena de jogo                                               */

/* som da escolha em si */
static int sfx_for(const Option *o) {
    switch (o->kind) {
    case OPT_PLAY_HAND: case OPT_PLAY_PREP: case OPT_CARD: case OPT_DRAFT: case OPT_TUCK: return SFX_CARD;
    case OPT_PLAY_OPP: return SFX_REVEAL;
    case OPT_SKIP: case OPT_PASS: return SFX_BACK;
    case OPT_DEMOTE: return SFX_ERROR;
    default: return -1;   /* o efeito no tabuleiro decide (ver event_sounds) */
    }
}

typedef struct { int mars, star2, earth, energy, ndeck; } Snap;
static Snap snap_state(void) {
    Snap s = { 0, 0, 0, 0, G.ndeck };
    for (int p = 0; p < G.np; p++) {
        s.mars += game_on_mars(&G, p); s.earth += G.p[p].earth; s.energy += G.p[p].energy;
        for (int b = 0; b < 4; b++) s.star2 += G.b[b].star2[p];
    }
    return s;
}

/* sons do que mudou no tabuleiro (inclui habilidades automaticas dos edificios) */
static void event_sounds(const Snap *a, const Snap *b, int first) {
    int n = first >= 0;
    if (first >= 0) audio_sfx(first);
    if (n < 2 && b->mars > a->mars) { audio_sfx(SFX_LAND); n++; }
    if (n < 2 && b->star2 > a->star2) { audio_sfx(SFX_PROMOTE); n++; }
    if (n < 2 && b->earth < a->earth) { audio_sfx(SFX_LAUNCH); n++; }
    if (n < 2 && b->energy > a->energy) { audio_sfx(SFX_ENERGY); n++; }
    if (n < 2 && b->ndeck < a->ndeck && G.phase != PH_SETUP_PREP) { audio_sfx(SFX_DRAW); n++; }
    if (n == 0) audio_sfx(SFX_OK);
}

static void apply_option(int idx) {
    Option o = opts[idx];
    int who = G.decider;
    int card = -1;
    const char *how = NULL;
    if (G.phase == PH_ACTION) {
        if (o.kind == OPT_PLAY_HAND) { card = G.p[who].hand[o.a]; how = TR("joga da mão", "plays from hand", "juega de la mano"); }
        else if (o.kind == OPT_PLAY_PREP) { card = G.p[who].prep[o.a]; how = TR("conecta do módulo", "connects from module", "conecta del módulo"); }
        else if (o.kind == OPT_PLAY_OPP) { card = G.p[o.a].prep[o.b]; how = TR("revela módulo alheio", "reveals a rival module", "revela módulo ajeno"); }
    }
    if (card >= 0) {
        lastCard = card;
        if (!G.p[who].human || autoplayHuman || o.kind == OPT_PLAY_OPP) {
            popupCard = card;
            popupUntil = SDL_GetTicks() + (cfgSpeed == 0 ? 1700 : cfgSpeed == 1 ? 900 : 500);
            snprintf(popupText, sizeof popupText, "%s %s", player_name(who), how);
            if (cfgSpeed == 0 && !G.p[who].human) cpuReadyAt = popupUntil;
        }
    }
    Snap before = snap_state();
    int first = sfx_for(&o);
    game_apply(&G, &o);
    Snap after = snap_state();
    event_sounds(&before, &after, first);
    if (G.lastBld < 5) { flashBld = G.lastBld; flashUntil = SDL_GetTicks() + 900; G.lastBld = 255; }
    if (G.chance || (G.decider != undoOwner && G.phase != PH_ACTION)) undoOk = 0;
    if (G.phase == PH_ACTION) undoOk = 0;
    (void)who;
    optsDirty = 1;
    optSel = 0; optScroll = 0;
}

/* carta relacionada a opcao em foco (para o zoom com quadrado) */
static int focused_card(void) {
    if (!human_turn_now() || optSel >= nopts) return -1;
    const Option *fo = &opts[optSel];
    int h, pr;
    option_card(fo, &h, &pr);
    const Player *P = &G.p[G.decider];
    if (h >= 0 && h < P->nhand) return P->hand[h];
    if (pr >= 0 && pr < P->nprep) return P->prep[pr];
    if (fo->kind == OPT_DRAFT) return G.pool[G.decider][fo->a];
    if (G.phase == PH_FX) return G.ctxCard;
    return -1;
}

static int pauseSrc = -1;
static const int *pressedSrc_any(void) { return pressed; }

static void update_game(void) {
    /* quem pode comandar agora: o controle do jogador humano que decide;
     * no turno da CPU (ou fim de jogo), qualquer controle */
    int d0 = G.decider;
    int src = -1;
    if (paused) src = pauseSrc;
    else if (G.phase != PH_OVER && d0 >= 0 && G.p[d0].human && !autoplayHuman) src = src_for_player(d0);
    const int *pressed = src >= 0 ? pressedSrc[src] : pressedSrc_any();
    if (paused) {
        if (pressed[B_UP]) { pauseSel = (pauseSel + 4) % 5; audio_sfx(SFX_MOVE); }
        if (pressed[B_DOWN]) { pauseSel = (pauseSel + 1) % 5; audio_sfx(SFX_MOVE); }
        int dir = pressed[B_RIGHT] ? 1 : pressed[B_LEFT] ? -1 : 0;
        if (dir && (pauseSel == 1 || pauseSel == 2)) {
            int *v = pauseSel == 1 ? &cfgMusic : &cfgSfx;
            *v += dir; if (*v < 0) *v = 0; if (*v > 10) *v = 10;
            audio_volume(cfgMusic, cfgSfx);
            audio_sfx(SFX_MOVE);
        }
        if (pressed[B_START] || pressed[B_BACK]) { paused = 0; audio_sfx(SFX_BACK); }
        if (pressed[B_OK]) {
            audio_sfx(SFX_OK);
            if (pauseSel == 0) paused = 0;
            else if (pauseSel == 3) { paused = 0; rulesReturn = SC_GAME; rulesPage = 0; scene = SC_RULES; }
            else if (pauseSel == 4) { paused = 0; scene = SC_TITLE; }
        }
        return;
    }
    if (zoomCard >= 0) {
        if (pressed[B_SELECT] || pressed[B_BACK] || pressed[B_OK] || pressed[B_START]) { zoomCard = -1; audio_sfx(SFX_BACK); }
        return;
    }
    if (pressed[B_SELECT] && !curtain) {
        int c = focused_card();
        if (c < 0) c = lastCard;
        if (c >= 0) { zoomCard = c; popupCard = -1; audio_sfx(SFX_CARD); }
        else audio_sfx(SFX_ERROR);
        return;
    }
    if (pressed[B_START]) { paused = 1; pauseSel = 0; pauseSrc = src; audio_sfx(SFX_OK); return; }
    if (pressed[B_INFO]) { inspect = !inspect; audio_sfx(inspect ? SFX_OK : SFX_BACK); }
    if (inspect) {
        int n = 5 + G.np;
        if (pressed[B_LEFT] || pressed[B_UP]) { inspectSel = (inspectSel + n - 1) % n; audio_sfx(SFX_MOVE); }
        if (pressed[B_RIGHT] || pressed[B_DOWN]) { inspectSel = (inspectSel + 1) % n; audio_sfx(SFX_MOVE); }
        if (pressed[B_BACK]) { inspect = 0; audio_sfx(SFX_BACK); }
        return;
    }

    if (G.phase == PH_OVER) {
        if (!overAt) { overAt = SDL_GetTicks(); audio_sfx(SFX_WIN); }
        if (SDL_GetTicks() - overAt > 1500 || pressed[B_OK]) { scene = SC_OVER; overSel = 0; }
        return;
    }
    overAt = 0;
    if (optsDirty) {
        nopts = game_options(&G, opts);
        optsDirty = 0;
        if (optSel >= nopts) optSel = 0;
        /* nova decisao humana de acao: salvar estado para desfazer */
        if (G.phase == PH_ACTION && G.p[G.decider].human) {
            undoSnap = G; undoOk = 1; undoOwner = G.decider;
            if (!autoplayHuman) audio_sfx(SFX_ALERT);
        }
    }
    int d = G.decider;
    if (d < 0) return;

    if (G.p[d].human && !autoplayHuman) {
        if (num_humans() > 1 && viewer != d && !curtain) curtain = 1;
        if (curtain) {
            if (pressed[B_OK]) { curtain = 0; viewer = d; audio_sfx(SFX_OK); }
            return;
        }
        viewer = d;
        if (pressed[B_UP]) { optSel = (optSel + nopts - 1) % nopts; audio_sfx(SFX_MOVE); }
        if (pressed[B_DOWN]) { optSel = (optSel + 1) % nopts; audio_sfx(SFX_MOVE); }
        if (pressed[B_L1] || pressed[B_LEFT]) { optSel = optSel >= 8 ? optSel - 8 : 0; audio_sfx(SFX_MOVE); }
        if (pressed[B_R1] || pressed[B_RIGHT]) { optSel = optSel + 8 < nopts ? optSel + 8 : nopts - 1; audio_sfx(SFX_MOVE); }
        if (pressed[B_BACK]) {
            if (undoOk && G.phase != PH_ACTION) {
                G = undoSnap; optsDirty = 1; optSel = 0; audio_sfx(SFX_BACK);
                game_log(&G, TR("Jogada desfeita.", "Move undone.", "Jugada deshecha."));
            } else audio_sfx(SFX_ERROR);
        }
        if (pressed[B_OK] && nopts > 0) apply_option(optSel);
        cpuReadyAt = SDL_GetTicks() + 500;
        return;
    }

    /* CPU */
    Uint32 delay = cfgSpeed == 0 ? 850 : cfgSpeed == 1 ? 300 : 0;
    int quick = (G.phase == PH_SETUP_PREP || G.phase == PH_DRAFT || G.phase == PH_CLEANUP);
    if (quick) delay /= 4;
    if (SDL_GetTicks() < cpuReadyAt) return;
    int k = ai_choose(&G, &aiSeed);
    if (k < 0 || k >= nopts) k = 0;
    apply_option(k);
    cpuReadyAt = SDL_GetTicks() + delay;
}

/* ------------------------------------------------------------------ */
/* titulo                                                               */

static void draw_title(void) {
    r_title(&TITLE_KEYART, 0, 0, SCREEN_W, SCREEN_H, WHITE);
    /* menu */
    const char *items[5] = { TR("Novo jogo", "New game", "Nuevo juego"), TR("Como jogar", "How to play", "Cómo jugar"),
                             TR("Idioma", "Language", "Idioma"), TR("Créditos", "Credits", "Créditos"), TR("Sair", "Quit", "Salir") };
    int mx = 452, my = 272, mw = 178, mh = 161;
    r_rrect(mx - 2, my - 2, mw + 4, mh + 4, RGBA(10, 6, 20, 200));
    r_rrect(mx, my, mw, mh, RGBA(30, 20, 50, 210));
    r_frame(mx, my, mw, mh, RGBA(255, 150, 80, 200));
    for (int i = 0; i < 5; i++) {
        int y = my + 10 + i * 29;
        if (i == titleSel) {
            r_rrect(mx + 6, y - 4, mw - 12, 24, RGBA(220, 80, 40, 220 + (int)(30 * sinf(frame * 0.15f))));
            r_icon(IC_ARROW, mx + 12, y, 16, WHITE);
        }
        r_text(FONT_MED, mx + 36, y, items[i], WHITE, 0);
    }
    if ((frame / 30) % 2) {
        r_rrect(214, 420, 212, 20, RGBA(10, 6, 20, 170));
        r_text_center(FONT_SMALL, 320, 423, TR("Pressione {X} ou {Z}", "Press {X} or {Z}", "Pulsa {X} o {Z}"), RGBA(255, 230, 190, 255));
    }
    r_text(FONT_SMALL, 7, 433, "v1.5 Homebrew PS2", RGBA(0, 0, 0, 200), 0);
    r_text(FONT_SMALL, 6, 432, "v1.5 Homebrew PS2", RGBA(255, 230, 210, 255), 0);
}

/* ------------------------------------------------------------------ */
/* escolha de idioma (primeira tela)                                    */

static void draw_lang(void) {
    r_title(&TITLE_KEYART, 0, 0, SCREEN_W, SCREEN_H, WHITE);
    r_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(5, 3, 15, 150));
    static const char *NAMES[3] = { "Português", "English", "Español" };
    static const char *SUB[3] = { "Idioma principal", "Language", "Idioma" };
    static const char *ASK[3] = { "Escolha o idioma", "Choose your language", "Elige tu idioma" };
    int px = 170, py = 150, pw = 300, ph = 200;
    r_rrect(px - 2, py - 2, pw + 4, ph + 4, RGBA(10, 6, 20, 230));
    r_rrect(px, py, pw, ph, RGBA(30, 20, 50, 235));
    r_frame(px, py, pw, ph, RGBA(255, 150, 80, 220));
    r_text_center(FONT_MED, 320, py + 10, ASK[langSel], RGBA(255, 210, 160, 255));
    for (int i = 0; i < 3; i++) {
        int y = py + 44 + i * 46;
        if (i == langSel) {
            r_rrect(px + 14, y - 4, pw - 28, 40, RGBA(220, 80, 40, 220 + (int)(30 * sinf(frame * 0.15f))));
            r_icon(IC_ARROW, px + 24, y + 8, 16, WHITE);
        }
        r_text_scale(1.3f);
        r_text(FONT_MED, px + 52, y, NAMES[i], WHITE, 0);
        r_text_scale(1.0f);
        r_text(FONT_SMALL, px + 54, y + 21, SUB[i], RGBA(220, 200, 190, 255), 0);
    }
    r_text_center(FONT_SMALL, 320, py + ph + 12, "{X} OK", WHITE);
}

static void update_lang(void) {
    if (pressed[B_UP]) { langSel = (langSel + 2) % 3; audio_sfx(SFX_MOVE); }
    if (pressed[B_DOWN]) { langSel = (langSel + 1) % 3; audio_sfx(SFX_MOVE); }
    if (pressed[B_OK] || pressed[B_START]) { g_lang = langSel; audio_sfx(SFX_OK); scene = SC_TITLE; }
    if (pressed[B_BACK] && frame > 60) { audio_sfx(SFX_BACK); scene = SC_TITLE; }
}

static void update_title(void) {
    if (pressed[B_UP]) { titleSel = (titleSel + 4) % 5; audio_sfx(SFX_MOVE); }
    if (pressed[B_DOWN]) { titleSel = (titleSel + 1) % 5; audio_sfx(SFX_MOVE); }
    if (pressed[B_OK] || pressed[B_START]) {
        audio_sfx(SFX_OK);
        if (titleSel == 0) { scene = SC_SETUP; setupSel = 0; }
        else if (titleSel == 1) { scene = SC_RULES; rulesReturn = SC_TITLE; rulesPage = 0; }
        else if (titleSel == 2) { scene = SC_LANG; langSel = g_lang; }
        else if (titleSel == 3) scene = SC_CREDITS;
        else quitReq = 1;
    }
}

/* ------------------------------------------------------------------ */
/* configuracao                                                         */

#define SETUP_ROWS 10
static void draw_setup(void) {
    draw_space();
    r_mars(520, 320, 120);
    r_chrome(30, 22, 30, TR("NOVA MISSÃO", "NEW MISSION", "NUEVA MISIÓN"), RGBA(255, 210, 180, 255));
    panel(24, 70, 420, 340, RGBA(200, 90, 60, 255));
    char t[64];
    for (int i = 0; i < SETUP_ROWS; i++) {
        int y = 82 + i * 32;
        int dis = (i >= 1 && i <= 4 && i - 1 >= cfgNp);
        if (i == setupSel) r_rrect(32, y - 5, 404, 27, RGBA(200, 70, 40, 190));
        Color lc = dis ? RGBA(110, 110, 120, 255) : WHITE;
        if (i == 0) {
            r_text(FONT_MED, 44, y, TR("Jogadores", "Players", "Jugadores"), lc, 0);
            snprintf(t, sizeof t, "< %d >", cfgNp);
        } else if (i <= 4) {
            int p = i - 1;
            if (dis) r_text(FONT_MED, 44, y, player_name(p), lc, 0);
            else r_chrome(44, y + 1, 14, player_name(p), c_mul(COL_PLAYER[p], 1.15f));
            snprintf(t, sizeof t, "< %s >", SLOT_NAME[cfgSlot[p]]);
        } else if (i == 5) {
            r_text(FONT_MED, 44, y, TR("Variante: draft de cartas", "Variant: card draft", "Variante: draft de cartas"), lc, 0);
            snprintf(t, sizeof t, "< %s >", cfgDraft ? TR("Sim", "Yes", "Sí") : TR("Não", "No", "No"));
        } else if (i == 6) {
            r_text(FONT_MED, 44, y, TR("Velocidade da CPU", "CPU speed", "Velocidad de la CPU"), lc, 0);
            snprintf(t, sizeof t, "< %s >", SPEED_NAME[cfgSpeed]);
        } else if (i == 7 || i == 8) {
            r_text(FONT_MED, 44, y, i == 7 ? TR("Volume da música", "Music volume", "Volumen de la música") : TR("Volume dos efeitos", "Effects volume", "Volumen de efectos"), lc, 0);
            volume_bar(316, y + 1, i == 7 ? cfgMusic : cfgSfx);
            continue;
        } else {
            r_text_center(FONT_MED, 234, y, TR("{R} INICIAR MISSÃO", "{R} START MISSION", "{R} INICIAR MISIÓN"), RGBA(255, 230, 160, 255));
            continue;
        }
        r_text(FONT_MED, 428 - r_text_w(FONT_MED, t), y, t, lc, 0);
    }
    r_text(FONT_SMALL, 30, 420, TR("{X} Confirmar   < > Alterar   {O} Voltar", "{X} Confirm   < > Change   {O} Back", "{X} Confirmar   < > Cambiar   {O} Volver"), RGBA(220, 220, 230, 255), 0);
    r_text_wrap(FONT_SMALL, 462, 80, 168,
                TR("Controle 1: só o jogador 1 (Vermelho). Controle 2: os demais jogadores humanos. Sem o controle 2, todos usam o controle 1 (hot-seat).", "Controller 1: player 1 (Red) only. Controller 2: the other human players. Without controller 2, everyone uses controller 1 (hot-seat).", "Mando 1: solo el jugador 1 (Rojo). Mando 2: los demás jugadores humanos. Sin el mando 2, todos usan el mando 1 (hot-seat)."),
                RGBA(220, 210, 200, 255), 14);
    const SprRect *m = &ART_MASC[MASC_THUMBS];
    r_art(m, 470, 170, m->w * 3 / 2, m->h * 3 / 2, WHITE);
}

static void update_setup(void) {
    if (pressed[B_UP]) { do setupSel = (setupSel + SETUP_ROWS - 1) % SETUP_ROWS; while (setupSel >= 1 && setupSel <= 4 && setupSel - 1 >= cfgNp); audio_sfx(SFX_MOVE); }
    if (pressed[B_DOWN]) { do setupSel = (setupSel + 1) % SETUP_ROWS; while (setupSel >= 1 && setupSel <= 4 && setupSel - 1 >= cfgNp); audio_sfx(SFX_MOVE); }
    int dir = pressed[B_RIGHT] ? 1 : pressed[B_LEFT] ? -1 : 0;
    if (pressed[B_OK] && setupSel != 9 && setupSel != 7 && setupSel != 8) dir = 1;
    if (dir) {
        audio_sfx(SFX_MOVE);
        if (setupSel == 0) { cfgNp += dir; if (cfgNp > 4) cfgNp = 2; if (cfgNp < 2) cfgNp = 4; }
        else if (setupSel <= 4) { int p = setupSel - 1; cfgSlot[p] = (cfgSlot[p] + 4 + dir) % 4; }
        else if (setupSel == 5) cfgDraft = !cfgDraft;
        else if (setupSel == 6) cfgSpeed = (cfgSpeed + 3 + dir) % 3;
        else if (setupSel == 7 || setupSel == 8) {
            int *v = setupSel == 7 ? &cfgMusic : &cfgSfx;
            *v += dir; if (*v < 0) *v = 0; if (*v > 10) *v = 10;
            audio_volume(cfgMusic, cfgSfx);
        }
    }
    if ((pressed[B_OK] && setupSel == 9) || pressed[B_START]) { audio_sfx(SFX_OK); start_game(); }
    if (pressed[B_BACK]) { audio_sfx(SFX_BACK); scene = SC_TITLE; }
}

/* ------------------------------------------------------------------ */
/* fim de jogo                                                          */

static void draw_over(void) {
    draw_space();
    int win = game_winners(&G);
    r_chrome_center(320, 12, 28, TR("MISSÃO CONCLUÍDA", "MISSION COMPLETE", "MISIÓN CUMPLIDA"), RGBA(255, 220, 190, 255));
    char t[96];
    int nw = 0, wp = 0;
    for (int i = 0; i < G.np; i++) if (win >> i & 1) { nw++; wp = i; }
    if (nw == 1) snprintf(t, sizeof t, TR("Vitória de %s!", "%s wins!", "¡Victoria de %s!"), player_name(wp));
    else snprintf(t, sizeof t, TR("Empate! A vitória é compartilhada.", "Tie! The victory is shared.", "¡Empate! La victoria es compartida."));
    r_text_center(FONT_MED, 320, 54, t, nw == 1 ? COL_PLAYER[wp] : RGBA(255, 220, 120, 255));

    int tx = 40, ty = 84, cw = (560 - 150) / G.np;
    panel(tx - 10, ty - 6, 580, 290, RGBA(200, 90, 60, 255));
    const char *rows[8] = { TR("Colonos na nave (1)", "Colonists on ship (1)", "Colonos en la nave (1)"), TR("Colonos em {S} (2)", "Colonists on {S} (2)", "Colonos en {S} (2)"), TR("Colonos em {D} (4)", "Colonists on {D} (4)", "Colonos en {D} (4)"),
                            TR("Nos 4 edifícios (+2)", "In all 4 buildings (+2)", "En los 4 edificios (+2)"), TR("4+ num edifício (+3)", "4+ in one building (+3)", "4+ en un edificio (+3)"), TR("Maior energia (+1)", "Most energy (+1)", "Más energía (+1)"), TR("Colonos em Marte", "Colonists on Mars", "Colonos en Marte"), "TOTAL" };
    for (int r = 0; r < 8; r++) {
        int y = ty + 34 + r * 28;
        if (r == 7) r_rect(tx - 4, y - 5, 568, 1, RGBA(255, 255, 255, 80));
        r_text(r == 7 ? FONT_MED : FONT_SMALL, tx, y, rows[r], r == 6 ? RGBA(150, 150, 160, 255) : RGBA(230, 230, 230, 255), 0);
    }
    for (int p = 0; p < G.np; p++) {
        int x = tx + 150 + p * cw + cw / 2;
        Score s; game_score(&G, p, &s);
        if (win >> p & 1) r_icon(IC_CROWN, x - r_text_w(FONT_MED, player_name(p)) / 2 - 26, ty + 6, 22, RGBA(255, 210, 60, 255));
        r_text_center(FONT_MED, x, ty + 10, player_name(p), COL_PLAYER[p]);
        int vals[8] = { s.ship, s.star1, s.star2, s.allFour, s.fourInOne, s.energy, s.onMars, s.total };
        for (int r = 0; r < 8; r++) {
            snprintf(t, sizeof t, "%d", vals[r]);
            r_text_center(r == 7 ? FONT_MED : FONT_SMALL, x, ty + 34 + r * 28, t,
                          r == 7 ? RGBA(255, 220, 120, 255) : r == 6 ? RGBA(150, 150, 160, 255) : RGBA(255, 255, 255, 255));
        }
    }
    r_text_center(FONT_SMALL, 320, 380, TR("Empate: vence quem tiver mais colonos em Marte.", "Tie: most colonists on Mars wins.", "Empate: gana quien tenga más colonos en Marte."), RGBA(170, 170, 180, 255));
    {
        const SprRect *a = &ART_MASC[MASC_CHEER], *b = &ART_MASC[MASC_THUMBS];
        int bob = (int)(4 * sinf(frame * 0.15f));
        r_art(a, 4, 330 + bob, a->w * 5 / 4, a->h * 5 / 4, WHITE);
        r_art(b, 640 - b->w * 5 / 4 - 4, 330 - bob, b->w * 5 / 4, b->h * 5 / 4, WHITE);
    }
    const char *it[2] = { TR("Jogar novamente", "Play again", "Jugar otra vez"), TR("Menu principal", "Main menu", "Menú principal") };
    for (int i = 0; i < 2; i++) {
        int x = 190 + i * 260;
        if (i == overSel) r_rrect(x - 90, 404, 180, 26, RGBA(200, 70, 40, 220));
        r_text_center(FONT_MED, x, 408, it[i], RGBA(255, 255, 255, 255));
    }
}

static void update_over(void) {
    if (pressed[B_LEFT] || pressed[B_RIGHT] || pressed[B_UP] || pressed[B_DOWN]) { overSel ^= 1; audio_sfx(SFX_MOVE); }
    if (pressed[B_OK]) { audio_sfx(SFX_OK); if (overSel == 0) start_game(); else scene = SC_TITLE; }
    if (pressed[B_BACK]) { scene = SC_TITLE; audio_sfx(SFX_BACK); }
}

/* ------------------------------------------------------------------ */
/* regras e creditos                                                    */

static const char *RULES_TITLES[LANG_COUNT][9] = {
    { "Objetivo", "Preparação", "As cartas", "Seu turno", "Jogar do módulo", "Regra das 4 cartas", "Edifícios", "Fim e pontuação", "Controles" },
    { "Goal", "Setup", "The cards", "Your turn", "Playing from the module", "The 4-card rule", "Buildings", "End and scoring", "Controls" },
    { "Objetivo", "Preparación", "Las cartas", "Tu turno", "Jugar del módulo", "Regla de las 4 cartas", "Edificios", "Final y puntuación", "Controles" },
};
static const char *RULES_TEXTS[LANG_COUNT][9] = {
    {
    "Cada jogador comanda uma agência espacial com 7 colonos. Use cartas de projeto para levar colonos da Terra para a sua nave {R} e da nave para os edifícios de Marte.\n\n"
    "Quem distribuir melhor seus colonos fará mais pontos e vencerá. Colonos nos espaços {D} valem mais, mas há poucas vagas!",
    "Cada jogador começa com 1 {C} na nave, 6 {C} na Terra e 1 {E} de energia.\n\n"
    "Cada um recebe 4 cartas de projeto e escolhe 2 para o seu {M} módulo de preparação (viradas para baixo; os outros só veem a cor). As outras 2 ficam na {H} mão.\n\n"
    "Variante draft: cada um recebe 5 cartas, fica com 1 e passa o resto, até ter 4.",
    "Cada carta de projeto tem uma COR (o edifício onde é conectada), um VALOR de 0 a 7 e duas ações.\n\n"
    "{H} AÇÃO DA MÃO: usada quando você joga a carta da mão.\n"
    "{M} AÇÃO DO MÓDULO: usada quando a carta é conectada a um edifício a partir de um módulo.\n\n"
    "O valor só importa no módulo: se for maior que o valor visível do edifício, você pousa um colono.\n\n"
    "Pressione {Q} durante o jogo para ver qualquer carta em tamanho grande.",
    "No seu turno faça UMA destas ações:\n\n"
    "{H} Jogar uma carta da mão: use a ação de cima dela e descarte-a.\n"
    "{M} Jogar uma carta do seu módulo (veja a próxima página).\n"
    "{M} Jogar uma carta do módulo de outro jogador.\n"
    "{W} Trazer 1 {C} seu da Terra para a nave.\n"
    "{E} Descartar 1 carta (mão ou módulo) para ganhar 1 energia (máx. 7).",
    "A carta é conectada ao edifício da mesma cor, por cima das anteriores. Depois, na ordem:\n\n"
    "1. Se o valor dela for MAIOR que o da carta de baixo (ou que 3, se for a primeira), você pode pousar 1 {C} da nave no {S} desse edifício.\n"
    "2. Você pode usar a ação de baixo {M} da carta.\n"
    "3. Você pode usar a habilidade do edifício.\n\n"
    "Se jogar a carta do módulo de OUTRO jogador: não há pouso; o DONO da carta pode usar a ação {M}, e você usa a habilidade do edifício.",
    "No fim de cada turno, todos (começando por quem jogou) ficam com exatamente 4 cartas: 2 na mão e 2 no módulo.\n\n"
    "Mais de 4: descarte da mão. Menos de 4: compre do baralho. Módulo com menos de 2: passe cartas da mão para o módulo (nunca o contrário!).\n\n"
    "Se o baralho acabar, as cartas cobertas dos edifícios e o descarte formam um novo baralho.",
    "{0} Ecossistema: mova qualquer {C} de qualquer jogador entre {S} e {D} num edifício.\n"
    "{1} Energia: ganhe 1 {E}.\n"
    "{2} Ciência: traga 1 {C} seu da Terra para a nave.\n"
    "{3} Água: coloque 1 carta sua no fundo do baralho.\n"
    "{4} Construção: mova 1 {C} da nave para o {S} de qualquer edifício (não abriga colonos).\n\n"
    "Espaço {D}: máx. 1 colono com 2 jogadores, 2 colonos com 3-4 jogadores.",
    "Ao final de uma rodada, se alguém tiver os 7 colonos em Marte, o jogo acaba.\n\n"
    "1 ponto por {C} na nave, 2 por {C} em {S}, 4 por {C} em {D}.\n"
    "+2 por ter colonos nos 4 edifícios. +3 por ter 4+ colonos num mesmo edifício.\n"
    "+1 para quem tiver mais energia (empates: todos ganham).\n"
    "Colonos na Terra não pontuam. Empate: mais colonos em Marte.",
    "{X} Confirmar   {O} Voltar / desfazer jogada   {T} Inspecionar tabuleiro\n"
    "Direcional: navegar   L1/R1: rolar listas   START: pausa\n\n"
    "No PC: setas, Enter/Z (confirmar), Esc/X (voltar), Tab (inspecionar), P (pausa).\n\n"
    "Dica: a coluna de pontos de cada jogador mostra a pontuação se o jogo acabasse agora."
    },
    {
    "Each player commands a space agency with 7 colonists. Use project cards to bring colonists from Earth to your ship {R} and from the ship to the buildings on Mars.\n\n"
    "Whoever places their colonists best scores the most points and wins. Colonists on {D} spaces are worth more, but there are few slots!",
    "Each player starts with 1 {C} on the ship, 6 {C} on Earth and 1 {E} energy.\n\n"
    "Everyone gets 4 project cards and picks 2 for their {M} prep module (face down; others only see the color). The other 2 stay in your {H} hand.\n\n"
    "Draft variant: everyone gets 5 cards, keeps 1 and passes the rest, until they have 4.",
    "Each project card has a COLOR (the building where it is connected), a VALUE from 0 to 7 and two actions.\n\n"
    "{H} HAND ACTION: used when you play the card from your hand.\n"
    "{M} MODULE ACTION: used when the card is connected to a building from a module.\n\n"
    "The value only matters from the module: if it beats the building's visible value, you land a colonist.\n\n"
    "Press {Q} during the game to see any card full size.",
    "On your turn, do ONE of these actions:\n\n"
    "{H} Play a card from your hand: use its top action and discard it.\n"
    "{M} Play a card from your module (see next page).\n"
    "{M} Play a card from another player's module.\n"
    "{W} Bring 1 of your {C} from Earth to the ship.\n"
    "{E} Discard 1 card (hand or module) to gain 1 energy (max. 7).",
    "The card is connected to the building of the same color, on top of the previous ones. Then, in order:\n\n"
    "1. If its value is HIGHER than the card below (or than 3, if it is the first), you may land 1 {C} from your ship on that building's {S}.\n"
    "2. You may use the card's bottom {M} action.\n"
    "3. You may use the building's ability.\n\n"
    "If you play a card from ANOTHER player's module: no landing; the card's OWNER may use the {M} action, and you use the building's ability.",
    "At the end of each turn, everyone (starting with whoever played) ends with exactly 4 cards: 2 in hand and 2 in the module.\n\n"
    "More than 4: discard from hand. Fewer than 4: draw from the deck. Module with fewer than 2: move cards from hand to module (never the other way!).\n\n"
    "If the deck runs out, the covered cards in the buildings and the discard pile form a new deck.",
    "{0} Ecosystem: move any {C} of any player between {S} and {D} in a building.\n"
    "{1} Energy: gain 1 {E}.\n"
    "{2} Science: bring 1 of your {C} from Earth to the ship.\n"
    "{3} Water: put 1 of your cards at the bottom of the deck.\n"
    "{4} Construction: move 1 {C} from the ship to the {S} of any building (holds no colonists).\n\n"
    "{D} space: max. 1 colonist with 2 players, 2 colonists with 3-4 players.",
    "At the end of a round, if someone has all 7 colonists on Mars, the game ends.\n\n"
    "1 point per {C} on the ship, 2 per {C} on {S}, 4 per {C} on {D}.\n"
    "+2 for having colonists in all 4 buildings. +3 for having 4+ colonists in one building.\n"
    "+1 for whoever has the most energy (ties: all score).\n"
    "Colonists on Earth score nothing. Tie: most colonists on Mars.",
    "Controls"
    },
    {
    "Cada jugador dirige una agencia espacial con 7 colonos. Usa cartas de proyecto para llevar colonos de la Tierra a tu nave {R} y de la nave a los edificios de Marte.\n\n"
    "Quien mejor reparta sus colonos sumará más puntos y ganará. Los colonos en espacios {D} valen más, ¡pero hay pocas plazas!",
    "Cada jugador empieza con 1 {C} en la nave, 6 {C} en la Tierra y 1 {E} de energía.\n\n"
    "Cada uno recibe 4 cartas de proyecto y elige 2 para su {M} módulo de preparación (boca abajo; los demás solo ven el color). Las otras 2 quedan en la {H} mano.\n\n"
    "Variante draft: cada uno recibe 5 cartas, se queda 1 y pasa el resto, hasta tener 4.",
    "Cada carta de proyecto tiene un COLOR (el edificio donde se conecta), un VALOR de 0 a 7 y dos acciones.\n\n"
    "{H} ACCIÓN DE MANO: se usa cuando juegas la carta desde la mano.\n"
    "{M} ACCIÓN DE MÓDULO: se usa cuando la carta se conecta a un edificio desde un módulo.\n\n"
    "El valor solo importa en el módulo: si supera el valor visible del edificio, aterrizas un colono.\n\n"
    "Pulsa {Q} durante el juego para ver cualquier carta en grande.",
    "En tu turno haz UNA de estas acciones:\n\n"
    "{H} Jugar una carta de la mano: usa su acción de arriba y descártala.\n"
    "{M} Jugar una carta de tu módulo (ver la página siguiente).\n"
    "{M} Jugar una carta del módulo de otro jugador.\n"
    "{W} Traer 1 {C} tuyo de la Tierra a la nave.\n"
    "{E} Descartar 1 carta (mano o módulo) para ganar 1 energía (máx. 7).",
    "La carta se conecta al edificio del mismo color, encima de las anteriores. Después, en orden:\n\n"
    "1. Si su valor es MAYOR que el de la carta de abajo (o que 3, si es la primera), puedes aterrizar 1 {C} de la nave en el {S} de ese edificio.\n"
    "2. Puedes usar la acción de abajo {M} de la carta.\n"
    "3. Puedes usar la habilidad del edificio.\n\n"
    "Si juegas la carta del módulo de OTRO jugador: no hay aterrizaje; el DUEÑO de la carta puede usar la acción {M}, y tú usas la habilidad del edificio.",
    "Al final de cada turno, todos (empezando por quien jugó) quedan con exactamente 4 cartas: 2 en la mano y 2 en el módulo.\n\n"
    "Más de 4: descarta de la mano. Menos de 4: roba del mazo. Módulo con menos de 2: pasa cartas de la mano al módulo (¡nunca al revés!).\n\n"
    "Si se acaba el mazo, las cartas cubiertas de los edificios y el descarte forman un mazo nuevo.",
    "{0} Ecosistema: mueve cualquier {C} de cualquier jugador entre {S} y {D} en un edificio.\n"
    "{1} Energía: gana 1 {E}.\n"
    "{2} Ciencia: trae 1 {C} tuyo de la Tierra a la nave.\n"
    "{3} Agua: pon 1 carta tuya al fondo del mazo.\n"
    "{4} Construcción: mueve 1 {C} de la nave al {S} de cualquier edificio (no aloja colonos).\n\n"
    "Espacio {D}: máx. 1 colono con 2 jugadores, 2 colonos con 3-4 jugadores.",
    "Al final de una ronda, si alguien tiene sus 7 colonos en Marte, el juego termina.\n\n"
    "1 punto por {C} en la nave, 2 por {C} en {S}, 4 por {C} en {D}.\n"
    "+2 por tener colonos en los 4 edificios. +3 por tener 4+ colonos en un mismo edificio.\n"
    "+1 para quien tenga más energía (empates: todos ganan).\n"
    "Los colonos en la Tierra no puntúan. Empate: más colonos en Marte.",
    "Controles"
    },
};
#define RULES_TITLE RULES_TITLES[g_lang]
#define RULES_TEXT RULES_TEXTS[g_lang]
#define NRULES 9

static void draw_rules(void) {
    draw_space();
    char t[48]; snprintf(t, sizeof t, "%d / %d", rulesPage + 1, NRULES);
    int controls = (rulesPage == NRULES - 1);
    if (controls) {
        /* tela de controles: usa a textura TITLE (botoes); titulo com fonte normal ampliada */
        r_text_scale(2.0f);
        r_text(FONT_MED, 30, 14, TR("COMO JOGAR", "HOW TO PLAY", "CÓMO JUGAR"), RGBA(255, 230, 210, 255), 0);
        r_text_scale(1.0f);
    } else {
        r_chrome(30, 18, 28, TR("COMO JOGAR", "HOW TO PLAY", "CÓMO JUGAR"), RGBA(255, 210, 180, 255));
    }
    r_text(FONT_MED, 600 - r_text_w(FONT_MED, t), 28, t, RGBA(220, 200, 190, 255), 0);
    panel(24, 64, 592, 340, RGBA(200, 90, 60, 255));
    r_text(FONT_MED, 40, 76, RULES_TITLE[rulesPage], RGBA(255, 190, 140, 255), 0);
    if (controls) {
        const struct { int pad; int x, y; const char *txt; } C[] = {
            { PAD_DPAD, 46, 110, TR("Navegar", "Navigate", "Navegar") }, { PAD_CROSS, 330, 104, TR("Confirmar", "Confirm", "Confirmar") },
            { PAD_CIRCLE, 330, 150, TR("Voltar / desfazer", "Back / undo", "Volver / deshacer") }, { PAD_TRI, 330, 196, TR("Inspecionar tabuleiro", "Inspect board", "Inspeccionar tablero") },
            { PAD_SQUARE, 330, 242, TR("Ver carta em tamanho grande", "View card full size", "Ver carta en grande") }, { PAD_L1, 40, 240, TR("Rolar lista", "Scroll list", "Desplazar lista") },
            { PAD_R1, 150, 240, TR("Rolar lista", "Scroll list", "Desplazar lista") }, { PAD_START, 46, 304, TR("Pausa", "Pause", "Pausa") },
        };
        for (unsigned i = 0; i < sizeof C / sizeof C[0]; i++) {
            const SprRect *r = &TITLE_PAD[C[i].pad];
            r_title(r, C[i].x, C[i].y, r->w, r->h, WHITE);
            if (C[i].pad == PAD_DPAD) r_text(FONT_SMALL, C[i].x + r->w + 10, C[i].y + 40, C[i].txt, WHITE, 0);
            else if (C[i].pad == PAD_L1 || C[i].pad == PAD_R1)
                r_text(FONT_SMALL, C[i].x + 4, C[i].y + r->h + 2, C[i].txt, WHITE, 0);
            else r_text(FONT_SMALL, C[i].x + r->w + 10, C[i].y + 13, C[i].txt, WHITE, 0);
        }
        r_text_wrap(FONT_SMALL, 40, 372, 560, TR("Controle 1 comanda o jogador 1; controle 2 comanda os jogadores 2 a 4. No PC: setas, Enter/Z (confirmar), Esc/X (voltar), Tab (inspecionar), R (ver carta), P (pausa).", "Controller 1 drives player 1; controller 2 drives players 2-4. On PC: arrows, Enter/Z (confirm), Esc/X (back), Tab (inspect), R (view card), P (pause).", "El mando 1 controla al jugador 1; el mando 2, a los jugadores 2 a 4. En PC: flechas, Enter/Z (confirmar), Esc/X (volver), Tab (inspeccionar), R (ver carta), P (pausa)."),
                    RGBA(200, 200, 210, 255), 15);
    } else if (rulesPage == 2) {
        /* anatomia da carta */
        draw_card_full(44, 100, 196, 14);
        r_text_wrap(FONT_SMALL, 264, 104, 336, RULES_TEXT[rulesPage], RGBA(235, 235, 235, 255), 17);
    } else {
        r_text_wrap(FONT_SMALL, 40, 104, 560, RULES_TEXT[rulesPage], RGBA(235, 235, 235, 255), 17);
        const SprRect *m = &ART_MASC[rulesPage % 2 ? MASC_ROBOT : MASC_POINT];
        r_art(m, 600 - m->w, 400 - m->h - 4, m->w, m->h, WHITE);
    }
    r_text(FONT_SMALL, 30, 418, TR("< > Página   {O} Voltar", "< > Page   {O} Back", "< > Página   {O} Volver"), RGBA(220, 220, 230, 255), 0);
}

static void update_rules(void) {
    if (pressed[B_RIGHT] || pressed[B_OK] || pressed[B_R1]) {
        if (rulesPage < NRULES - 1) { rulesPage++; audio_sfx(SFX_MOVE); }
        else if (pressed[B_OK]) { scene = rulesReturn; audio_sfx(SFX_BACK); }
    }
    if ((pressed[B_LEFT] || pressed[B_L1]) && rulesPage > 0) { rulesPage--; audio_sfx(SFX_MOVE); }
    if (pressed[B_BACK] || pressed[B_START]) { scene = rulesReturn; audio_sfx(SFX_BACK); }
}

static void draw_credits(void) {
    draw_space();
    r_mars(470, 90, 120);
    r_chrome(30, 22, 30, TR("CRÉDITOS", "CREDITS", "CRÉDITOS"), RGBA(255, 210, 180, 255));
    panel(24, 70, 400, 320, RGBA(200, 90, 60, 255));
    r_text_wrap(FONT_SMALL, 40, 86, 370,
        TR("MARS COLONY: PROJECT RED PS2\n"
        "Homebrew para PlayStation 2 feito com PS2SDK, gsKit e SDL2.\n\n"
        "Mecânicas adaptadas das regras do jogo de cartas Pocket Mars (design de Michał Jagodziński, Board&Dice), "
        "fornecidas pelo jogador. Nomes, cartas, efeitos e código deste jogo são originais.\n\n"
        "Arte das cartas, personagens, logotipo e fonte cromada: fornecidos pelo jogador.\n"
        "Fonte de texto: DejaVu Sans (licença livre).\n"
        "Música: 3 trilhas espaciais originais (Horizonte Vermelho, Colônia à Deriva e fanfarra de vitória) e efeitos de ficção científica, sintetizados em tempo real.\n\n"
        "Projeto de fã, sem fins comerciais.",
        "MARS COLONY: PROJECT RED PS2\n"
        "PlayStation 2 homebrew made with PS2SDK, gsKit and SDL2.\n\n"
        "Mechanics adapted from the rules of the card game Pocket Mars (designed by Michał Jagodziński, Board&Dice), "
        "provided by the player. Names, cards, effects and code of this game are original.\n\n"
        "Card art, characters, logo and chrome font: provided by the player.\n"
        "Text font: DejaVu Sans (free license).\n"
        "Music: 3 original space tracks (Red Horizon, Drifting Colony and victory fanfare) and sci-fi effects, synthesized in real time.\n\n"
        "Fan project, non-commercial.",
        "MARS COLONY: PROJECT RED PS2\n"
        "Homebrew para PlayStation 2 hecho con PS2SDK, gsKit y SDL2.\n\n"
        "Mecánicas adaptadas de las reglas del juego de cartas Pocket Mars (diseño de Michał Jagodziński, Board&Dice), "
        "proporcionadas por el jugador. Nombres, cartas, efectos y código de este juego son originales.\n\n"
        "Arte de las cartas, personajes, logotipo y fuente cromada: proporcionados por el jugador.\n"
        "Fuente de texto: DejaVu Sans (licencia libre).\n"
        "Música: 3 pistas espaciales originales (Horizonte Rojo, Colonia a la Deriva y fanfarria de victoria) y efectos de ciencia ficción, sintetizados en tiempo real.\n\n"
        "Proyecto de fan, sin fines comerciales."),
        RGBA(235, 235, 235, 255), 16);
    const SprRect *a = &ART_MASC[MASC_CHEER], *b = &ART_MASC[MASC_ROBOT];
    r_art(a, 440, 300, a->w * 3 / 2, a->h * 3 / 2, WHITE);
    r_art(b, 540, 320, b->w * 5 / 4, b->h * 5 / 4, WHITE);
    r_text(FONT_SMALL, 30, 418, TR("{O} Voltar", "{O} Back", "{O} Volver"), RGBA(220, 220, 230, 255), 0);
}

/* ------------------------------------------------------------------ */
/* testes automatizados (somente PC)                                     */

static SDL_Window *win;

static void present(void) { SDL_RenderPresent(R); }

static void draw_scene(void) {
    switch (scene) {
    case SC_TITLE: draw_title(); break;
    case SC_LANG: draw_lang(); break;
    case SC_SETUP: draw_setup(); break;
    case SC_GAME: draw_game(); break;
    case SC_OVER: draw_over(); break;
    case SC_RULES: draw_rules(); break;
    case SC_CREDITS: draw_credits(); break;
    }
}

#ifndef PS2
static void shot(const char *dir, const char *name) {
    SDL_SetRenderDrawColor(R, 0, 0, 0, 255);
    SDL_RenderClear(R);
    draw_scene();
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_W, SCREEN_H, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_RenderReadPixels(R, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
    char path[512]; snprintf(path, sizeof path, "%s/%s.bmp", dir, name);
    SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
    printf("screenshot: %s\n", path);
}

/* avanca a partida com a IA ate uma decisao humana na fase desejada */
static void run_until_human(int phase, int maxSteps) {
    for (int i = 0; i < maxSteps && G.phase != PH_OVER; i++) {
        nopts = game_options(&G, opts);
        if (G.p[G.decider].human && (phase < 0 || G.phase == phase)) break;
        int k = ai_choose(&G, &aiSeed);
        apply_option(k);
    }
    nopts = game_options(&G, opts); optsDirty = 0;
    viewer = 0;
}

/* teste do roteamento de controles: dois controles virtuais, dois humanos */
static void push_pad(SDL_JoystickID id, int button) {
    SDL_Event e; memset(&e, 0, sizeof e);
    e.type = SDL_CONTROLLERBUTTONDOWN; e.cbutton.which = id; e.cbutton.button = (Uint8)button; SDL_PushEvent(&e);
    e.type = SDL_CONTROLLERBUTTONUP; SDL_PushEvent(&e);
}
static int pad_step(SDL_JoystickID id, int button) {
    poll_input(); update_game();   /* quadro sem entrada: estabiliza (ex.: cortina) */
    Game before = G;
    int cBefore = curtain;
    push_pad(id, button);
    SDL_PumpEvents();
    poll_input();       /* processa DOWN e UP: o "pressed" do quadro fica marcado */
    update_game();
    return memcmp(&before, &G, sizeof G) != 0 || cBefore != curtain;
}
static int padtest(void) {
    padId[0] = 100; padId[1] = 101;
    cfgNp = 2; cfgSlot[0] = 0; cfgSlot[1] = 0; cfgDraft = 0; start_game();
    int ok = 1;
    /* preparacao: jogador 1 (indice 0) escolhe cartas */
    update_game();
    printf("decide jogador %d (fase %d)\n", G.decider + 1, G.phase);
    int r = pad_step(101, SDL_CONTROLLER_BUTTON_A);
    printf("controle 2 aperta X na vez do jogador 1: %s\n", r ? "AGIU (ERRO)" : "ignorado (ok)");
    ok &= !r;
    if (curtain) pad_step(100, SDL_CONTROLLER_BUTTON_A);
    r = pad_step(100, SDL_CONTROLLER_BUTTON_A);
    printf("controle 1 aperta X na vez do jogador 1: %s\n", r ? "agiu (ok)" : "IGNORADO (ERRO)");
    ok &= r;
    /* avanca ate a vez do jogador 2 */
    for (int i = 0; i < 20 && G.decider == 0; i++) { if (curtain) pad_step(100, SDL_CONTROLLER_BUTTON_A); pad_step(100, SDL_CONTROLLER_BUTTON_A); }
    printf("decide jogador %d, cortina %d\n", G.decider + 1, curtain);
    r = pad_step(100, SDL_CONTROLLER_BUTTON_A);
    printf("controle 1 aperta X na vez do jogador 2: %s\n", r ? "AGIU (ERRO)" : "ignorado (ok)");
    ok &= !r;
    r = pad_step(101, SDL_CONTROLLER_BUTTON_A);
    printf("controle 2 aperta X na vez do jogador 2: %s\n", r ? "agiu (ok)" : "IGNORADO (ERRO)");
    ok &= r;
    /* sem controle 2: controle 1 comanda todos */
    padId[1] = -1;
    if (G.decider == 1) {
        r = pad_step(100, SDL_CONTROLLER_BUTTON_A);
        printf("sem controle 2, controle 1 na vez do jogador 2: %s\n", r ? "agiu (ok)" : "IGNORADO (ERRO)");
        ok &= r;
    }
    printf(ok ? "PADTEST OK\n" : "PADTEST FALHOU\n");
    return ok ? 0 : 1;
}

static int autotest(const char *dir) {
    frame = 100;
    scene = SC_LANG; shot(dir, "00_idioma");
    scene = SC_TITLE; shot(dir, "01_titulo");
    scene = SC_SETUP; setupSel = 1; cfgNp = 3; cfgSlot[0] = 0; cfgSlot[1] = 2; cfgSlot[2] = 3; shot(dir, "02_config");
    start_game();
    G.p[0].ailevel = 2;
    run_until_human(PH_ACTION, 2000);
    shot(dir, "03_jogo_acao");
    /* algumas rodadas depois */
    for (int r = 0; r < 4; r++) {
        Option o = opts[0]; (void)o;
        int k = ai_choose(&G, &aiSeed); apply_option(k);
        run_until_human(PH_ACTION, 2000);
    }
    optSel = 1; shot(dir, "04_jogo_meio");
    inspect = 1; inspectSel = 0; shot(dir, "05_inspecionar");
    inspect = 0;
    zoomCard = focused_card(); if (zoomCard < 0) zoomCard = 12;
    shot(dir, "05b_zoom_carta"); zoomCard = -1;
    popupCard = 26; popupUntil = SDL_GetTicks() + 100000; snprintf(popupText, sizeof popupText, "Azul joga da mão");
    G.decider = 1; G.p[1].human = 0; shot(dir, "05c_cpu_revela"); G.p[1].human = 0; popupCard = -1;
    G.decider = 0;
    /* procurar uma decisao de efeito/habilidade humana */
    for (int i = 0; i < 3000 && G.phase != PH_OVER; i++) {
        nopts = game_options(&G, opts);
        if (G.p[G.decider].human && (G.phase == PH_FX || G.phase == PH_ABILITY || G.phase == PH_CONNECT_MOVE)) break;
        apply_option(ai_choose(&G, &aiSeed));
    }
    nopts = game_options(&G, opts); viewer = 0;
    shot(dir, "06_jogo_efeito");
    autoplayHuman = 1;
    G.p[1].human = 0;
    for (int i = 0; i < 5000 && G.phase != PH_OVER; i++) { nopts = game_options(&G, opts); apply_option(ai_choose(&G, &aiSeed)); }
    autoplayHuman = 0;
    scene = SC_OVER; shot(dir, "07_fim");
    scene = SC_RULES; rulesPage = 2; shot(dir, "08_regras_cartas");
    rulesPage = 6; shot(dir, "09_regras_edificios");
    rulesPage = 8; shot(dir, "09b_controles");
    scene = SC_CREDITS; shot(dir, "10_creditos");
    /* ingles e espanhol */
    for (int L = 1; L <= 2; L++) {
        g_lang = L;
        char nm[32];
        scene = SC_TITLE; snprintf(nm, sizeof nm, "12_titulo_%s", L == 1 ? "en" : "es"); shot(dir, nm);
        scene = SC_GAME; viewer = 0; curtain = 0; nopts = game_options(&G, opts);
        snprintf(nm, sizeof nm, "13_jogo_%s", L == 1 ? "en" : "es"); shot(dir, nm);
        zoomCard = 20; snprintf(nm, sizeof nm, "14_carta_%s", L == 1 ? "en" : "es"); shot(dir, nm); zoomCard = -1;
        scene = SC_SETUP; snprintf(nm, sizeof nm, "15_config_%s", L == 1 ? "en" : "es"); shot(dir, nm);
        scene = SC_RULES; rulesPage = 4; snprintf(nm, sizeof nm, "16_regras_%s", L == 1 ? "en" : "es"); shot(dir, nm);
        scene = SC_OVER; snprintf(nm, sizeof nm, "17_fim_%s", L == 1 ? "en" : "es"); shot(dir, nm);
    }
    g_lang = 0;
    /* hot-seat: cortina */
    cfgNp = 2; cfgSlot[0] = 0; cfgSlot[1] = 0; start_game();
    nopts = game_options(&G, opts);
    viewer = 0; curtain = 0; apply_option(0); apply_option(0);
    nopts = game_options(&G, opts); curtain = 1;
    scene = SC_GAME; padId[1] = 7; shot(dir, "11_cortina"); padId[1] = -1;
    return 0;
}
#endif

/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    const char *autodir = NULL;
    int demo = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--autotest") && i + 1 < argc) autodir = argv[++i];
        else if (!strcmp(argv[i], "--demo")) demo = 1;
#ifndef PS2
        else if (!strcmp(argv[i], "--padtest")) autodir = "padtest";
#endif   /* partida so de CPUs (testes) */
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
        printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
#ifdef PS2
    win = SDL_CreateWindow("Mars Colony: Project Red PS2", 0, 0, SCREEN_W, SCREEN_H, SDL_WINDOW_FULLSCREEN);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
#else
    win = SDL_CreateWindow("Mars Colony: Project Red PS2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           autodir ? SCREEN_W : SCREEN_W * 2, autodir ? SCREEN_H : SCREEN_H * 2, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, autodir ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
#endif
    if (!win || !ren) { printf("janela/renderer: %s\n", SDL_GetError()); return 1; }
    SDL_RenderSetLogicalSize(ren, SCREEN_W, SCREEN_H);
    if (!r_init(ren)) { printf("falha ao criar texturas: %s\n", SDL_GetError()); return 1; }
    init_stars();
    for (int i = 0; i < SDL_NumJoysticks(); i++) open_pad(i);

#ifndef PS2
    if (autodir && !strcmp(autodir, "padtest")) return padtest();
    if (autodir) return autotest(autodir);
#else
    (void)autodir;
#endif
    audio_init();
    audio_volume(cfgMusic, cfgSfx);
    ai_idle = audio_pump;
    int lastScene = -1;
    Uint32 demoEnd = 0;
    if (demo) {
        cfgNp = 3; cfgSlot[0] = cfgSlot[1] = cfgSlot[2] = 2; cfgSpeed = 1;
        start_game();
    }

    while (!quitReq) {
        Uint32 t0 = SDL_GetTicks();
        audio_pump();
        poll_input();
        switch (scene) {
        case SC_TITLE: update_title(); break;
        case SC_LANG: update_lang(); break;
        case SC_SETUP: update_setup(); break;
        case SC_GAME: update_game(); break;
        case SC_OVER: update_over(); break;
        case SC_RULES: update_rules(); break;
        case SC_CREDITS: if (pressed[B_BACK] || pressed[B_OK]) { scene = SC_TITLE; audio_sfx(SFX_BACK); } break;
        }
        if (scene != lastScene) {
            /* trilha por cena; a fanfarra de vitoria e disparada pelo SFX_WIN */
            if (scene == SC_GAME) audio_music(TRACK_GAME);
            else if (scene == SC_RULES && rulesReturn == SC_GAME) { /* mantem a trilha da partida */ }
            else if (scene != SC_OVER) audio_music(TRACK_TITLE);
            lastScene = scene;
        }
        SDL_SetRenderDrawColor(R, 0, 0, 0, 255);
        SDL_RenderClear(R);
        draw_scene();
        audio_pump();
        present();
        frame++;
        if (demo && scene == SC_OVER) {
            if (!demoEnd) demoEnd = SDL_GetTicks() + 6000;
            else if (SDL_GetTicks() > demoEnd) quitReq = 1;
        }
        Uint32 dt = SDL_GetTicks() - t0;
        if (dt < 16) SDL_Delay(16 - dt);
    }
    audio_quit();
    SDL_Quit();
    return 0;
}
