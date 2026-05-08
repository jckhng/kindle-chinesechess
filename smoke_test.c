/*
 * Exact Chinese Chess smoke tests
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "xiangqi_engine.h"

#include <stdio.h>

int main(void) {
    XiangqiGame game;
    XiangqiMove move;
    XiangqiMove moves[XIANGQI_MAX_MOVES];
    int count;

    xiangqi_init(&game);
    count = xiangqi_generate_moves(&game, XIANGQI_RED, moves, XIANGQI_MAX_MOVES);
    if (count <= 0) {
        fprintf(stderr, "expected legal opening moves\n");
        return 1;
    }
    if (!xiangqi_apply_move(&game, 26, 6, 1, &move)) {
        fprintf(stderr, "expected red cannon to move from b3 to b4\n");
        return 1;
    }
    if (!xiangqi_undo(&game) || game.history_len != 0 || game.turn != XIANGQI_RED) {
        fprintf(stderr, "undo failed\n");
        return 1;
    }
    if (!xiangqi_choose_ai_move(&game, XIANGQI_AI_EASY, &move)) {
        fprintf(stderr, "AI did not choose a move\n");
        return 1;
    }
    printf("xiangqi smoke test passed: %d opening moves\n", count);
    return 0;
}
