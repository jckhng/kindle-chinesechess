/*
 * Exact Chinese Chess PortMaster SDL2 frontend.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "../../xiangqi_engine.h"

#include <SDL.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PIECE_ASSET_SET_COUNT 12

static const int piece_asset_sizes[PIECE_ASSET_SET_COUNT] = { 24, 28, 34, 40, 42, 46, 52, 60, 72, 84, 96, 128 };

typedef struct {
    bool available;
    bool ready;
    pid_t pid;
    int in_fd;
    int out_fd;
    int movetime_ms;
    int search_depth;
    int multipv;
    char path[256];
    char dir[256];
    char line[512];
    size_t line_len;
} UciEngine;

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

typedef struct {
    char ch;
    unsigned char rows[7];
} Glyph;

static const Glyph font[] = {
    {' ', {0,0,0,0,0,0,0}},
    {'!', {4,4,4,4,4,0,4}},
    {'-', {0,0,0,14,0,0,0}},
    {'.', {0,0,0,0,0,0,4}},
    {':', {0,4,0,0,0,4,0}},
    {'/', {1,1,2,4,8,8,16}},
    {'0', {14,17,19,21,25,17,14}},
    {'1', {4,12,4,4,4,4,14}},
    {'2', {14,17,1,2,4,8,31}},
    {'3', {30,1,1,14,1,1,30}},
    {'4', {2,6,10,18,31,2,2}},
    {'5', {31,16,30,1,1,17,14}},
    {'6', {6,8,16,30,17,17,14}},
    {'7', {31,1,2,4,8,8,8}},
    {'8', {14,17,17,14,17,17,14}},
    {'9', {14,17,17,15,1,2,12}},
    {'<', {2,4,8,16,8,4,2}},
    {'>', {8,4,2,1,2,4,8}},
    {'A', {14,17,17,31,17,17,17}},
    {'B', {30,17,17,30,17,17,30}},
    {'C', {14,17,16,16,16,17,14}},
    {'D', {30,17,17,17,17,17,30}},
    {'E', {31,16,16,30,16,16,31}},
    {'F', {31,16,16,30,16,16,16}},
    {'G', {14,17,16,23,17,17,14}},
    {'H', {17,17,17,31,17,17,17}},
    {'I', {14,4,4,4,4,4,14}},
    {'J', {7,2,2,2,18,18,12}},
    {'K', {17,18,20,24,20,18,17}},
    {'L', {16,16,16,16,16,16,31}},
    {'M', {17,27,21,21,17,17,17}},
    {'N', {17,25,21,19,17,17,17}},
    {'O', {14,17,17,17,17,17,14}},
    {'P', {30,17,17,30,16,16,16}},
    {'Q', {14,17,17,17,21,18,13}},
    {'R', {30,17,17,30,20,18,17}},
    {'S', {15,16,16,14,1,1,30}},
    {'T', {31,4,4,4,4,4,4}},
    {'U', {17,17,17,17,17,17,14}},
    {'V', {17,17,17,17,17,10,4}},
    {'W', {17,17,17,21,21,21,10}},
    {'X', {17,17,10,4,10,17,17}},
    {'Y', {17,17,10,4,4,4,4}},
    {'Z', {31,1,2,4,8,16,31}},
};

static const unsigned char *glyph_rows(char ch) {
    size_t i;
    if (ch >= 'a' && ch <= 'z') {
        ch = (char)(ch - 'a' + 'A');
    }
    for (i = 0; i < sizeof(font) / sizeof(font[0]); i++) {
        if (font[i].ch == ch) {
            return font[i].rows;
        }
    }
    return font[0].rows;
}

static void set_color(SDL_Renderer *r, int red, int green, int blue, int alpha) {
    SDL_SetRenderDrawColor(r, (Uint8)red, (Uint8)green, (Uint8)blue, (Uint8)alpha);
}

static void log_sdl_drivers(void) {
    int i;
    SDL_version compiled;
    SDL_version linked;
    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);
    fprintf(stderr, "SDL compiled version: %d.%d.%d\n", compiled.major, compiled.minor, compiled.patch);
    fprintf(stderr, "SDL linked version: %d.%d.%d\n", linked.major, linked.minor, linked.patch);
    fprintf(stderr, "SDL video driver env: %s\n", SDL_getenv("SDL_VIDEODRIVER") ? SDL_getenv("SDL_VIDEODRIVER") : "(unset)");
    fprintf(stderr, "SDL render driver env: %s\n", SDL_getenv("SDL_RENDER_DRIVER") ? SDL_getenv("SDL_RENDER_DRIVER") : "(unset)");
    fprintf(stderr, "Available video drivers:");
    for (i = 0; i < SDL_GetNumVideoDrivers(); i++) {
        fprintf(stderr, " %s", SDL_GetVideoDriver(i));
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "Available render drivers:");
    for (i = 0; i < SDL_GetNumRenderDrivers(); i++) {
        SDL_RendererInfo info;
        if (SDL_GetRenderDriverInfo(i, &info) == 0) {
            fprintf(stderr, " %s", info.name);
        }
    }
    fprintf(stderr, "\n");
}

static void fill_circle(SDL_Renderer *r, int cx, int cy, int radius) {
    int y;
    for (y = -radius; y <= radius; y++) {
        int x;
        for (x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                SDL_RenderDrawPoint(r, cx + x, cy + y);
            }
        }
    }
}

static void draw_text(SDL_Renderer *r, const char *text, int x, int y, int scale) {
    int pen = x;
    size_t i;
    for (i = 0; text[i] != '\0'; i++) {
        const unsigned char *rows = glyph_rows(text[i]);
        int row;
        for (row = 0; row < 7; row++) {
            int col;
            for (col = 0; col < 5; col++) {
                if (rows[row] & (1 << (4 - col))) {
                    SDL_Rect px = { pen + col * scale, y + row * scale, scale, scale };
                    SDL_RenderFillRect(r, &px);
                }
            }
        }
        pen += 6 * scale;
    }
}

static void draw_text_xy_tracked(SDL_Renderer *r, const char *text, int x, int y, int sx, int sy, int tracking) {
    int pen = x;
    size_t i;
    for (i = 0; text[i] != '\0'; i++) {
        const unsigned char *rows = glyph_rows(text[i]);
        int row;
        for (row = 0; row < 7; row++) {
            int col;
            for (col = 0; col < 5; col++) {
                if (rows[row] & (1 << (4 - col))) {
                    SDL_Rect px = { pen + col * sx, y + row * sy, sx, sy };
                    SDL_RenderFillRect(r, &px);
                }
            }
        }
        pen += 6 * sx + tracking;
    }
}

static void draw_text_mid_bold(SDL_Renderer *r, const char *text, int x, int y) {
    draw_text_xy_tracked(r, text, x, y, 1, 2, 2);
    draw_text_xy_tracked(r, text, x + 1, y, 1, 2, 2);
}

static void draw_text_title(SDL_Renderer *r, const char *text, int x, int y) {
    draw_text_xy_tracked(r, text, x, y, 1, 3, 1);
    draw_text_xy_tracked(r, text, x + 1, y, 1, 3, 1);
}

static const char *piece_code(const XiangqiPiece *piece) {
    static const char *red[] = { "RG", "RA", "RE", "RH", "RR", "RC", "RS" };
    static const char *black[] = { "BG", "BA", "BE", "BH", "BR", "BC", "BS" };
    return piece->side == XIANGQI_RED ? red[piece->type] : black[piece->type];
}

static const char *mode_name(PlayMode mode) {
    switch (mode) {
    case MODE_PLAY_RED: return "PLAY RED";
    case MODE_PLAY_BLACK: return "PLAY BLACK";
    case MODE_TWO_PLAYER: return "2 PLAYER";
    case MODE_AI_DEMO: return "AI DEMO";
    }
    return "PLAY RED";
}

static const char *difficulty_name(XiangqiAiLevel level) {
    switch (level) {
    case XIANGQI_AI_EASY: return "EASY";
    case XIANGQI_AI_MEDIUM: return "MED";
    case XIANGQI_AI_HARD: return "HARD";
    }
    return "EASY";
}

static bool is_latest_view(const App *app) {
    return app->review_ply < 0 || app->review_ply >= app->game.history_len;
}

static bool is_human_turn(const App *app) {
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

static void set_status(App *app, const char *status) {
    snprintf(app->status, sizeof(app->status), "%s", status);
}

static void move_to_uci(const XiangqiMove *move, char *buf, size_t len) {
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

static void move_ascii_label(const XiangqiGame *game, const XiangqiMove *move, char *buf, size_t len) {
    const XiangqiPiece *piece = &game->pieces[move->move_id];
    snprintf(buf, len, "%s %c%d-%c%d",
             piece_code(piece),
             (char)('A' + move->from_col), 9 - move->from_row,
             (char)('A' + move->to_col), 9 - move->to_row);
}

static void engine_init(UciEngine *engine) {
    memset(engine, 0, sizeof(*engine));
    engine->pid = -1;
    engine->in_fd = -1;
    engine->out_fd = -1;
    engine->movetime_ms = 450;
    engine->search_depth = 0;
    engine->multipv = 1;
}

static void engine_stop(UciEngine *engine) {
    if (engine->in_fd >= 0) {
        dprintf(engine->in_fd, "quit\n");
        close(engine->in_fd);
        engine->in_fd = -1;
    }
    if (engine->out_fd >= 0) {
        close(engine->out_fd);
        engine->out_fd = -1;
    }
    if (engine->pid > 0) {
        int status;
        if (waitpid(engine->pid, &status, WNOHANG) == 0) {
            kill(engine->pid, SIGTERM);
            SDL_Delay(50);
            if (waitpid(engine->pid, &status, WNOHANG) == 0) {
                kill(engine->pid, SIGKILL);
                waitpid(engine->pid, &status, 0);
            }
        }
        engine->pid = -1;
    }
    engine->available = false;
    engine->ready = false;
}

static bool engine_write(UciEngine *engine, const char *line) {
    size_t len;
    ssize_t written;
    if (!engine->available || engine->in_fd < 0) {
        return false;
    }
    len = strlen(line);
    written = write(engine->in_fd, line, len);
    if (written != (ssize_t)len || write(engine->in_fd, "\n", 1) != 1) {
        engine_stop(engine);
        return false;
    }
    return true;
}

static bool engine_read_line(UciEngine *engine, char *out, size_t out_len, int timeout_ms) {
    Uint32 deadline = SDL_GetTicks() + (Uint32)timeout_ms;
    while (SDL_GetTicks() <= deadline) {
        fd_set readfds;
        struct timeval tv;
        int rc;
        char ch;
        FD_ZERO(&readfds);
        FD_SET(engine->out_fd, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 50000;
        rc = select(engine->out_fd + 1, &readfds, NULL, NULL, &tv);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (rc == 0) {
            continue;
        }
        while (read(engine->out_fd, &ch, 1) == 1) {
            if (ch == '\r') {
                continue;
            }
            if (ch == '\n') {
                engine->line[engine->line_len] = '\0';
                snprintf(out, out_len, "%s", engine->line);
                engine->line_len = 0;
                return true;
            }
            if (engine->line_len + 1 < sizeof(engine->line)) {
                engine->line[engine->line_len++] = ch;
            }
        }
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            return false;
        }
    }
    return false;
}

static bool engine_wait_for(UciEngine *engine, const char *prefix, char *line, size_t line_len, int timeout_ms) {
    Uint32 deadline = SDL_GetTicks() + (Uint32)timeout_ms;
    while (SDL_GetTicks() <= deadline) {
        int remaining = (int)(deadline - SDL_GetTicks());
        if (remaining < 1) {
            remaining = 1;
        }
        if (!engine_read_line(engine, line, line_len, remaining)) {
            continue;
        }
        fprintf(stderr, "[pikafish<-] %s\n", line);
        if (strncmp(line, prefix, strlen(prefix)) == 0) {
            return true;
        }
    }
    return false;
}

static bool engine_find_binary(UciEngine *engine) {
    static const char *candidates[] = {
        "bin/aarch64/pikafish",
        "bin/armhf/pikafish",
        "pikafish"
    };
    size_t i;
    for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (access(candidates[i], X_OK) == 0) {
            char *slash;
            snprintf(engine->path, sizeof(engine->path), "%s", candidates[i]);
            snprintf(engine->dir, sizeof(engine->dir), "%s", candidates[i]);
            slash = strrchr(engine->dir, '/');
            if (slash) {
                *slash = '\0';
            } else {
                snprintf(engine->dir, sizeof(engine->dir), ".");
            }
            return true;
        }
    }
    return false;
}

static bool engine_start(UciEngine *engine) {
    int to_child[2];
    int from_child[2];
    char line[512];
    if (!engine_find_binary(engine)) {
        fprintf(stderr, "Pikafish: not packaged; using built-in AI.\n");
        return false;
    }
    if (pipe(to_child) != 0 || pipe(from_child) != 0) {
        return false;
    }
    engine->pid = fork();
    if (engine->pid == 0) {
        dup2(to_child[0], STDIN_FILENO);
        dup2(from_child[1], STDOUT_FILENO);
        dup2(from_child[1], STDERR_FILENO);
        close(to_child[0]);
        close(to_child[1]);
        close(from_child[0]);
        close(from_child[1]);
        if (chdir(engine->dir) != 0) {
            _exit(126);
        }
        unsetenv("LD_LIBRARY_PATH");
        execl(engine->path[0] == '/' ? engine->path : strrchr(engine->path, '/') ? strrchr(engine->path, '/') + 1 : engine->path,
              engine->path, (char *)NULL);
        _exit(127);
    }
    close(to_child[0]);
    close(from_child[1]);
    engine->in_fd = to_child[1];
    engine->out_fd = from_child[0];
    fcntl(engine->out_fd, F_SETFL, fcntl(engine->out_fd, F_GETFL, 0) | O_NONBLOCK);
    engine->available = engine->pid > 0;
    if (!engine->available) {
        close(engine->in_fd);
        close(engine->out_fd);
        return false;
    }
    fprintf(stderr, "Pikafish: starting %s in %s\n", engine->path, engine->dir);
    engine_write(engine, "uci");
    if (!engine_wait_for(engine, "uciok", line, sizeof(line), 10000)) {
        fprintf(stderr, "Pikafish: no uciok; using built-in AI.\n");
        engine_stop(engine);
        return false;
    }
    engine_write(engine, "setoption name Threads value 1");
    engine_write(engine, "setoption name Hash value 16");
    engine_write(engine, "setoption name Ponder value false");
    engine_write(engine, "isready");
    if (!engine_wait_for(engine, "readyok", line, sizeof(line), 10000)) {
        fprintf(stderr, "Pikafish: no readyok; using built-in AI.\n");
        engine_stop(engine);
        return false;
    }
    engine->ready = true;
    engine_write(engine, "ucinewgame");
    fprintf(stderr, "Pikafish: ready.\n");
    return true;
}

static void engine_set_level(UciEngine *engine, XiangqiAiLevel level) {
    if (level == XIANGQI_AI_HARD) {
        engine->movetime_ms = 1200;
        engine->search_depth = 0;
        engine->multipv = 1;
    } else if (level == XIANGQI_AI_MEDIUM) {
        engine->movetime_ms = 0;
        engine->search_depth = 1;
        engine->multipv = 4;
    } else {
        engine->movetime_ms = 0;
        engine->search_depth = 1;
        engine->multipv = 4;
    }
}

static int engine_weak_frequency(XiangqiAiLevel level) {
    if (level == XIANGQI_AI_EASY) {
        return 2;
    }
    if (level == XIANGQI_AI_MEDIUM) {
        return 5;
    }
    return 0;
}

static void copy_uci_move(char *dst, size_t dst_len, const char *src) {
    size_t i;
    if (dst_len == 0) {
        return;
    }
    for (i = 0; i + 1 < dst_len && src[i] != '\0' && i < 7; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static void engine_parse_pv_move(const char *line, char candidates[][8], int max_candidates) {
    const char *multipv_text;
    const char *pv_text;
    int multipv = 1;
    char move[8];
    multipv_text = strstr(line, " multipv ");
    if (multipv_text) {
        multipv = atoi(multipv_text + 8);
    }
    if (multipv < 1 || multipv > max_candidates) {
        return;
    }
    pv_text = strstr(line, " pv ");
    if (!pv_text) {
        return;
    }
    if (sscanf(pv_text + 4, "%7s", move) == 1 && strlen(move) >= 4) {
        snprintf(candidates[multipv - 1], 8, "%s", move);
    }
}

static void engine_choose_candidate(XiangqiAiLevel level, char candidates[][8], int max_candidates, const char *engine_best, char *bestmove, size_t bestmove_len) {
    int frequency = engine_weak_frequency(level);
    int usable = 0;
    int weak_count;
    int weak_index;
    int i;
    for (i = 0; i < max_candidates; i++) {
        if (candidates[i][0] != '\0') {
            usable = i + 1;
        }
    }
    if (frequency > 0 && usable >= 2 && (rand() % frequency) == 0) {
        weak_count = usable - 1;
        if (weak_count > 3) {
            weak_count = 3;
        }
        weak_index = 1 + (rand() % weak_count);
        copy_uci_move(bestmove, bestmove_len, candidates[weak_index]);
        return;
    }
    if (candidates[0][0] != '\0') {
        copy_uci_move(bestmove, bestmove_len, candidates[0]);
        return;
    }
    copy_uci_move(bestmove, bestmove_len, engine_best);
}

static bool engine_request_move(UciEngine *engine, const XiangqiGame *game, XiangqiAiLevel level, char *bestmove, size_t bestmove_len) {
    char position[4096];
    char line[512];
    char option[64];
    char candidates[4][8];
    char engine_best[8];
    int i;
    if (!engine->available || !engine->ready) {
        return false;
    }
    memset(candidates, 0, sizeof(candidates));
    memset(engine_best, 0, sizeof(engine_best));
    snprintf(position, sizeof(position), "position startpos");
    if (game->history_len > 0) {
        size_t used = strlen(position);
        used += snprintf(position + used, sizeof(position) - used, " moves");
        for (i = 0; i < game->history_len && used + 8 < sizeof(position); i++) {
            char uci[8];
            move_to_uci(&game->history[i], uci, sizeof(uci));
            used += snprintf(position + used, sizeof(position) - used, " %s", uci);
        }
    }
    if (!engine_write(engine, position)) {
        return false;
    }
    snprintf(option, sizeof(option), "setoption name MultiPV value %d", engine->multipv);
    if (!engine_write(engine, option)) {
        return false;
    }
    if (engine->search_depth > 0) {
        snprintf(line, sizeof(line), "go depth %d", engine->search_depth);
    } else {
        snprintf(line, sizeof(line), "go movetime %d", engine->movetime_ms);
    }
    if (!engine_write(engine, line)) {
        return false;
    }
    for (;;) {
        int timeout_ms = engine->search_depth > 0 ? 4000 : engine->movetime_ms + 4000;
        if (!engine_read_line(engine, line, sizeof(line), timeout_ms)) {
            fprintf(stderr, "Pikafish: no bestmove; disabling engine.\n");
            engine_stop(engine);
            return false;
        }
        fprintf(stderr, "[pikafish<-] %s\n", line);
        if (strncmp(line, "info ", 5) == 0) {
            engine_parse_pv_move(line, candidates, engine->multipv);
        } else if (sscanf(line, "bestmove %7s", engine_best) == 1 && strlen(engine_best) >= 4) {
            engine_choose_candidate(level, candidates, engine->multipv, engine_best, bestmove, bestmove_len);
            bestmove[bestmove_len - 1] = '\0';
            return true;
        }
    }
}

static void build_review_game(const App *app, XiangqiGame *view) {
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

static void review_to_latest(App *app) {
    app->review_ply = app->game.history_len;
}

static void review_step(App *app, int delta) {
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

static bool save_game(App *app) {
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
        move_to_uci(&app->game.history[i], uci, sizeof(uci));
        fprintf(file, "%s\n", uci);
    }
    fclose(file);
    set_status(app, "SAVED");
    return true;
}

static bool load_game(App *app) {
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

static const char *piece_asset_name(XiangqiSide side, XiangqiPieceType type) {
    static const char *red[] = { "r_k.bmp", "r_a.bmp", "r_b.bmp", "r_n.bmp", "r_r.bmp", "r_c.bmp", "r_p.bmp" };
    static const char *black[] = { "b_k.bmp", "b_a.bmp", "b_b.bmp", "b_n.bmp", "b_r.bmp", "b_c.bmp", "b_p.bmp" };
    if (side == XIANGQI_RED) {
        return red[type];
    }
    return black[type];
}

static SDL_Texture *load_bmp_texture(SDL_Renderer *renderer, const char *path, bool color_key) {
    SDL_Surface *surface = SDL_LoadBMP(path);
    SDL_Texture *texture;
    if (!surface) {
        fprintf(stderr, "SDL_LoadBMP failed for %s: %s\n", path, SDL_GetError());
        return NULL;
    }
    if (color_key) {
        SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format, 255, 0, 255));
    }
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTextureFromSurface failed for %s: %s\n", path, SDL_GetError());
    }
    return texture;
}

static int nearest_piece_asset_index(int piece_size) {
    int best = 0;
    int best_delta = abs(piece_asset_sizes[0] - piece_size);
    int i;
    for (i = 1; i < PIECE_ASSET_SET_COUNT; i++) {
        int delta = abs(piece_asset_sizes[i] - piece_size);
        if (delta < best_delta) {
            best = i;
            best_delta = delta;
        }
    }
    return best;
}

static void load_assets(App *app) {
    char path[256];
    int side;
    int type;
    int set;
    app->use_art = false;
    app->board_texture = load_bmp_texture(app->renderer, "assets/xiangqi/board.bmp", false);
    for (side = XIANGQI_BLACK; side <= XIANGQI_RED; side++) {
        for (type = XIANGQI_GENERAL; type <= XIANGQI_SOLDIER; type++) {
            for (set = 0; set < PIECE_ASSET_SET_COUNT; set++) {
                snprintf(path, sizeof(path), "assets/xiangqi/pieces_%d/%s",
                         piece_asset_sizes[set],
                         piece_asset_name((XiangqiSide)side, (XiangqiPieceType)type));
                app->piece_textures[side][type][set] = load_bmp_texture(app->renderer, path, true);
            }
        }
    }
    app->use_art = app->board_texture != NULL;
    fprintf(stderr, "Artwork: %s\n", app->use_art ? "enabled" : "fallback");
}

static void destroy_assets(App *app) {
    int side;
    int type;
    int set;
    if (app->board_texture) {
        SDL_DestroyTexture(app->board_texture);
    }
    for (side = XIANGQI_BLACK; side <= XIANGQI_RED; side++) {
        for (type = XIANGQI_GENERAL; type <= XIANGQI_SOLDIER; type++) {
            for (set = 0; set < PIECE_ASSET_SET_COUNT; set++) {
                if (app->piece_textures[side][type][set]) {
                    SDL_DestroyTexture(app->piece_textures[side][type][set]);
                }
            }
        }
    }
}

static int board_size(const App *app) {
    int usable_h = app->height - 24;
    int usable_w = app->width < 560 ? app->width - 24 : app->width - 190;
    int cell_from_w = usable_w / XIANGQI_COLS;
    int cell_from_h = usable_h / XIANGQI_ROWS;
    int cell = cell_from_w < cell_from_h ? cell_from_w : cell_from_h;
    if (cell < 20) {
        cell = 20;
    }
    return cell;
}

static int board_left(const App *app, int cell) {
    int board_w = cell * (XIANGQI_COLS - 1);
    int panel = app->width < 560 ? 0 : 170;
    return (app->width - panel - board_w) / 2;
}

static int board_top(const App *app, int cell) {
    int board_h = cell * (XIANGQI_ROWS - 1);
    return 12 + (app->height - 24 - board_h) / 2;
}

static void clamp_pointer(App *app) {
    if (app->pointer_x < 0) app->pointer_x = 0;
    if (app->pointer_y < 0) app->pointer_y = 0;
    if (app->pointer_x >= app->width) app->pointer_x = app->width - 1;
    if (app->pointer_y >= app->height) app->pointer_y = app->height - 1;
}

static void warp_mouse_to_pointer(App *app) {
    int x = app->pointer_x;
    int y = app->pointer_y;
    if (!app->window) {
        return;
    }
    if (app->renderer) {
        SDL_RenderLogicalToWindow(app->renderer, (float)app->pointer_x, (float)app->pointer_y, &x, &y);
    }
    SDL_WarpMouseInWindow(app->window, x, y);
}

static void update_pointer(App *app) {
    if (app->pointer_vx != 0 || app->pointer_vy != 0) {
        app->pointer_visible = true;
        app->pointer_x += app->pointer_vx;
        app->pointer_y += app->pointer_vy;
        clamp_pointer(app);
        warp_mouse_to_pointer(app);
    }
}

static void compute_board_layout(const App *app, int *left, int *top, int *cell) {
    int c = board_size(app);
    int l = board_left(app, c);
    int t = board_top(app, c);
    if (app->use_art) {
        int max_h = app->height - 24;
        int max_w = app->width < 560 ? app->width - 16 : app->width - 180;
        int w = max_w;
        int h = (w * 600) / 540;
        if (h > max_h) {
            h = max_h;
            w = (h * 540) / 600;
        }
        l = (app->width - (app->width < 560 ? 0 : 170) - w) / 2 + (30 * w) / 540;
        t = 12 + (app->height - 24 - h) / 2 + (30 * h) / 600;
        c = (60 * w) / 540;
    }
    *left = l;
    *top = t;
    *cell = c;
}

static bool board_point_to_cell(const App *app, int x, int y, int *row, int *col) {
    int left;
    int top;
    int cell;
    int c;
    int r;
    compute_board_layout(app, &left, &top, &cell);
    c = (x - left + cell / 2) / cell;
    r = (y - top + cell / 2) / cell;
    if (r < 0 || r >= XIANGQI_ROWS || c < 0 || c >= XIANGQI_COLS) {
        return false;
    }
    if (abs(x - (left + c * cell)) > cell / 2 || abs(y - (top + r * cell)) > cell / 2) {
        return false;
    }
    *row = r;
    *col = c;
    return true;
}

static void set_mode(App *app, PlayMode mode) {
    app->mode = mode;
    app->ai_enabled = app->mode != MODE_TWO_PLAYER;
    app->selected_id = -1;
    app->ai_pending = !is_human_turn(app);
    set_status(app, mode_name(app->mode));
}

static void set_difficulty(App *app, XiangqiAiLevel level) {
    app->ai_level = level;
    engine_set_level(&app->engine, app->ai_level);
    set_status(app, difficulty_name(app->ai_level));
}

static bool point_in_rect(int x, int y, SDL_Rect rect) {
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

static int max_move_scroll(const App *app, int rows) {
    int max = app->game.history_len - rows;
    return max > 0 ? max : 0;
}

static void clamp_move_scroll(App *app, int rows) {
    int max = max_move_scroll(app, rows);
    if (app->move_scroll < 0) {
        app->move_scroll = 0;
    }
    if (app->move_scroll > max) {
        app->move_scroll = max;
    }
}

static void reset_game(App *app);
static void undo_move(App *app);
static void activate_cell(App *app);

static void pointer_click(App *app, int x, int y) {
    int row;
    int col;
    int panel_x = app->width >= 560 ? app->width - 174 : 6;
    int list_rows = (430 - 162) / 18;
    int tab;
    if (list_rows < 3) {
        list_rows = 3;
    }
    if (app->width >= 560 && y >= 440 && y < 474) {
        for (tab = 0; tab < 3; tab++) {
            if (point_in_rect(x, y, (SDL_Rect){ panel_x + tab * 55, 440, 52, 34 })) {
                app->side_page = (SidePage)tab;
                app->open_dropdown = DROPDOWN_NONE;
                return;
            }
        }
    }
    if (app->open_dropdown == DROPDOWN_MODE) {
        int i;
        SDL_Rect box = { panel_x, 160, 162, 34 };
        if (point_in_rect(x, y, box)) {
            app->open_dropdown = DROPDOWN_NONE;
            return;
        }
        for (i = 0; i < 4; i++) {
            SDL_Rect opt = { panel_x, 194 + i * 34, 162, 34 };
            if (point_in_rect(x, y, opt)) {
                set_mode(app, (PlayMode)i);
                app->open_dropdown = DROPDOWN_NONE;
                return;
            }
        }
        app->open_dropdown = DROPDOWN_NONE;
        return;
    }
    if (app->open_dropdown == DROPDOWN_DIFFICULTY) {
        int i;
        SDL_Rect box = { panel_x, 222, 162, 34 };
        if (point_in_rect(x, y, box)) {
            app->open_dropdown = DROPDOWN_NONE;
            return;
        }
        for (i = 0; i < 3; i++) {
            SDL_Rect opt = { panel_x, 256 + i * 34, 162, 34 };
            if (point_in_rect(x, y, opt)) {
                set_difficulty(app, (XiangqiAiLevel)i);
                app->open_dropdown = DROPDOWN_NONE;
                return;
            }
        }
        app->open_dropdown = DROPDOWN_NONE;
        return;
    }
    if (app->side_page == PAGE_MAIN) {
        if (point_in_rect(x, y, (SDL_Rect){ panel_x, 58, 162, 34 })) {
            reset_game(app);
        } else if (point_in_rect(x, y, (SDL_Rect){ panel_x, 96, 162, 34 })) {
            undo_move(app);
        } else if (point_in_rect(x, y, (SDL_Rect){ panel_x, 160, 162, 34 })) {
            app->open_dropdown = DROPDOWN_MODE;
        } else if (point_in_rect(x, y, (SDL_Rect){ panel_x, 222, 162, 34 })) {
            app->open_dropdown = DROPDOWN_DIFFICULTY;
        } else if (point_in_rect(x, y, (SDL_Rect){ panel_x, 352, 78, 34 })) {
            save_game(app);
        } else if (point_in_rect(x, y, (SDL_Rect){ panel_x + 84, 352, 78, 34 })) {
            load_game(app);
        } else if (point_in_rect(x, y, (SDL_Rect){ panel_x, 394, 162, 34 })) {
            app->running = false;
        } else if (board_point_to_cell(app, x, y, &row, &col)) {
            app->cursor_row = row;
            app->cursor_col = col;
            activate_cell(app);
        }
    } else if (app->side_page == PAGE_MOVES) {
        if (app->width >= 560 && point_in_rect(x, y, (SDL_Rect){ panel_x, 58, 34, 34 })) {
            review_step(app, -app->game.history_len);
        } else if (app->width >= 560 && point_in_rect(x, y, (SDL_Rect){ panel_x + 42, 58, 34, 34 })) {
            review_step(app, -1);
        } else if (app->width >= 560 && point_in_rect(x, y, (SDL_Rect){ panel_x + 86, 58, 34, 34 })) {
            review_step(app, 1);
        } else if (app->width >= 560 && point_in_rect(x, y, (SDL_Rect){ panel_x + 128, 58, 34, 34 })) {
            review_to_latest(app);
            set_status(app, "LIVE BOARD");
        } else if (app->width >= 560 && point_in_rect(x, y, (SDL_Rect){ panel_x + 144, 162, 18, 26 })) {
            app->move_scroll++;
            clamp_move_scroll(app, list_rows);
        } else if (app->width >= 560 && point_in_rect(x, y, (SDL_Rect){ panel_x + 144, 404, 18, 26 })) {
            app->move_scroll--;
            clamp_move_scroll(app, list_rows);
        } else if (board_point_to_cell(app, x, y, &row, &col)) {
            app->cursor_row = row;
            app->cursor_col = col;
            activate_cell(app);
        }
    } else if (board_point_to_cell(app, x, y, &row, &col)) {
        app->open_dropdown = DROPDOWN_NONE;
        app->cursor_row = row;
        app->cursor_col = col;
        activate_cell(app);
    }
}

static bool pointer_over_panel(const App *app) {
    return app->width >= 560 && app->pointer_x >= app->width - 182;
}

static void draw_bezel(SDL_Renderer *r, SDL_Rect rect, bool pressed) {
    SDL_Rect fill = { rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2 };
    SDL_Rect shadow = { rect.x + 2, rect.y + 2, rect.w, rect.h };
    if (rect.w <= 2 || rect.h <= 2) {
        SDL_RenderDrawRect(r, &rect);
        return;
    }
    set_color(r, 145, 140, 126, 255);
    SDL_RenderFillRect(r, &shadow);
    set_color(r, pressed ? 178 : 212, pressed ? 174 : 208, pressed ? 158 : 192, 255);
    SDL_RenderFillRect(r, &fill);
    if (pressed) {
        set_color(r, 80, 78, 70, 255);
        SDL_RenderDrawLine(r, rect.x, rect.y, rect.x + rect.w - 1, rect.y);
        SDL_RenderDrawLine(r, rect.x, rect.y, rect.x, rect.y + rect.h - 1);
        set_color(r, 235, 231, 212, 255);
        SDL_RenderDrawLine(r, rect.x, rect.y + rect.h - 1, rect.x + rect.w - 1, rect.y + rect.h - 1);
        SDL_RenderDrawLine(r, rect.x + rect.w - 1, rect.y, rect.x + rect.w - 1, rect.y + rect.h - 1);
    } else {
        set_color(r, 245, 241, 222, 255);
        SDL_RenderDrawLine(r, rect.x, rect.y, rect.x + rect.w - 1, rect.y);
        SDL_RenderDrawLine(r, rect.x, rect.y, rect.x, rect.y + rect.h - 1);
        set_color(r, 80, 78, 70, 255);
        SDL_RenderDrawLine(r, rect.x, rect.y + rect.h - 1, rect.x + rect.w - 1, rect.y + rect.h - 1);
        SDL_RenderDrawLine(r, rect.x + rect.w - 1, rect.y, rect.x + rect.w - 1, rect.y + rect.h - 1);
    }
    set_color(r, 25, 25, 25, 255);
}

static void draw_button(SDL_Renderer *r, const char *label, SDL_Rect rect) {
    draw_bezel(r, rect, false);
    draw_text_mid_bold(r, label, rect.x + 6, rect.y + (rect.h - 14) / 2);
}

static void draw_section_label(SDL_Renderer *r, const char *label, int x, int y, int w) {
    int line_x = x + (int)strlen(label) * 8 + 8;
    draw_text_mid_bold(r, label, x, y);
    if (line_x < x + w) {
        SDL_RenderDrawLine(r, line_x, y + 16, x + w, y + 16);
    }
}

static void draw_dropdown(SDL_Renderer *r, const char *value, SDL_Rect rect, bool open) {
    draw_bezel(r, rect, open);
    draw_text_mid_bold(r, value, rect.x + 7, rect.y + (rect.h - 14) / 2);
    SDL_RenderDrawLine(r, rect.x + rect.w - 16, rect.y + 10, rect.x + rect.w - 8, rect.y + 10);
    SDL_RenderDrawLine(r, rect.x + rect.w - 15, rect.y + 11, rect.x + rect.w - 9, rect.y + 11);
    SDL_RenderDrawLine(r, rect.x + rect.w - 14, rect.y + 12, rect.x + rect.w - 10, rect.y + 12);
}

static void draw_dropdown_menu(SDL_Renderer *r, SDL_Rect rect, const char **items, int count, int selected) {
    int i;
    set_color(r, 210, 206, 190, 255);
    SDL_RenderFillRect(r, &rect);
    set_color(r, 25, 25, 25, 255);
    draw_bezel(r, rect, false);
    for (i = 0; i < count; i++) {
        SDL_Rect item = { rect.x, rect.y + i * 25, rect.w, 25 };
        if (i == selected) {
            set_color(r, 25, 25, 25, 255);
            SDL_RenderFillRect(r, &(SDL_Rect){ item.x + 1, item.y + 1, item.w - 2, item.h - 2 });
            set_color(r, 222, 218, 199, 255);
        } else {
            set_color(r, 25, 25, 25, 255);
        }
        draw_text_mid_bold(r, items[i], item.x + 7, item.y + 5);
    }
    set_color(r, 25, 25, 25, 255);
}

static void draw_scroll_button(SDL_Renderer *r, SDL_Rect rect, bool up) {
    int cx = rect.x + rect.w / 2;
    int cy = rect.y + rect.h / 2;
    draw_bezel(r, rect, false);
    if (up) {
        SDL_RenderDrawLine(r, cx, cy - 4, cx - 5, cy + 3);
        SDL_RenderDrawLine(r, cx, cy - 4, cx + 5, cy + 3);
        SDL_RenderDrawLine(r, cx - 5, cy + 3, cx + 5, cy + 3);
    } else {
        SDL_RenderDrawLine(r, cx, cy + 4, cx - 5, cy - 3);
        SDL_RenderDrawLine(r, cx, cy + 4, cx + 5, cy - 3);
        SDL_RenderDrawLine(r, cx - 5, cy - 3, cx + 5, cy - 3);
    }
}

static void draw_page_tabs(SDL_Renderer *r, const App *app, int panel_x) {
    const char *labels[] = { "MAIN", "MOVES", "HELP" };
    int i;
    for (i = 0; i < 3; i++) {
        SDL_Rect tab = { panel_x + i * 55, 440, 52, 34 };
        if ((int)app->side_page == i) {
            draw_bezel(r, tab, true);
            set_color(r, 222, 218, 199, 255);
            draw_text_mid_bold(r, labels[i], tab.x + 5, tab.y + 10);
            set_color(r, 25, 25, 25, 255);
        } else {
            draw_bezel(r, tab, false);
            draw_text_mid_bold(r, labels[i], tab.x + 5, tab.y + 10);
        }
    }
}

static SDL_Rect icon_slot(SDL_Rect rect) {
    SDL_Rect icon = { rect.x + 6, rect.y + 4, 18, rect.h - 8 };
    if (icon.h > 14) {
        icon.y += (icon.h - 14) / 2;
        icon.h = 14;
    }
    return icon;
}

static void draw_save_icon(SDL_Renderer *r, SDL_Rect rect) {
    draw_bezel(r, rect, false);
    if (rect.w >= 48) {
        SDL_Rect icon = icon_slot(rect);
        SDL_RenderDrawRect(r, &icon);
        SDL_RenderFillRect(r, &(SDL_Rect){ icon.x + 3, icon.y + 2, icon.w - 6, 3 });
        SDL_RenderDrawRect(r, &(SDL_Rect){ icon.x + 5, icon.y + icon.h - 5, icon.w - 10, 3 });
        draw_text_mid_bold(r, "SAVE", rect.x + 31, rect.y + 6);
    } else {
        SDL_RenderDrawRect(r, &(SDL_Rect){ rect.x + 6, rect.y + 4, rect.w - 12, rect.h - 8 });
        SDL_RenderFillRect(r, &(SDL_Rect){ rect.x + 8, rect.y + 5, rect.w - 16, 4 });
        SDL_RenderDrawRect(r, &(SDL_Rect){ rect.x + 10, rect.y + rect.h - 8, rect.w - 20, 4 });
    }
}

static void draw_load_icon(SDL_Renderer *r, SDL_Rect rect) {
    draw_bezel(r, rect, false);
    if (rect.w >= 48) {
        SDL_Rect icon = icon_slot(rect);
        SDL_RenderDrawLine(r, icon.x, icon.y + 3, icon.x + 6, icon.y + 3);
        SDL_RenderDrawLine(r, icon.x + 6, icon.y + 3, icon.x + 9, icon.y + 6);
        SDL_RenderDrawRect(r, &(SDL_Rect){ icon.x, icon.y + 5, icon.w, icon.h - 5 });
        SDL_RenderDrawLine(r, icon.x + icon.w / 2, icon.y + 1, icon.x + icon.w / 2, icon.y + icon.h - 2);
        SDL_RenderDrawLine(r, icon.x + icon.w / 2, icon.y + icon.h - 2, icon.x + icon.w / 2 - 4, icon.y + icon.h - 6);
        SDL_RenderDrawLine(r, icon.x + icon.w / 2, icon.y + icon.h - 2, icon.x + icon.w / 2 + 4, icon.y + icon.h - 6);
        draw_text_mid_bold(r, "LOAD", rect.x + 31, rect.y + 6);
    } else {
        SDL_RenderDrawLine(r, rect.x + 5, rect.y + 7, rect.x + 10, rect.y + 7);
        SDL_RenderDrawLine(r, rect.x + 10, rect.y + 7, rect.x + 13, rect.y + 10);
        SDL_RenderDrawRect(r, &(SDL_Rect){ rect.x + 5, rect.y + 9, rect.w - 10, rect.h - 13 });
        SDL_RenderDrawLine(r, rect.x + rect.w / 2, rect.y + 6, rect.x + rect.w / 2, rect.y + rect.h - 5);
        SDL_RenderDrawLine(r, rect.x + rect.w / 2, rect.y + rect.h - 5, rect.x + rect.w / 2 - 4, rect.y + rect.h - 9);
        SDL_RenderDrawLine(r, rect.x + rect.w / 2, rect.y + rect.h - 5, rect.x + rect.w / 2 + 4, rect.y + rect.h - 9);
    }
}

static void draw_board(App *app) {
    SDL_Renderer *r = app->renderer;
    XiangqiGame view;
    const XiangqiGame *game;
    int cell = board_size(app);
    int left = board_left(app, cell);
    int top = board_top(app, cell);
    int right;
    int bottom;
    SDL_Rect board_rect;
    double sx;
    double sy;
    int row;
    int col;
    int i;
    char text[48];

    build_review_game(app, &view);
    game = is_latest_view(app) ? &app->game : &view;

    set_color(r, 222, 218, 199, 255);
    SDL_RenderClear(r);

    if (app->use_art) {
        int max_h = app->height - 24;
        int max_w = app->width < 560 ? app->width - 16 : app->width - 180;
        int w = max_w;
        int h = (w * 600) / 540;
        if (h > max_h) {
            h = max_h;
            w = (h * 540) / 600;
        }
        board_rect.w = w;
        board_rect.h = h;
        board_rect.x = (app->width - (app->width < 560 ? 0 : 170) - w) / 2;
        board_rect.y = 12 + (app->height - 24 - h) / 2;
        SDL_RenderCopy(r, app->board_texture, NULL, &board_rect);
        sx = board_rect.w / 540.0;
        sy = board_rect.h / 600.0;
        cell = (int)(60 * sx);
        left = board_rect.x + (int)(30 * sx);
        top = board_rect.y + (int)(30 * sy);
    } else {
        right = left + cell * (XIANGQI_COLS - 1);
        bottom = top + cell * (XIANGQI_ROWS - 1);
        set_color(r, 30, 30, 30, 255);
        for (row = 0; row < XIANGQI_ROWS; row++) {
            int y = top + row * cell;
            SDL_RenderDrawLine(r, left, y, right, y);
        }
        for (col = 0; col < XIANGQI_COLS; col++) {
            int x = left + col * cell;
            SDL_RenderDrawLine(r, x, top, x, top + 4 * cell);
            SDL_RenderDrawLine(r, x, top + 5 * cell, x, bottom);
        }
        SDL_RenderDrawLine(r, left + 3 * cell, top, left + 5 * cell, top + 2 * cell);
        SDL_RenderDrawLine(r, left + 5 * cell, top, left + 3 * cell, top + 2 * cell);
        SDL_RenderDrawLine(r, left + 3 * cell, bottom, left + 5 * cell, bottom - 2 * cell);
        SDL_RenderDrawLine(r, left + 5 * cell, bottom, left + 3 * cell, bottom - 2 * cell);

        set_color(r, 80, 80, 80, 255);
        draw_text(r, "RIVER", left + cell * 3, top + cell * 4 + cell / 2 - 4, 1);
    }

    if (app->game.history_len > 0) {
        int ply = is_latest_view(app) ? app->game.history_len : app->review_ply;
        if (ply > 0) {
            const XiangqiMove *last = &app->game.history[ply - 1];
            SDL_Rect from = {
                left + last->from_col * cell - cell / 2,
                top + last->from_row * cell - cell / 2,
                cell,
                cell
            };
            SDL_Rect to = {
                left + last->to_col * cell - cell / 2,
                top + last->to_row * cell - cell / 2,
                cell,
                cell
            };
            set_color(r, 55, 108, 180, 255);
            SDL_RenderDrawRect(r, &from);
            SDL_RenderDrawRect(r, &(SDL_Rect){ from.x + 2, from.y + 2, from.w - 4, from.h - 4 });
            SDL_RenderDrawRect(r, &to);
            SDL_RenderDrawRect(r, &(SDL_Rect){ to.x + 2, to.y + 2, to.w - 4, to.h - 4 });
        }
    }

    if (is_latest_view(app) && app->selected_id >= 0) {
        set_color(r, 27, 128, 64, 255);
        for (row = 0; row < XIANGQI_ROWS; row++) {
            for (col = 0; col < XIANGQI_COLS; col++) {
                if (xiangqi_is_legal_move(&app->game, app->selected_id, row, col)) {
                    int x = left + col * cell;
                    int y = top + row * cell;
                    SDL_Rect mark = { x - cell / 6, y - cell / 6, cell / 3, cell / 3 };
                    SDL_RenderDrawRect(r, &mark);
                    if (xiangqi_piece_at(&app->game, row, col) >= 0) {
                        SDL_RenderDrawRect(r, &(SDL_Rect){ x - cell / 2 + 4, y - cell / 2 + 4, cell - 8, cell - 8 });
                    }
                }
            }
        }
    }

    for (i = 0; i < XIANGQI_PIECES; i++) {
        const XiangqiPiece *piece = &game->pieces[i];
        int x;
        int y;
        if (piece->dead) {
            continue;
        }
        x = left + piece->col * cell;
        y = top + piece->row * cell;
        if (app->use_art) {
            int piece_size = (int)(cell * 0.95);
            int set = nearest_piece_asset_index(piece_size);
            SDL_Texture *texture = app->piece_textures[piece->side][piece->type][set];
            if (texture) {
                int draw_size = piece_size;
                SDL_Rect dst = { x - draw_size / 2, y - draw_size / 2, draw_size, draw_size };
                SDL_RenderCopy(r, texture, NULL, &dst);
                continue;
            }
        }
        if (piece->side == XIANGQI_RED) {
            set_color(r, 230, 230, 230, 255);
        } else {
            set_color(r, 40, 40, 40, 255);
        }
        fill_circle(r, x, y, cell / 3);
        if (piece->side == XIANGQI_RED) {
            set_color(r, 170, 0, 0, 255);
        } else {
            set_color(r, 245, 245, 245, 255);
        }
        draw_text(r, piece_code(piece), x - 8, y - 6, cell >= 40 ? 2 : 1);
    }

    if (is_latest_view(app) && app->selected_id >= 0) {
        XiangqiPiece *piece = &app->game.pieces[app->selected_id];
        SDL_Rect selected = {
            left + piece->col * cell - cell / 2,
            top + piece->row * cell - cell / 2,
            cell,
            cell
        };
        set_color(r, 0, 0, 0, 255);
        SDL_RenderDrawRect(r, &selected);
        SDL_RenderDrawRect(r, &(SDL_Rect){ selected.x + 1, selected.y + 1, selected.w - 2, selected.h - 2 });
    }

    SDL_RenderDrawRect(r, &(SDL_Rect){
        left + app->cursor_col * cell - cell / 2,
        top + app->cursor_row * cell - cell / 2,
        cell,
        cell
    });
    SDL_RenderDrawRect(r, &(SDL_Rect){
        left + app->cursor_col * cell - cell / 2 + 3,
        top + app->cursor_row * cell - cell / 2 + 3,
        cell - 6,
        cell - 6
    });

    set_color(r, 25, 25, 25, 255);
    if (app->width >= 560) {
        int panel_x = app->width - 174;
        int panel_w = 166;
        int move_y = 162;
        int move_bottom = 430;
        int move_rows = (move_bottom - move_y) / 18;
        int first_move;
        const char *mode_items[] = { "PLAY RED", "PLAY BLACK", "2 PLAYER", "AI DEMO" };
        const char *diff_items[] = { "EASY", "MED", "HARD" };
        if (move_rows < 4) {
            move_rows = 4;
        }
        clamp_move_scroll(app, move_rows);
        first_move = app->game.history_len > move_rows ? app->game.history_len - move_rows - app->move_scroll : 0;
        if (first_move < 0) {
            first_move = 0;
        }
        set_color(r, 198, 194, 178, 255);
        SDL_RenderFillRect(r, &(SDL_Rect){ panel_x - 8, 0, 182, app->height });
        set_color(r, 25, 25, 25, 255);
        draw_text_title(r, "EXACT CHINESE CHESS", panel_x, 4);

        if (app->side_page == PAGE_MAIN) {
            draw_section_label(r, "MAIN", panel_x, 34, panel_w);
            draw_button(r, "NEW GAME", (SDL_Rect){ panel_x, 58, 162, 34 });
            draw_button(r, "UNDO", (SDL_Rect){ panel_x, 96, 162, 34 });

            draw_section_label(r, "MODE", panel_x, 136, panel_w);
            draw_dropdown(r, mode_name(app->mode), (SDL_Rect){ panel_x, 160, 162, 34 },
                          app->open_dropdown == DROPDOWN_MODE);

            draw_section_label(r, "DIFFICULTY", panel_x, 198, panel_w);
            draw_dropdown(r, difficulty_name(app->ai_level), (SDL_Rect){ panel_x, 222, 162, 34 },
                          app->open_dropdown == DROPDOWN_DIFFICULTY);

            draw_section_label(r, "STATUS", panel_x, 260, panel_w);
            snprintf(text, sizeof(text), "TURN %s", xiangqi_side_name(game->turn));
            draw_text_mid_bold(r, text, panel_x, 284);
            draw_text_mid_bold(r, app->engine.available ? "AI PIKAFISH" : "AI BUILTIN", panel_x, 304);
            draw_text_mid_bold(r, app->status, panel_x, 324);

            draw_save_icon(r, (SDL_Rect){ panel_x, 352, 78, 34 });
            draw_load_icon(r, (SDL_Rect){ panel_x + 84, 352, 78, 34 });
            draw_button(r, "QUIT", (SDL_Rect){ panel_x, 394, 162, 34 });
        } else if (app->side_page == PAGE_MOVES) {
            draw_section_label(r, "PLAYBACK", panel_x, 34, panel_w);
            draw_button(r, "<<", (SDL_Rect){ panel_x, 58, 34, 34 });
            draw_button(r, "<", (SDL_Rect){ panel_x + 42, 58, 34, 34 });
            draw_button(r, ">", (SDL_Rect){ panel_x + 86, 58, 34, 34 });
            draw_button(r, ">>", (SDL_Rect){ panel_x + 128, 58, 34, 34 });
            if (!is_latest_view(app)) {
                snprintf(text, sizeof(text), "PLY %03d/%03d", app->review_ply, app->game.history_len);
            } else {
                snprintf(text, sizeof(text), "PLY %03d/%03d", app->game.history_len, app->game.history_len);
            }
            draw_text_mid_bold(r, text, panel_x, 98);
            draw_section_label(r, "MOVES", panel_x, 116, panel_w);
            draw_scroll_button(r, (SDL_Rect){ panel_x + 144, 162, 18, 26 }, true);
            draw_scroll_button(r, (SDL_Rect){ panel_x + 144, 404, 18, 26 }, false);
            for (i = first_move; i < app->game.history_len && i < first_move + move_rows; i++) {
                char label[32];
                if (!is_latest_view(app) && i == app->review_ply - 1) {
                    set_color(r, 0, 0, 0, 255);
                    SDL_RenderFillRect(r, &(SDL_Rect){ panel_x - 2, move_y - 1, 141, 16 });
                    set_color(r, 222, 218, 199, 255);
                } else {
                    set_color(r, 25, 25, 25, 255);
                }
                move_ascii_label(&app->game, &app->game.history[i], label, sizeof(label));
                snprintf(text, sizeof(text), "%02d %s", i + 1, label);
                draw_text_mid_bold(r, text, panel_x, move_y);
                move_y += 18;
            }
            if (app->game.history_len > move_rows) {
                int bar_top = 192;
                int bar_h = 208;
                int thumb_h = (bar_h * move_rows) / app->game.history_len;
                int thumb_y = bar_top + (bar_h * first_move) / app->game.history_len;
                if (thumb_h < 10) thumb_h = 10;
                set_color(r, 120, 120, 110, 255);
                SDL_RenderDrawRect(r, &(SDL_Rect){ panel_x + 150, bar_top, 7, bar_h });
                SDL_RenderFillRect(r, &(SDL_Rect){ panel_x + 152, thumb_y, 3, thumb_h });
            }
        } else {
            set_color(r, 25, 25, 25, 255);
            draw_section_label(r, "HELP", panel_x, 34, panel_w);
            draw_text_mid_bold(r, "DPAD MOVE", panel_x, 70);
            draw_text_mid_bold(r, "A SELECT", panel_x, 104);
            draw_text_mid_bold(r, "B CANCEL", panel_x, 138);
            draw_text_mid_bold(r, "X UNDO", panel_x, 172);
            draw_text_mid_bold(r, "Y NEW GAME", panel_x, 206);
            draw_text_mid_bold(r, "L1 PREV", panel_x, 240);
            draw_text_mid_bold(r, "R2 NEXT", panel_x, 274);
            draw_text_mid_bold(r, "R STICK UI", panel_x, 308);
            draw_text_mid_bold(r, "R1 CLICK", panel_x, 342);
            draw_text_mid_bold(r, "SELECT QUIT", panel_x, 376);
        }
        if (app->open_dropdown == DROPDOWN_MODE) {
            draw_dropdown_menu(r, (SDL_Rect){ panel_x, 194, 162, 136 }, mode_items, 4, (int)app->mode);
        } else if (app->open_dropdown == DROPDOWN_DIFFICULTY) {
            draw_dropdown_menu(r, (SDL_Rect){ panel_x, 256, 162, 102 }, diff_items, 3, (int)app->ai_level);
        }
        draw_page_tabs(r, app, panel_x);
    } else {
        snprintf(text, sizeof(text), "%s %s", xiangqi_side_name(game->turn), mode_name(app->mode));
        draw_text(r, text, 8, 6, 1);
        draw_text(r, app->status, 8, app->height - 14, 1);
    }

    if (app->pointer_visible) {
        set_color(r, 0, 0, 0, 255);
        SDL_RenderDrawLine(r, app->pointer_x, app->pointer_y, app->pointer_x + 12, app->pointer_y + 5);
        SDL_RenderDrawLine(r, app->pointer_x, app->pointer_y, app->pointer_x + 5, app->pointer_y + 12);
        SDL_RenderDrawLine(r, app->pointer_x + 5, app->pointer_y + 12, app->pointer_x + 7, app->pointer_y + 8);
        SDL_RenderDrawLine(r, app->pointer_x + 12, app->pointer_y + 5, app->pointer_x + 7, app->pointer_y + 8);
        set_color(r, 245, 245, 245, 255);
        SDL_RenderDrawPoint(r, app->pointer_x + 1, app->pointer_y + 1);
    }
}

static void reset_game(App *app) {
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

static void apply_ai_if_needed(App *app) {
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

static void activate_cell(App *app) {
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

static void move_cursor(App *app, int dc, int dr) {
    app->cursor_col += dc;
    app->cursor_row += dr;
    if (app->cursor_col < 0) app->cursor_col = 0;
    if (app->cursor_col >= XIANGQI_COLS) app->cursor_col = XIANGQI_COLS - 1;
    if (app->cursor_row < 0) app->cursor_row = 0;
    if (app->cursor_row >= XIANGQI_ROWS) app->cursor_row = XIANGQI_ROWS - 1;
}

static void undo_move(App *app) {
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

static void handle_key(App *app, SDL_Keycode key) {
    switch (key) {
    case SDLK_UP: move_cursor(app, 0, -1); break;
    case SDLK_DOWN: move_cursor(app, 0, 1); break;
    case SDLK_LEFT: move_cursor(app, -1, 0); break;
    case SDLK_RIGHT: move_cursor(app, 1, 0); break;
    case SDLK_RETURN:
    case SDLK_SPACE: activate_cell(app); break;
    case SDLK_ESCAPE: app->selected_id = -1; set_status(app, "CANCELLED"); break;
    case SDLK_u: undo_move(app); break;
    case SDLK_n: reset_game(app); break;
    case SDLK_s: save_game(app); break;
    case SDLK_l: load_game(app); break;
    case SDLK_m:
        app->open_dropdown = app->open_dropdown == DROPDOWN_MODE ? DROPDOWN_NONE : DROPDOWN_MODE;
        break;
    case SDLK_d:
        app->open_dropdown = app->open_dropdown == DROPDOWN_DIFFICULTY ? DROPDOWN_NONE : DROPDOWN_DIFFICULTY;
        break;
    case SDLK_1:
        app->ai_level = XIANGQI_AI_EASY;
        engine_set_level(&app->engine, app->ai_level);
        set_status(app, "AI EASY");
        break;
    case SDLK_2:
        app->ai_level = XIANGQI_AI_MEDIUM;
        engine_set_level(&app->engine, app->ai_level);
        set_status(app, "AI MEDIUM");
        break;
    case SDLK_3:
        app->ai_level = XIANGQI_AI_HARD;
        engine_set_level(&app->engine, app->ai_level);
        set_status(app, "AI HARD");
        break;
    case SDLK_HOME: review_step(app, -app->game.history_len); break;
    case SDLK_END: review_to_latest(app); set_status(app, "LIVE BOARD"); break;
    case SDLK_COMMA: review_step(app, -1); break;
    case SDLK_PERIOD: review_step(app, 1); break;
    case SDLK_q: app->running = false; break;
    default: break;
    }
}

static void handle_controller_button(App *app, SDL_ControllerButtonEvent *button) {
    if (button->state != SDL_PRESSED) {
        return;
    }
    switch (button->button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: handle_key(app, SDLK_UP); break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: handle_key(app, SDLK_DOWN); break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: handle_key(app, SDLK_LEFT); break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: handle_key(app, SDLK_RIGHT); break;
    case SDL_CONTROLLER_BUTTON_A:
        if (app->pointer_visible && pointer_over_panel(app)) {
            pointer_click(app, app->pointer_x, app->pointer_y);
        } else {
            handle_key(app, SDLK_RETURN);
        }
        break;
    case SDL_CONTROLLER_BUTTON_B:
        if (app->open_dropdown != DROPDOWN_NONE) {
            app->open_dropdown = DROPDOWN_NONE;
        } else {
            handle_key(app, SDLK_ESCAPE);
        }
        break;
    case SDL_CONTROLLER_BUTTON_X: handle_key(app, SDLK_u); break;
    case SDL_CONTROLLER_BUTTON_Y: handle_key(app, SDLK_n); break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: handle_key(app, SDLK_COMMA); break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
        app->pointer_visible = true;
        pointer_click(app, app->pointer_x, app->pointer_y);
        break;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
        app->pointer_visible = true;
        pointer_click(app, app->pointer_x, app->pointer_y);
        break;
    case SDL_CONTROLLER_BUTTON_BACK:
        app->back_down = true;
        handle_key(app, SDLK_q);
        break;
    case SDL_CONTROLLER_BUTTON_START:
        app->start_down = true;
        if (app->back_down) {
            handle_key(app, SDLK_q);
        } else {
            app->side_page = (SidePage)(((int)app->side_page + 1) % 3);
            app->open_dropdown = DROPDOWN_NONE;
        }
        break;
    default:
        break;
    }
}

static void handle_controller_button_up(App *app, SDL_ControllerButtonEvent *button) {
    switch (button->button) {
    case SDL_CONTROLLER_BUTTON_BACK:
        app->back_down = false;
        break;
    case SDL_CONTROLLER_BUTTON_START:
        app->start_down = false;
        break;
    default:
        break;
    }
}

static void handle_controller_axis(App *app, SDL_ControllerAxisEvent *axis) {
    static Uint32 next_axis_tick = 0;
    static bool left_trigger_down = false;
    static bool right_trigger_down = false;
    Uint32 now = SDL_GetTicks();
    if (axis->axis == SDL_CONTROLLER_AXIS_RIGHTX) {
        app->pointer_vx = abs(axis->value) > 5000 ? axis->value / 4000 : 0;
        app->pointer_visible = app->pointer_visible || app->pointer_vx != 0;
        return;
    }
    if (axis->axis == SDL_CONTROLLER_AXIS_RIGHTY) {
        app->pointer_vy = abs(axis->value) > 5000 ? axis->value / 4000 : 0;
        app->pointer_visible = app->pointer_visible || app->pointer_vy != 0;
        return;
    }
    if (axis->axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) {
        if (axis->value > 22000 && !left_trigger_down) {
            left_trigger_down = true;
            handle_key(app, SDLK_HOME);
        } else if (axis->value < 8000) {
            left_trigger_down = false;
        }
        return;
    }
    if (axis->axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
        if (axis->value > 22000 && !right_trigger_down) {
            right_trigger_down = true;
            handle_key(app, SDLK_PERIOD);
        } else if (axis->value < 8000) {
            right_trigger_down = false;
        }
        return;
    }
    if (now < next_axis_tick) {
        return;
    }
    if (axis->axis == SDL_CONTROLLER_AXIS_LEFTX) {
        if (axis->value < -22000) {
            handle_key(app, SDLK_LEFT);
            next_axis_tick = now + 180;
        } else if (axis->value > 22000) {
            handle_key(app, SDLK_RIGHT);
            next_axis_tick = now + 180;
        }
    } else if (axis->axis == SDL_CONTROLLER_AXIS_LEFTY) {
        if (axis->value < -22000) {
            handle_key(app, SDLK_UP);
            next_axis_tick = now + 180;
        } else if (axis->value > 22000) {
            handle_key(app, SDLK_DOWN);
            next_axis_tick = now + 180;
        }
    }
}

static void open_controllers(App *app) {
    int i;
    int count = SDL_NumJoysticks();
    fprintf(stderr, "SDL joysticks: %d\n", count);
    for (i = 0; i < count && app->controller_count < 4; i++) {
        const char *name = SDL_JoystickNameForIndex(i);
        fprintf(stderr, "Joystick %d: %s controller=%s\n",
                i, name ? name : "(unknown)", SDL_IsGameController(i) ? "yes" : "no");
        if (SDL_IsGameController(i)) {
            SDL_GameController *controller = SDL_GameControllerOpen(i);
            if (controller) {
                app->controllers[app->controller_count++] = controller;
                fprintf(stderr, "Opened controller: %s\n", SDL_GameControllerName(controller));
            }
        }
    }
}

static void close_controllers(App *app) {
    int i;
    for (i = 0; i < app->controller_count; i++) {
        if (app->controllers[i]) {
            SDL_GameControllerClose(app->controllers[i]);
        }
    }
    app->controller_count = 0;
}

int main(int argc, char **argv) {
    App app;
    SDL_Event event;
    Uint32 last_ai_tick = 0;
    Uint32 input_ready_tick = 0;
    (void)argc;
    (void)argv;

    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());

    memset(&app, 0, sizeof(app));
    app.width = 640;
    app.height = 480;
    app.running = true;
    app.ai_enabled = false;
    app.ai_pending = false;
    app.ai_level = XIANGQI_AI_EASY;
    app.pointer_x = 480;
    app.pointer_y = 72;
    app.pointer_visible = true;
    app.show_moves = true;
    app.side_page = PAGE_MAIN;
    engine_init(&app.engine);
    engine_set_level(&app.engine, app.ai_level);
    reset_game(&app);

    if (!SDL_getenv("SDL_RENDER_DRIVER")) {
        SDL_setenv("SDL_RENDER_DRIVER", "software", 0);
    }
    if (!SDL_getenv("SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS")) {
        SDL_setenv("SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS", "0", 0);
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    log_sdl_drivers();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    fprintf(stderr, "Current video driver: %s\n", SDL_GetCurrentVideoDriver());

    app.window = SDL_CreateWindow("Exact Chinese Chess",
                                  SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                  app.width, app.height,
                                  SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!app.window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_GetWindowSize(app.window, &app.width, &app.height);
    fprintf(stderr, "Window size: %dx%d\n", app.width, app.height);
    if (app.width >= 560) {
        app.pointer_x = app.width - 160;
        app.pointer_y = 72;
    }
    SDL_ShowCursor(SDL_DISABLE);

    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_SOFTWARE);
    if (!app.renderer) {
        fprintf(stderr, "Software renderer failed: %s\n", SDL_GetError());
        app.renderer = SDL_CreateRenderer(app.window, -1, 0);
    }
    if (!app.renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(app.window);
        SDL_Quit();
        return 1;
    }
    {
        SDL_RendererInfo info;
        if (SDL_GetRendererInfo(app.renderer, &info) == 0) {
            fprintf(stderr, "Renderer: %s flags=0x%x\n", info.name, info.flags);
        }
    }
    SDL_RenderSetLogicalSize(app.renderer, app.width, app.height);
    warp_mouse_to_pointer(&app);
    open_controllers(&app);
    load_assets(&app);
    if (engine_start(&app.engine)) {
        set_status(&app, "PIKAFISH READY");
    }
    input_ready_tick = SDL_GetTicks() + 800;

    while (app.running) {
        int events_this_frame = 0;
        while (events_this_frame < 32 && SDL_PollEvent(&event)) {
            events_this_frame++;
            switch (event.type) {
            case SDL_QUIT:
                app.running = false;
                break;
            case SDL_KEYDOWN:
                handle_key(&app, event.key.keysym.sym);
                break;
            case SDL_KEYUP:
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                if (SDL_GetTicks() >= input_ready_tick) {
                    handle_controller_button(&app, &event.cbutton);
                }
                break;
            case SDL_CONTROLLERBUTTONUP:
                if (SDL_GetTicks() >= input_ready_tick) {
                    handle_controller_button_up(&app, &event.cbutton);
                }
                break;
            case SDL_CONTROLLERAXISMOTION:
                if (SDL_GetTicks() >= input_ready_tick) {
                    handle_controller_axis(&app, &event.caxis);
                }
                break;
            case SDL_MOUSEMOTION:
                if (SDL_GetTicks() >= input_ready_tick) {
                    app.pointer_visible = true;
                    app.pointer_x = event.motion.x;
                    app.pointer_y = event.motion.y;
                    clamp_pointer(&app);
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (SDL_GetTicks() >= input_ready_tick) {
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        bool had_pointer = app.pointer_visible;
                        if (!had_pointer) {
                            app.pointer_x = event.button.x;
                            app.pointer_y = event.button.y;
                            clamp_pointer(&app);
                        }
                        app.pointer_visible = true;
                        pointer_click(&app, app.pointer_x, app.pointer_y);
                    }
                }
                break;
            default:
                break;
            }
        }
        if (app.ai_pending && SDL_GetTicks() - last_ai_tick > 300) {
            apply_ai_if_needed(&app);
            last_ai_tick = SDL_GetTicks();
        }
        update_pointer(&app);
        draw_board(&app);
        SDL_RenderPresent(app.renderer);
        SDL_Delay(16);
    }

    destroy_assets(&app);
    engine_stop(&app.engine);
    close_controllers(&app);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    return 0;
}
