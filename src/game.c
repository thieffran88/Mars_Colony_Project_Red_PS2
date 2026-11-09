/*
 * Mars Colony: Project Red PS2 - motor de regras.
 *
 * Marcadores de icone usados nos textos (desenhados pela UI):
 *   {S} espaco *   {D} espaco **   {E} energia   {C} colono
 *   {H} mao        {M} modulo      {0}..{4} icone do edificio
 */
#include "game.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

int g_lang = LANG_PT;

static const char *COLOR_NAMES[LANG_COUNT][NCOLORS] = {
    { "Ecossistema", "Energia", "Ciência", "Água", "Construção" },
    { "Ecosystem", "Energy", "Science", "Water", "Construction" },
    { "Ecosistema", "Energía", "Ciencia", "Agua", "Construcción" },
};
static const char *BLD_NAMES[LANG_COUNT][NCOLORS] = {
    { "ECOSSISTEMA", "ENERGIA", "CIÊNCIA", "ÁGUA", "CONSTRUÇÃO" },
    { "ECOSYSTEM", "ENERGY", "SCIENCE", "WATER", "CONSTRUCTION" },
    { "ECOSISTEMA", "ENERGÍA", "CIENCIA", "AGUA", "CONSTRUCCIÓN" },
};
static const char *BLD_ABILITIES[LANG_COUNT][NCOLORS] = {
    { "Mova qualquer {C} de um edifício de {S} para {D} ou de {D} para {S}.",
      "Ganhe 1 {E}.",
      "Traga 1 {C} seu da Terra para a nave.",
      "Coloque 1 carta sua (mão ou módulo) no fundo do baralho.",
      "Mova 1 {C} da nave para o {S} de qualquer edifício." },
    { "Move any {C} in a building from {S} to {D} or from {D} to {S}.",
      "Gain 1 {E}.",
      "Bring 1 of your {C} from Earth to your ship.",
      "Put 1 of your cards (hand or module) at the bottom of the deck.",
      "Move 1 {C} from your ship to the {S} of any building." },
    { "Mueve cualquier {C} de un edificio de {S} a {D} o de {D} a {S}.",
      "Gana 1 {E}.",
      "Trae 1 {C} tuyo de la Tierra a la nave.",
      "Pon 1 carta tuya (mano o módulo) al fondo del mazo.",
      "Mueve 1 {C} de la nave al {S} de cualquier edificio." },
};
static const char *PLAYER_NAMES[LANG_COUNT][MAXP] = {
    { "Vermelho", "Azul", "Verde", "Amarelo" },
    { "Red", "Blue", "Green", "Yellow" },
    { "Rojo", "Azul", "Verde", "Amarillo" },
};
static const char *CARD_NAMES_EN[NCARDS] = {
    "Seed Bank", "Adapted Lichen", "Bioreactor", "Vertical Farm", "Hydroponic Greenhouse", "Algae Dome", "Sealed Forest",
    "Solar Panel", "Thermal Generator", "Wind Turbine", "Sodium Battery", "Compact Reactor", "Power Grid", "Experimental Fusion",
    "Deep Antenna", "Mobile Lab", "Orbital Probe", "Research Center", "Giant Telescope", "Medical Clinic", "Martian Academy",
    "Ice Well", "Regolith Filter", "Condenser", "Recycler", "Aqueduct", "Electrolysis Plant", "Reservoir",
    "Excavator", "3D Printer", "Crane", "Inflatable Module", "Lava Tube", "Landing Pad", "Cargo Elevator",
};
static const char *CARD_NAMES_ES[NCARDS] = {
    "Banco de Semillas", "Liquen Adaptado", "Biorreactor", "Granja Vertical", "Invernadero Hidropónico", "Domo de Algas", "Bosque Sellado",
    "Panel Solar", "Generador Térmico", "Turbina Eólica", "Batería de Sodio", "Reactor Compacto", "Red Eléctrica", "Fusión Experimental",
    "Antena Profunda", "Laboratorio Móvil", "Sonda Orbital", "Centro de Investigación", "Telescopio Gigante", "Clínica Médica", "Academia Marciana",
    "Pozo de Hielo", "Filtro de Regolito", "Condensador", "Reciclador", "Acueducto", "Planta de Electrólisis", "Embalse",
    "Excavadora", "Impresora 3D", "Grúa", "Módulo Inflable", "Tubo de Lava", "Plataforma de Aterrizaje", "Elevador de Carga",
};

static int L(void) { return g_lang >= 0 && g_lang < LANG_COUNT ? g_lang : 0; }
const char *color_name(int b) { return COLOR_NAMES[L()][b]; }
const char *bld_name(int b) { return BLD_NAMES[L()][b]; }
const char *bld_ability(int b) { return BLD_ABILITIES[L()][b]; }
const char *player_name(int p) { return PLAYER_NAMES[L()][p]; }
const char *card_name(int c) {
    return L() == LANG_EN ? CARD_NAMES_EN[c] : L() == LANG_ES ? CARD_NAMES_ES[c] : CARDS[c].name;
}

#define F(t,a,b) { t, a, b }
const CardDef CARDS[NCARDS] = {
    /* ECOSSISTEMA */
    { "Banco de Sementes",   C_ECO, 0, F(FX_EARTH_IF_HAND, C_ECO, 0),   F(FX_SWITCH, 0, 0) },
    { "Líquen Adaptado",     C_ECO, 1, F(FX_PROMOTE, -1, 0),            F(FX_EARTH, 1, 0) },
    { "Biorreator",          C_ECO, 2, F(FX_ENERGY, 2, 0),              F(FX_PROMOTE, C_ECO, 0) },
    { "Fazenda Vertical",    C_ECO, 4, F(FX_SWITCH, 0, 0),              F(FX_EARTH_IF_ABSENT, C_ECO, 2) },
    { "Estufa Hidropônica",  C_ECO, 5, F(FX_DRAW, 2, 0),                F(FX_PROMOTE, -1, 0) },
    { "Domo de Algas",       C_ECO, 6, F(FX_EARTH_ENERGY, 0, 0),        F(FX_SWITCH, 0, 0) },
    { "Floresta Selada",     C_ECO, 7, F(FX_PROMOTE, C_ECO, 0),         F(FX_ENERGY, 1, 0) },
    /* ENERGIA */
    { "Painel Solar",        C_ENERGY, 0, F(FX_ENERGY, 2, 0),           F(FX_EARTH_IF_ABSENT, C_ENERGY, 2) },
    { "Gerador Térmico",     C_ENERGY, 1, F(FX_ENERGY_PER_BLD, 0, 0),   F(FX_PAY_PROMOTE, 1, 0) },
    { "Turbina Eólica",      C_ENERGY, 2, F(FX_EARTH_ENERGY, 0, 0),     F(FX_ENERGY, 2, 0) },
    { "Bateria de Sódio",    C_ENERGY, 3, F(FX_ENERGY_IF_LOW, 3, 0),    F(FX_PAY_SHIP_TO, 1, 0) },
    { "Reator Compacto",     C_ENERGY, 5, F(FX_PAY_EARTH, 2, 2),        F(FX_ENERGY, 1, 0) },
    { "Rede Elétrica",       C_ENERGY, 6, F(FX_PAY_SHIP_TO, 2, 0),      F(FX_EARTH_ENERGY, 0, 0) },
    { "Fusão Experimental",  C_ENERGY, 7, F(FX_ENERGY, 3, 0),           F(FX_DRAW, 1, 0) },
    /* CIENCIA */
    { "Antena Profunda",     C_SCIENCE, 0, F(FX_EARTH_IF_HAND, C_SCIENCE, 0), F(FX_EARTH, 1, 0) },
    { "Laboratório Móvel",   C_SCIENCE, 1, F(FX_DRAW, 2, 0),            F(FX_EARTH_IF_HAND, C_SCIENCE, 0) },
    { "Sonda Orbital",       C_SCIENCE, 3, F(FX_EARTH, 1, 0),           F(FX_SHIP_TO, C_SCIENCE, 0) },
    { "Centro de Pesquisa",  C_SCIENCE, 4, F(FX_PAY_EARTH, 1, 2),       F(FX_EARTH, 1, 0) },
    { "Telescópio Gigante",  C_SCIENCE, 5, F(FX_EARTH_IF_ABSENT, C_SCIENCE, 2), F(FX_DRAW, 1, 0) },
    { "Clínica Médica",      C_SCIENCE, 6, F(FX_EARTH, 2, 0),           F(FX_PROMOTE, C_SCIENCE, 0) },
    { "Academia Marciana",   C_SCIENCE, 7, F(FX_EARTH_ENERGY, 0, 0),    F(FX_EARTH, 1, 0) },
    /* AGUA */
    { "Poço de Gelo",        C_WATER, 0, F(FX_TUCK_DRAW, 0, 0),         F(FX_EARTH_IF_HAND, C_WATER, 0) },
    { "Filtro de Regolito",  C_WATER, 2, F(FX_PROMOTE_TUCK, C_WATER, 0), F(FX_DRAW, 1, 0) },
    { "Condensador",         C_WATER, 3, F(FX_DRAW, 2, 0),              F(FX_PROMOTE, C_WATER, 0) },
    { "Reciclador",          C_WATER, 4, F(FX_EARTH_IF_HAND, C_WATER, 0), F(FX_TUCK_DRAW, 0, 0) },
    { "Aqueduto",            C_WATER, 5, F(FX_PROMOTE_TUCK, -1, 0),     F(FX_EARTH_IF_HAND, C_WATER, 0) },
    { "Usina de Eletrólise", C_WATER, 6, F(FX_ENERGY, 2, 0),            F(FX_PROMOTE_TUCK, -1, 0) },
    { "Reservatório",        C_WATER, 7, F(FX_DRAW, 3, 0),              F(FX_EARTH, 1, 0) },
    /* CONSTRUCAO */
    { "Escavadeira",         C_CONSTR, 0, F(FX_SHIP_TO, -1, 0),         F(FX_DEMOTE, 0, 0) },
    { "Impressora 3D",       C_CONSTR, 1, F(FX_PAY_SHIP_TO, 1, 0),      F(FX_SHIP_TO, C_ECO, 0) },
    { "Guindaste",           C_CONSTR, 2, F(FX_SHIP_TO, C_ENERGY, 0),   F(FX_EARTH, 1, 0) },
    { "Módulo Inflável",     C_CONSTR, 3, F(FX_SHIP_TO, C_SCIENCE, 0),  F(FX_ENERGY, 1, 0) },
    { "Túnel de Lava",       C_CONSTR, 4, F(FX_SHIP_TO, C_WATER, 0),    F(FX_PAY_PROMOTE, 1, 0) },
    { "Plataforma de Pouso", C_CONSTR, 5, F(FX_EARTH, 1, 0),            F(FX_SHIP_TO, -1, 0) },
    { "Elevador de Carga",   C_CONSTR, 6, F(FX_DEMOTE, 1, 0),           F(FX_PAY_SHIP_TO, 1, 0) },
};
#undef F

/* ------------------------------------------------------------------ */
/* utilitarios                                                          */

static uint32_t rnd(Game *g) {
    uint32_t x = g->rng ? g->rng : 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g->rng = x;
    return x;
}

static void shuffle(Game *g, uint8_t *a, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = (int)(rnd(g) % (uint32_t)(i + 1));
        uint8_t t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

void game_log(Game *g, const char *fmt, ...) {
    if (g->quiet) return;
    if (g->nlog == LOG_LINES) {
        memmove(g->log[0], g->log[1], sizeof(g->log[0]) * (LOG_LINES - 1));
        g->nlog--;
    }
    va_list ap; va_start(ap, fmt);
    vsnprintf(g->log[g->nlog], LOG_LEN, fmt, ap);
    va_end(ap);
    g->nlog++;
}

int game_star2_cap(const Game *g) { return g->np == 2 ? 1 : 2; }

int game_star2_total(const Game *g, int b) {
    int t = 0;
    for (int i = 0; i < g->np; i++) t += g->b[b].star2[i];
    return t;
}

int game_on_mars(const Game *g, int p) {
    int t = 0;
    for (int b = 0; b < 4; b++) t += g->b[b].star1[p] + g->b[b].star2[p];
    return t;
}

int game_top_value(const Game *g, int b) {
    const Building *B = &g->b[b];
    if (B->nstack == 0) return 3;
    return CARDS[B->stack[B->nstack - 1]].value;
}

static const char *pname(const Game *g, int p) { (void)g; return player_name(p); }

static void remove_at(uint8_t *a, uint8_t *n, int idx) {
    for (int i = idx; i < *n - 1; i++) a[i] = a[i + 1];
    (*n)--;
}

static int hand_has_color(const Game *g, int p, int color) {
    for (int i = 0; i < g->p[p].nhand; i++)
        if (CARDS[g->p[p].hand[i]].color == color) return 1;
    return 0;
}

static void refill_deck(Game *g) {
    /* cartas por baixo do topo nos edificios vao para o descarte */
    for (int b = 0; b < NCOLORS; b++) {
        Building *B = &g->b[b];
        if (B->nstack > 1) {
            for (int i = 0; i < B->nstack - 1; i++) g->discard[g->ndiscard++] = B->stack[i];
            B->stack[0] = B->stack[B->nstack - 1];
            B->nstack = 1;
        }
    }
    for (int i = 0; i < g->ndiscard; i++) g->deck[i] = g->discard[i];
    g->ndeck = g->ndiscard;
    g->ndiscard = 0;
    shuffle(g, g->deck, g->ndeck);
    g->chance = 1;
    game_log(g, TR("O baralho acabou: novo baralho embaralhado (%d cartas).", "The deck ran out: new deck shuffled (%d cards).", "Se acabó el mazo: nuevo mazo barajado (%d cartas)."), g->ndeck);
}

static int draw_card(Game *g, int p) {
    if (g->ndeck == 0) refill_deck(g);
    if (g->ndeck == 0 || g->p[p].nhand >= MAX_HAND) return 0;
    g->p[p].hand[g->p[p].nhand++] = g->deck[--g->ndeck];
    g->chance = 1;
    return 1;
}

static void tuck_card(Game *g, int card) {
    /* fundo do baralho = indice 0 */
    for (int i = g->ndeck; i > 0; i--) g->deck[i] = g->deck[i - 1];
    g->deck[0] = (uint8_t)card;
    g->ndeck++;
}

static int take_earth(Game *g, int p, int n) {
    int k = 0;
    while (k < n && g->p[p].earth > 0) { g->p[p].earth--; g->p[p].ship++; k++; }
    return k;
}

static void gain_energy(Game *g, int p, int n) {
    int e = g->p[p].energy + n;
    g->p[p].energy = (uint8_t)(e > MAX_ENERGY ? MAX_ENERGY : e);
}

/* ------------------------------------------------------------------ */
/* inicio                                                               */

void game_init(Game *g, int np, const int human[MAXP], const int ailevel[MAXP],
               int draft, uint32_t seed) {
    memset(g, 0, sizeof(*g));
    g->np = np;
    g->rng = seed ? seed : 12345u;
    for (int i = 0; i < 8; i++) rnd(g);
    for (int i = 0; i < np; i++) {
        g->p[i].ship = 1;
        g->p[i].earth = NCOLONISTS - 1;
        g->p[i].energy = 1;
        g->p[i].human = (uint8_t)human[i];
        g->p[i].ailevel = (uint8_t)ailevel[i];
    }
    for (int i = 0; i < NCARDS; i++) g->deck[i] = (uint8_t)i;
    g->ndeck = NCARDS;
    shuffle(g, g->deck, g->ndeck);
    g->start = (int)(rnd(g) % (uint32_t)np);
    g->cur = g->start;
    g->round = 1;
    g->useDraft = (int8_t)draft;
    g->lastBld = 255;

    game_log(g, TR("Bem-vindos a Marte! %s começa a rodada.", "Welcome to Mars! %s starts the round.", "¡Bienvenidos a Marte! %s empieza la ronda."), pname(g, g->start));
    if (draft) {
        for (int i = 0; i < np; i++) {
            g->npool[i] = 5;
            for (int k = 0; k < 5; k++) g->pool[i][k] = g->deck[--g->ndeck];
        }
        g->phase = PH_DRAFT;
        g->setupIdx = 0;
        g->draftPick = 0;
        g->decider = 0;
    } else {
        for (int i = 0; i < np; i++)
            for (int k = 0; k < 4; k++) g->p[i].hand[g->p[i].nhand++] = g->deck[--g->ndeck];
        g->phase = PH_SETUP_PREP;
        g->setupIdx = 0;
        g->decider = 0;
    }
}

/* ------------------------------------------------------------------ */
/* opcoes de efeitos                                                    */

static int add(Option *o, int n, int kind, int a, int b, int c) {
    if (n >= MAX_OPTS) return n;
    o[n].kind = (uint8_t)kind; o[n].a = (int8_t)a; o[n].b = (int8_t)b; o[n].c = (int8_t)c;
    return n + 1;
}

static int opts_ship_to(const Game *g, int p, int bld, Option *o, int n) {
    if (g->p[p].ship == 0) return n;
    for (int b = 0; b < 4; b++)
        if (bld < 0 || bld == b) n = add(o, n, OPT_SHIP_TO, b, 0, 0);
    return n;
}

static int opts_promote(const Game *g, int p, int bld, Option *o, int n) {
    int cap = game_star2_cap(g);
    for (int b = 0; b < 4; b++) {
        if (bld >= 0 && bld != b) continue;
        if (g->b[b].star1[p] > 0 && game_star2_total(g, b) < cap)
            n = add(o, n, OPT_PROMOTE, b, 0, 0);
    }
    return n;
}

static int opts_eco(const Game *g, Option *o, int n) {
    int cap = game_star2_cap(g);
    for (int b = 0; b < 4; b++)
        for (int q = 0; q < g->np; q++) {
            if (g->b[b].star1[q] > 0 && game_star2_total(g, b) < cap) n = add(o, n, OPT_ECO_MOVE, b, q, 0);
            if (g->b[b].star2[q] > 0) n = add(o, n, OPT_ECO_MOVE, b, q, 1);
        }
    return n;
}

static int opts_switch(const Game *g, Option *o, int n) {
    for (int b = 0; b < 4; b++)
        for (int up = 0; up < g->np; up++) {
            if (!g->b[b].star1[up]) continue;
            for (int dn = 0; dn < g->np; dn++) {
                if (!g->b[b].star2[dn] || dn == up) continue;
                n = add(o, n, OPT_SWITCH, b, up, dn);
            }
        }
    return n;
}

static int opts_demote(const Game *g, int p, Option *o, int n) {
    for (int b = 0; b < 4; b++)
        for (int q = 0; q < g->np; q++)
            if (q != p && g->b[b].star2[q] > 0) n = add(o, n, OPT_DEMOTE, b, q, 0);
    return n;
}

static int opts_tuck_hand(const Game *g, int p, Option *o, int n) {
    for (int i = 0; i < g->p[p].nhand; i++) n = add(o, n, OPT_TUCK, 0, i, 0);
    return n;
}

/* Opcoes "concretas" do efeito no passo atual. Retorna 0 se automatico. */
static int fx_options(const Game *g, const Fx *fx, int actor, int step, Option *o) {
    int n = 0;
    const Player *P = &g->p[actor];
    switch (fx->type) {
    case FX_SHIP_TO:      n = opts_ship_to(g, actor, fx->a, o, n); break;
    case FX_PAY_SHIP_TO:  if (P->energy >= fx->a) n = opts_ship_to(g, actor, -1, o, n); break;
    case FX_PROMOTE:      n = opts_promote(g, actor, fx->a, o, n); break;
    case FX_PROMOTE_TUCK:
        if (step == 0) n = opts_promote(g, actor, fx->a, o, n);
        else n = opts_tuck_hand(g, actor, o, n);
        break;
    case FX_SWITCH:       n = opts_switch(g, o, n); break;
    case FX_PAY_PROMOTE:  if (P->energy >= fx->a) n = opts_promote(g, actor, -1, o, n); break;
    case FX_DEMOTE:       if (P->energy >= fx->a) n = opts_demote(g, actor, o, n); break;
    case FX_PAY_EARTH:    if (P->energy >= fx->a && P->earth > 0) n = add(o, n, OPT_TAKE_EARTH_FX, 0, 0, 0); break;
    case FX_TUCK_DRAW:    n = opts_tuck_hand(g, actor, o, n); break;
    default: break;
    }
    return n;
}

static int fx_is_choice(int type) {
    switch (type) {
    case FX_SHIP_TO: case FX_PAY_SHIP_TO: case FX_PROMOTE: case FX_PROMOTE_TUCK:
    case FX_SWITCH: case FX_PAY_PROMOTE: case FX_DEMOTE: case FX_PAY_EARTH: case FX_TUCK_DRAW:
        return 1;
    }
    return 0;
}

static void fx_auto(Game *g, const Fx *fx, int actor) {
    Player *P = &g->p[actor];
    int k;
    switch (fx->type) {
    case FX_EARTH:
        k = take_earth(g, actor, fx->a);
        if (k) game_log(g, TR("%s traz %d {C} da Terra.", "%s brings %d {C} from Earth.", "%s trae %d {C} de la Tierra."), pname(g, actor), k);
        break;
    case FX_EARTH_IF_HAND:
        if (hand_has_color(g, actor, fx->a)) {
            k = take_earth(g, actor, 1);
            if (k) game_log(g, TR("%s tem {%d} na mão e traz 1 {C}.", "%s has {%d} in hand and brings 1 {C}.", "%s tiene {%d} en la mano y trae 1 {C}."), pname(g, actor), fx->a);
        } else game_log(g, TR("%s não tem carta {%d} na mão.", "%s has no {%d} card in hand.", "%s no tiene carta {%d} en la mano."), pname(g, actor), fx->a);
        break;
    case FX_EARTH_IF_ABSENT:
        if (g->b[fx->a].star1[actor] + g->b[fx->a].star2[actor] == 0) {
            k = take_earth(g, actor, fx->b);
            if (k) game_log(g, TR("%s traz %d {C} da Terra.", "%s brings %d {C} from Earth.", "%s trae %d {C} de la Tierra."), pname(g, actor), k);
        } else game_log(g, TR("%s já tem colono em {%d}: sem efeito.", "%s already has a colonist in {%d}: no effect.", "%s ya tiene colono en {%d}: sin efecto."), pname(g, actor), fx->a);
        break;
    case FX_ENERGY:
        gain_energy(g, actor, fx->a);
        game_log(g, TR("%s ganha %d {E}.", "%s gains %d {E}.", "%s gana %d {E}."), pname(g, actor), fx->a);
        break;
    case FX_EARTH_ENERGY:
        take_earth(g, actor, 1);
        gain_energy(g, actor, 1);
        game_log(g, TR("%s traz 1 {C} e ganha 1 {E}.", "%s brings 1 {C} and gains 1 {E}.", "%s trae 1 {C} y gana 1 {E}."), pname(g, actor));
        break;
    case FX_DRAW:
        k = 0;
        for (int i = 0; i < fx->a; i++) k += draw_card(g, actor);
        game_log(g, TR("%s compra %d carta(s).", "%s draws %d card(s).", "%s roba %d carta(s)."), pname(g, actor), k);
        break;
    case FX_ENERGY_IF_LOW:
        if (P->energy <= 2) { gain_energy(g, actor, fx->a); game_log(g, TR("%s ganha %d {E}.", "%s gains %d {E}.", "%s gana %d {E}."), pname(g, actor), fx->a); }
        else game_log(g, TR("%s tem energia demais: sem efeito.", "%s has too much energy: no effect.", "%s tiene demasiada energía: sin efecto."), pname(g, actor));
        break;
    case FX_ENERGY_PER_BLD: {
        int c = 0;
        for (int b = 0; b < 4; b++) if (g->b[b].star1[actor] + g->b[b].star2[actor]) c++;
        if (c > 2) c = 2;
        gain_energy(g, actor, c);
        game_log(g, TR("%s ganha %d {E}.", "%s gains %d {E}.", "%s gana %d {E}."), pname(g, actor), c);
        break; }
    default: break;
    }
}

/* ------------------------------------------------------------------ */
/* fluxo                                                                */

static void advance(Game *g);

static void begin_fx(Game *g, int card, int bottom, int actor) {
    g->ctxCard = (int8_t)card;
    g->ctxBottom = (int8_t)bottom;
    g->ctxActor = (int8_t)actor;
    g->ctxStep = 0;
    const Fx *fx = bottom ? &CARDS[card].bottom : &CARDS[card].top;
    if (!fx_is_choice(fx->type)) {
        fx_auto(g, fx, actor);
        return;
    }
    Option tmp[MAX_OPTS];
    if (fx_options(g, fx, actor, 0, tmp) == 0) {
        game_log(g, TR("Efeito sem alvo possível para %s.", "No valid target for %s.", "Efecto sin objetivo posible para %s."), pname(g, actor));
        return;
    }
    g->phase = PH_FX;
    g->decider = actor;
}

static int next_player(const Game *g, int p) { return (p + 1) % g->np; }

static void end_turn(Game *g) {
    g->cur = next_player(g, g->cur);
    g->chance = 0;
    if (g->cur == g->start) {
        int over = 0;
        for (int i = 0; i < g->np; i++) if (game_on_mars(g, i) >= NCOLONISTS) over = 1;
        if (over || g->round >= 60) {
            g->phase = PH_OVER;
            g->decider = -1;
            game_log(g, TR("Fim de jogo! Contando os pontos...", "Game over! Counting points...", "¡Fin del juego! Contando puntos..."));
            return;
        }
        g->round++;
        game_log(g, TR("--- Rodada %d ---", "--- Round %d ---", "--- Ronda %d ---"), g->round);
    }
    g->phase = PH_ACTION;
    g->decider = g->cur;
    g->ctxFlow = 0;
}

/* Regra das 4 cartas. Retorna 1 se uma decisao ficou pendente. */
static int cleanup_step(Game *g) {
    while (g->cleanupDone < g->np) {
        int p = (g->cur + g->cleanupDone) % g->np;
        Player *P = &g->p[p];
        int total = P->nhand + P->nprep;
        if (total > 4 && P->nhand > 0) { g->phase = PH_CLEANUP; g->decider = p; return 1; }
        if (total < 4) {
            if (draw_card(g, p)) continue;
        }
        if (P->nprep < 2 && P->nhand > 0) {
            int need = 2 - P->nprep;
            if (P->nhand <= need) {
                while (P->nprep < 2 && P->nhand > 0) {
                    P->prep[P->nprep++] = P->hand[0];
                    remove_at(P->hand, &P->nhand, 0);
                }
                continue;
            }
            g->phase = PH_CLEANUP; g->decider = p; return 1;
        }
        g->cleanupDone++;
    }
    return 0;
}

/* ctxFlow para jogada de modulo: 1 mover colono, 2 acao inferior, 3 habilidade, 4 limpeza */
static void advance(Game *g) {
    for (;;) {
        if (g->phase == PH_OVER) return;
        switch (g->ctxFlow) {
        case 1: { /* comparacao de valores */
            g->ctxFlow = 2;
            int b = g->ctxBld;
            if (g->ctxFromOpp || b == C_CONSTR) break;
            const Building *B = &g->b[b];
            int under = B->nstack >= 2 ? CARDS[B->stack[B->nstack - 2]].value : 3;
            int v = CARDS[g->ctxCard].value;
            if (v > under && g->p[g->cur].ship > 0) {
                g->phase = PH_CONNECT_MOVE; g->decider = g->cur; return;
            } else if (v <= under) {
                game_log(g, TR("Valor %d não supera %d: sem pouso de colono.", "Value %d does not beat %d: no landing.", "Valor %d no supera %d: sin aterrizaje."), v, under);
            }
            break; }
        case 2: /* acao inferior (do dono) */
            g->ctxFlow = 3;
            begin_fx(g, g->ctxCard, 1, g->ctxOwner);
            if (g->phase == PH_FX) return;
            break;
        case 3: { /* habilidade do edificio */
            g->ctxFlow = 4;
            int b = g->ctxBld, p = g->cur;
            g->ctxStep = 0;
            if (b == C_ENERGY) {
                if (g->p[p].energy < MAX_ENERGY) { gain_energy(g, p, 1); game_log(g, TR("Edifício {1}: %s ganha 1 {E}.", "Building {1}: %s gains 1 {E}.", "Edificio {1}: %s gana 1 {E}."), pname(g, p)); }
            } else if (b == C_SCIENCE) {
                if (take_earth(g, p, 1)) game_log(g, TR("Edifício {2}: %s traz 1 {C} da Terra.", "Building {2}: %s brings 1 {C} from Earth.", "Edificio {2}: %s trae 1 {C} de la Tierra."), pname(g, p));
            } else {
                Option tmp[MAX_OPTS];
                int n = 0;
                if (b == C_ECO) n = opts_eco(g, tmp, 0);
                else if (b == C_WATER) n = g->p[p].nhand + g->p[p].nprep;
                else if (b == C_CONSTR) n = opts_ship_to(g, p, -1, tmp, 0);
                if (n > 0) { g->phase = PH_ABILITY; g->decider = p; return; }
            }
            break; }
        case 4:
            g->ctxFlow = 5;
            g->cleanupDone = 0;
            break;
        case 5:
            if (cleanup_step(g)) return;
            g->ctxFlow = 0;
            end_turn(g);
            return;
        default:
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* opcoes                                                               */

int game_options(const Game *g, Option *o) {
    int n = 0;
    const Player *P;
    switch (g->phase) {
    case PH_DRAFT:
        for (int i = 0; i < g->npool[g->decider]; i++) n = add(o, n, OPT_DRAFT, i, 0, 0);
        break;
    case PH_SETUP_PREP:
    case PH_CLEANUP:
        P = &g->p[g->decider];
        for (int i = 0; i < P->nhand; i++) n = add(o, n, OPT_CARD, i, 0, 0);
        break;
    case PH_ACTION:
        P = &g->p[g->cur];
        for (int i = 0; i < P->nhand; i++) n = add(o, n, OPT_PLAY_HAND, i, 0, 0);
        for (int i = 0; i < P->nprep; i++) n = add(o, n, OPT_PLAY_PREP, i, 0, 0);
        for (int q = 0; q < g->np; q++) {
            if (q == g->cur) continue;
            for (int i = 0; i < g->p[q].nprep; i++) n = add(o, n, OPT_PLAY_OPP, q, i, 0);
        }
        if (P->earth > 0) n = add(o, n, OPT_TAKE_EARTH, 0, 0, 0);
        for (int i = 0; i < P->nhand; i++) n = add(o, n, OPT_DISCARD_ENERGY, 0, i, 0);
        for (int i = 0; i < P->nprep; i++) n = add(o, n, OPT_DISCARD_ENERGY, 1, i, 0);
        if (n == 0) n = add(o, n, OPT_PASS, 0, 0, 0);
        break;
    case PH_FX: {
        const Fx *fx = g->ctxBottom ? &CARDS[g->ctxCard].bottom : &CARDS[g->ctxCard].top;
        n = fx_options(g, fx, g->ctxActor, g->ctxStep, o);
        n = add(o, n, OPT_SKIP, 0, 0, 0);
        break; }
    case PH_CONNECT_MOVE:
        n = add(o, n, OPT_MOVE_YES, g->ctxBld, 0, 0);
        n = add(o, n, OPT_SKIP, 0, 0, 0);
        break;
    case PH_ABILITY: {
        int b = g->ctxBld, p = g->cur;
        if (b == C_ECO) n = opts_eco(g, o, n);
        else if (b == C_WATER) {
            for (int i = 0; i < g->p[p].nhand; i++) n = add(o, n, OPT_TUCK, 0, i, 0);
            for (int i = 0; i < g->p[p].nprep; i++) n = add(o, n, OPT_TUCK, 1, i, 0);
        } else if (b == C_CONSTR) n = opts_ship_to(g, p, -1, o, n);
        n = add(o, n, OPT_SKIP, 0, 0, 0);
        break; }
    default: break;
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* aplicar                                                              */

static void connect_card(Game *g, int card) {
    int b = CARDS[card].color;
    g->b[b].stack[g->b[b].nstack++] = (uint8_t)card;
    g->ctxBld = (int8_t)b;
    g->ctxCard = (int8_t)card;
    g->lastBld = (uint8_t)b;
}

static void do_ship_to(Game *g, int p, int b) {
    g->p[p].ship--;
    g->b[b].star1[p]++;
    g->lastBld = (uint8_t)b;
}

static void do_promote(Game *g, int p, int b) {
    g->b[b].star1[p]--;
    g->b[b].star2[p]++;
    g->lastBld = (uint8_t)b;
}

static void apply_fx_choice(Game *g, const Option *o) {
    int a = g->ctxActor;
    const Fx *fx = g->ctxBottom ? &CARDS[g->ctxCard].bottom : &CARDS[g->ctxCard].top;
    Player *P = &g->p[a];
    int done = 1;
    if (o->kind == OPT_SKIP) {
        if (!(fx->type == FX_PROMOTE_TUCK && g->ctxStep == 1))
            game_log(g, TR("%s não usa o efeito.", "%s skips the effect.", "%s no usa el efecto."), pname(g, a));
    } else switch (fx->type) {
    case FX_SHIP_TO:
        do_ship_to(g, a, o->a);
        game_log(g, TR("%s pousa 1 {C} em {%d} ({S}).", "%s lands 1 {C} on {%d} ({S}).", "%s aterriza 1 {C} en {%d} ({S})."), pname(g, a), o->a);
        break;
    case FX_PAY_SHIP_TO:
        P->energy -= (uint8_t)fx->a;
        do_ship_to(g, a, o->a);
        game_log(g, TR("%s paga %d {E} e pousa 1 {C} em {%d}.", "%s pays %d {E} and lands 1 {C} on {%d}.", "%s paga %d {E} y aterriza 1 {C} en {%d}."), pname(g, a), fx->a, o->a);
        break;
    case FX_PROMOTE:
        do_promote(g, a, o->a);
        game_log(g, TR("%s promove 1 {C} para {D} em {%d}.", "%s promotes 1 {C} to {D} in {%d}.", "%s asciende 1 {C} a {D} en {%d}."), pname(g, a), o->a);
        break;
    case FX_PROMOTE_TUCK:
        if (g->ctxStep == 0) {
            do_promote(g, a, o->a);
            game_log(g, TR("%s promove 1 {C} para {D} em {%d}.", "%s promotes 1 {C} to {D} in {%d}.", "%s asciende 1 {C} a {D} en {%d}."), pname(g, a), o->a);
            if (P->nhand > 0) { g->ctxStep = 1; done = 0; }
        } else {
            int c = P->hand[o->b];
            remove_at(P->hand, &P->nhand, o->b);
            tuck_card(g, c);
            game_log(g, TR("%s põe 1 carta no fundo do baralho.", "%s puts 1 card at the bottom of the deck.", "%s pone 1 carta al fondo del mazo."), pname(g, a));
        }
        break;
    case FX_SWITCH:
        g->b[o->a].star1[o->b]--; g->b[o->a].star2[o->b]++;
        g->b[o->a].star2[o->c]--; g->b[o->a].star1[o->c]++;
        g->lastBld = (uint8_t)o->a;
        game_log(g, TR("Troca em {%d}: %s sobe para {D}, %s desce para {S}.", "Swap in {%d}: %s goes up to {D}, %s goes down to {S}.", "Cambio en {%d}: %s sube a {D}, %s baja a {S}."), o->a, pname(g, o->b), pname(g, o->c));
        break;
    case FX_PAY_PROMOTE:
        P->energy -= (uint8_t)fx->a;
        do_promote(g, a, o->a);
        game_log(g, TR("%s paga %d {E} e promove 1 {C} em {%d}.", "%s pays %d {E} and promotes 1 {C} in {%d}.", "%s paga %d {E} y asciende 1 {C} en {%d}."), pname(g, a), fx->a, o->a);
        break;
    case FX_DEMOTE:
        P->energy -= (uint8_t)fx->a;
        g->b[o->a].star2[o->b]--; g->b[o->a].star1[o->b]++;
        g->lastBld = (uint8_t)o->a;
        game_log(g, TR("%s rebaixa um {C} de %s em {%d}.", "%s demotes a {C} of %s in {%d}.", "%s baja un {C} de %s en {%d}."), pname(g, a), pname(g, o->b), o->a);
        break;
    case FX_PAY_EARTH: {
        P->energy -= (uint8_t)fx->a;
        int k = take_earth(g, a, fx->b);
        game_log(g, TR("%s paga %d {E} e traz %d {C}.", "%s pays %d {E} and brings %d {C}.", "%s paga %d {E} y trae %d {C}."), pname(g, a), fx->a, k);
        break; }
    case FX_TUCK_DRAW: {
        int c = P->hand[o->b];
        remove_at(P->hand, &P->nhand, o->b);
        tuck_card(g, c);
        int k = draw_card(g, a); k += draw_card(g, a);
        game_log(g, TR("%s recicla 1 carta e compra %d.", "%s recycles 1 card and draws %d.", "%s recicla 1 carta y roba %d."), pname(g, a), k);
        break; }
    }
    if (!done) return; /* continua em PH_FX */
    g->phase = PH_ACTION; /* marcador; advance decide */
    g->decider = g->cur;
}

static void apply_ability(Game *g, const Option *o) {
    int p = g->cur, b = g->ctxBld;
    Player *P = &g->p[p];
    if (o->kind == OPT_SKIP) return;
    if (b == C_ECO) {
        Building *B = &g->b[o->a];
        if (o->c == 0) { B->star1[o->b]--; B->star2[o->b]++; }
        else           { B->star2[o->b]--; B->star1[o->b]++; }
        g->lastBld = (uint8_t)o->a;
        game_log(g, TR("Edifício {0}: %s move {C} de %s para %s em {%d}.", "Building {0}: %s moves %s's {C} to %s in {%d}.", "Edificio {0}: %s mueve {C} de %s a %s en {%d}."), pname(g, p), pname(g, o->b),
                 o->c == 0 ? "{D}" : "{S}", o->a);
    } else if (b == C_WATER) {
        int c;
        if (o->a == 0) { c = P->hand[o->b]; remove_at(P->hand, &P->nhand, o->b); }
        else           { c = P->prep[o->b]; remove_at(P->prep, &P->nprep, o->b); }
        tuck_card(g, c);
        game_log(g, TR("Edifício {3}: %s põe 1 carta no fundo do baralho.", "Building {3}: %s puts 1 card at the bottom of the deck.", "Edificio {3}: %s pone 1 carta al fondo del mazo."), pname(g, p));
    } else if (b == C_CONSTR) {
        do_ship_to(g, p, o->a);
        game_log(g, TR("Edifício {4}: %s pousa 1 {C} em {%d}.", "Building {4}: %s lands 1 {C} on {%d}.", "Edificio {4}: %s aterriza 1 {C} en {%d}."), pname(g, p), o->a);
    }
}

void game_apply(Game *g, const Option *o) {
    Player *P;
    switch (g->phase) {
    case PH_DRAFT: {
        int p = g->decider;
        P = &g->p[p];
        P->hand[P->nhand++] = g->pool[p][o->a];
        remove_at(g->pool[p], &g->npool[p], o->a);
        if (p + 1 < g->np) { g->decider = p + 1; return; }
        /* todos escolheram: passa os lotes para a esquerda */
        g->draftPick++;
        if (g->draftPick >= 4) {
            for (int i = 0; i < g->np; i++)
                for (int k = 0; k < g->npool[i]; k++) g->deck[g->ndeck++] = g->pool[i][k];
            for (int i = 0; i < g->np; i++) g->npool[i] = 0;
            shuffle(g, g->deck, g->ndeck);
            g->phase = PH_SETUP_PREP; g->setupIdx = 0; g->decider = 0;
            game_log(g, TR("Draft concluído. Escolham as cartas do módulo.", "Draft complete. Choose your module cards.", "Draft terminado. Elegid las cartas del módulo."));
            return;
        }
        uint8_t tmp[MAXP][5]; uint8_t tn[MAXP];
        memcpy(tmp, g->pool, sizeof(tmp)); memcpy(tn, g->npool, sizeof(tn));
        for (int i = 0; i < g->np; i++) {
            int from = (i - 1 + g->np) % g->np;
            memcpy(g->pool[i], tmp[from], 5); g->npool[i] = tn[from];
        }
        g->decider = 0;
        return; }
    case PH_SETUP_PREP: {
        int p = g->decider;
        P = &g->p[p];
        P->prep[P->nprep++] = P->hand[o->a];
        remove_at(P->hand, &P->nhand, o->a);
        if (P->nprep < 2) return;
        if (p + 1 < g->np) { g->decider = p + 1; return; }
        g->phase = PH_ACTION; g->cur = g->start; g->decider = g->cur; g->ctxFlow = 0;
        game_log(g, TR("--- Rodada 1 ---", "--- Round 1 ---", "--- Ronda 1 ---"));
        return; }
    case PH_ACTION: {
        int p = g->cur;
        P = &g->p[p];
        g->ctxFromOpp = 0;
        g->ctxOwner = (int8_t)p;
        switch (o->kind) {
        case OPT_PLAY_HAND: {
            int c = P->hand[o->a];
            remove_at(P->hand, &P->nhand, o->a);
            g->discard[g->ndiscard++] = (uint8_t)c;
            game_log(g, TR("%s joga {H} \"%s\".", "%s plays {H} \"%s\".", "%s juega {H} \"%s\"."), pname(g, p), card_name(c));
            g->ctxFlow = 4;
            begin_fx(g, c, 0, p);
            if (g->phase == PH_FX) return;
            break; }
        case OPT_PLAY_PREP: {
            int c = P->prep[o->a];
            remove_at(P->prep, &P->nprep, o->a);
            connect_card(g, c);
            game_log(g, TR("%s conecta {M} \"%s\" (%d) em {%d}.", "%s connects {M} \"%s\" (%d) to {%d}.", "%s conecta {M} \"%s\" (%d) en {%d}."), pname(g, p), card_name(c), CARDS[c].value, CARDS[c].color);
            g->ctxFlow = 1;
            break; }
        case OPT_PLAY_OPP: {
            int q = o->a;
            int c = g->p[q].prep[o->b];
            remove_at(g->p[q].prep, &g->p[q].nprep, o->b);
            connect_card(g, c);
            g->ctxFromOpp = 1;
            g->ctxOwner = (int8_t)q;
            game_log(g, TR("%s revela o módulo de %s: \"%s\" (%d) em {%d}.", "%s reveals %s's module: \"%s\" (%d) to {%d}.", "%s revela el módulo de %s: \"%s\" (%d) en {%d}."), pname(g, p), pname(g, q),
                     card_name(c), CARDS[c].value, CARDS[c].color);
            g->ctxFlow = 1;
            break; }
        case OPT_TAKE_EARTH:
            take_earth(g, p, 1);
            game_log(g, TR("%s traz 1 {C} da Terra para a nave.", "%s brings 1 {C} from Earth to the ship.", "%s trae 1 {C} de la Tierra a la nave."), pname(g, p));
            g->ctxFlow = 4;
            break;
        case OPT_DISCARD_ENERGY: {
            int c;
            if (o->a == 0) { c = P->hand[o->b]; remove_at(P->hand, &P->nhand, o->b); }
            else           { c = P->prep[o->b]; remove_at(P->prep, &P->nprep, o->b); }
            g->discard[g->ndiscard++] = (uint8_t)c;
            gain_energy(g, p, 1);
            game_log(g, TR("%s descarta 1 carta e ganha 1 {E}.", "%s discards 1 card and gains 1 {E}.", "%s descarta 1 carta y gana 1 {E}."), pname(g, p));
            g->ctxFlow = 4;
            break; }
        default:
            game_log(g, TR("%s passa a vez.", "%s passes.", "%s pasa el turno."), pname(g, p));
            g->ctxFlow = 4;
            break;
        }
        g->phase = PH_ACTION;
        advance(g);
        return; }
    case PH_FX:
        apply_fx_choice(g, o);
        if (g->phase == PH_FX) return;
        advance(g);
        return;
    case PH_CONNECT_MOVE:
        if (o->kind == OPT_MOVE_YES) {
            do_ship_to(g, g->cur, g->ctxBld);
            game_log(g, TR("%s pousa 1 {C} em {%d} ({S}).", "%s lands 1 {C} on {%d} ({S}).", "%s aterriza 1 {C} en {%d} ({S})."), pname(g, g->cur), g->ctxBld);
        }
        g->phase = PH_ACTION;
        advance(g);
        return;
    case PH_ABILITY:
        apply_ability(g, o);
        g->phase = PH_ACTION;
        advance(g);
        return;
    case PH_CLEANUP: {
        int p = g->decider;
        P = &g->p[p];
        int total = P->nhand + P->nprep;
        int c = P->hand[o->a];
        remove_at(P->hand, &P->nhand, o->a);
        if (total > 4) g->discard[g->ndiscard++] = (uint8_t)c;
        else P->prep[P->nprep++] = (uint8_t)c;
        g->phase = PH_ACTION;
        advance(g);
        return; }
    default: return;
    }
}

/* ------------------------------------------------------------------ */
/* pontuacao                                                            */

void game_score(const Game *g, int p, Score *s) {
    memset(s, 0, sizeof(*s));
    int all = 1, four = 0;
    for (int b = 0; b < 4; b++) {
        int c = g->b[b].star1[p] + g->b[b].star2[p];
        s->star1 += 2 * g->b[b].star1[p];
        s->star2 += 4 * g->b[b].star2[p];
        if (c == 0) all = 0;
        if (c >= 4) four = 1;
    }
    s->ship = g->p[p].ship;
    s->allFour = all ? 2 : 0;
    s->fourInOne = four ? 3 : 0;
    int maxe = 0;
    for (int i = 0; i < g->np; i++) if (g->p[i].energy > maxe) maxe = g->p[i].energy;
    s->energy = g->p[p].energy == maxe ? 1 : 0;
    s->onMars = game_on_mars(g, p);
    s->total = s->ship + s->star1 + s->star2 + s->allFour + s->fourInOne + s->energy;
}

int game_winners(const Game *g) {
    int best = -1, bestMars = -1, mask = 0;
    for (int i = 0; i < g->np; i++) {
        Score s; game_score(g, i, &s);
        if (s.total > best || (s.total == best && s.onMars > bestMars)) {
            best = s.total; bestMars = s.onMars; mask = 1 << i;
        } else if (s.total == best && s.onMars == bestMars) mask |= 1 << i;
    }
    return mask;
}

/* ------------------------------------------------------------------ */
/* textos                                                               */

void game_fx_text(const Fx *fx, char *buf, int n) {
    char bl[8] = "";
    if (fx->a >= 0) snprintf(bl, sizeof(bl), "{%d}", fx->a);
    switch (fx->type) {
    case FX_EARTH: snprintf(buf, n, TR("Traga %d {C} da Terra.", "Bring %d {C} from Earth.", "Trae %d {C} de la Tierra."), fx->a); break;
    case FX_EARTH_IF_HAND: snprintf(buf, n, TR("Traga 1 {C} da Terra se tiver carta {%d} na {H}.", "Bring 1 {C} from Earth if you have a {%d} card in {H}.", "Trae 1 {C} de la Tierra si tienes carta {%d} en la {H}."), fx->a); break;
    case FX_EARTH_IF_ABSENT: snprintf(buf, n, TR("Traga %d {C} da Terra se não tiver {C} em {%d}.", "Bring %d {C} from Earth if you have no {C} in {%d}.", "Trae %d {C} de la Tierra si no tienes {C} en {%d}."), fx->b, fx->a); break;
    case FX_ENERGY: snprintf(buf, n, TR("Ganhe %d {E}.", "Gain %d {E}.", "Gana %d {E}."), fx->a); break;
    case FX_EARTH_ENERGY: snprintf(buf, n, TR("Traga 1 {C} da Terra e ganhe 1 {E}.", "Bring 1 {C} from Earth and gain 1 {E}.", "Trae 1 {C} de la Tierra y gana 1 {E}.")); break;
    case FX_DRAW: snprintf(buf, n, TR("Compre %d carta%s.", "Draw %d card%s.", "Roba %d carta%s."), fx->a, fx->a > 1 ? "s" : ""); break;
    case FX_ENERGY_IF_LOW: snprintf(buf, n, TR("Se tiver 2 {E} ou menos, ganhe %d {E}.", "If you have 2 {E} or less, gain %d {E}.", "Si tienes 2 {E} o menos, gana %d {E}."), fx->a); break;
    case FX_ENERGY_PER_BLD: snprintf(buf, n, TR("Ganhe 1 {E} por edifício com {C} seu (máx. 2).", "Gain 1 {E} per building with your {C} (max. 2).", "Gana 1 {E} por edificio con {C} tuyo (máx. 2).")); break;
    case FX_SHIP_TO:
        if (fx->a < 0) snprintf(buf, n, TR("Mova 1 {C} da nave para o {S} de qualquer edifício.", "Move 1 {C} from your ship to the {S} of any building.", "Mueve 1 {C} de la nave al {S} de cualquier edificio."));
        else snprintf(buf, n, TR("Mova 1 {C} da nave para o {S} de %s.", "Move 1 {C} from your ship to the {S} of %s.", "Mueve 1 {C} de la nave al {S} de %s."), bl);
        break;
    case FX_PAY_SHIP_TO: snprintf(buf, n, TR("Pague %d {E}: mova 1 {C} da nave para o {S} de qualquer edifício.", "Pay %d {E}: move 1 {C} from your ship to the {S} of any building.", "Paga %d {E}: mueve 1 {C} de la nave al {S} de cualquier edificio."), fx->a); break;
    case FX_PROMOTE:
        if (fx->a < 0) snprintf(buf, n, TR("Mova 1 {C} seu de {S} para {D} em qualquer edifício.", "Move 1 of your {C} from {S} to {D} in any building.", "Mueve 1 {C} tuyo de {S} a {D} en cualquier edificio."));
        else snprintf(buf, n, TR("Mova 1 {C} seu de {S} para {D} em %s.", "Move 1 of your {C} from {S} to {D} in %s.", "Mueve 1 {C} tuyo de {S} a {D} en %s."), bl);
        break;
    case FX_PROMOTE_TUCK:
        if (fx->a < 0) snprintf(buf, n, TR("Mova 1 {C} seu de {S} para {D}. Pode pôr 1 carta da {H} no fundo do baralho.", "Move 1 of your {C} from {S} to {D}. You may put 1 card from {H} at the bottom of the deck.", "Mueve 1 {C} tuyo de {S} a {D}. Puedes poner 1 carta de la {H} al fondo del mazo."));
        else snprintf(buf, n, TR("Mova 1 {C} seu de {S} para {D} em %s. Pode pôr 1 carta da {H} no fundo do baralho.", "Move 1 of your {C} from {S} to {D} in %s. You may put 1 card from {H} at the bottom of the deck.", "Mueve 1 {C} tuyo de {S} a {D} en %s. Puedes poner 1 carta de la {H} al fondo del mazo."), bl);
        break;
    case FX_SWITCH: snprintf(buf, n, TR("Troque 2 {C} (de jogadores diferentes) entre {S} e {D} num edifício.", "Swap 2 {C} (of different players) between {S} and {D} in a building.", "Intercambia 2 {C} (de jugadores distintos) entre {S} y {D} en un edificio.")); break;
    case FX_PAY_PROMOTE: snprintf(buf, n, TR("Pague %d {E}: mova 1 {C} seu de {S} para {D}.", "Pay %d {E}: move 1 of your {C} from {S} to {D}.", "Paga %d {E}: mueve 1 {C} tuyo de {S} a {D}."), fx->a); break;
    case FX_DEMOTE:
        if (fx->a > 0) snprintf(buf, n, TR("Pague %d {E}: mova 1 {C} adversário de {D} para {S}.", "Pay %d {E}: move 1 opponent {C} from {D} to {S}.", "Paga %d {E}: mueve 1 {C} rival de {D} a {S}."), fx->a);
        else snprintf(buf, n, TR("Mova 1 {C} adversário de {D} para {S}.", "Move 1 opponent {C} from {D} to {S}.", "Mueve 1 {C} rival de {D} a {S}."));
        break;
    case FX_PAY_EARTH: snprintf(buf, n, TR("Pague %d {E}: traga %d {C} da Terra.", "Pay %d {E}: bring %d {C} from Earth.", "Paga %d {E}: trae %d {C} de la Tierra."), fx->a, fx->b); break;
    case FX_TUCK_DRAW: snprintf(buf, n, TR("Ponha 1 carta da {H} no fundo do baralho e compre 2.", "Put 1 card from {H} at the bottom of the deck and draw 2.", "Pon 1 carta de la {H} al fondo del mazo y roba 2.")); break;
    default: snprintf(buf, n, "-"); break;
    }
}

void game_option_label(const Game *g, const Option *o, char *buf, int n) {
    const Player *P = (g->decider >= 0) ? &g->p[g->decider] : &g->p[g->cur];
    switch (o->kind) {
    case OPT_SKIP:
        if (g->phase == PH_CONNECT_MOVE) snprintf(buf, n, TR("Não mover colono", "Don't land a colonist", "No aterrizar colono"));
        else if (g->phase == PH_FX && g->ctxStep == 1) snprintf(buf, n, TR("Manter as cartas", "Keep the cards", "Conservar las cartas"));
        else snprintf(buf, n, TR("Pular", "Skip", "Saltar"));
        break;
    case OPT_PLAY_HAND: snprintf(buf, n, TR("{H} Jogar \"%s\"", "{H} Play \"%s\"", "{H} Jugar \"%s\""), card_name(P->hand[o->a])); break;
    case OPT_PLAY_PREP: { int c = P->prep[o->a];
        snprintf(buf, n, TR("{M} Conectar \"%s\" (%d) em {%d}", "{M} Connect \"%s\" (%d) to {%d}", "{M} Conectar \"%s\" (%d) en {%d}"), card_name(c), CARDS[c].value, CARDS[c].color); break; }
    case OPT_PLAY_OPP: { int c = g->p[o->a].prep[o->b];
        snprintf(buf, n, TR("{M} Módulo de %s: carta {%d} #%d", "{M} %s's module: {%d} card #%d", "{M} Módulo de %s: carta {%d} #%d"), player_name(o->a), CARDS[c].color, o->b + 1); break; }
    case OPT_TAKE_EARTH: snprintf(buf, n, TR("{C} Trazer 1 colono da Terra", "{C} Bring 1 colonist from Earth", "{C} Traer 1 colono de la Tierra")); break;
    case OPT_DISCARD_ENERGY: { int c = o->a == 0 ? P->hand[o->b] : P->prep[o->b];
        snprintf(buf, n, TR("{E} Descartar \"%s\" (%s) +1 energia", "{E} Discard \"%s\" (%s) +1 energy", "{E} Descartar \"%s\" (%s) +1 energía"), card_name(c), o->a == 0 ? TR("mão", "hand", "mano") : TR("módulo", "module", "módulo")); break; }
    case OPT_MOVE_YES: snprintf(buf, n, TR("Pousar 1 {C} da nave em {%d} ({S})", "Land 1 {C} from your ship on {%d} ({S})", "Aterrizar 1 {C} de la nave en {%d} ({S})"), o->a); break;
    case OPT_SHIP_TO: snprintf(buf, n, TR("Pousar {C} em {%d} %s", "Land {C} on {%d} %s", "Aterrizar {C} en {%d} %s"), o->a, color_name(o->a)); break;
    case OPT_PROMOTE: snprintf(buf, n, TR("Promover {C} em {%d} %s ({S}>{D})", "Promote {C} in {%d} %s ({S}>{D})", "Ascender {C} en {%d} %s ({S}>{D})"), o->a, color_name(o->a)); break;
    case OPT_ECO_MOVE: snprintf(buf, n, TR("{%d} %s: {C} %s %s", "{%d} %s: %s's {C} %s", "{%d} %s: {C} de %s %s"), o->a, color_name(o->a), player_name(o->b),
                                o->c == 0 ? "{S}>{D}" : "{D}>{S}"); break;
    case OPT_SWITCH: snprintf(buf, n, TR("{%d} %s sobe, %s desce", "{%d} %s up, %s down", "{%d} %s sube, %s baja"), o->a, player_name(o->b), player_name(o->c)); break;
    case OPT_DEMOTE: snprintf(buf, n, TR("{%d} Rebaixar {C} de %s", "{%d} Demote %s's {C}", "{%d} Bajar {C} de %s"), o->a, player_name(o->b)); break;
    case OPT_TUCK: { int c = o->a == 0 ? P->hand[o->b] : P->prep[o->b];
        snprintf(buf, n, TR("Fundo do baralho: \"%s\" (%s)", "Deck bottom: \"%s\" (%s)", "Fondo del mazo: \"%s\" (%s)"), card_name(c), o->a == 0 ? TR("mão", "hand", "mano") : TR("módulo", "module", "módulo")); break; }
    case OPT_CARD: { int c = P->hand[o->a];
        int discard = (g->phase == PH_CLEANUP && P->nhand + P->nprep > 4);
        snprintf(buf, n, "%s \"%s\" {%d} %d", discard ? TR("Descartar", "Discard", "Descartar") : TR("Ao módulo:", "To module:", "Al módulo:"), card_name(c), CARDS[c].color, CARDS[c].value); break; }
    case OPT_DRAFT: { int c = g->pool[g->decider][o->a];
        snprintf(buf, n, TR("Ficar com \"%s\" {%d} %d", "Keep \"%s\" {%d} %d", "Quedarse \"%s\" {%d} %d"), card_name(c), CARDS[c].color, CARDS[c].value); break; }
    case OPT_TAKE_EARTH_FX: snprintf(buf, n, TR("Pagar e trazer colonos", "Pay and bring colonists", "Pagar y traer colonos")); break;
    case OPT_PASS: snprintf(buf, n, TR("Passar a vez", "Pass", "Pasar el turno")); break;
    default: snprintf(buf, n, "?"); break;
    }
}

/* ------------------------------------------------------------------ */
/* invariantes                                                          */

int game_check(const Game *g, char *err, int n) {
    int seen[NCARDS] = {0};
    int total = 0;
#define MARK(c) do { if ((c) >= NCARDS || seen[(c)]++) { snprintf(err, n, "carta duplicada/invalida %d", (c)); return 0; } total++; } while (0)
    for (int i = 0; i < g->ndeck; i++) MARK(g->deck[i]);
    for (int i = 0; i < g->ndiscard; i++) MARK(g->discard[i]);
    for (int b = 0; b < NCOLORS; b++) for (int i = 0; i < g->b[b].nstack; i++) {
        MARK(g->b[b].stack[i]);
        if (CARDS[g->b[b].stack[i]].color != b) { snprintf(err, n, "cor errada no edificio"); return 0; }
    }
    for (int p = 0; p < g->np; p++) {
        for (int i = 0; i < g->p[p].nhand; i++) MARK(g->p[p].hand[i]);
        for (int i = 0; i < g->p[p].nprep; i++) MARK(g->p[p].prep[i]);
        for (int i = 0; i < g->npool[p]; i++) MARK(g->pool[p][i]);
        int col = g->p[p].ship + g->p[p].earth + game_on_mars(g, p);
        if (col != NCOLONISTS) { snprintf(err, n, "jogador %d tem %d colonos", p, col); return 0; }
        if (g->p[p].energy > MAX_ENERGY) { snprintf(err, n, "energia > max"); return 0; }
        if (g->p[p].nprep > 2) { snprintf(err, n, "modulo > 2"); return 0; }
    }
#undef MARK
    if (total != NCARDS) { snprintf(err, n, "total de cartas %d", total); return 0; }
    for (int b = 0; b < 4; b++)
        if (game_star2_total(g, b) > game_star2_cap(g)) { snprintf(err, n, "** cheio demais em %d", b); return 0; }
    for (int b = 0; b < NCOLORS; b++) if (b == C_CONSTR) {
        for (int p = 0; p < g->np; p++) if (g->b[b].star1[p] || g->b[b].star2[p]) { snprintf(err, n, "colono na construcao"); return 0; }
    }
    return 1;
}
