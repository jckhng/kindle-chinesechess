/*
 * Exact Chinese Chess Xiangqi rules engine
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Adapted from the GPL-3.0-or-later XMuli ChineseChess project.
 */

#include "xiangqi_engine.h"

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int row;
    int col;
    XiangqiPieceType type;
} StartPiece;

static const StartPiece black_start[16] = {
    {0, 0, XIANGQI_ROOK}, {0, 1, XIANGQI_HORSE}, {0, 2, XIANGQI_ELEPHANT},
    {0, 3, XIANGQI_ADVISOR}, {0, 4, XIANGQI_GENERAL}, {0, 5, XIANGQI_ADVISOR},
    {0, 6, XIANGQI_ELEPHANT}, {0, 7, XIANGQI_HORSE}, {0, 8, XIANGQI_ROOK},
    {2, 1, XIANGQI_CANNON}, {2, 7, XIANGQI_CANNON},
    {3, 0, XIANGQI_SOLDIER}, {3, 2, XIANGQI_SOLDIER}, {3, 4, XIANGQI_SOLDIER},
    {3, 6, XIANGQI_SOLDIER}, {3, 8, XIANGQI_SOLDIER}
};

static const int piece_value[] = { 10000, 20, 40, 60, 100, 80, 10 };

static bool in_bounds(int row, int col) {
    return row >= 0 && row < XIANGQI_ROWS && col >= 0 && col < XIANGQI_COLS;
}

static XiangqiSide other_side(XiangqiSide side) {
    return side == XIANGQI_RED ? XIANGQI_BLACK : XIANGQI_RED;
}

void xiangqi_init(XiangqiGame *game) {
    int i;

    memset(game, 0, sizeof(*game));
    for (i = 0; i < 16; i++) {
        game->pieces[i].row = black_start[i].row;
        game->pieces[i].col = black_start[i].col;
        game->pieces[i].type = black_start[i].type;
        game->pieces[i].side = XIANGQI_BLACK;
        game->pieces[i].id = i;
        game->pieces[i].dead = false;

        game->pieces[i + 16].row = 9 - black_start[i].row;
        game->pieces[i + 16].col = 8 - black_start[i].col;
        game->pieces[i + 16].type = black_start[i].type;
        game->pieces[i + 16].side = XIANGQI_RED;
        game->pieces[i + 16].id = i + 16;
        game->pieces[i + 16].dead = false;
    }
    game->turn = XIANGQI_RED;
    game->winner = XIANGQI_BLACK;
    game->selected_id = -1;
}

int xiangqi_piece_at(const XiangqiGame *game, int row, int col) {
    int i;

    if (!in_bounds(row, col)) {
        return -1;
    }
    for (i = 0; i < XIANGQI_PIECES; i++) {
        if (!game->pieces[i].dead && game->pieces[i].row == row && game->pieces[i].col == col) {
            return i;
        }
    }
    return -1;
}

static int line_count(const XiangqiGame *game, int row1, int col1, int row2, int col2) {
    int count = 0;
    int r;
    int c;

    if (row1 != row2 && col1 != col2) {
        return -1;
    }
    if (row1 == row2) {
        int min = col1 < col2 ? col1 : col2;
        int max = col1 > col2 ? col1 : col2;
        for (c = min + 1; c < max; c++) {
            if (xiangqi_piece_at(game, row1, c) != -1) {
                count++;
            }
        }
    } else {
        int min = row1 < row2 ? row1 : row2;
        int max = row1 > row2 ? row1 : row2;
        for (r = min + 1; r < max; r++) {
            if (xiangqi_piece_at(game, r, col1) != -1) {
                count++;
            }
        }
    }
    return count;
}

static bool palace_contains(XiangqiSide side, int row, int col) {
    if (col < 3 || col > 5) {
        return false;
    }
    return side == XIANGQI_RED ? (row >= 7 && row <= 9) : (row >= 0 && row <= 2);
}

static bool raw_move_legal(const XiangqiGame *game, int move_id, int capture_id, int row, int col) {
    const XiangqiPiece *p;
    int dr;
    int dc;
    int adr;
    int adc;
    int count;

    if (move_id < 0 || move_id >= XIANGQI_PIECES || !in_bounds(row, col)) {
        return false;
    }
    p = &game->pieces[move_id];
    if (p->dead) {
        return false;
    }
    if (capture_id != -1 && game->pieces[capture_id].side == p->side) {
        return false;
    }

    dr = row - p->row;
    dc = col - p->col;
    adr = abs(dr);
    adc = abs(dc);

    switch (p->type) {
    case XIANGQI_GENERAL:
        if (capture_id != -1 && game->pieces[capture_id].type == XIANGQI_GENERAL) {
            return line_count(game, p->row, p->col, row, col) == 0;
        }
        return palace_contains(p->side, row, col) && ((adr == 1 && adc == 0) || (adr == 0 && adc == 1));
    case XIANGQI_ADVISOR:
        return palace_contains(p->side, row, col) && adr == 1 && adc == 1;
    case XIANGQI_ELEPHANT:
        if (adr != 2 || adc != 2) {
            return false;
        }
        if (xiangqi_piece_at(game, (p->row + row) / 2, (p->col + col) / 2) != -1) {
            return false;
        }
        return p->side == XIANGQI_RED ? row >= 5 : row <= 4;
    case XIANGQI_HORSE:
        if (!((adr == 1 && adc == 2) || (adr == 2 && adc == 1))) {
            return false;
        }
        if (adr == 1) {
            return xiangqi_piece_at(game, p->row, (p->col + col) / 2) == -1;
        }
        return xiangqi_piece_at(game, (p->row + row) / 2, p->col) == -1;
    case XIANGQI_ROOK:
        return line_count(game, p->row, p->col, row, col) == 0;
    case XIANGQI_CANNON:
        count = line_count(game, p->row, p->col, row, col);
        return capture_id == -1 ? count == 0 : count == 1;
    case XIANGQI_SOLDIER:
        if (!((adr == 1 && adc == 0) || (adr == 0 && adc == 1))) {
            return false;
        }
        if (p->side == XIANGQI_RED) {
            if (dr > 0) {
                return false;
            }
            if (p->row >= 5 && dr == 0) {
                return false;
            }
        } else {
            if (dr < 0) {
                return false;
            }
            if (p->row <= 4 && dr == 0) {
                return false;
            }
        }
        return true;
    }
    return false;
}

static void fake_move(XiangqiGame *game, const XiangqiMove *move) {
    if (move->capture_id != -1) {
        game->pieces[move->capture_id].dead = true;
    }
    game->pieces[move->move_id].row = move->to_row;
    game->pieces[move->move_id].col = move->to_col;
    game->turn = other_side(game->turn);
}

static void unfake_move(XiangqiGame *game, const XiangqiMove *move) {
    game->turn = other_side(game->turn);
    game->pieces[move->move_id].row = move->from_row;
    game->pieces[move->move_id].col = move->from_col;
    if (move->capture_id != -1) {
        game->pieces[move->capture_id].dead = false;
    }
}

bool xiangqi_in_check(const XiangqiGame *game, XiangqiSide side) {
    int general = side == XIANGQI_RED ? 20 : 4;
    int i;

    if (game->pieces[general].dead) {
        return true;
    }
    for (i = 0; i < XIANGQI_PIECES; i++) {
        if (game->pieces[i].dead || game->pieces[i].side == side) {
            continue;
        }
        if (raw_move_legal(game, i, general, game->pieces[general].row, game->pieces[general].col)) {
            return true;
        }
    }
    return false;
}

bool xiangqi_is_legal_move(const XiangqiGame *game, int move_id, int row, int col) {
    XiangqiGame copy;
    XiangqiMove move;
    int capture_id;

    if (move_id < 0 || move_id >= XIANGQI_PIECES || game->pieces[move_id].side != game->turn) {
        return false;
    }
    capture_id = xiangqi_piece_at(game, row, col);
    if (!raw_move_legal(game, move_id, capture_id, row, col)) {
        return false;
    }

    copy = *game;
    move.move_id = move_id;
    move.capture_id = capture_id;
    move.from_row = game->pieces[move_id].row;
    move.from_col = game->pieces[move_id].col;
    move.to_row = row;
    move.to_col = col;
    fake_move(&copy, &move);
    return !xiangqi_in_check(&copy, game->turn);
}

int xiangqi_generate_moves(const XiangqiGame *game, XiangqiSide side, XiangqiMove *moves, int max_moves) {
    XiangqiGame copy;
    int count = 0;
    int saved_turn;
    int id;
    int row;
    int col;

    copy = *game;
    saved_turn = copy.turn;
    copy.turn = side;
    for (id = 0; id < XIANGQI_PIECES; id++) {
        if (copy.pieces[id].dead || copy.pieces[id].side != side) {
            continue;
        }
        for (row = 0; row < XIANGQI_ROWS; row++) {
            for (col = 0; col < XIANGQI_COLS; col++) {
                if (xiangqi_is_legal_move(&copy, id, row, col)) {
                    if (count < max_moves) {
                        moves[count].move_id = id;
                        moves[count].capture_id = xiangqi_piece_at(&copy, row, col);
                        moves[count].from_row = copy.pieces[id].row;
                        moves[count].from_col = copy.pieces[id].col;
                        moves[count].to_row = row;
                        moves[count].to_col = col;
                    }
                    count++;
                }
            }
        }
    }
    copy.turn = saved_turn;
    return count;
}

bool xiangqi_has_legal_move(const XiangqiGame *game, XiangqiSide side) {
    XiangqiMove moves[1];
    return xiangqi_generate_moves(game, side, moves, 1) > 0;
}

bool xiangqi_apply_move(XiangqiGame *game, int move_id, int row, int col, XiangqiMove *out_move) {
    XiangqiMove move;

    if (game->game_over || !xiangqi_is_legal_move(game, move_id, row, col)) {
        return false;
    }
    if (game->history_len >= XIANGQI_MAX_HISTORY) {
        return false;
    }
    move.move_id = move_id;
    move.capture_id = xiangqi_piece_at(game, row, col);
    move.from_row = game->pieces[move_id].row;
    move.from_col = game->pieces[move_id].col;
    move.to_row = row;
    move.to_col = col;

    fake_move(game, &move);
    game->history[game->history_len++] = move;
    if (out_move != NULL) {
        *out_move = move;
    }

    if (game->pieces[4].dead || game->pieces[20].dead || !xiangqi_has_legal_move(game, game->turn)) {
        game->game_over = true;
        game->winner = other_side(game->turn);
    }
    return true;
}

bool xiangqi_undo(XiangqiGame *game) {
    XiangqiMove move;

    if (game->history_len <= 0) {
        return false;
    }
    move = game->history[--game->history_len];
    unfake_move(game, &move);
    game->game_over = false;
    game->selected_id = -1;
    return true;
}

static int position_score(XiangqiPieceType type, int row, int col, XiangqiSide side) {
    int forward = side == XIANGQI_RED ? 9 - row : row;
    int center_bonus = 4 - abs(4 - col);

    switch (type) {
    case XIANGQI_HORSE:
        return center_bonus * 2 + (forward >= 3 && forward <= 6 ? 4 : 0);
    case XIANGQI_ROOK:
        return center_bonus + forward;
    case XIANGQI_CANNON:
        return center_bonus * 2;
    case XIANGQI_SOLDIER:
        return forward >= 5 ? 12 + center_bonus : forward * 2;
    default:
        return 0;
    }
}

static int evaluate(const XiangqiGame *game, XiangqiSide ai_side, XiangqiAiLevel level) {
    int score = 0;
    int i;

    if (game->pieces[4].dead) {
        return ai_side == XIANGQI_RED ? 100000 : -100000;
    }
    if (game->pieces[20].dead) {
        return ai_side == XIANGQI_BLACK ? 100000 : -100000;
    }

    for (i = 0; i < XIANGQI_PIECES; i++) {
        int value;
        if (game->pieces[i].dead) {
            continue;
        }
        value = piece_value[game->pieces[i].type];
        if (level >= XIANGQI_AI_MEDIUM) {
            value += position_score(game->pieces[i].type, game->pieces[i].row, game->pieces[i].col, game->pieces[i].side);
        }
        score += game->pieces[i].side == ai_side ? value : -value;
    }
    return score;
}

static int alphabeta(XiangqiGame *game, int depth, int alpha, int beta, XiangqiSide ai_side, XiangqiAiLevel level) {
    XiangqiMove moves[XIANGQI_MAX_MOVES];
    int count;
    int i;
    bool maxing;
    int best;

    if (depth == 0 || game->game_over) {
        return evaluate(game, ai_side, level);
    }

    count = xiangqi_generate_moves(game, game->turn, moves, XIANGQI_MAX_MOVES);
    if (count == 0) {
        return game->turn == ai_side ? -90000 - depth : 90000 + depth;
    }

    maxing = game->turn == ai_side;
    best = maxing ? INT_MIN : INT_MAX;
    for (i = 0; i < count; i++) {
        int score;
        fake_move(game, &moves[i]);
        score = alphabeta(game, depth - 1, alpha, beta, ai_side, level);
        unfake_move(game, &moves[i]);

        if (maxing) {
            if (score > best) {
                best = score;
            }
            if (score > alpha) {
                alpha = score;
            }
        } else {
            if (score < best) {
                best = score;
            }
            if (score < beta) {
                beta = score;
            }
        }
        if (alpha >= beta) {
            break;
        }
    }
    return best;
}

bool xiangqi_choose_ai_move(XiangqiGame *game, XiangqiAiLevel level, XiangqiMove *best_move) {
    XiangqiMove moves[XIANGQI_MAX_MOVES];
    int depth = level == XIANGQI_AI_EASY ? 1 : (level == XIANGQI_AI_MEDIUM ? 2 : 3);
    int count;
    int best_score = INT_MIN;
    int best_index = -1;
    int i;
    XiangqiSide ai_side = game->turn;

    count = xiangqi_generate_moves(game, ai_side, moves, XIANGQI_MAX_MOVES);
    for (i = 0; i < count; i++) {
        int score;
        fake_move(game, &moves[i]);
        score = alphabeta(game, depth - 1, INT_MIN + 1, INT_MAX - 1, ai_side, level);
        unfake_move(game, &moves[i]);
        if (moves[i].capture_id != -1) {
            score += piece_value[game->pieces[moves[i].capture_id].type] / 4;
        }
        if (score > best_score) {
            best_score = score;
            best_index = i;
        }
    }
    if (best_index < 0) {
        return false;
    }
    *best_move = moves[best_index];
    return true;
}

const char *xiangqi_piece_text(const XiangqiPiece *piece) {
    static const char *red[] = { "帥", "仕", "相", "馬", "車", "炮", "兵" };
    static const char *black[] = { "將", "士", "象", "馬", "車", "砲", "卒" };
    return piece->side == XIANGQI_RED ? red[piece->type] : black[piece->type];
}

const char *xiangqi_side_name(XiangqiSide side) {
    return side == XIANGQI_RED ? "Red" : "Black";
}

void xiangqi_move_label(const XiangqiGame *game, const XiangqiMove *move, char *buf, int len) {
    const XiangqiPiece *piece = &game->pieces[move->move_id];
    (void)game;
    snprintf(buf, len, "%s %c%d-%c%d",
             xiangqi_piece_text(piece),
             "abcdefghi"[move->from_col], 10 - move->from_row,
             "abcdefghi"[move->to_col], 10 - move->to_row);
}
