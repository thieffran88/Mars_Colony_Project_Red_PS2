#ifndef AI_H
#define AI_H
#include "game.h"

/* Escolhe uma opcao (indice em game_options) para g->decider. */
int ai_choose(const Game *g, uint32_t *seed);

/* chamado periodicamente durante a busca (ex.: manter o audio do PS2 tocando) */
extern void (*ai_idle)(void);

#endif
