/*
 * Exact Chinese Chess PortMaster game/application state.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef EXACT_CHINESE_CHESS_PORTMASTER_GAME_H
#define EXACT_CHINESE_CHESS_PORTMASTER_GAME_H

#include "../../xiangqi_engine.h"
#include "engine.h"

#include <SDL.h>
#include <stdbool.h>
#include <stddef.h>

#define PIECE_ASSET_SET_COUNT 12

typedef enum {
    MODE_PLAY_RED = 0,
    MODE_PLAY_BLACK,
    MODE_TWO_PLAYER,
    MODE_AI_DEMO
} PlayMode;

typedef enum {
    DROPDOWN_NONE = 0,
    DROPDOWN_MODE,
    DROPDOWN_DIFFICULTY
} OpenDropdown;

typedef enum {
    PAGE_MAIN = 0,
    PAGE_MOVES,
    PAGE_HELP
} SidePage;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *board_texture;
    SDL_Texture *piece_textures[2][XIANGQI_SOLDIER + 1][PIECE_ASSET_SET_COUNT];
    bool use_art;
    int width;
    int height;
    XiangqiGame game;
    int cursor_row;
    int cursor_col;
    int selected_id;
    bool running;
    bool ai_enabled;
    bool ai_pending;
    XiangqiAiLevel ai_level;
    PlayMode mode;
    int review_ply;
    bool back_down;
    bool start_down;
    int pointer_x;
    int pointer_y;
    int pointer_vx;
    int pointer_vy;
    bool pointer_visible;
    bool show_moves;
    int move_scroll;
    OpenDropdown open_dropdown;
    SidePage side_page;
    SDL_GameController *controllers[4];
    int controller_count;
    UciEngine engine;
    char status[128];
} App;

const char *game_piece_code(const XiangqiPiece *piece);
const char *mode_name(PlayMode mode);
const char *difficulty_name(XiangqiAiLevel level);
bool is_latest_view(const App *app);
bool is_human_turn(const App *app);
void set_status(App *app, const char *status);
void game_move_to_uci(const XiangqiMove *move, char *buf, size_t len);
void move_ascii_label(const XiangqiGame *game, const XiangqiMove *move, char *buf, size_t len);
void build_review_game(const App *app, XiangqiGame *view);
void review_to_latest(App *app);
void review_step(App *app, int delta);
bool save_game(App *app);
bool load_game(App *app);
void set_mode(App *app, PlayMode mode);
void set_difficulty(App *app, XiangqiAiLevel level);
void reset_game(App *app);
void apply_ai_if_needed(App *app);
void activate_cell(App *app);
void move_cursor(App *app, int dc, int dr);
void undo_move(App *app);

#endif
