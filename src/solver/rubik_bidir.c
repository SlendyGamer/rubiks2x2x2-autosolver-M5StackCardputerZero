/*
 * SPDX-License-Identifier: MIT
 *
 * Bidirectional BFS for the 2x2x2 cube.
 *
 * This is NOT part of the original author's solver. It is a separate file that
 * leaves legacy/ untouched and reuses the author's move functions through
 * rubik_apply_move() (rubik_solver.c).
 *
 * Why it exists: the original BFS has no visited set, so the same cube state
 * is stored once per path that reaches it and memory explodes (~1.4 GB for a
 * 12-move scramble). Here every state is stored once, and the search runs from
 * the scrambled cube and from the solved cube at the same time, meeting in the
 * middle (at most 7 moves from each side, so at most 14 in total, the same
 * limit as the original search()).
 *
 * The result is a shortest solution (same metric as the original BFS: each of
 * x X y Y z Z counts as 1 move):
 *  - every move has an inverse, so searching backwards from the solved cube is
 *    valid;
 *  - layers are expanded completely, and a meeting is only looked for against
 *    layers that were already completely checked, so the first meeting found
 *    has length (depth_a + depth_b + 1) and nothing shorter can exist.
 */

#include "rubik_solver.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MOVE_COUNT 6
#define SIDES 2
#define MAX_DEPTH_PER_SIDE 7
#define NO_PARENT (-1)

static const char kMoves[MOVE_COUNT] = {'x', 'X', 'y', 'Y', 'z', 'Z'};

typedef struct {
    char pattern[RUBIK_STICKER_COUNT];
    char move;      /* move applied to the parent to reach this state */
    uint8_t side;   /* 0: grown from the scrambled cube, 1: from the solved cube */
    uint8_t depth;
    int32_t parent;
} Entry;

typedef struct {
    Entry* entries;
    size_t count;
    size_t capacity;
    int32_t* slots; /* open addressing, -1 = empty */
    size_t slot_count;
} Table;

typedef struct {
    int32_t* items;
    size_t count;
    size_t capacity;
} Frontier;

static char inverse_move(char move) {
    return (move >= 'a' && move <= 'z') ? (char)(move - 'a' + 'A') : (char)(move - 'A' + 'a');
}

static uint64_t hash_pattern(const char* pattern) {
    uint64_t h = 1469598103934665603ULL;
    for (int i = 0; i < RUBIK_STICKER_COUNT; ++i) {
        h ^= (uint8_t)pattern[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static int table_init(Table* t) {
    t->count = 0;
    t->capacity = 1024;
    t->slot_count = 4096;
    t->entries = (Entry*)malloc(t->capacity * sizeof(Entry));
    t->slots = (int32_t*)malloc(t->slot_count * sizeof(int32_t));
    if (!t->entries || !t->slots) {
        return 0;
    }
    memset(t->slots, 0xff, t->slot_count * sizeof(int32_t));
    return 1;
}

static void table_free(Table* t) {
    free(t->entries);
    free(t->slots);
}

static int32_t table_find(const Table* t, const char* pattern) {
    size_t i = (size_t)hash_pattern(pattern) & (t->slot_count - 1);
    while (t->slots[i] != -1) {
        if (memcmp(t->entries[t->slots[i]].pattern, pattern, RUBIK_STICKER_COUNT) == 0) {
            return t->slots[i];
        }
        i = (i + 1) & (t->slot_count - 1);
    }
    return -1;
}

static int table_grow_slots(Table* t) {
    const size_t new_count = t->slot_count * 2;
    int32_t* slots = (int32_t*)malloc(new_count * sizeof(int32_t));
    if (!slots) {
        return 0;
    }
    memset(slots, 0xff, new_count * sizeof(int32_t));
    for (size_t e = 0; e < t->count; ++e) {
        size_t i = (size_t)hash_pattern(t->entries[e].pattern) & (new_count - 1);
        while (slots[i] != -1) {
            i = (i + 1) & (new_count - 1);
        }
        slots[i] = (int32_t)e;
    }
    free(t->slots);
    t->slots = slots;
    t->slot_count = new_count;
    return 1;
}

/* Returns the new entry index, or -1 when out of memory. */
static int32_t table_insert(Table* t, const char* pattern, char move, uint8_t side,
                            uint8_t depth, int32_t parent) {
    if (t->count == t->capacity) {
        Entry* grown = (Entry*)realloc(t->entries, t->capacity * 2 * sizeof(Entry));
        if (!grown) {
            return -1;
        }
        t->entries = grown;
        t->capacity *= 2;
    }
    if ((t->count + 1) * 2 > t->slot_count && !table_grow_slots(t)) {
        return -1;
    }

    Entry* e = &t->entries[t->count];
    memcpy(e->pattern, pattern, RUBIK_STICKER_COUNT);
    e->move = move;
    e->side = side;
    e->depth = depth;
    e->parent = parent;

    size_t i = (size_t)hash_pattern(pattern) & (t->slot_count - 1);
    while (t->slots[i] != -1) {
        i = (i + 1) & (t->slot_count - 1);
    }
    t->slots[i] = (int32_t)t->count;
    return (int32_t)t->count++;
}

static int frontier_push(Frontier* f, int32_t item) {
    if (f->count == f->capacity) {
        const size_t capacity = f->capacity ? f->capacity * 2 : 256;
        int32_t* grown = (int32_t*)realloc(f->items, capacity * sizeof(int32_t));
        if (!grown) {
            return 0;
        }
        f->items = grown;
        f->capacity = capacity;
    }
    f->items[f->count++] = item;
    return 1;
}

/* Moves from the scrambled cube (root of side 0) down to `idx`, first move first. */
static int append_start_to(const Table* t, int32_t idx, char* out, int n, int max) {
    int len = 0;
    for (int32_t i = idx; t->entries[i].parent != NO_PARENT; i = t->entries[i].parent) {
        ++len;
    }
    if (n + len > max) {
        return -1;
    }
    int pos = n + len;
    for (int32_t i = idx; t->entries[i].parent != NO_PARENT; i = t->entries[i].parent) {
        out[--pos] = t->entries[i].move;
    }
    return n + len;
}

/* Moves from `idx` (side 1) to the solved cube: the inverses of the stored moves. */
static int append_to_goal(const Table* t, int32_t idx, char* out, int n, int max) {
    for (int32_t i = idx; t->entries[i].parent != NO_PARENT; i = t->entries[i].parent) {
        if (n >= max) {
            return -1;
        }
        out[n++] = inverse_move(t->entries[i].move);
    }
    return n;
}

int rubik_bidir_solve(const char pattern[RUBIK_STICKER_COUNT], char* moves_out, int max_moves) {
    char goal[RUBIK_STICKER_COUNT];
    rubik_goal_pattern(goal);
    if (memcmp(pattern, goal, RUBIK_STICKER_COUNT) == 0) {
        return 0;
    }

    Table table;
    Frontier current[SIDES] = {{NULL, 0, 0}, {NULL, 0, 0}};
    Frontier next = {NULL, 0, 0};
    int result = -1;

    if (!table_init(&table)) {
        goto cleanup;
    }
    {
        const int32_t start = table_insert(&table, pattern, '-', 0, 0, NO_PARENT);
        const int32_t end = table_insert(&table, goal, '-', 1, 0, NO_PARENT);
        if (start < 0 || end < 0 || !frontier_push(&current[0], start) ||
            !frontier_push(&current[1], end)) {
            goto cleanup;
        }
    }

    int depth[SIDES] = {0, 0};
    while (depth[0] < MAX_DEPTH_PER_SIDE || depth[1] < MAX_DEPTH_PER_SIDE) {
        /* Expand the smaller frontier; either order keeps the result optimal. */
        int side = current[0].count <= current[1].count ? 0 : 1;
        if (depth[side] >= MAX_DEPTH_PER_SIDE) {
            side = 1 - side;
        }
        if (current[side].count == 0) {
            break;
        }

        int best_total = -1;
        int32_t best_parent = -1;
        int32_t best_other = -1;
        char best_move = 0;

        next.count = 0;
        for (size_t f = 0; f < current[side].count; ++f) {
            const int32_t parent = current[side].items[f];
            for (int m = 0; m < MOVE_COUNT; ++m) {
                char candidate[RUBIK_STICKER_COUNT];
                memcpy(candidate, table.entries[parent].pattern, RUBIK_STICKER_COUNT);
                rubik_apply_move(candidate, kMoves[m]);

                const int32_t found = table_find(&table, candidate);
                if (found < 0) {
                    const int32_t added = table_insert(&table, candidate, kMoves[m], (uint8_t)side,
                                                       (uint8_t)(depth[side] + 1), parent);
                    if (added < 0 || !frontier_push(&next, added)) {
                        goto cleanup;
                    }
                } else if (table.entries[found].side != side) {
                    const int total = depth[side] + 1 + table.entries[found].depth;
                    if (best_total < 0 || total < best_total) {
                        best_total = total;
                        best_parent = parent;
                        best_other = found;
                        best_move = kMoves[m];
                    }
                }
            }
        }

        if (best_total >= 0) {
            int n = 0;
            if (side == 0) {
                /* start .. parent, move, then other (side 1) down to the goal */
                n = append_start_to(&table, best_parent, moves_out, 0, max_moves);
                if (n < 0 || n >= max_moves) {
                    goto cleanup;
                }
                moves_out[n++] = best_move;
                n = append_to_goal(&table, best_other, moves_out, n, max_moves);
            } else {
                /* start .. other (side 0), undo move to reach parent (side 1), then to the goal */
                n = append_start_to(&table, best_other, moves_out, 0, max_moves);
                if (n < 0 || n >= max_moves) {
                    goto cleanup;
                }
                moves_out[n++] = inverse_move(best_move);
                n = append_to_goal(&table, best_parent, moves_out, n, max_moves);
            }
            if (n < 0) {
                goto cleanup;
            }

            /* Defensive check: the returned moves must really solve the cube. */
            char replay[RUBIK_STICKER_COUNT];
            memcpy(replay, pattern, RUBIK_STICKER_COUNT);
            for (int i = 0; i < n; ++i) {
                rubik_apply_move(replay, moves_out[i]);
            }
            if (memcmp(replay, goal, RUBIK_STICKER_COUNT) == 0) {
                result = n;
            }
            goto cleanup;
        }

        Frontier swap = current[side];
        current[side] = next;
        next = swap;
        ++depth[side];
    }

cleanup:
    free(current[0].items);
    free(current[1].items);
    free(next.items);
    table_free(&table);
    return result;
}
