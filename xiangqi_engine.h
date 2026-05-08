/*
 * Exact Chinese Chess Xiangqi rules engine
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef XIANGQI_ENGINE_H
#define XIANGQI_ENGINE_H

#include <stdbool.h>

#define XIANGQI_ROWS 10
#define XIANGQI_COLS 9
#define XIANGQI_PIECES 32
#define XIANGQI_MAX_HISTORY 512
#define XIANGQI_MAX_MOVES 256

typedef enum {
    XIANGQI_BLACK = 0,
    XIANGQI_RED = 1
} XiangqiSide;

typedef enum {
    XIANGQI_GENERAL = 0,
    XIANGQI_ADVISOR,
    XIANGQI_ELEPHANT,
    XIANGQI_HORSE,
    XIANGQI_ROOK,
    XIANGQI_CANNON,
    XIANGQI_SOLDIER
} XiangqiPieceType;

typedef struct {
    int row;
    int col;
    int id;
    bool dead;
    XiangqiSide side;
    XiangqiPieceType type;
} XiangqiPiece;

typedef struct {
    int move_id;
    int capture_id;
    int from_row;
    int from_col;
    int to_row;
    int to_col;
} XiangqiMove;

typedef struct {
    XiangqiPiece pieces[XIANGQI_PIECES];
    XiangqiMove history[XIANGQI_MAX_HISTORY];
    int history_len;
    XiangqiSide turn;
    bool game_over;
    XiangqiSide winner;
    int selected_id;
} XiangqiGame;

typedef enum {
    XIANGQI_AI_EASY = 0,
    XIANGQI_AI_MEDIUM,
    XIANGQI_AI_HARD
} XiangqiAiLevel;

void xiangqi_init(XiangqiGame *game);
int xiangqi_piece_at(const XiangqiGame *game, int row, int col);
bool xiangqi_is_legal_move(const XiangqiGame *game, int move_id, int row, int col);
bool xiangqi_apply_move(XiangqiGame *game, int move_id, int row, int col, XiangqiMove *out_move);
bool xiangqi_undo(XiangqiGame *game);
int xiangqi_generate_moves(const XiangqiGame *game, XiangqiSide side, XiangqiMove *moves, int max_moves);
bool xiangqi_in_check(const XiangqiGame *game, XiangqiSide side);
bool xiangqi_has_legal_move(const XiangqiGame *game, XiangqiSide side);
bool xiangqi_choose_ai_move(XiangqiGame *game, XiangqiAiLevel level, XiangqiMove *best_move);
const char *xiangqi_piece_text(const XiangqiPiece *piece);
const char *xiangqi_side_name(XiangqiSide side);
void xiangqi_move_label(const XiangqiGame *game, const XiangqiMove *move, char *buf, int len);

#endif
