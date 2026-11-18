/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Thin wrapper over the original solver in legacy/ (actions.h, pilhas.h, FILA.h, no.h).
 * Those files are the author's code and are kept untouched.
 */

#define RUBIK_STICKER_COUNT 24

typedef enum {
    RUBIK_ALGO_DFS = 0,
    RUBIK_ALGO_BFS = 1,
} RubikAlgo;

void rubik_goal_pattern(char out[RUBIK_STICKER_COUNT]);
int rubik_is_solved(const char pattern[RUBIK_STICKER_COUNT]);

/* move is one of x X y Y z Z; any other value is ignored. */
void rubik_apply_move(char pattern[RUBIK_STICKER_COUNT], char move);

/*
 * Runs the original search() from `pattern` to the solved pattern.
 * Returns the number of moves written to `moves_out` (first move first),
 * or -1 when no solution was found. Blocking: call it from a worker thread.
 */
int rubik_solve(const char pattern[RUBIK_STICKER_COUNT],
                RubikAlgo algo,
                char* moves_out,
                int max_moves);

#ifdef __cplusplus
}
#endif
