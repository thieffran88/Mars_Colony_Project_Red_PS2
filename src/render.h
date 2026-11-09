#ifndef RENDER_H
#define RENDER_H
#include <SDL.h>
#include <stdint.h>
#include "icons.h"
#include "art.h"

#define SCREEN_W 640
#define SCREEN_H 448

enum { FONT_SMALL = 0, FONT_MED };

typedef uint32_t Color; /* 0xRRGGBBAA */
#define RGBA(r,g,b,a) ((Color)(((uint32_t)(r) << 24) | ((uint32_t)(g) << 16) | ((uint32_t)(b) << 8) | (uint32_t)(a)))

extern SDL_Renderer *R;
extern const Color COL_BLD[5];
extern const Color COL_PLAYER[4];

int  r_init(SDL_Renderer *ren);
void r_rect(int x, int y, int w, int h, Color c);
void r_frame(int x, int y, int w, int h, Color c);
void r_rrect(int x, int y, int w, int h, Color c);
void r_vgrad(int x, int y, int w, int h, Color top, Color bottom);
void r_icon(int id, int x, int y, int size, Color c);
void r_mars(int x, int y, int size);
void r_art(const SprRect *r, int x, int y, int w, int h, Color c);
void r_art_fit(const SprRect *r, int x, int y, int w, int h, Color c);
void r_title(const SprRect *r, int x, int y, int w, int h, Color c);
/* fonte cromada (titulos): h = altura das maiusculas */
int  r_chrome(int x, int y, int h, const char *s, Color c);
int  r_chrome_w(int h, const char *s);
int  r_chrome_center(int cx, int y, int h, const char *s, Color c);
void r_text_scale(float s);   /* escala das fontes S/M (1.0 = normal) */
#define WHITE RGBA(255, 255, 255, 255)
/* Texto UTF-8 com marcadores {S} {D} {E} {C} {H} {M} {0}..{4}.
 * maxw > 0 corta com reticencias. Retorna largura desenhada. */
int  r_text(int font, int x, int y, const char *s, Color c, int maxw);
int  r_text_w(int font, const char *s);
int  r_text_center(int font, int cx, int y, const char *s, Color c);
/* Texto com quebra de linha. Retorna altura usada. */
int  r_text_wrap(int font, int x, int y, int w, const char *s, Color c, int lineh);
int  r_font_h(int font);
Color c_mul(Color c, float k);
Color c_alpha(Color c, int a);

#endif
