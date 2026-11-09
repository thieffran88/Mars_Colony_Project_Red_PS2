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
static int held[NB], pressed[NB];
static Uint32 repeatAt[NB];
static int quitReq;

static void press(int b, int down) {
    if (b < 0) return;
    if (down && !held[b]) { pressed[b] = 1; repeatAt[b] = SDL_GetTicks() + 320; }
    held[b] = down;
}

static int key_button(SDL_Keycode k) {
    switch (k) {
    case SDLK_UP: return B_UP;       case SDLK_DOWN: return B_DOWN;
    case SDLK_LEFT: return B_LEFT;   case SDLK_RIGHT: return B_RIGHT;
    case SDLK_RETURN: case SDLK_z: case SDLK_SPACE: return B_OK;
    case SDLK_ESCAPE: case SDLK_x: case SDLK_BACKSPACE: return B_BACK;
    case SDLK_TAB: case SDLK_i: return B_INFO;
    case SDLK_p: case SDLK_F1: return B_START;
    case SDLK_r: return B_SELECT;
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
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT: quitReq = 1; break;
        case SDL_KEYDOWN: if (!e.key.repeat) press(key_button(e.key.keysym.sym), 1); break;
        case SDL_KEYUP: press(key_button(e.key.keysym.sym), 0); break;
        case SDL_CONTROLLERDEVICEADDED: SDL_GameControllerOpen(e.cdevice.which); break;
        case SDL_CONTROLLERBUTTONDOWN: press(pad_button(e.cbutton.button), 1); break;
        case SDL_CONTROLLERBUTTONUP: press(pad_button(e.cbutton.button), 0); break;
        case SDL_CONTROLLERAXISMOTION: {
            int v = e.caxis.value;
            if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                press(B_UP, v < -20000); press(B_DOWN, v > 20000);
            } else if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
                press(B_LEFT, v < -20000); press(B_RIGHT, v > 20000);
            }
            break; }
        }
    }
    Uint32 now = SDL_GetTicks();
    for (int b = B_UP; b <= B_RIGHT; b++)
        if (held[b] && now >= repeatAt[b]) { pressed[b] = 1; repeatAt[b] = now + 90; }
}

/* ------------------------------------------------------------------ */
/* estado da interface                                                  */

enum { SC_TITLE, SC_SETUP, SC_GAME, SC_OVER, SC_RULES, SC_CREDITS };
static int scene = SC_TITLE, rulesReturn = SC_TITLE;
static Uint32 frame;

/* configuracao */
static int cfgNp = 2;
static int cfgSlot[MAXP] = { 0, 2, 2, 2 };   /* 0 humano, 1 facil, 2 normal, 3 dificil */
static int cfgDraft = 0, cfgSpeed = 0;
static int setupSel = 0, titleSel = 0;
static const char *SLOT_NAME[4] = { "Humano", "CPU Fácil", "CPU Normal", "CPU Difícil" };
static const char *SPEED_NAME[3] = { "Normal", "Rápida", "Instantânea" };

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
    r_rrect(x, y, w, h, c_mul(base, dim ? 0.35f : 0.55f));
    r_rect(x + 2, y + 2, w - 4, 16, c_mul(base, dim ? 0.5f : 1.0f));
    char v[4]; snprintf(v, sizeof v, "%d", cd->value);
    r_text(FONT_MED, x + 4, y + 1, v, RGBA(255, 255, 255, 255), 0);
    if (w >= 34) r_text(FONT_MED, x + w - 4 - r_text_w(FONT_MED, v), y + 1, v, RGBA(255, 255, 255, 160), 0);
    int is = w >= 50 ? 24 : 16;
    r_icon(IC_LEAF + cd->color, x + (w - is) / 2, y + 22, is, RGBA(255, 255, 255, dim ? 90 : 210));
    if (w >= 50 && h >= 70)
        r_text_wrap(FONT_SMALL, x + 4, y + 22 + is + 2, w - 8, cd->name, RGBA(240, 240, 240, dim ? 120 : 255), 12);
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

static const char *ABILITY_SHORT[5] = {
    "Mova {C} {S}<>{D}", "Ganhe 1 {E}", "1 {C} da Terra", "Carta ao fundo", "{C} nave > {S}"
};

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
    r_text(FONT_SMALL, x + 21, y + 3, COLOR_NAME[b], RGBA(255, 255, 255, 255), w - 44);
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
        r_text_wrap(FONT_SMALL, x + 6, cy + 4, w - 12, "Não abriga colonos. Recebe cartas de construção.", RGBA(190, 190, 200, 255), 13);
    }
    /* habilidade */
    int ay = y + 95;
    r_text(FONT_SMALL, x + 5, ay, ABILITY_SHORT[b], RGBA(255, 230, 170, 255), w - 10);
    /* pilha */
    int sy = y + 112;
    if (B->nstack == 0) {
        r_frame(x + 6, sy, 30, 42, RGBA(255, 255, 255, 50));
        r_text(FONT_SMALL, x + 42, sy + 6, b == C_CONSTR ? "Sem cartas" : "Valor base 3", RGBA(170, 170, 180, 255), w - 46);
    } else {
        int top = B->stack[B->nstack - 1];
        draw_card_mini(x + 6, sy, 30, 42, top, 0, 0);
        char t[64];
        r_text(FONT_SMALL, x + 42, sy + 2, CARDS[top].name, RGBA(255, 255, 255, 255), w - 46);
        int n = 0, off = snprintf(t, sizeof t, "Antes:");
        for (int i = B->nstack - 2; i >= 0 && n < 6; i--, n++) off += snprintf(t + off, sizeof t - off, " %d", CARDS[B->stack[i]].value);
        if (B->nstack > 1) r_text(FONT_SMALL, x + 42, sy + 17, t, RGBA(170, 170, 180, 255), w - 46);
        snprintf(t, sizeof t, "%d carta%s", B->nstack, B->nstack > 1 ? "s" : "");
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
        if (P->human) snprintf(t, sizeof t, "%s", PLAYER_NAME[p]);
        else snprintf(t, sizeof t, "%s (CPU)", PLAYER_NAME[p]);
        r_text(FONT_MED, x + 9, y + 2, t, COL_PLAYER[p], w - 60);
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
        snprintf(buf, sizeof buf, "%s  {%d} valor %d", cd->name, cd->color, cd->value);
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
    if (b == C_CONSTR) snprintf(buf, n, "%s - %s", BLD_NAME[b], BLD_ABILITY[b]);
    else snprintf(buf, n, "%s (valor visível %d) - %s", BLD_NAME[b], game_top_value(&G, b), BLD_ABILITY[b]);
}

static void draw_my_cards(int x, int y, int w, int h, int hlHand, int hlPrep) {
    panel(x, y, w, h, RGBA(90, 80, 110, 255));
    if (viewer < 0 || curtain) {
        r_text_center(FONT_MED, x + w / 2, y + h / 2 - 20, "Cartas ocultas", RGBA(160, 160, 170, 255));
        return;
    }
    Player *P = &G.p[viewer];
    char t[64];
    snprintf(t, sizeof t, "Cartas de %s", PLAYER_NAME[viewer]);
    r_text(FONT_SMALL, x + 6, y + 3, t, COL_PLAYER[viewer], 0);
    int n = P->nhand + P->nprep;
    int isDraft = G.phase == PH_DRAFT && G.decider == viewer;
    if (isDraft) n += G.npool[viewer];
    if (n == 0) return;
    int avail = w - 12 - 10;
    int cw = (avail - (n - 1) * 4) / n; if (cw > 58) cw = 58;
    int ch = cw * 4 / 3 + 4; if (ch > 82) ch = 82;
    int cx = x + 6, cy = y + 20;
    r_text(FONT_SMALL, cx, cy + ch + 2, "{H} mão", RGBA(170, 170, 180, 255), 0);
    for (int i = 0; i < P->nhand; i++, cx += cw + 4) draw_card_mini(cx, cy, cw, ch, P->hand[i], i == hlHand, 0);
    cx += 6;
    if (P->nprep) r_text(FONT_SMALL, cx, cy + ch + 2, "{M} módulo", RGBA(170, 170, 180, 255), 0);
    for (int i = 0; i < P->nprep; i++, cx += cw + 4) draw_card_mini(cx, cy, cw, ch, P->prep[i], i == hlPrep, 0);
    if (isDraft) {
        cx += 6;
        r_text(FONT_SMALL, cx, cy + ch + 2, "lote", RGBA(255, 220, 140, 255), 0);
        int sel = (optSel < nopts && opts[optSel].kind == OPT_DRAFT) ? opts[optSel].a : -1;
        for (int i = 0; i < G.npool[viewer]; i++, cx += cw + 4) draw_card_mini(cx, cy, cw, ch, G.pool[viewer][i], i == sel, 0);
    }
}

static const char *phase_prompt(char *buf, int n) {
    int d = G.decider;
    switch (G.phase) {
    case PH_DRAFT: snprintf(buf, n, "Draft: fique com 1 carta (%d de 4)", G.draftPick + 1); break;
    case PH_SETUP_PREP: snprintf(buf, n, "Preparação: carta para o {M} módulo (%d/2)", G.p[d].nprep + 1); break;
    case PH_ACTION: snprintf(buf, n, "Sua vez! Escolha 1 ação"); break;
    case PH_FX: snprintf(buf, n, "%s \"%s\"", G.ctxBottom ? "{M}" : "{H}", CARDS[(int)G.ctxCard].name); break;
    case PH_CONNECT_MOVE: {
        int under = G.b[(int)G.ctxBld].nstack >= 2 ? CARDS[G.b[(int)G.ctxBld].stack[G.b[(int)G.ctxBld].nstack - 2]].value : 3;
        snprintf(buf, n, "Valor %d supera %d! Pousar colono?", CARDS[(int)G.ctxCard].value, under); break; }
    case PH_ABILITY: snprintf(buf, n, "Habilidade {%d} %s", G.ctxBld, COLOR_NAME[(int)G.ctxBld]); break;
    case PH_CLEANUP:
        if (G.p[d].nhand + G.p[d].nprep > 4) snprintf(buf, n, "Regra das 4 cartas: descarte da mão");
        else snprintf(buf, n, "Regra das 4 cartas: escolha p/ o {M}");
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

static void draw_game(void) {
    draw_space();
    /* barra superior */
    r_rect(0, 0, SCREEN_W, 20, RGBA(0, 0, 0, 150));
    char t[160];
    snprintf(t, sizeof t, "RODADA %d   {K} Baralho %d   Descarte %d", G.round, G.ndeck, G.ndiscard);
    r_text(FONT_SMALL, 6, 3, t, RGBA(255, 230, 200, 255), 0);
    const char *hint = human_turn_now()
        ? (undoOk && G.phase != PH_ACTION ? "{X} OK  {O} Desfazer  {T} Inspecionar  START Pausa" : "{X} OK  {T} Inspecionar  START Pausa")
        : "{T} Inspecionar   START Pausa";
    if (inspect) hint = "{T}/{O} Sair   < > Navegar";
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
                snprintf(ex, sizeof ex, "%s: nave %d, Terra %d, energia %d, em Marte %d/7.\nPontos agora: %d (nave %d, {S} %d, {D} %d, bônus %d, energia %d).",
                         PLAYER_NAME[p], G.p[p].ship, G.p[p].earth, G.p[p].energy, s.onMars, s.total, s.ship, s.star1, s.star2,
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
                    extra = beat ? "Supera o valor visível: pode pousar 1 {C}!" : "Não supera o valor visível.";
                }
                draw_card_detail(dx, dy, dw, card, extra);
            } else if (fo->kind == OPT_PLAY_OPP) {
                int c = CARDS[G.p[fo->a].prep[fo->b]].color;
                snprintf(ex, sizeof ex, "Carta oculta de %s. O dono pode usar a ação {M}; você usa a habilidade: %s",
                         PLAYER_NAME[fo->a], BLD_ABILITY[c]);
                r_text_wrap(FONT_SMALL, dx, dy, dw, ex, RGBA(235, 235, 235, 255), 13);
            } else if (G.phase == PH_FX) {
                draw_card_detail(dx, dy, dw, G.ctxCard, NULL);
            } else if (hlBld >= 0) {
                building_info(hlBld, ex, sizeof ex);
                r_text_wrap(FONT_SMALL, dx, dy, dw, ex, RGBA(235, 235, 235, 255), 13);
            } else if (fo->kind == OPT_TAKE_EARTH) {
                r_text_wrap(FONT_SMALL, dx, dy, dw, "Pegue 1 colono seu da Terra e coloque-o na sua nave.", RGBA(235, 235, 235, 255), 13);
            } else {
                r_text_wrap(FONT_SMALL, dx, dy, dw, "Use as setas para ver as opções.", RGBA(180, 180, 190, 255), 13);
            }
        } else {
            draw_log(dx, dy, dw, 5);
        }
    }

    /* painel direito: decisoes */
    int px = 270, py = 240, pw = 366, ph = 205;
    panel(px, py, pw, ph, RGBA(120, 70, 60, 255));
    if (G.phase == PH_OVER) {
        r_text_center(FONT_MED, px + pw / 2, py + 80, "Fim de jogo!", RGBA(255, 220, 120, 255));
    } else if (curtain) {
        snprintf(t, sizeof t, "Vez de %s", PLAYER_NAME[G.decider]);
        r_text_center(FONT_MED, px + pw / 2, py + 60, t, COL_PLAYER[G.decider]);
        r_text_center(FONT_SMALL, px + pw / 2, py + 86, "Passe o controle e pressione {X}", RGBA(230, 230, 230, 255));
    } else if (human_turn_now()) {
        char pr[120];
        r_text(FONT_MED, px + 8, py + 5, phase_prompt(pr, sizeof pr), RGBA(255, 210, 160, 255), pw - 16);
        int rows = 8, rh = 16, ly = py + 26;
        if (optSel < optScroll) optScroll = optSel;
        if (optSel >= optScroll + rows) optScroll = optSel - rows + 1;
        for (int i = optScroll; i < nopts && i < optScroll + rows; i++) {
            int ry = ly + (i - optScroll) * rh;
            if (i == optSel) {
                r_rect(px + 4, ry - 1, pw - 8, rh, RGBA(200, 80, 50, 170 + (int)(40 * sinf(frame * 0.2f))));
                r_icon(IC_ARROW, px + 7, ry + 1, 13, RGBA(255, 255, 255, 255));
            }
            char lb[160]; game_option_label(&G, &opts[i], lb, sizeof lb);
            r_text(FONT_SMALL, px + 24, ry + 1, lb, i == optSel ? RGBA(255, 255, 255, 255) : RGBA(210, 210, 215, 255), pw - 32);
        }
        if (optScroll > 0) r_text(FONT_SMALL, px + pw - 20, ly - 12, "^", RGBA(255, 255, 255, 200), 0);
        if (optScroll + rows < nopts) r_text(FONT_SMALL, px + pw - 20, ly + rows * rh - 2, "v", RGBA(255, 255, 255, 200), 0);
        r_rect(px + 6, py + ph - 48, pw - 12, 1, RGBA(255, 255, 255, 50));
        draw_log(px + 8, py + ph - 45, pw - 16, 3);
    } else {
        if (G.decider >= 0) {
            int dots = (int)(frame / 15) % 4;
            snprintf(t, sizeof t, "%s está pensando%.*s", PLAYER_NAME[G.decider], dots, "...");
            r_icon(IC_CHIP, px + 8, py + 6, 16, COL_PLAYER[G.decider]);
            r_text(FONT_MED, px + 28, py + 5, t, COL_PLAYER[G.decider], pw - 36);
        }
        draw_log(px + 8, py + 30, pw - 16, LOG_LINES);
    }

    /* pausa */
    if (paused) {
        r_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, 160));
        panel(200, 140, 240, 150, RGBA(255, 120, 80, 255));
        r_text_center(FONT_MED, 320, 150, "PAUSA", RGBA(255, 220, 160, 255));
        const char *it[3] = { "Continuar", "Como jogar", "Sair para o título" };
        for (int i = 0; i < 3; i++) {
            int yy = 185 + i * 28;
            if (i == pauseSel) r_rect(214, yy - 3, 212, 22, RGBA(200, 80, 50, 200));
            r_text_center(FONT_MED, 320, yy, it[i], RGBA(255, 255, 255, 255));
        }
    }
}

/* ------------------------------------------------------------------ */
/* logica da cena de jogo                                               */

static void sfx_for(const Option *o) {
    switch (o->kind) {
    case OPT_PLAY_HAND: case OPT_PLAY_PREP: case OPT_PLAY_OPP: case OPT_CARD: case OPT_DRAFT: case OPT_TUCK:
        audio_sfx(SFX_CARD); break;
    case OPT_SHIP_TO: case OPT_MOVE_YES: case OPT_PROMOTE: case OPT_ECO_MOVE: case OPT_SWITCH:
        audio_sfx(SFX_LAND); break;
    case OPT_DISCARD_ENERGY: audio_sfx(SFX_ENERGY); break;
    case OPT_TAKE_EARTH: case OPT_TAKE_EARTH_FX: audio_sfx(SFX_LAND); break;
    default: audio_sfx(SFX_OK); break;
    }
}

static void apply_option(int idx) {
    Option o = opts[idx];
    int who = G.decider;
    sfx_for(&o);
    game_apply(&G, &o);
    if (G.lastBld < 5) { flashBld = G.lastBld; flashUntil = SDL_GetTicks() + 900; G.lastBld = 255; }
    if (G.chance || (G.decider != undoOwner && G.phase != PH_ACTION)) undoOk = 0;
    if (G.phase == PH_ACTION) undoOk = 0;
    (void)who;
    optsDirty = 1;
    optSel = 0; optScroll = 0;
}

static void update_game(void) {
    if (paused) {
        if (pressed[B_UP]) { pauseSel = (pauseSel + 2) % 3; audio_sfx(SFX_MOVE); }
        if (pressed[B_DOWN]) { pauseSel = (pauseSel + 1) % 3; audio_sfx(SFX_MOVE); }
        if (pressed[B_START] || pressed[B_BACK]) { paused = 0; audio_sfx(SFX_BACK); }
        if (pressed[B_OK]) {
            audio_sfx(SFX_OK);
            if (pauseSel == 0) paused = 0;
            else if (pauseSel == 1) { paused = 0; rulesReturn = SC_GAME; rulesPage = 0; scene = SC_RULES; }
            else { paused = 0; scene = SC_TITLE; audio_music(1); }
        }
        return;
    }
    if (pressed[B_START]) { paused = 1; pauseSel = 0; audio_sfx(SFX_OK); return; }
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
                game_log(&G, "Jogada desfeita.");
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
    draw_space();
    float bob = sinf(frame * 0.02f) * 6;
    r_mars(330, 60 + (int)bob, 300);
    /* foguete cruzando */
    int rx = (int)((frame * 1.3f)) % (SCREEN_W + 200) - 100;
    r_icon(IC_ROCKET, rx, 330 - rx / 8, 24, RGBA(255, 255, 255, 200));
    r_rect(rx - 30, 342 - rx / 8, 30, 2, RGBA(255, 150, 80, 120));

    r_text(FONT_BIG, 40, 70, "MARS COLONY", RGBA(255, 240, 225, 255), 0);
    r_text(FONT_BIG, 40, 108, "PROJECT RED", RGBA(240, 70, 50, 255), 0);
    r_rrect(40, 150, 62, 26, RGBA(30, 60, 160, 255));
    r_text(FONT_MED, 52, 154, "PS2", RGBA(255, 255, 255, 255), 0);
    r_text(FONT_SMALL, 112, 157, "Colonize Marte. Construa o futuro.", RGBA(220, 200, 190, 255), 0);

    const char *items[4] = { "Novo jogo", "Como jogar", "Créditos", "Sair" };
    for (int i = 0; i < 4; i++) {
        int y = 220 + i * 32;
        if (i == titleSel) {
            r_rrect(36, y - 4, 210, 26, RGBA(200, 70, 40, 200 + (int)(40 * sinf(frame * 0.15f))));
            r_icon(IC_ARROW, 44, y, 16, RGBA(255, 255, 255, 255));
        }
        r_text(FONT_MED, 68, y, items[i], RGBA(255, 255, 255, 255), 0);
    }
    if ((frame / 30) % 2) r_text(FONT_SMALL, 40, 360, "Pressione {X} ou START", RGBA(255, 220, 180, 255), 0);
    r_text(FONT_SMALL, 40, 420, "v1.0  Homebrew para PlayStation 2  -  1 a 4 jogadores", RGBA(150, 140, 150, 255), 0);
}

static void update_title(void) {
    if (pressed[B_UP]) { titleSel = (titleSel + 3) % 4; audio_sfx(SFX_MOVE); }
    if (pressed[B_DOWN]) { titleSel = (titleSel + 1) % 4; audio_sfx(SFX_MOVE); }
    if (pressed[B_OK] || pressed[B_START]) {
        audio_sfx(SFX_OK);
        if (titleSel == 0) { scene = SC_SETUP; setupSel = 0; }
        else if (titleSel == 1) { scene = SC_RULES; rulesReturn = SC_TITLE; rulesPage = 0; }
        else if (titleSel == 2) scene = SC_CREDITS;
        else quitReq = 1;
    }
}

/* ------------------------------------------------------------------ */
/* configuracao                                                         */

#define SETUP_ROWS 8
static void draw_setup(void) {
    draw_space();
    r_mars(470, 250, 220);
    r_text(FONT_BIG, 30, 20, "NOVA MISSÃO", RGBA(255, 240, 225, 255), 0);
    panel(24, 70, 420, 340, RGBA(200, 90, 60, 255));
    char t[64];
    for (int i = 0; i < SETUP_ROWS; i++) {
        int y = 84 + i * 38;
        int dis = (i >= 1 && i <= 4 && i - 1 >= cfgNp);
        if (i == setupSel) r_rrect(32, y - 6, 404, 30, RGBA(200, 70, 40, 190));
        Color lc = dis ? RGBA(110, 110, 120, 255) : RGBA(255, 255, 255, 255);
        if (i == 0) {
            r_text(FONT_MED, 44, y, "Jogadores", lc, 0);
            snprintf(t, sizeof t, "< %d >", cfgNp);
        } else if (i <= 4) {
            int p = i - 1;
            r_rect(44, y + 2, 10, 14, dis ? RGBA(80, 80, 80, 255) : COL_PLAYER[p]);
            r_text(FONT_MED, 62, y, PLAYER_NAME[p], lc, 0);
            snprintf(t, sizeof t, "< %s >", SLOT_NAME[cfgSlot[p]]);
        } else if (i == 5) {
            r_text(FONT_MED, 44, y, "Variante: draft de cartas", lc, 0);
            snprintf(t, sizeof t, "< %s >", cfgDraft ? "Sim" : "Não");
        } else if (i == 6) {
            r_text(FONT_MED, 44, y, "Velocidade da CPU", lc, 0);
            snprintf(t, sizeof t, "< %s >", SPEED_NAME[cfgSpeed]);
        } else {
            r_text_center(FONT_MED, 234, y, "{R} INICIAR MISSÃO", RGBA(255, 230, 160, 255));
            continue;
        }
        r_text(FONT_MED, 428 - r_text_w(FONT_MED, t), y, t, lc, 0);
    }
    r_text(FONT_SMALL, 30, 420, "{X} Confirmar   < > Alterar   {O} Voltar", RGBA(220, 220, 230, 255), 0);
    r_text_wrap(FONT_SMALL, 460, 80, 170,
                "Vários humanos jogam passando o controle (hot-seat). As cartas ficam ocultas entre os turnos.",
                RGBA(220, 210, 200, 255), 14);
}

static void update_setup(void) {
    if (pressed[B_UP]) { do setupSel = (setupSel + SETUP_ROWS - 1) % SETUP_ROWS; while (setupSel >= 1 && setupSel <= 4 && setupSel - 1 >= cfgNp); audio_sfx(SFX_MOVE); }
    if (pressed[B_DOWN]) { do setupSel = (setupSel + 1) % SETUP_ROWS; while (setupSel >= 1 && setupSel <= 4 && setupSel - 1 >= cfgNp); audio_sfx(SFX_MOVE); }
    int dir = pressed[B_RIGHT] ? 1 : pressed[B_LEFT] ? -1 : 0;
    if (pressed[B_OK] && setupSel != 7) dir = 1;
    if (dir) {
        audio_sfx(SFX_MOVE);
        if (setupSel == 0) { cfgNp += dir; if (cfgNp > 4) cfgNp = 2; if (cfgNp < 2) cfgNp = 4; }
        else if (setupSel <= 4) { int p = setupSel - 1; cfgSlot[p] = (cfgSlot[p] + 4 + dir) % 4; }
        else if (setupSel == 5) cfgDraft = !cfgDraft;
        else if (setupSel == 6) cfgSpeed = (cfgSpeed + 3 + dir) % 3;
    }
    if ((pressed[B_OK] && setupSel == 7) || pressed[B_START]) { audio_sfx(SFX_OK); start_game(); }
    if (pressed[B_BACK]) { audio_sfx(SFX_BACK); scene = SC_TITLE; }
}

/* ------------------------------------------------------------------ */
/* fim de jogo                                                          */

static void draw_over(void) {
    draw_space();
    int win = game_winners(&G);
    r_text_center(FONT_BIG, 320, 14, "MISSÃO CONCLUÍDA", RGBA(255, 235, 215, 255));
    char t[96];
    int nw = 0, wp = 0;
    for (int i = 0; i < G.np; i++) if (win >> i & 1) { nw++; wp = i; }
    if (nw == 1) snprintf(t, sizeof t, "Vitória de %s!", PLAYER_NAME[wp]);
    else snprintf(t, sizeof t, "Empate! A vitória é compartilhada.");
    r_text_center(FONT_MED, 320, 54, t, nw == 1 ? COL_PLAYER[wp] : RGBA(255, 220, 120, 255));

    int tx = 40, ty = 84, cw = (560 - 150) / G.np;
    panel(tx - 10, ty - 6, 580, 290, RGBA(200, 90, 60, 255));
    const char *rows[8] = { "Colonos na nave (1)", "Colonos em {S} (2)", "Colonos em {D} (4)",
                            "Nos 4 edifícios (+2)", "4+ num edifício (+3)", "Maior energia (+1)", "Colonos em Marte", "TOTAL" };
    for (int r = 0; r < 8; r++) {
        int y = ty + 34 + r * 28;
        if (r == 7) r_rect(tx - 4, y - 5, 568, 1, RGBA(255, 255, 255, 80));
        r_text(r == 7 ? FONT_MED : FONT_SMALL, tx, y, rows[r], r == 6 ? RGBA(150, 150, 160, 255) : RGBA(230, 230, 230, 255), 0);
    }
    for (int p = 0; p < G.np; p++) {
        int x = tx + 150 + p * cw + cw / 2;
        Score s; game_score(&G, p, &s);
        if (win >> p & 1) r_icon(IC_CROWN, x - r_text_w(FONT_MED, PLAYER_NAME[p]) / 2 - 26, ty + 6, 22, RGBA(255, 210, 60, 255));
        r_text_center(FONT_MED, x, ty + 10, PLAYER_NAME[p], COL_PLAYER[p]);
        int vals[8] = { s.ship, s.star1, s.star2, s.allFour, s.fourInOne, s.energy, s.onMars, s.total };
        for (int r = 0; r < 8; r++) {
            snprintf(t, sizeof t, "%d", vals[r]);
            r_text_center(r == 7 ? FONT_MED : FONT_SMALL, x, ty + 34 + r * 28, t,
                          r == 7 ? RGBA(255, 220, 120, 255) : r == 6 ? RGBA(150, 150, 160, 255) : RGBA(255, 255, 255, 255));
        }
    }
    r_text_center(FONT_SMALL, 320, 380, "Empate: vence quem tiver mais colonos em Marte.", RGBA(170, 170, 180, 255));
    const char *it[2] = { "Jogar novamente", "Menu principal" };
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

static const char *RULES_TITLE[] = {
    "Objetivo", "Preparação", "Seu turno", "Jogar do módulo", "Regra das 4 cartas", "Edifícios", "Fim e pontuação", "Controles"
};
static const char *RULES_TEXT[] = {
    "Cada jogador comanda uma agência espacial com 7 colonos. Use cartas de projeto para levar colonos da Terra para a sua nave {R} e da nave para os edifícios de Marte.\n\n"
    "Quem distribuir melhor seus colonos fará mais pontos e vencerá. Colonos nos espaços {D} valem mais, mas há poucas vagas!",
    "Cada jogador começa com 1 {C} na nave, 6 {C} na Terra e 1 {E} de energia.\n\n"
    "Cada um recebe 4 cartas de projeto e escolhe 2 para o seu {M} módulo de preparação (viradas para baixo; os outros só veem a cor). As outras 2 ficam na {H} mão.\n\n"
    "Variante draft: cada um recebe 5 cartas, fica com 1 e passa o resto, até ter 4.",
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
};
#define NRULES 8

static void draw_rules(void) {
    draw_space();
    r_text(FONT_BIG, 30, 16, "COMO JOGAR", RGBA(255, 240, 225, 255), 0);
    char t[48]; snprintf(t, sizeof t, "%d / %d", rulesPage + 1, NRULES);
    r_text(FONT_MED, 600 - r_text_w(FONT_MED, t), 28, t, RGBA(220, 200, 190, 255), 0);
    panel(24, 64, 592, 340, RGBA(200, 90, 60, 255));
    r_text(FONT_MED, 40, 76, RULES_TITLE[rulesPage], RGBA(255, 190, 140, 255), 0);
    r_text_wrap(FONT_SMALL, 40, 104, 560, RULES_TEXT[rulesPage], RGBA(235, 235, 235, 255), 17);
    r_text(FONT_SMALL, 30, 418, "< > Página   {O} Voltar", RGBA(220, 220, 230, 255), 0);
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
    r_mars(400, 120, 260);
    r_text(FONT_BIG, 30, 20, "CRÉDITOS", RGBA(255, 240, 225, 255), 0);
    panel(24, 70, 400, 320, RGBA(200, 90, 60, 255));
    r_text_wrap(FONT_SMALL, 40, 86, 370,
        "MARS COLONY: PROJECT RED PS2\n"
        "Homebrew para PlayStation 2 feito com PS2SDK, gsKit e SDL2.\n\n"
        "Mecânicas adaptadas das regras do jogo de cartas Pocket Mars (design de Michał Jagodziński, Board&Dice), "
        "fornecidas pelo jogador. Nomes, cartas, efeitos, arte e código deste jogo são originais.\n\n"
        "Fonte: DejaVu Sans (licença livre).\n"
        "Música e efeitos: sintetizados em tempo real.\n\n"
        "Projeto de fã, sem fins comerciais.",
        RGBA(235, 235, 235, 255), 16);
    r_text(FONT_SMALL, 30, 418, "{O} Voltar", RGBA(220, 220, 230, 255), 0);
}

/* ------------------------------------------------------------------ */
/* testes automatizados (somente PC)                                     */

static SDL_Window *win;

static void present(void) { SDL_RenderPresent(R); }

static void draw_scene(void) {
    switch (scene) {
    case SC_TITLE: draw_title(); break;
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

static int autotest(const char *dir) {
    frame = 100;
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
    scene = SC_RULES; rulesPage = 3; shot(dir, "08_regras");
    rulesPage = 5; shot(dir, "09_regras_edificios");
    scene = SC_CREDITS; shot(dir, "10_creditos");
    /* hot-seat: cortina */
    cfgNp = 2; cfgSlot[0] = 0; cfgSlot[1] = 0; start_game();
    nopts = game_options(&G, opts);
    viewer = 0; curtain = 0; apply_option(0); apply_option(0);
    nopts = game_options(&G, opts); curtain = 1;
    scene = SC_GAME; shot(dir, "11_cortina");
    return 0;
}
#endif

/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    const char *autodir = NULL;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--autotest") && i + 1 < argc) autodir = argv[++i];

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
    for (int i = 0; i < SDL_NumJoysticks(); i++) if (SDL_IsGameController(i)) SDL_GameControllerOpen(i);

#ifndef PS2
    if (autodir) return autotest(autodir);
#else
    (void)autodir;
#endif
    audio_init();
    audio_music(1);

    while (!quitReq) {
        Uint32 t0 = SDL_GetTicks();
        poll_input();
        switch (scene) {
        case SC_TITLE: update_title(); break;
        case SC_SETUP: update_setup(); break;
        case SC_GAME: update_game(); break;
        case SC_OVER: update_over(); break;
        case SC_RULES: update_rules(); break;
        case SC_CREDITS: if (pressed[B_BACK] || pressed[B_OK]) { scene = SC_TITLE; audio_sfx(SFX_BACK); } break;
        }
        SDL_SetRenderDrawColor(R, 0, 0, 0, 255);
        SDL_RenderClear(R);
        draw_scene();
        present();
        frame++;
        Uint32 dt = SDL_GetTicks() - t0;
        if (dt < 16) SDL_Delay(16 - dt);
    }
    audio_quit();
    SDL_Quit();
    return 0;
}
