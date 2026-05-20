/*
 * Exact Chinese Chess PortMaster game/application state.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *game_piece_code(const XiangqiPiece *piece) {
    static const char *red[] = { "RG", "RA", "RE", "RH", "RR", "RC", "RS" };
    static const char *black[] = { "BG", "BA", "BE", "BH", "BR", "BC", "BS" };
    return piece->side == XIANGQI_RED ? red[piece->type] : black[piece->type];
}

const char *mode_name(PlayMode mode) {
    switch (mode) {
    case MODE_PLAY_RED: return "PLAY RED";
    case MODE_PLAY_BLACK: return "PLAY BLACK";
    case MODE_TWO_PLAYER: return "2 PLAYER";
    case MODE_AI_DEMO: return "AI DEMO";
    }
    return "PLAY RED";
}

const char *difficulty_name(XiangqiAiLevel level) {
    switch (level) {
    case XIANGQI_AI_EASY: return "EASY";
    case XIANGQI_AI_MEDIUM: return "MED";
    case XIANGQI_AI_HARD: return "HARD";
    }
    return "EASY";
}

bool is_latest_view(const App *app) {
    return app->review_ply < 0 || app->review_ply >= app->game.history_len;
}

bool is_human_turn(const App *app) {
    if (app->game.game_over) {
        return true;
    }
    switch (app->mode) {
    case MODE_TWO_PLAYER:
        return true;
    case MODE_PLAY_RED:
        return app->game.turn == XIANGQI_RED;
    case MODE_PLAY_BLACK:
        return app->game.turn == XIANGQI_BLACK;
    case MODE_AI_DEMO:
        return false;
    }
    return true;
}

void set_status(App *app, const char *status) {
    snprintf(app->status, sizeof(app->status), "%s", status);
}

void game_move_to_uci(const XiangqiMove *move, char *buf, size_t len) {
    if (len < 5) {
        return;
    }
    buf[0] = (char)('a' + move->from_col);
    buf[1] = (char)('0' + (9 - move->from_row));
    buf[2] = (char)('a' + move->to_col);
    buf[3] = (char)('0' + (9 - move->to_row));
    buf[4] = '\0';
}

static bool uci_to_move(const XiangqiGame *game, const char *uci, int *move_id, int *row, int *col) {
    int from_col;
    int from_row;
    int to_col;
    int to_row;
    if (!uci || strlen(uci) < 4) {
        return false;
    }
    from_col = uci[0] - 'a';
    from_row = 9 - (uci[1] - '0');
    to_col = uci[2] - 'a';
    to_row = 9 - (uci[3] - '0');
    if (from_col < 0 || from_col >= XIANGQI_COLS ||
        to_col < 0 || to_col >= XIANGQI_COLS ||
        from_row < 0 || from_row >= XIANGQI_ROWS ||
        to_row < 0 || to_row >= XIANGQI_ROWS) {
        return false;
    }
    *move_id = xiangqi_piece_at(game, from_row, from_col);
    *row = to_row;
    *col = to_col;
    return *move_id >= 0;
}

void move_ascii_label(const XiangqiGame *game, const XiangqiMove *move, char *buf, size_t len) {
    const XiangqiPiece *piece = &game->pieces[move->move_id];
    snprintf(buf, len, "%s %c%d-%c%d",
             game_piece_code(piece),
             (char)('A' + move->from_col), 9 - move->from_row,
             (char)('A' + move->to_col), 9 - move->to_row);
}

void build_review_game(const App *app, XiangqiGame *view) {
    int i;
    int ply = is_latest_view(app) ? app->game.history_len : app->review_ply;
    xiangqi_init(view);
    if (ply < 0) {
        ply = 0;
    }
    if (ply > app->game.history_len) {
        ply = app->game.history_len;
    }
    for (i = 0; i < ply; i++) {
        XiangqiMove ignored;
        xiangqi_apply_move(view,
                           app->game.history[i].move_id,
                           app->game.history[i].to_row,
                           app->game.history[i].to_col,
                           &ignored);
    }
}

void review_to_latest(App *app) {
    app->review_ply = app->game.history_len;
}

void review_step(App *app, int delta) {
    int ply = is_latest_view(app) ? app->game.history_len : app->review_ply;
    ply += delta;
    if (ply < 0) {
        ply = 0;
    }
    if (ply > app->game.history_len) {
        ply = app->game.history_len;
    }
    app->review_ply = ply;
    snprintf(app->status, sizeof(app->status), "REVIEW %d/%d", app->review_ply, app->game.history_len);
}

bool save_game(App *app) {
    const char *base = getenv("XDG_DATA_HOME");
    char path[256];
    FILE *file;
    int i;
    if (!base) {
        base = ".";
    }
    snprintf(path, sizeof(path), "%s/save.txt", base);
    file = fopen(path, "w");
    if (!file) {
        set_status(app, "SAVE FAILED");
        return false;
    }
    fprintf(file, "mode %d\nlevel %d\n", (int)app->mode, (int)app->ai_level);
    for (i = 0; i < app->game.history_len; i++) {
        char uci[8];
        game_move_to_uci(&app->game.history[i], uci, sizeof(uci));
        fprintf(file, "%s\n", uci);
    }
    fclose(file);
    set_status(app, "SAVED");
    return true;
}

bool load_game(App *app) {
    const char *base = getenv("XDG_DATA_HOME");
    char path[256];
    char line[64];
    FILE *file;
    XiangqiGame loaded;
    if (!base) {
        base = ".";
    }
    snprintf(path, sizeof(path), "%s/save.txt", base);
    file = fopen(path, "r");
    if (!file) {
        set_status(app, "NO SAVE");
        return false;
    }
    xiangqi_init(&loaded);
    while (fgets(line, sizeof(line), file)) {
        int value;
        int move_id;
        int row;
        int col;
        XiangqiMove applied;
        if (sscanf(line, "mode %d", &value) == 1) {
            if (value >= MODE_PLAY_RED && value <= MODE_AI_DEMO) {
                app->mode = (PlayMode)value;
            }
            continue;
        }
        if (sscanf(line, "level %d", &value) == 1) {
            if (value >= XIANGQI_AI_EASY && value <= XIANGQI_AI_HARD) {
                app->ai_level = (XiangqiAiLevel)value;
                engine_set_level(&app->engine, app->ai_level);
            }
            continue;
        }
        if (uci_to_move(&loaded, line, &move_id, &row, &col)) {
            if (!xiangqi_apply_move(&loaded, move_id, row, col, &applied)) {
                fclose(file);
                set_status(app, "LOAD BAD MOVE");
                return false;
            }
        }
    }
    fclose(file);
    app->game = loaded;
    app->selected_id = -1;
    app->move_scroll = 0;
    review_to_latest(app);
    app->ai_pending = !is_human_turn(app);
    set_status(app, "LOADED");
    return true;
}
void set_mode(App *app, PlayMode mode) {
    app->mode = mode;
    app->ai_enabled = app->mode != MODE_TWO_PLAYER;
    app->selected_id = -1;
    app->ai_pending = !is_human_turn(app);
    set_status(app, mode_name(app->mode));
}

void set_difficulty(App *app, XiangqiAiLevel level) {
    app->ai_level = level;
    engine_set_level(&app->engine, app->ai_level);
    set_status(app, difficulty_name(app->ai_level));
}

void reset_game(App *app) {
    xiangqi_init(&app->game);
    app->cursor_row = 9;
    app->cursor_col = 4;
    app->selected_id = -1;
    app->move_scroll = 0;
    review_to_latest(app);
    app->ai_enabled = app->mode != MODE_TWO_PLAYER;
    app->ai_pending = !is_human_turn(app);
    if (app->engine.available) {
        engine_write(&app->engine, "ucinewgame");
    }
    set_status(app, "NEW GAME");
}

void apply_ai_if_needed(App *app) {
    XiangqiMove ai_move;
    char bestmove[8];
    int move_id;
    int row;
    int col;
    if (app->game.game_over || is_human_turn(app) || !is_latest_view(app)) {
        app->ai_pending = false;
        return;
    }
    if (engine_request_move(&app->engine, &app->game, app->ai_level, bestmove, sizeof(bestmove)) &&
        uci_to_move(&app->game, bestmove, &move_id, &row, &col) &&
        xiangqi_apply_move(&app->game, move_id, row, col, NULL)) {
        snprintf(app->status, sizeof(app->status), "PIKAFISH %s", bestmove);
        app->move_scroll = 0;
    } else if (xiangqi_choose_ai_move(&app->game, app->ai_level, &ai_move) &&
        xiangqi_apply_move(&app->game, ai_move.move_id, ai_move.to_row, ai_move.to_col, NULL)) {
        set_status(app, "AI MOVED");
        app->move_scroll = 0;
    }
    review_to_latest(app);
    app->ai_pending = !is_human_turn(app);
}

void activate_cell(App *app) {
    int id;
    if (!is_latest_view(app)) {
        review_to_latest(app);
        set_status(app, "LIVE BOARD");
        return;
    }
    if (app->game.game_over) {
        reset_game(app);
        return;
    }
    if (!is_human_turn(app)) {
        return;
    }
    id = xiangqi_piece_at(&app->game, app->cursor_row, app->cursor_col);
    if (app->selected_id < 0) {
        if (id >= 0 && app->game.pieces[id].side == app->game.turn) {
            app->selected_id = id;
            set_status(app, "PIECE SELECTED");
        } else {
            set_status(app, "SELECT OWN PIECE");
        }
        return;
    }
    if (id == app->selected_id) {
        app->selected_id = -1;
        set_status(app, "CANCELLED");
        return;
    }
    if (xiangqi_apply_move(&app->game, app->selected_id, app->cursor_row, app->cursor_col, NULL)) {
        app->selected_id = -1;
        if (app->game.game_over) {
            set_status(app, app->game.winner == XIANGQI_RED ? "RED WINS" : "BLACK WINS");
        } else {
            set_status(app, "MOVED");
            app->move_scroll = 0;
            review_to_latest(app);
            app->ai_pending = !is_human_turn(app);
        }
    } else if (id >= 0 && app->game.pieces[id].side == app->game.turn) {
        app->selected_id = id;
        set_status(app, "PIECE SELECTED");
    } else {
        set_status(app, "ILLEGAL MOVE");
    }
}

void move_cursor(App *app, int dc, int dr) {
    app->cursor_col += dc;
    app->cursor_row += dr;
    if (app->cursor_col < 0) app->cursor_col = 0;
    if (app->cursor_col >= XIANGQI_COLS) app->cursor_col = XIANGQI_COLS - 1;
    if (app->cursor_row < 0) app->cursor_row = 0;
    if (app->cursor_row >= XIANGQI_ROWS) app->cursor_row = XIANGQI_ROWS - 1;
}

void undo_move(App *app) {
    if (!is_latest_view(app)) {
        review_to_latest(app);
        set_status(app, "LIVE BOARD");
        return;
    }
    if (app->mode != MODE_TWO_PLAYER && app->mode != MODE_AI_DEMO && app->game.history_len >= 2) {
        xiangqi_undo(&app->game);
        xiangqi_undo(&app->game);
    } else {
        xiangqi_undo(&app->game);
    }
    app->selected_id = -1;
    app->move_scroll = 0;
    review_to_latest(app);
    app->ai_pending = false;
    set_status(app, "UNDONE");
}
