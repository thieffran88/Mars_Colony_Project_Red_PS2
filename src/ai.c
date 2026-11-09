/*
 * IA da CPU: busca exaustiva (com orcamento) de todas as sequencias de
 * decisoes do proprio turno, avaliando a posicao resultante com uma
 * heuristica baseada na pontuacao final.
 */
#include "ai.h"
#include <string.h>

static uint32_t xr(uint32_t *s) {
    uint32_t x = *s ? *s : 0xA5A5A5A5u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return x;
}
static float frand(uint32_t *s) { return (float)(xr(s) & 0xFFFF) / 65535.0f; }

static float fx_value(const Game *g, int p, const Fx *fx) {
    const Player *P = &g->p[p];
    int star1 = 0;
    for (int b = 0; b < 4; b++) star1 += g->b[b].star1[p];
    switch (fx->type) {
    case FX_EARTH: return (fx->a < P->earth ? fx->a : P->earth) * 1.2f;
    case FX_EARTH_IF_HAND: return P->earth ? 0.8f : 0.f;
    case FX_EARTH_IF_ABSENT: return (g->b[fx->a].star1[p] + g->b[fx->a].star2[p]) ? 0.1f : 1.6f;
    case FX_ENERGY: return 0.35f * fx->a;
    case FX_EARTH_ENERGY: return 1.5f;
    case FX_DRAW: return 0.45f * fx->a;
    case FX_ENERGY_IF_LOW: return P->energy <= 2 ? 1.0f : 0.1f;
    case FX_ENERGY_PER_BLD: return 0.5f;
    case FX_SHIP_TO: return P->ship ? 2.2f : 0.8f;
    case FX_PAY_SHIP_TO: return P->ship ? 1.8f : 0.6f;
    case FX_PROMOTE: return star1 ? 2.0f : 0.6f;
    case FX_PROMOTE_TUCK: return star1 ? 2.2f : 0.7f;
    case FX_SWITCH: return star1 ? 1.4f : 0.5f;
    case FX_PAY_PROMOTE: return star1 ? 1.5f : 0.4f;
    case FX_DEMOTE: return 1.2f;
    case FX_PAY_EARTH: return P->earth ? 1.5f : 0.f;
    case FX_TUCK_DRAW: return 0.6f;
    }
    return 0.f;
}

static float prep_score(const Game *g, int p, int c) {
    const CardDef *cd = &CARDS[c];
    float s = fx_value(g, p, &cd->bottom) * 0.6f + cd->value * 0.25f;
    if (cd->color == C_CONSTR) s += 1.2f;
    else if (cd->value > game_top_value(g, cd->color)) s += 3.5f;
    return s;
}

static float hand_score(const Game *g, int p, int c) {
    const CardDef *cd = &CARDS[c];
    return fx_value(g, p, &cd->top) + (7 - cd->value) * 0.12f;
}

static float potential(const Game *g, int q) {
    float v = 0;
    int kinds = 0, maxin = 0, mars = 0;
    for (int b = 0; b < 4; b++) {
        int c = g->b[b].star1[q] + g->b[b].star2[q];
        v += 4.f * g->b[b].star2[q] + 2.f * g->b[b].star1[q];
        if (c) kinds++;
        if (c > maxin) maxin = c;
        mars += c;
    }
    v += 1.4f * g->p[q].ship + 0.3f * g->p[q].earth;
    v += kinds == 4 ? 2.f : 0.35f * kinds;
    v += maxin >= 4 ? 3.f : 0.45f * maxin;
    v += 0.3f * g->p[q].energy;
    v += 0.15f * mars;
    for (int i = 0; i < g->p[q].nprep; i++) {
        int c = g->p[q].prep[i];
        if (CARDS[c].color != C_CONSTR && CARDS[c].value > game_top_value(g, CARDS[c].color)) v += 0.5f;
    }
    v += 0.1f * (g->p[q].nhand + g->p[q].nprep);
    return v;
}

static float evaluate(const Game *g, int p) {
    if (g->phase == PH_OVER) {
        Score s; game_score(g, p, &s);
        int best = -1000;
        for (int i = 0; i < g->np; i++) if (i != p) {
            Score t; game_score(g, i, &t);
            if (t.total > best) best = t.total;
        }
        int w = game_winners(g);
        return (s.total - best) * 3.f + ((w >> p) & 1 ? 100.f : -100.f);
    }
    float own = potential(g, p), opp = -1000.f;
    for (int i = 0; i < g->np; i++) if (i != p) {
        float o = potential(g, i);
        if (o > opp) opp = o;
    }
    return own - (g->np == 2 ? 1.0f : 0.8f) * opp;
}

static int turn_done(const Game *g) {
    return g->phase == PH_OVER || g->phase == PH_ACTION || g->phase == PH_CLEANUP ||
           g->phase == PH_SETUP_PREP || g->phase == PH_DRAFT;
}

static float search(const Game *g, int root, int *budget) {
    if (turn_done(g) || *budget <= 0) return evaluate(g, root);
    Option o[MAX_OPTS];
    int n = game_options(g, o);
    if (g->decider != root) {
        /* decisao de outro jogador (acao inferior do dono): guloso */
        int who = g->decider, bi = 0; float bv = -1e9f;
        for (int i = 0; i < n; i++) {
            Game c = *g; game_apply(&c, &o[i]);
            float v = evaluate(&c, who);
            if (v > bv) { bv = v; bi = i; }
        }
        Game c = *g; game_apply(&c, &o[bi]);
        (*budget)--;
        return search(&c, root, budget);
    }
    float best = -1e9f;
    for (int i = 0; i < n; i++) {
        Game c = *g; game_apply(&c, &o[i]);
        (*budget)--;
        float v = search(&c, root, budget);
        if (v > best) best = v;
    }
    return best;
}

int ai_choose(const Game *g, uint32_t *seed) {
    Option o[MAX_OPTS];
    int n = game_options(g, o);
    if (n <= 1) return 0;
    int p = g->decider;
    int lvl = g->p[p].ailevel;

    /* decisoes de cartas por heuristica */
    if (g->phase == PH_SETUP_PREP || g->phase == PH_CLEANUP || g->phase == PH_DRAFT) {
        const Player *P = &g->p[p];
        int discard = g->phase == PH_CLEANUP && P->nhand + P->nprep > 4;
        int bi = 0; float bv = -1e9f;
        for (int i = 0; i < n; i++) {
            float v;
            if (g->phase == PH_DRAFT) {
                int c = g->pool[p][o[i].a];
                float a = prep_score(g, p, c), b = hand_score(g, p, c);
                v = a > b ? a : b;
            } else {
                int c = P->hand[o[i].a];
                if (discard) v = -(hand_score(g, p, c) + 0.5f * prep_score(g, p, c));
                else v = prep_score(g, p, c) - hand_score(g, p, c);
            }
            if (lvl == 0) v += frand(seed) * 2.f;
            if (v > bv) { bv = v; bi = i; }
        }
        return bi;
    }

    if (lvl == 0 && frand(seed) < 0.3f) return (int)(xr(seed) % (uint32_t)n);
    int budgetPer = lvl == 0 ? 60 : lvl == 1 ? 400 : 1500;
    float noise = lvl == 0 ? 1.5f : lvl == 1 ? 0.35f : 0.f;

    int bi = 0; float bv = -1e9f;
    for (int i = 0; i < n; i++) {
        Game c = *g;
        c.quiet = 1;
        game_apply(&c, &o[i]);
        int budget = budgetPer;
        float v = search(&c, p, &budget);
        /* pequena preferencia por nao "pular" quando ha alternativa */
        if (o[i].kind == OPT_SKIP) v -= 0.05f;
        v += (frand(seed) - 0.5f) * 2.f * noise;
        if (v > bv) { bv = v; bi = i; }
    }
    return bi;
}
