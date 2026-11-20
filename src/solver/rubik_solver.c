/*
 * SPDX-License-Identifier: MIT
 */

#include "rubik_solver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The legacy headers define non-static functions: include them from this file only. */
#include "legacy/no.h"
#include "legacy/FILA.h"
#include "legacy/pilhas.h"
#include "legacy/actions.h"

/* Same strategy tables as 3d/main.c */
static const strat DFS = { (void* (*)(void))CriaPilha, (void (*)(void*, No*))push,       (No* (*)(void*))pop,        (int (*)(void*))vaziaPilha, (void (*)(void*, Pilha*))preparePathPilha, (void* (*)(void*))libera};
static const strat BFS = { (void* (*)(void))CriaFila,  (void (*)(void*, No*))InsereFila, (No* (*)(void*))RetiraFila, (int (*)(void*))VaziaFila,  (void (*)(void*, Pilha*))preparePathFila, (void* (*)(void*))liberaFila};

/* Same as `solution` in 3d/main.c */
static const char kGoal[RUBIK_STICKER_COUNT] = {
    18, 17, 4,  1,
    12, 21, 9,  6,
    22, 20, 8,  5,
    15, 14, 0,  10,
    3,  2,  7,  11,
    23, 13, 19, 16,
};

void rubik_goal_pattern(char out[RUBIK_STICKER_COUNT]) {
    memcpy(out, kGoal, RUBIK_STICKER_COUNT);
}

int rubik_is_solved(const char pattern[RUBIK_STICKER_COUNT]) {
    return memcmp(pattern, kGoal, RUBIK_STICKER_COUNT) == 0;
}

void rubik_apply_move(char pattern[RUBIK_STICKER_COUNT], char move) {
    No node;
    memset(&node, 0, sizeof(node));
    memcpy(node.pattern, pattern, RUBIK_STICKER_COUNT);
    switch(move) {
        case 'x': rotate_xX(&node); break;
        case 'X': rotate_NOTxX(&node); break;
        case 'y': rotate_yY(&node); break;
        case 'Y': rotate_NOTyY(&node); break;
        case 'z': rotate_zZ(&node); break;
        case 'Z': rotate_NOTzZ(&node); break;
        default: return;
    }
    memcpy(pattern, node.pattern, RUBIK_STICKER_COUNT);
}

int rubik_solve(const char pattern[RUBIK_STICKER_COUNT],
                RubikAlgo algo,
                char* moves_out,
                int max_moves) {
    if(algo == RUBIK_ALGO_BFS_BIDIR) {
        return rubik_bidir_solve(pattern, moves_out, max_moves);
    }

    char goal[RUBIK_STICKER_COUNT];
    memcpy(goal, kGoal, RUBIK_STICKER_COUNT);

    /* Same sequence as the 'd' / 'b' keys in 3d/main.c */
    No* state = (No*)malloc(sizeof(No));
    /* search() does not initialise cNx/cNy/cNz of the root; garbage there prunes valid moves. */
    memset(state, 0, sizeof(No));
    memcpy(state->pattern, pattern, RUBIK_STICKER_COUNT);
    const strat ds = (algo == RUBIK_ALGO_BFS) ? BFS : DFS;
    void* open = ds.criaStruct();
    Pilha* path = CriaPilha();

    int result = -1;
    if(search(open, state, goal, path, ds)) {
        int count = 0;
        for(No* n = path->Topo; n != NULL && count < max_moves; n = n->prox) {
            moves_out[count++] = n->rotation;
        }
        result = count;
    }

    ds.liberaStruct(open);
    libera(path);
    return result;
}
