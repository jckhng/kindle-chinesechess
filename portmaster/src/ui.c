/*
 * Exact Chinese Chess PortMaster SDL UI.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ui.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int piece_asset_sizes[PIECE_ASSET_SET_COUNT] = { 24, 28, 34, 40, 42, 46, 52, 60, 72, 84, 96, 128 };

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

void ui_log_sdl_drivers(void) {
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

void ui_load_assets(App *app) {
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

void ui_destroy_assets(App *app) {
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

void ui_clamp_pointer(App *app) {
    if (app->pointer_x < 0) app->pointer_x = 0;
    if (app->pointer_y < 0) app->pointer_y = 0;
    if (app->pointer_x >= app->width) app->pointer_x = app->width - 1;
    if (app->pointer_y >= app->height) app->pointer_y = app->height - 1;
}

void ui_warp_mouse_to_pointer(App *app) {
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

bool ui_update_pointer(App *app) {
    if (app->pointer_vx != 0 || app->pointer_vy != 0) {
        app->pointer_visible = true;
        app->pointer_x += app->pointer_vx;
        app->pointer_y += app->pointer_vy;
        ui_clamp_pointer(app);
        ui_warp_mouse_to_pointer(app);
        return true;
    }
    return false;
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

void ui_pointer_click(App *app, int x, int y) {
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

bool ui_pointer_over_panel(const App *app) {
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

void ui_draw_board(App *app) {
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
        draw_text(r, game_piece_code(piece), x - 8, y - 6, cell >= 40 ? 2 : 1);
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
