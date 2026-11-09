/* Simulador sem interface: joga muitas partidas CPU x CPU (e aleatorias),
 * verificando invariantes a cada decisao. */
#include "../src/game.h"
#include "../src/ai.h"
#include <stdio.h>
#include <stdlib.h>

static int play(int np, int draft, uint32_t seed, int randomOnly, const int lv[MAXP], int *rounds, int *winmask,
                int scores[MAXP]) {
    int human[MAXP] = {0}, al[MAXP];
    for (int i = 0; i < MAXP; i++) al[i] = lv[i];
    Game g;
    game_init(&g, np, human, al, draft, seed);
    uint32_t s = seed * 2654435761u + 1;
    int steps = 0;
    char err[128];
    while (g.phase != PH_OVER) {
        Option o[MAX_OPTS];
        int n = game_options(&g, o);
        if (n <= 0) { printf("seed %u: sem opcoes na fase %d\n", seed, g.phase); return 0; }
        int k;
        if (randomOnly) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; k = (int)(s % (uint32_t)n); }
        else k = ai_choose(&g, &s);
        game_apply(&g, &o[k]);
        if (!game_check(&g, err, sizeof err)) { printf("seed %u: invariante: %s (fase %d)\n", seed, err, g.phase); return 0; }
        if (++steps > 20000) { printf("seed %u: loop infinito?\n", seed); return 0; }
    }
    *rounds = g.round;
    *winmask = game_winners(&g);
    for (int i = 0; i < np; i++) { Score sc; game_score(&g, i, &sc); scores[i] = sc.total; }
    return 1;
}

int main(int argc, char **argv) {
    int games = argc > 1 ? atoi(argv[1]) : 200;
    int ok = 1;
    for (int mode = 0; mode < 2; mode++) {
        for (int np = 2; np <= 4; np++) {
            long rsum = 0; int wins[MAXP] = {0}; long ssum = 0; int rmax = 0;
            int lv[MAXP] = {1, 1, 1, 1};
            if (mode == 1) { lv[0] = 2; lv[1] = 0; lv[2] = 0; lv[3] = 0; }
            int N = mode == 0 ? games * 5 : games;
            for (int i = 0; i < N; i++) {
                int r, w, sc[MAXP];
                if (!play(np, i & 1, (uint32_t)(i * 7919 + np), mode == 0, lv, &r, &w, sc)) { ok = 0; break; }
                rsum += r; if (r > rmax) rmax = r;
                for (int p = 0; p < np; p++) { if (w >> p & 1) wins[p]++; ssum += sc[p]; }
            }
            printf("%s %dp: rodadas med %.1f (max %d), pontos med %.1f, vitorias:",
                   mode == 0 ? "ALEATORIO" : "IA dificil(P0) x faceis", np,
                   (double)rsum / N, rmax, (double)ssum / N / np);
            for (int p = 0; p < np; p++) printf(" %d", wins[p]);
            printf("\n");
        }
    }
    printf(ok ? "OK\n" : "FALHOU\n");
    return ok ? 0 : 1;
}
