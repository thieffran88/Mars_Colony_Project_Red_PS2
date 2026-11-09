#include "render.h"
#include "assets.h"
#include <string.h>
#include <stdlib.h>

SDL_Renderer *R;
static SDL_Texture *atlas;

const Color COL_BLD[5] = {
    RGBA(46, 160, 67, 255),   /* ecossistema - verde   */
    RGBA(232, 140, 30, 255),  /* energia - laranja     */
    RGBA(150, 60, 170, 255),  /* ciencia - roxo        */
    RGBA(40, 120, 210, 255),  /* agua - azul           */
    RGBA(70, 70, 78, 255),    /* construcao - grafite  */
};
const Color COL_PLAYER[4] = {
    RGBA(230, 60, 50, 255),
    RGBA(60, 150, 255, 255),
    RGBA(70, 205, 90, 255),
    RGBA(250, 210, 50, 255),
};
#define COL_GOLD   RGBA(255, 205, 60, 255)
#define COL_ENERGY RGBA(255, 225, 70, 255)

Color c_mul(Color c, float k) {
    int r = (int)(((c >> 24) & 255) * k), g = (int)(((c >> 16) & 255) * k), b = (int)(((c >> 8) & 255) * k);
    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    return RGBA(r, g, b, c & 255);
}
Color c_alpha(Color c, int a) { return (c & 0xFFFFFF00u) | (uint32_t)(a & 255); }

static void setc(Color c) {
    SDL_SetRenderDrawColor(R, (c >> 24) & 255, (c >> 16) & 255, (c >> 8) & 255, c & 255);
}

int r_init(SDL_Renderer *ren) {
    R = ren;
    uint8_t *alpha = (uint8_t *)malloc(ATLAS_W * ATLAS_H);
    if (!alpha) return 0;
    int o = 0;
    for (int i = 0; i < ATLAS_RLE_LEN && o < ATLAS_W * ATLAS_H; ) {
        uint8_t v = ATLAS_RLE[i++];
        if (v == 0 || v == 255) {
            int n = ATLAS_RLE[i++];
            while (n-- > 0 && o < ATLAS_W * ATLAS_H) alpha[o++] = v;
        } else alpha[o++] = v;
    }
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, ATLAS_W, ATLAS_H, 32, SDL_PIXELFORMAT_ABGR8888);
    if (!s) { free(alpha); return 0; }
    for (int y = 0; y < ATLAS_H; y++) {
        uint8_t *row = (uint8_t *)s->pixels + y * s->pitch;
        for (int x = 0; x < ATLAS_W; x++) {
            uint8_t *px = row + x * 4;
            px[0] = px[1] = px[2] = 255;
            px[3] = alpha[y * ATLAS_W + x];
        }
    }
    for (int y = 0; y < MARS_S; y++)
        memcpy((uint8_t *)s->pixels + (MARS_Y + y) * s->pitch + MARS_X * 4, MARS_RGBA + y * MARS_S * 4, MARS_S * 4);
    free(alpha);
    atlas = SDL_CreateTextureFromSurface(R, s);
    SDL_FreeSurface(s);
    if (!atlas) return 0;
    SDL_SetTextureBlendMode(atlas, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    return 1;
}

void r_rect(int x, int y, int w, int h, Color c) {
    if (w <= 0 || h <= 0) return;
    SDL_Rect rc = { x, y, w, h };
    setc(c);
    SDL_RenderFillRect(R, &rc);
}

void r_frame(int x, int y, int w, int h, Color c) {
    r_rect(x, y, w, 1, c);
    r_rect(x, y + h - 1, w, 1, c);
    r_rect(x, y + 1, 1, h - 2, c);
    r_rect(x + w - 1, y + 1, 1, h - 2, c);
}

void r_rrect(int x, int y, int w, int h, Color c) {
    if (w < 6 || h < 6) { r_rect(x, y, w, h, c); return; }
    r_rect(x + 3, y, w - 6, h, c);
    r_rect(x, y + 3, 3, h - 6, c);
    r_rect(x + w - 3, y + 3, 3, h - 6, c);
    r_rect(x + 1, y + 1, 2, 2, c);
    r_rect(x + w - 3, y + 1, 2, 2, c);
    r_rect(x + 1, y + h - 3, 2, 2, c);
    r_rect(x + w - 3, y + h - 3, 2, 2, c);
}

void r_vgrad(int x, int y, int w, int h, Color a, Color b) {
    int steps = h < 16 ? h : 16;
    if (steps <= 0) return;
    for (int i = 0; i < steps; i++) {
        int y0 = y + h * i / steps, y1 = y + h * (i + 1) / steps;
        float t = steps > 1 ? (float)i / (steps - 1) : 0.f;
        int r = (int)(((a >> 24) & 255) * (1 - t) + ((b >> 24) & 255) * t);
        int g = (int)(((a >> 16) & 255) * (1 - t) + ((b >> 16) & 255) * t);
        int bl = (int)(((a >> 8) & 255) * (1 - t) + ((b >> 8) & 255) * t);
        int al = (int)((a & 255) * (1 - t) + (b & 255) * t);
        r_rect(x, y0, w, y1 - y0, RGBA(r, g, bl, al));
    }
}

static void blit(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, Color c) {
    SDL_Rect src = { sx, sy, sw, sh }, dst = { dx, dy, dw, dh };
    SDL_SetTextureColorMod(atlas, (c >> 24) & 255, (c >> 16) & 255, (c >> 8) & 255);
    SDL_SetTextureAlphaMod(atlas, c & 255);
    SDL_RenderCopy(R, atlas, &src, &dst);
}

void r_icon(int id, int x, int y, int size, Color c) {
    if (id < 0 || id >= ICON_COUNT) return;
    if (size <= 20) blit(ICON16_POS[id][0], ICON16_POS[id][1], 16, 16, x, y, size, size, c);
    else blit(ICON32_POS[id][0], ICON32_POS[id][1], 32, 32, x, y, size, size, c);
}

void r_mars(int x, int y, int size) {
    blit(MARS_X, MARS_Y, MARS_S, MARS_S, x, y, size, size, RGBA(255, 255, 255, 255));
}

static const Glyph *font_tab(int f) { return f == FONT_SMALL ? FONT_S : f == FONT_MED ? FONT_M : FONT_L; }
int r_font_h(int f) { return f == FONT_SMALL ? FONT_S_H : f == FONT_MED ? FONT_M_H : FONT_L_H; }

/* decodifica 1 codepoint UTF-8 */
static int utf8(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    int c = *s;
    if (c < 0x80) { *p += 1; return c; }
    if ((c & 0xE0) == 0xC0 && s[1]) { *p += 2; return ((c & 0x1F) << 6) | (s[1] & 0x3F); }
    if ((c & 0xF0) == 0xE0 && s[1] && s[2]) { *p += 3; return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); }
    *p += 1; return '?';
}

/* marcador {X}: retorna id do icone e cor (0 = usa cor do texto); -1 se nao for marcador */
static int markup(const char *s, int *len, Color *col, int *twice) {
    *twice = 0; *col = 0;
    if (s[0] != '{' || !s[1] || s[2] != '}') return -1;
    *len = 3;
    switch (s[1]) {
    case 'S': *col = COL_GOLD; return IC_STAR;
    case 'D': *col = COL_GOLD; *twice = 1; return IC_STAR;
    case 'E': *col = COL_ENERGY; return IC_BOLT;
    case 'C': return IC_COLONIST;
    case 'H': return IC_HAND;
    case 'M': return IC_MODULE;
    case 'X': *col = RGBA(120, 170, 255, 255); return IC_BCROSS;
    case 'O': *col = RGBA(255, 100, 100, 255); return IC_BCIRCLE;
    case 'T': *col = RGBA(90, 220, 160, 255); return IC_BTRI;
    case 'Q': *col = RGBA(240, 130, 220, 255); return IC_BSQUARE;
    case 'R': *col = RGBA(240, 120, 90, 255); return IC_ROCKET;
    case 'W': *col = RGBA(110, 180, 255, 255); return IC_EARTH;
    case 'K': return IC_CARD;
    case '0': case '1': case '2': case '3': case '4': {
        int b = s[1] - '0';
        *col = b == 4 ? RGBA(190, 190, 200, 255) : c_mul(COL_BLD[b], 1.25f);
        return IC_LEAF + b; }
    }
    return -1;
}

static int icon_size(int f) { return f == FONT_SMALL ? 13 : f == FONT_MED ? 16 : 30; }

static int draw_run(int f, int x, int y, const char *s, const char *end, Color c, int draw) {
    const Glyph *G = font_tab(f);
    int fh = r_font_h(f);
    int cx = x;
    while (*s && (!end || s < end)) {
        int ml, tw; Color ic;
        int id = markup(s, &ml, &ic, &tw);
        if (id >= 0) {
            int is = icon_size(f);
            int n = tw ? 2 : 1;
            for (int k = 0; k < n; k++) {
                if (draw) r_icon(id, cx, y + (fh - is) / 2, is, ic ? c_alpha(ic, c & 255) : c);
                cx += is - (tw ? 3 : 0);
            }
            cx += tw ? 4 : 1;
            s += ml;
            continue;
        }
        int cp = utf8(&s);
        if (cp == 0x2605) cp = '*';
        if (cp < 32 || cp > 255) cp = '?';
        const Glyph *g = &G[cp - 32];
        if (draw && g->w) blit(g->x, g->y, g->w, g->h, cx + g->ox, y + g->oy, g->w, g->h, c);
        cx += g->adv;
    }
    return cx - x;
}

int r_text_w(int f, const char *s) { return draw_run(f, 0, 0, s, NULL, 0, 0); }

int r_text(int f, int x, int y, const char *s, Color c, int maxw) {
    if (maxw > 0 && r_text_w(f, s) > maxw) {
        /* corta ate caber com "..." */
        int ell = r_text_w(f, "...");
        const char *p = s, *last = s;
        while (*p) {
            const char *q = p;
            int ml, tw; Color ic;
            if (markup(q, &ml, &ic, &tw) >= 0) q += ml; else utf8(&q);
            if (draw_run(f, 0, 0, s, q, 0, 0) + ell > maxw) break;
            last = q; p = q;
        }
        int w = draw_run(f, x, y, s, last, c, 1);
        return w + draw_run(f, x + w, y, "...", NULL, c, 1);
    }
    return draw_run(f, x, y, s, NULL, c, 1);
}

int r_text_center(int f, int cx, int y, const char *s, Color c) {
    int w = r_text_w(f, s);
    return r_text(f, cx - w / 2, y, s, c, 0);
}

int r_text_wrap(int f, int x, int y, int w, const char *s, Color c, int lineh) {
    if (lineh <= 0) lineh = r_font_h(f) + 1;
    int lines = 0;
    const char *line = s;
    while (*line) {
        while (*line == ' ') line++;
        if (!*line) break;
        const char *p = line, *brk = NULL;
        while (*p && *p != '\n') {
            const char *q = p;
            while (*q && *q != ' ' && *q != '\n') q++;
            if (draw_run(f, 0, 0, line, q, 0, 0) > w && brk) break;
            brk = q;
            p = q;
            if (*p == ' ') p++;
        }
        if (!brk) brk = p;
        draw_run(f, x, y + lines * lineh, line, brk, c, 1);
        lines++;
        line = brk;
        if (*line == '\n') line++;
    }
    return lines * lineh;
}
