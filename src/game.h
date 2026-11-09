/*
 * Mars Colony: Project Red PS2
 * Motor de regras (C portatil, sem dependencias). O estado do jogo e uma
 * struct sem ponteiros, de modo que a IA pode copia-la livremente para
 * simular jogadas.
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define MAXP        4
#define NCARDS      35
#define NCOLONISTS  7
#define MAX_ENERGY  7
#define MAX_HAND    10
#define MAX_OPTS    96
#define LOG_LINES   6
#define LOG_LEN     96

/* Cores / tipos de edificio e de carta de projeto */
enum { C_ECO = 0, C_ENERGY, C_SCIENCE, C_WATER, C_CONSTR, NCOLORS };

/* Fases (cada fase com decisao pendente tem um "decider") */
enum {
    PH_DRAFT = 0,     /* variante: escolher 1 carta do lote               */
    PH_SETUP_PREP,    /* preparacao: escolher cartas para o modulo        */
    PH_ACTION,        /* escolher uma das 5 acoes do turno                */
    PH_FX,            /* resolver efeito de carta (com alvo a escolher)   */
    PH_CONNECT_MOVE,  /* valor maior: mover colono da nave para o edificio*/
    PH_ABILITY,       /* habilidade do edificio                           */
    PH_CLEANUP,       /* regra das 4 cartas                               */
    PH_OVER
};

/* Tipos de efeito das cartas */
enum {
    FX_NONE = 0,
    FX_EARTH,          /* a = n: traga n colonos da Terra                   */
    FX_EARTH_IF_HAND,  /* a = cor: traga 1 se tiver carta dessa cor na mao   */
    FX_EARTH_IF_ABSENT,/* a = edificio, b = n                                */
    FX_ENERGY,         /* a = n                                              */
    FX_EARTH_ENERGY,   /* traga 1 colono e ganhe 1 energia                   */
    FX_DRAW,           /* a = n                                              */
    FX_ENERGY_IF_LOW,  /* a = n, se energia <= 2                             */
    FX_ENERGY_PER_BLD, /* +1 energia por edificio com colono seu (max 2)     */
    FX_SHIP_TO,        /* a = edificio (-1 qualquer)                         */
    FX_PAY_SHIP_TO,    /* a = custo                                          */
    FX_PROMOTE,        /* a = edificio (-1 qualquer)                         */
    FX_PROMOTE_TUCK,   /* a = edificio; depois pode por 1 carta no fundo     */
    FX_SWITCH,         /* troca 2 colonos entre * e ** num edificio          */
    FX_PAY_PROMOTE,    /* a = custo                                          */
    FX_DEMOTE,         /* a = custo: rebaixa colono adversario ** -> *       */
    FX_PAY_EARTH,      /* a = custo, b = n                                   */
    FX_TUCK_DRAW,      /* poe 1 carta da mao no fundo e compra 2             */
    FX_COUNT
};

/* Tipos de opcao de decisao */
enum {
    OPT_SKIP = 0,
    OPT_PLAY_HAND,      /* a = indice na mao                       */
    OPT_PLAY_PREP,      /* a = indice no modulo                    */
    OPT_PLAY_OPP,       /* a = jogador, b = indice no modulo       */
    OPT_TAKE_EARTH,
    OPT_DISCARD_ENERGY, /* a = 0 mao / 1 modulo, b = indice        */
    OPT_MOVE_YES,
    OPT_SHIP_TO,        /* a = edificio                            */
    OPT_PROMOTE,        /* a = edificio                            */
    OPT_ECO_MOVE,       /* a = edificio, b = jogador, c = 0 sobe / 1 desce */
    OPT_SWITCH,         /* a = edificio, b = quem sobe, c = quem desce     */
    OPT_DEMOTE,         /* a = edificio, b = jogador               */
    OPT_TUCK,           /* a = 0 mao / 1 modulo, b = indice        */
    OPT_CARD,           /* a = indice na mao (descarte / modulo)   */
    OPT_DRAFT,          /* a = indice no lote                      */
    OPT_TAKE_EARTH_FX,  /* confirmar pagamento p/ trazer colonos   */
    OPT_PASS
};

typedef struct { uint8_t type; int8_t a, b; } Fx;

typedef struct {
    const char *name;
    uint8_t color;
    uint8_t value;
    Fx top;     /* jogada da mao   */
    Fx bottom;  /* jogada do modulo */
} CardDef;

typedef struct { uint8_t kind; int8_t a, b, c; } Option;

typedef struct {
    uint8_t ship, earth, energy;
    uint8_t hand[MAX_HAND]; uint8_t nhand;
    uint8_t prep[2];        uint8_t nprep;
    uint8_t human;          /* 1 = humano, 0 = CPU         */
    uint8_t ailevel;        /* 0 facil, 1 normal, 2 dificil */
} Player;

typedef struct {
    uint8_t star1[MAXP];    /* espaco *  (ilimitado)        */
    uint8_t star2[MAXP];    /* espaco ** (limitado)         */
    uint8_t stack[NCARDS];  /* cartas conectadas, topo = ultimo */
    uint8_t nstack;
} Building;

typedef struct {
    int np;
    Player p[MAXP];
    Building b[NCOLORS];
    uint8_t deck[NCARDS];    uint8_t ndeck;   /* topo = ultimo */
    uint8_t discard[NCARDS]; uint8_t ndiscard;

    int start, cur, round;
    int phase, decider;

    /* contexto da jogada em andamento */
    int8_t ctxCard, ctxOwner, ctxActor, ctxBld, ctxStep, ctxFlow, ctxFromOpp, ctxBottom;
    int8_t cleanupDone;      /* quantos jogadores ja passaram pela regra das 4 cartas */
    int8_t setupIdx;

    /* variante draft */
    int8_t useDraft, draftPick;
    uint8_t pool[MAXP][5]; uint8_t npool[MAXP];

    uint32_t rng;
    uint8_t quiet;           /* nao registrar log (simulacoes da IA) */
    uint8_t chance;          /* houve compra/embaralhamento neste turno */
    uint8_t lastBld;         /* ultimo edificio afetado (destaque na UI) */

    char log[LOG_LINES][LOG_LEN];
    int nlog;
} Game;

typedef struct {
    int ship, star1, star2, allFour, fourInOne, energy, total, onMars;
} Score;

extern const CardDef CARDS[NCARDS];

/* idioma: 0 portugues (principal), 1 ingles, 2 espanhol */
enum { LANG_PT = 0, LANG_EN, LANG_ES, LANG_COUNT };
extern int g_lang;
#define TR(pt, en, es) (g_lang == LANG_EN ? (en) : g_lang == LANG_ES ? (es) : (pt))
const char *card_name(int c);
const char *color_name(int b);
const char *bld_name(int b);
const char *bld_ability(int b);
const char *player_name(int p);

void game_init(Game *g, int np, const int human[MAXP], const int ailevel[MAXP],
               int draft, uint32_t seed);
int  game_options(const Game *g, Option *out);      /* retorna n >= 1 se fase != OVER */
void game_apply(Game *g, const Option *o);
void game_score(const Game *g, int p, Score *s);
int  game_winners(const Game *g);                   /* bitmask */
int  game_star2_cap(const Game *g);
int  game_star2_total(const Game *g, int b);
int  game_on_mars(const Game *g, int p);
int  game_top_value(const Game *g, int b);          /* valor visivel (3 se vazio) */
void game_option_label(const Game *g, const Option *o, char *buf, int n);
void game_fx_text(const Fx *fx, char *buf, int n);
void game_log(Game *g, const char *fmt, ...);
int  game_check(const Game *g, char *err, int n);   /* invariantes (testes) */

#endif
