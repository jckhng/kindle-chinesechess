/*
 * Kindle ChineseChess
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Unofficial Kindle-focused Xiangqi adaptation. Code lineage, artwork
 * provenance, and Pikafish source notes are documented in docs/PROVENANCE.md.
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "pikafish_uci.h"
#include "xiangqi_engine.h"

#define APP_TITLE "Kindle ChineseChess"
#define KINDLE_WINDOW_TITLE "L:A_N:application_ID:kindlechinesechess_PC:N_O:URL"
#define KINDLE_WINDOW_TITLE_TOPBAR "L:A_N:application_PC:T_ID:kindlechinesechess_O:URL"
#define LOG_PATH "/mnt/us/kindle-chinesechess.log"
#define SAVE_PATH "/mnt/us/extensions/kindle-chinesechess/kindle-chinesechess.save"
#define LEGACY_SAVE_PATH "/mnt/us/documents/kindle-chinesechess.txt"
#define KINDLE_APP_WIDTH 1072
#define KINDLE_APP_HEIGHT 1448
#define DEFAULT_PIKAFISH_PATH "/mnt/us/extensions/kindle-chinesechess/bin/armhf/pikafish"

typedef enum {
    MODE_PLAY_RED = 0,
    MODE_PLAY_BLACK,
    MODE_TWO_PLAYER,
    MODE_AI_DEMO
} AppMode;

static const char *kindle_window_title(void)
{
    const char *value = g_getenv("KINDLE_SHOW_TOPBAR");
    return (value != NULL && value[0] != '\0' && strcmp(value, "0") != 0) ? KINDLE_WINDOW_TITLE_TOPBAR
                                                                          : KINDLE_WINDOW_TITLE;
}

typedef struct {
    GtkWidget *window;
    GtkWidget *board;
    GtkWidget *status;
    GtkWidget *mode_combo;
    GtkWidget *level_combo;
    GtkWidget *moves_label;
    GtkWidget *history_sidebar;
    GtkWidget *history_toggle_button;
    GtkWidget *history_view;
    GtkWidget *history_first_button;
    GtkWidget *history_prev_button;
    GtkWidget *history_next_button;
    GtkWidget *history_latest_button;
    XiangqiGame game;
    PikafishUci pikafish;
    GdkPixbuf *board_image;
    GdkPixbuf *piece_images[2][7];
    AppMode mode;
    XiangqiAiLevel level;
    gboolean pikafish_available;
    guint ai_source;
    int view_ply;
    char message[192];
    gboolean history_visible;
} AppState;

static AppState app;

static void update_ui(void);
static void maybe_schedule_ai(void);
static void report_history_to_pikafish(void);
static void set_latest_view(void);
static void update_history_panel(void);
static void load_assets(void);
static void clear_assets(void);
static gboolean board_expose(GtkWidget *widget, GdkEventExpose *event, gpointer data);
static gboolean board_button(GtkWidget *widget, GdkEventButton *event, gpointer data);

static void toggle_history_cb(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;

    app.history_visible = !app.history_visible;
    if (app.history_visible) {
        gtk_widget_show(app.history_sidebar);
        gtk_button_set_label(GTK_BUTTON(app.history_toggle_button), "Hide Moves");
    } else {
        gtk_widget_hide(app.history_sidebar);
        gtk_button_set_label(GTK_BUTTON(app.history_toggle_button), "Show Moves");
    }
    gtk_widget_queue_resize(app.board);
    gtk_widget_queue_draw(app.board);
}

static void app_log(const char *message) {
    FILE *f = fopen(LOG_PATH, "a");
    if (!f) {
        return;
    }
    fprintf(f, "[app] %s\n", message);
    fclose(f);
}

static GdkPixbuf *normalize_piece_pixbuf(GdkPixbuf *src, XiangqiSide side) {
    GdkPixbuf *rgba;
    int width;
    int height;
    int stride;
    int channels;
    int x;
    int y;
    guchar *pixels;
    guchar bg_r;
    guchar bg_g;
    guchar bg_b;
    double cx;
    double cy;
    double radius;

    if (src == NULL) {
        return NULL;
    }

    if (gdk_pixbuf_get_has_alpha(src)) {
        rgba = gdk_pixbuf_copy(src);
    } else {
        const guchar *src_pixels = gdk_pixbuf_get_pixels(src);
        int src_channels = gdk_pixbuf_get_n_channels(src);
        bg_r = src_pixels[0];
        bg_g = src_channels > 1 ? src_pixels[1] : src_pixels[0];
        bg_b = src_channels > 2 ? src_pixels[2] : src_pixels[0];
        rgba = gdk_pixbuf_add_alpha(src, TRUE, bg_r, bg_g, bg_b);
    }

    if (rgba == NULL) {
        return NULL;
    }

    width = gdk_pixbuf_get_width(rgba);
    height = gdk_pixbuf_get_height(rgba);
    stride = gdk_pixbuf_get_rowstride(rgba);
    channels = gdk_pixbuf_get_n_channels(rgba);
    pixels = gdk_pixbuf_get_pixels(rgba);
    bg_r = pixels[0];
    bg_g = pixels[1];
    bg_b = pixels[2];
    cx = (width - 1) / 2.0;
    cy = (height - 1) / 2.0;
    radius = MIN(width, height) * 0.49;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            guchar *p = pixels + y * stride + x * channels;
            int dr = (int)p[0] - (int)bg_r;
            int dg = (int)p[1] - (int)bg_g;
            int db = (int)p[2] - (int)bg_b;
            double dx = x - cx;
            double dy = y - cy;
            gboolean outside_piece = dx * dx + dy * dy > radius * radius;
            gboolean background_color = dr * dr + dg * dg + db * db < 64;

            if (outside_piece || background_color) {
                p[3] = 0;
                continue;
            }

            if (side == XIANGQI_BLACK && p[3] != 0 &&
                p[0] > 178 && p[1] > 178 && p[2] > 178) {
                p[0] = 165;
                p[1] = 165;
                p[2] = 165;
            }

            if (side == XIANGQI_RED && p[3] != 0 &&
                p[0] > 160 && p[0] > p[1] + 12 && p[0] > p[2] + 12) {
                p[0] = 70;
                p[1] = 70;
                p[2] = 70;
            }
        }
    }

    return rgba;
}

static void set_message(const char *message) {
    g_strlcpy(app.message, message, sizeof(app.message));
}

static int current_view_ply(void) {
    if (app.view_ply < 0 || app.view_ply > app.game.history_len) {
        return app.game.history_len;
    }
    return app.view_ply;
}

static gboolean is_viewing_latest(void) {
    return current_view_ply() == app.game.history_len;
}

static void set_latest_view(void) {
    app.view_ply = -1;
}

static gboolean is_ai_turn(void) {
    if (app.game.game_over) {
        return FALSE;
    }
    switch (app.mode) {
    case MODE_PLAY_RED:
        return app.game.turn == XIANGQI_BLACK;
    case MODE_PLAY_BLACK:
        return app.game.turn == XIANGQI_RED;
    case MODE_TWO_PLAYER:
        return FALSE;
    case MODE_AI_DEMO:
        return TRUE;
    }
    return FALSE;
}

static gboolean is_human_turn(void) {
    if (app.game.game_over) {
        return FALSE;
    }
    if (!is_viewing_latest()) {
        return FALSE;
    }
    switch (app.mode) {
    case MODE_PLAY_RED:
        return app.game.turn == XIANGQI_RED;
    case MODE_PLAY_BLACK:
        return app.game.turn == XIANGQI_BLACK;
    case MODE_TWO_PLAYER:
        return TRUE;
    case MODE_AI_DEMO:
        return FALSE;
    }
    return FALSE;
}

static const char *piece_ascii_label(const XiangqiPiece *piece) {
    static const char *red[] = { "K", "A", "E", "H", "R", "C", "P" };
    static const char *black[] = { "k", "a", "e", "h", "r", "c", "p" };
    return piece->side == XIANGQI_RED ? red[piece->type] : black[piece->type];
}

static void format_move_label(const XiangqiGame *position, const XiangqiMove *move, char *buf, int len) {
    const XiangqiPiece *piece = &position->pieces[move->move_id];
    snprintf(buf, len, "%s %c%d-%c%d",
             piece_ascii_label(piece),
             'a' + move->from_col, 9 - move->from_row,
             'a' + move->to_col, 9 - move->to_row);
}

static void build_view_game(XiangqiGame *view, int ply) {
    int i;

    xiangqi_init(view);
    if (ply < 0 || ply > app.game.history_len) {
        ply = app.game.history_len;
    }
    for (i = 0; i < ply; i++) {
        XiangqiMove applied;
        xiangqi_apply_move(view,
                           app.game.history[i].move_id,
                           app.game.history[i].to_row,
                           app.game.history[i].to_col,
                           &applied);
    }
    view->selected_id = is_viewing_latest() ? app.game.selected_id : -1;
}

static void move_to_uci(const XiangqiMove *move, char *buf, int len) {
    if (len < 5) {
        return;
    }
    buf[0] = (char)('a' + move->from_col);
    buf[1] = (char)('0' + (9 - move->from_row));
    buf[2] = (char)('a' + move->to_col);
    buf[3] = (char)('0' + (9 - move->to_row));
    buf[4] = '\0';
}

static gboolean uci_to_move(const char *uci, int *move_id, int *row, int *col) {
    int from_col;
    int from_row;
    int to_col;
    int to_row;

    if (uci == NULL || strlen(uci) < 4) {
        return FALSE;
    }
    from_col = uci[0] - 'a';
    from_row = 9 - (uci[1] - '0');
    to_col = uci[2] - 'a';
    to_row = 9 - (uci[3] - '0');
    if (from_col < 0 || from_col >= XIANGQI_COLS ||
        to_col < 0 || to_col >= XIANGQI_COLS ||
        from_row < 0 || from_row >= XIANGQI_ROWS ||
        to_row < 0 || to_row >= XIANGQI_ROWS) {
        return FALSE;
    }
    *move_id = xiangqi_piece_at(&app.game, from_row, from_col);
    *row = to_row;
    *col = to_col;
    return *move_id >= 0;
}

static void app_apply_high_contrast(GtkWidget *widget) {
    GdkColor black = { 0, 0x0000, 0x0000, 0x0000 };
    GdkColor white = { 0, 0xffff, 0xffff, 0xffff };

    gtk_widget_modify_fg(widget, GTK_STATE_NORMAL, &black);
    gtk_widget_modify_fg(widget, GTK_STATE_ACTIVE, &black);
    gtk_widget_modify_fg(widget, GTK_STATE_SELECTED, &white);
    gtk_widget_modify_text(widget, GTK_STATE_NORMAL, &black);
    gtk_widget_modify_text(widget, GTK_STATE_SELECTED, &white);
    gtk_widget_modify_base(widget, GTK_STATE_NORMAL, &white);
    gtk_widget_modify_base(widget, GTK_STATE_SELECTED, &black);
    gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &white);
    gtk_widget_modify_bg(widget, GTK_STATE_SELECTED, &black);
}

static void app_install_kindle_style(void) {
    gtk_rc_parse_string(
        "style \"kindle_high_contrast\" {\n"
        "  fg[NORMAL] = \"#000000\"\n"
        "  fg[ACTIVE] = \"#000000\"\n"
        "  fg[PRELIGHT] = \"#ffffff\"\n"
        "  fg[SELECTED] = \"#ffffff\"\n"
        "  text[NORMAL] = \"#000000\"\n"
        "  text[ACTIVE] = \"#000000\"\n"
        "  text[SELECTED] = \"#ffffff\"\n"
        "  base[NORMAL] = \"#ffffff\"\n"
        "  base[ACTIVE] = \"#ffffff\"\n"
        "  base[SELECTED] = \"#000000\"\n"
        "  bg[NORMAL] = \"#ffffff\"\n"
        "  bg[ACTIVE] = \"#ffffff\"\n"
        "  bg[PRELIGHT] = \"#000000\"\n"
        "  bg[SELECTED] = \"#000000\"\n"
        "}\n"
        "gtk-button-images = 0\n"
        "gtk-menu-images = 0\n"
        "class \"GtkComboBox\" style \"kindle_high_contrast\"\n"
        "class \"GtkCellView\" style \"kindle_high_contrast\"\n"
        "class \"GtkMenu\" style \"kindle_high_contrast\"\n"
        "class \"GtkMenuItem\" style \"kindle_high_contrast\"\n"
        "widget_class \"*GtkComboBox*\" style \"kindle_high_contrast\"\n"
        "widget_class \"*GtkMenu*\" style \"kindle_high_contrast\"\n"
    );
}

static const char *mode_name(AppMode mode) {
    switch (mode) {
    case MODE_PLAY_RED:
        return "Play Red";
    case MODE_PLAY_BLACK:
        return "Play Black";
    case MODE_TWO_PLAYER:
        return "2 Player";
    case MODE_AI_DEMO:
        return "AI Demo";
    }
    return "Play Red";
}

static gboolean ai_move_cb(gpointer data) {
    XiangqiMove move;
    char label[64];
    (void)data;

    app.ai_source = 0;
    if (!is_ai_turn()) {
        return FALSE;
    }
    /* Mark engine unavailable if it died before we could use it. */
    if (app.pikafish_available && app.pikafish.dead)
        app.pikafish_available = FALSE;
    if (app.pikafish_available && !app.pikafish.ready) {
        app.ai_source = g_timeout_add(250, ai_move_cb, NULL);
        return FALSE;
    }
    if (app.pikafish_available && pikafish_uci_request_move(&app.pikafish)) {
        return FALSE;
    }
    if (xiangqi_choose_ai_move(&app.game, app.level, &move) &&
        xiangqi_apply_move(&app.game, move.move_id, move.to_row, move.to_col, &move)) {
        char uci[8];
        format_move_label(&app.game, &move, label, sizeof(label));
        move_to_uci(&move, uci, sizeof(uci));
        pikafish_uci_report_move(&app.pikafish, uci);
        set_message(label);
        set_latest_view();
    } else {
        app.game.game_over = true;
        app.game.winner = app.game.turn == XIANGQI_RED ? XIANGQI_BLACK : XIANGQI_RED;
    }
    update_ui();
    maybe_schedule_ai();
    return FALSE;
}

static void pikafish_bestmove_cb(const char *uci, gpointer user_data) {
    int move_id;
    int row;
    int col;
    XiangqiMove move;
    char label[96];
    (void)user_data;

    if (!is_ai_turn()) {
        return;
    }

    if (uci_to_move(uci, &move_id, &row, &col) &&
        xiangqi_apply_move(&app.game, move_id, row, col, &move)) {
        char applied_uci[8];
        move_to_uci(&move, applied_uci, sizeof(applied_uci));
        pikafish_uci_report_move(&app.pikafish, applied_uci);
        snprintf(label, sizeof(label), "Pikafish: %s", uci);
        set_message(label);
        set_latest_view();
    } else {
        app.pikafish_available = FALSE;
        if (xiangqi_choose_ai_move(&app.game, app.level, &move) &&
            xiangqi_apply_move(&app.game, move.move_id, move.to_row, move.to_col, &move)) {
            char fallback_uci[8];
            format_move_label(&app.game, &move, label, sizeof(label));
            move_to_uci(&move, fallback_uci, sizeof(fallback_uci));
            pikafish_uci_report_move(&app.pikafish, fallback_uci);
            set_message(label);
            set_latest_view();
        } else {
            app.game.game_over = true;
            app.game.winner = app.game.turn == XIANGQI_RED ? XIANGQI_BLACK : XIANGQI_RED;
        }
    }
    update_ui();
    maybe_schedule_ai();
}

static void maybe_schedule_ai(void) {
    if (app.ai_source != 0) {
        g_source_remove(app.ai_source);
        app.ai_source = 0;
    }
    if (is_ai_turn()) {
        set_message("AI thinking.");
        app.ai_source = g_timeout_add(250, ai_move_cb, NULL);
    }
}

static void new_game(void) {
    if (app.ai_source != 0) {
        g_source_remove(app.ai_source);
        app.ai_source = 0;
    }
    xiangqi_init(&app.game);
    pikafish_uci_start_game(&app.pikafish);
    set_latest_view();
    set_message("Red to move.");
    update_ui();
    maybe_schedule_ai();
}

static void new_game_cb(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    new_game();
}

static void undo_cb(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    if (app.ai_source != 0) {
        g_source_remove(app.ai_source);
        app.ai_source = 0;
    }
    if (xiangqi_undo(&app.game)) {
        if (app.mode == MODE_PLAY_RED || app.mode == MODE_PLAY_BLACK) {
            xiangqi_undo(&app.game);
        }
        set_message("Move undone.");
        report_history_to_pikafish();
        set_latest_view();
        update_ui();
    }
}

static void save_cb(GtkWidget *widget, gpointer data) {
    FILE *f;
    int i;
    (void)widget;
    (void)data;

    f = fopen(SAVE_PATH, "w");
    if (!f) {
        set_message("Save failed.");
        update_ui();
        return;
    }
    fprintf(f, "kindle-chinesechess 1\n%d\n", app.game.history_len);
    for (i = 0; i < app.game.history_len; i++) {
        XiangqiMove *m = &app.game.history[i];
        fprintf(f, "%d %d\n", m->move_id, m->to_row * 9 + m->to_col);
    }
    fclose(f);
    set_message("Game saved.");
    update_ui();
}

static void load_cb(GtkWidget *widget, gpointer data) {
    FILE *f;
    char header[64];
    int count;
    int i;
    (void)widget;
    (void)data;

    f = fopen(SAVE_PATH, "r");
    if (f == NULL)
        f = fopen(LEGACY_SAVE_PATH, "r");
    if (!f) {
        set_message("No saved game found.");
        update_ui();
        return;
    }
    if (fscanf(f, "%63s %*d\n%d\n", header, &count) != 2 || strcmp(header, "kindle-chinesechess") != 0) {
        fclose(f);
        set_message("Save file is invalid.");
        update_ui();
        return;
    }
    xiangqi_init(&app.game);
    for (i = 0; i < count; i++) {
        int id;
        int square;
        XiangqiMove move;
        if (fscanf(f, "%d %d\n", &id, &square) != 2 ||
            !xiangqi_apply_move(&app.game, id, square / 9, square % 9, &move)) {
            fclose(f);
            set_message("Save replay failed.");
            update_ui();
            return;
        }
    }
    fclose(f);
    set_message("Loaded saved game.");
    report_history_to_pikafish();
    set_latest_view();
    update_ui();
    maybe_schedule_ai();
}

static void mode_changed_cb(GtkComboBox *combo, gpointer data) {
    (void)data;
    app.mode = (AppMode)gtk_combo_box_get_active(combo);
    new_game();
}

static void level_changed_cb(GtkComboBox *combo, gpointer data) {
    (void)data;
    app.level = (XiangqiAiLevel)gtk_combo_box_get_active(combo);
    pikafish_uci_set_difficulty(&app.pikafish, app.level);
    maybe_schedule_ai();
    update_ui();
}

static void history_first_cb(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    app.view_ply = 0;
    update_ui();
}

static void history_prev_cb(GtkWidget *widget, gpointer data) {
    int ply;
    (void)widget;
    (void)data;

    ply = current_view_ply();
    if (ply > 0) {
        app.view_ply = ply - 1;
    }
    update_ui();
}

static void history_next_cb(GtkWidget *widget, gpointer data) {
    int ply;
    (void)widget;
    (void)data;

    ply = current_view_ply();
    if (ply < app.game.history_len) {
        app.view_ply = ply + 1;
    }
    update_ui();
}

static void history_latest_cb(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    set_latest_view();
    update_ui();
}

static gboolean quit_cb(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    gtk_main_quit();
    return TRUE;
}

static void report_history_to_pikafish(void) {
    int i;

    pikafish_uci_start_game(&app.pikafish);
    for (i = 0; i < app.game.history_len; i++) {
        char uci[8];
        move_to_uci(&app.game.history[i], uci, sizeof(uci));
        pikafish_uci_report_move(&app.pikafish, uci);
    }
}

static void update_history_panel(void) {
    GtkTextBuffer *buffer;
    GString *text;
    XiangqiGame replay;
    int i;
    int ply;

    if (app.history_view == NULL) {
        return;
    }

    text = g_string_new("");
    xiangqi_init(&replay);
    ply = current_view_ply();
    for (i = 0; i < app.game.history_len; i++) {
        char label[64];
        XiangqiMove move = app.game.history[i];
        format_move_label(&replay, &move, label, sizeof(label));
        g_string_append_printf(text, "%c%3d. %s\n", i + 1 == ply ? '>' : ' ', i + 1, label);
        xiangqi_apply_move(&replay, move.move_id, move.to_row, move.to_col, NULL);
    }
    if (app.game.history_len == 0) {
        g_string_append(text, "No moves yet.\n");
    }

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app.history_view));
    gtk_text_buffer_set_text(buffer, text->str, -1);
    g_string_free(text, TRUE);

    gtk_widget_set_sensitive(app.history_first_button, ply > 0);
    gtk_widget_set_sensitive(app.history_prev_button, ply > 0);
    gtk_widget_set_sensitive(app.history_next_button, ply < app.game.history_len);
    gtk_widget_set_sensitive(app.history_latest_button, ply < app.game.history_len);
}

static void load_assets(void) {
    int side;
    int type;
    GError *error = NULL;
    const char *prefix = "assets/xiangqi";
    static const char *piece_names[] = { "k", "a", "b", "n", "r", "c", "p" };

    app.board_image = gdk_pixbuf_new_from_file("assets/xiangqi/board.png", &error);
    if (app.board_image == NULL && error != NULL) {
        app_log(error->message);
        g_error_free(error);
        error = NULL;
    }

    for (side = 0; side < 2; side++) {
        for (type = 0; type < 7; type++) {
            char path[128];
            snprintf(path, sizeof(path), "%s/%c_%s.png", prefix, side == XIANGQI_RED ? 'r' : 'b', piece_names[type]);
            GdkPixbuf *loaded = gdk_pixbuf_new_from_file(path, &error);
            if (loaded != NULL) {
                app.piece_images[side][type] = normalize_piece_pixbuf(loaded, side);
                g_object_unref(loaded);
            }
            if (app.piece_images[side][type] == NULL && error != NULL) {
                app_log(error->message);
                g_error_free(error);
                error = NULL;
            }
        }
    }
}

static void clear_assets(void) {
    int side;
    int type;

    if (app.board_image != NULL) {
        g_object_unref(app.board_image);
        app.board_image = NULL;
    }
    for (side = 0; side < 2; side++) {
        for (type = 0; type < 7; type++) {
            if (app.piece_images[side][type] != NULL) {
                g_object_unref(app.piece_images[side][type]);
                app.piece_images[side][type] = NULL;
            }
        }
    }
}

static void board_geometry(GtkWidget *widget, double *left, double *top, double *cell) {
    GtkAllocation a;
    double board_w;
    double board_h;
    double usable_w;
    double usable_h;

    gtk_widget_get_allocation(widget, &a);
    /* Piece radius = 0.42*cell; board margin must exceed it.
     * With factor f: margin = (1-f)/2 * W, cell = f*W/8.
     * Need margin > piece_radius → (1-f)/2 > 0.42*f/8 → f < 0.905.
     * Use 0.84 to give ~25px clear on a 700px-wide board widget. */
    usable_w = a.width * 0.84;
    usable_h = a.height * 0.84;
    *cell = fmin(usable_w / 8.0, usable_h / 9.0);
    board_w = *cell * 8.0;
    board_h = *cell * 9.0;
    *left = (a.width - board_w) / 2.0;
    *top = (a.height - board_h) / 2.0;
}

static void draw_centered_text(cairo_t *cr, const char *text, double x, double y, double size, gboolean bold) {
    cairo_text_extents_t extents;

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    cairo_text_extents(cr, text, &extents);
    cairo_move_to(cr, x - extents.width / 2.0 - extents.x_bearing, y - extents.height / 2.0 - extents.y_bearing);
    cairo_show_text(cr, text);
}

static void draw_star(cairo_t *cr, double x, double y, double s) {
    cairo_move_to(cr, x - s, y);
    cairo_line_to(cr, x + s, y);
    cairo_move_to(cr, x, y - s);
    cairo_line_to(cr, x, y + s);
}

static void draw_cell_highlight(cairo_t *cr, double left, double top, double cell, int row, int col, gboolean selected) {
    double x = left + col * cell;
    double y = top + row * cell;
    double r = cell * 0.38;

    if (row < 0 || row >= XIANGQI_ROWS || col < 0 || col >= XIANGQI_COLS) {
        return;
    }

    cairo_save(cr);
    cairo_new_path(cr);
    cairo_arc(cr, x, y, r, 0, 2 * G_PI);
    if (selected) {
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.26);
    } else {
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.14);
    }
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, selected ? 0.95 : 0.65);
    cairo_set_line_width(cr, selected ? 4.0 : 2.6);
    cairo_stroke(cr);
    cairo_restore(cr);
}

static void draw_cell_focus_ring(cairo_t *cr, double left, double top, double cell, int row, int col, gboolean current) {
    double x = left + col * cell;
    double y = top + row * cell;
    double r = cell * (current ? 0.49 : 0.44);

    if (row < 0 || row >= XIANGQI_ROWS || col < 0 || col >= XIANGQI_COLS) {
        return;
    }

    cairo_save(cr);
    cairo_new_path(cr);
    cairo_arc(cr, x, y, r, 0, 2 * G_PI);
    cairo_set_source_rgb(cr, current ? 0.0 : 1.0, current ? 0.0 : 1.0, current ? 0.0 : 1.0);
    cairo_set_line_width(cr, current ? cell * 0.075 : cell * 0.045);
    cairo_stroke_preserve(cr);
    cairo_set_source_rgb(cr, current ? 1.0 : 0.0, current ? 1.0 : 0.0, current ? 1.0 : 0.0);
    cairo_set_line_width(cr, current ? cell * 0.025 : cell * 0.02);
    cairo_stroke(cr);
    cairo_restore(cr);
}

static gboolean board_expose(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
    cairo_t *cr;
    XiangqiGame view;
    double left;
    double top;
    double cell;
    int row;
    int col;
    int i;
    int ply;
    (void)event;
    (void)data;

    cr = gdk_cairo_create(widget->window);
    ply = current_view_ply();
    build_view_game(&view, ply);
    board_geometry(widget, &left, &top, &cell);

    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);

    if (app.board_image != NULL) {
        double image_left = left - cell * 0.5;
        double image_top = top - cell * 0.5;
        double scale = (cell * 9.0) / gdk_pixbuf_get_width(app.board_image);

        cairo_save(cr);
        cairo_translate(cr, image_left, image_top);
        cairo_scale(cr, scale, scale);
        gdk_cairo_set_source_pixbuf(cr, app.board_image, 0, 0);
        cairo_rectangle(cr, 0, 0, gdk_pixbuf_get_width(app.board_image), gdk_pixbuf_get_height(app.board_image));
        cairo_fill(cr);
        cairo_restore(cr);
    } else {
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_set_line_width(cr, 2.0);

        cairo_new_path(cr);
        for (row = 0; row < XIANGQI_ROWS; row++) {
            double y = top + row * cell;
            cairo_move_to(cr, left, y);
            cairo_line_to(cr, left + 8 * cell, y);
        }
        for (col = 0; col < XIANGQI_COLS; col++) {
            double x = left + col * cell;
            cairo_move_to(cr, x, top);
            cairo_line_to(cr, x, top + 4 * cell);
            cairo_move_to(cr, x, top + 5 * cell);
            cairo_line_to(cr, x, top + 9 * cell);
        }
        cairo_rectangle(cr, left, top, 8 * cell, 9 * cell);
        cairo_move_to(cr, left + 3 * cell, top);
        cairo_line_to(cr, left + 5 * cell, top + 2 * cell);
        cairo_move_to(cr, left + 5 * cell, top);
        cairo_line_to(cr, left + 3 * cell, top + 2 * cell);
        cairo_move_to(cr, left + 3 * cell, top + 7 * cell);
        cairo_line_to(cr, left + 5 * cell, top + 9 * cell);
        cairo_move_to(cr, left + 5 * cell, top + 7 * cell);
        cairo_line_to(cr, left + 3 * cell, top + 9 * cell);
        cairo_stroke(cr);

        cairo_new_path(cr);
        cairo_set_line_width(cr, 1.5);
        draw_star(cr, left + 1 * cell, top + 2 * cell, cell * 0.08);
        draw_star(cr, left + 7 * cell, top + 2 * cell, cell * 0.08);
        for (col = 0; col <= 8; col += 2) {
            draw_star(cr, left + col * cell, top + 3 * cell, cell * 0.08);
            draw_star(cr, left + col * cell, top + 6 * cell, cell * 0.08);
        }
        cairo_stroke(cr);

        cairo_set_source_rgb(cr, 0, 0, 0);
        draw_centered_text(cr, "RIVER", left + 2.5 * cell, top + 4.5 * cell, cell * 0.22, TRUE);
        draw_centered_text(cr, "BORDER", left + 5.5 * cell, top + 4.5 * cell, cell * 0.22, TRUE);
    }

    if (ply > 0) {
        XiangqiMove last = app.game.history[ply - 1];
        draw_cell_highlight(cr, left, top, cell, last.from_row, last.from_col, FALSE);
        draw_cell_highlight(cr, left, top, cell, last.to_row, last.to_col, FALSE);
    }
    if (view.selected_id >= 0 && view.selected_id < XIANGQI_PIECES && !view.pieces[view.selected_id].dead) {
        XiangqiPiece *selected = &view.pieces[view.selected_id];
        draw_cell_highlight(cr, left, top, cell, selected->row, selected->col, TRUE);
    }

    for (i = 0; i < XIANGQI_PIECES; i++) {
        XiangqiPiece *p = &view.pieces[i];
        double x;
        double y;
        double r;

        if (p->dead) {
            continue;
        }
        x = left + p->col * cell;
        y = top + p->row * cell;
        r = cell * 0.38;

        if (app.piece_images[p->side][p->type] != NULL) {
            double size = cell * 0.84;
            double img_w = gdk_pixbuf_get_width(app.piece_images[p->side][p->type]);
            double scale = size / img_w;

            cairo_save(cr);
            cairo_new_path(cr);
            cairo_arc(cr, x, y, size * 0.48, 0, 2 * G_PI);
            if (p->side == XIANGQI_BLACK) {
                cairo_set_source_rgb(cr, 0.64, 0.64, 0.64);
            } else {
                cairo_set_source_rgb(cr, 1, 1, 1);
            }
            cairo_fill_preserve(cr);
            cairo_clip(cr);
            cairo_translate(cr, x - size * 0.5, y - size * 0.5);
            cairo_scale(cr, scale, scale);
            gdk_cairo_set_source_pixbuf(cr, app.piece_images[p->side][p->type], 0, 0);
            cairo_paint(cr);
            cairo_restore(cr);
            /* Red pieces use red ink which e-ink renders as light grey — darken
             * with a semi-transparent black ring so they stay legible. */
            if (p->side == XIANGQI_RED) {
                cairo_new_path(cr);
                cairo_arc(cr, x, y, size * 0.48, 0, 2 * G_PI);
                cairo_set_source_rgba(cr, 0, 0, 0, 0.45);
                cairo_set_line_width(cr, size * 0.06);
                cairo_stroke(cr);
            }
        } else {
            cairo_new_path(cr);
            cairo_arc(cr, x, y, r, 0, 2 * G_PI);
            cairo_set_source_rgb(cr, 1, 1, 1);
            cairo_fill_preserve(cr);
            if (view.selected_id == i || p->side == XIANGQI_RED) {
                cairo_set_source_rgb(cr, 0, 0, 0);
                cairo_set_line_width(cr, view.selected_id == i ? 5.0 : 2.0);
                cairo_stroke(cr);
            } else {
                cairo_new_path(cr);
            }

            if (p->side == XIANGQI_RED) {
                cairo_new_path(cr);
                cairo_arc(cr, x, y, r * 0.78, 0, 2 * G_PI);
                cairo_set_line_width(cr, 1.5);
                cairo_stroke(cr);
            }

            cairo_set_source_rgb(cr, 0, 0, 0);
            draw_centered_text(cr, piece_ascii_label(p), x, y, cell * 0.42, TRUE);
        }

        if (view.selected_id == i && app.piece_images[p->side][p->type] != NULL) {
            cairo_new_path(cr);
            cairo_arc(cr, x, y, cell * 0.42, 0, 2 * G_PI);
            cairo_set_source_rgb(cr, 0, 0, 0);
            cairo_set_line_width(cr, 4.0);
            cairo_stroke(cr);
        }
    }

    if (ply > 0) {
        XiangqiMove last = app.game.history[ply - 1];
        draw_cell_focus_ring(cr, left, top, cell, last.from_row, last.from_col, FALSE);
        draw_cell_focus_ring(cr, left, top, cell, last.to_row, last.to_col, TRUE);
    }
    if (view.selected_id >= 0 && view.selected_id < XIANGQI_PIECES && !view.pieces[view.selected_id].dead) {
        XiangqiPiece *selected = &view.pieces[view.selected_id];
        draw_cell_focus_ring(cr, left, top, cell, selected->row, selected->col, TRUE);
    }

    cairo_destroy(cr);
    return FALSE;
}

static gboolean board_button(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    double left;
    double top;
    double cell;
    int row;
    int col;
    int id;
    (void)data;

    if (event->button != 1 || !is_human_turn()) {
        if (!is_viewing_latest()) {
            set_message("Reviewing history. Tap >> to resume.");
            update_ui();
        }
        return TRUE;
    }

    board_geometry(widget, &left, &top, &cell);
    col = (int)floor((event->x - left + cell / 2.0) / cell);
    row = (int)floor((event->y - top + cell / 2.0) / cell);
    if (row < 0 || row >= XIANGQI_ROWS || col < 0 || col >= XIANGQI_COLS) {
        return TRUE;
    }

    id = xiangqi_piece_at(&app.game, row, col);
    if (app.game.selected_id < 0) {
        if (id != -1 && app.game.pieces[id].side == app.game.turn) {
            app.game.selected_id = id;
            set_message("Piece selected.");
        }
    } else {
        XiangqiMove move;
        if (id != -1 && app.game.pieces[id].side == app.game.turn) {
            app.game.selected_id = id;
            set_message("Piece selected.");
        } else if (xiangqi_apply_move(&app.game, app.game.selected_id, row, col, &move)) {
            char label[64];
            char uci[8];
            format_move_label(&app.game, &move, label, sizeof(label));
            move_to_uci(&move, uci, sizeof(uci));
            pikafish_uci_report_move(&app.pikafish, uci);
            set_message(label);
            app.game.selected_id = -1;
            set_latest_view();
            maybe_schedule_ai();
        } else {
            set_message("Illegal move.");
        }
    }
    update_ui();
    return TRUE;
}

static void update_ui(void) {
    char status[256];
    char moves[128];

    if (app.game.game_over) {
        snprintf(status, sizeof(status), "%s wins. %s", xiangqi_side_name(app.game.winner), app.message);
    } else if (is_ai_turn()) {
        snprintf(status, sizeof(status), "%s to move. %s thinking. %s",
                 xiangqi_side_name(app.game.turn),
                 app.pikafish_available ? "Pikafish" : "AI",
                 mode_name(app.mode));
    } else {
        snprintf(status, sizeof(status), "%s to move. %s", xiangqi_side_name(app.game.turn), app.message);
    }
    snprintf(moves, sizeof(moves), "Moves: %d  Engine: %s",
             app.game.history_len,
             app.pikafish_available ? "Pikafish" : "Built-in AI");
    if (!is_viewing_latest()) {
        char review[128];
        snprintf(review, sizeof(review), "Reviewing move %d/%d.", current_view_ply(), app.game.history_len);
        gtk_label_set_text(GTK_LABEL(app.status), review);
    } else {
        gtk_label_set_text(GTK_LABEL(app.status), status);
    }
    gtk_label_set_text(GTK_LABEL(app.moves_label), moves);
    update_history_panel();
    gtk_widget_queue_draw(app.board);
}

static gboolean combo_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)data;

    if (event->type != GDK_BUTTON_PRESS || event->button != 1) {
        return FALSE;
    }

    gtk_widget_grab_focus(widget);
    gtk_combo_box_popup(GTK_COMBO_BOX(widget));
    return TRUE;
}

static GtkWidget *make_combo(const char **items, int count, int active) {
    GtkWidget *combo = gtk_combo_box_new_text();
    int i;
    for (i = 0; i < count; i++) {
        gtk_combo_box_append_text(GTK_COMBO_BOX(combo), items[i]);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active);
    app_apply_high_contrast(combo);
    gtk_widget_add_events(combo, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(combo, "button-press-event", G_CALLBACK(combo_button_press_cb), NULL);
    return combo;
}

static void add_button(GtkWidget *box, const char *label, GCallback callback) {
    GtkWidget *button = gtk_button_new_with_label(label);
    gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);
    g_signal_connect(button, "clicked", callback, NULL);
}

static GtkWidget *add_history_button(GtkWidget *box, const char *label, GCallback callback) {
    GtkWidget *button = gtk_button_new_with_label(label);
    gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);
    g_signal_connect(button, "clicked", callback, NULL);
    return button;
}

static GtkWidget *labeled_combo(GtkWidget *row, const char *label_text, GtkWidget *combo) {
    GtkWidget *box = gtk_hbox_new(FALSE, 4);
    GtkWidget *label = gtk_label_new(label_text);
    gtk_box_pack_start(GTK_BOX(row), box, TRUE, TRUE, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0f, 0.5f);
    gtk_widget_set_size_request(label, 80, -1);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), combo, TRUE, TRUE, 0);
    return combo;
}

int main(int argc, char **argv) {
    GtkWidget *vbox;
    GtkWidget *title;
    GtkWidget *controls;
    GtkWidget *settings;
    GtkWidget *content;
    GtkWidget *side_panel;
    GtkWidget *history_frame;
    GtkWidget *history_scroll;
    GtkWidget *history_nav;
    const char *modes[] = { "Play Red", "Play Black", "2 Player", "AI Demo" };
    const char *levels[] = { "Easy", "Medium", "Hard" };

    gtk_init(&argc, &argv);
    app_install_kindle_style();
    app.mode = MODE_PLAY_RED;
    app.level = XIANGQI_AI_MEDIUM;
    set_latest_view();
    xiangqi_init(&app.game);
    pikafish_uci_init(&app.pikafish,
                      g_getenv("KINDLE_CHINESECHESS_PIKAFISH") != NULL ?
                      g_getenv("KINDLE_CHINESECHESS_PIKAFISH") :
                      DEFAULT_PIKAFISH_PATH);
    pikafish_uci_set_move_callback(&app.pikafish, pikafish_bestmove_cb, NULL);
    load_assets();
    set_message("Red to move.");

    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.window), kindle_window_title());
    gtk_window_set_default_size(GTK_WINDOW(app.window), KINDLE_APP_WIDTH, KINDLE_APP_HEIGHT);
    gtk_window_set_resizable(GTK_WINDOW(app.window), TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(app.window), 8);
    g_signal_connect(app.window, "delete-event", G_CALLBACK(quit_cb), NULL);

    vbox = gtk_vbox_new(FALSE, 8);
    gtk_container_add(GTK_CONTAINER(app.window), vbox);

    title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<b>Kindle ChineseChess</b>");
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    app.status = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(app.status), 0.5, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), app.status, FALSE, FALSE, 0);

    controls = gtk_hbox_new(TRUE, 8);
    gtk_box_pack_start(GTK_BOX(vbox), controls, FALSE, FALSE, 0);
    add_button(controls, "New", G_CALLBACK(new_game_cb));
    add_button(controls, "Undo", G_CALLBACK(undo_cb));
    add_button(controls, "Save", G_CALLBACK(save_cb));
    add_button(controls, "Load", G_CALLBACK(load_cb));
    add_button(controls, "Quit", G_CALLBACK(quit_cb));

    settings = gtk_hbox_new(FALSE, 12);
    gtk_box_pack_start(GTK_BOX(vbox), settings, FALSE, FALSE, 0);
    app.mode_combo = make_combo(modes, 4, app.mode);
    labeled_combo(settings, "Mode", app.mode_combo);
    app.level_combo = make_combo(levels, 3, app.level);
    labeled_combo(settings, "Level", app.level_combo);
    app.moves_label = gtk_label_new("Moves: 0");
    gtk_box_pack_start(GTK_BOX(settings), app.moves_label, FALSE, FALSE, 8);
    app.history_toggle_button = gtk_button_new_with_label("Hide Moves");
    gtk_box_pack_start(GTK_BOX(settings), app.history_toggle_button, FALSE, FALSE, 0);
    g_signal_connect(app.history_toggle_button, "clicked", G_CALLBACK(toggle_history_cb), NULL);

    content = gtk_hbox_new(FALSE, 12);
    gtk_box_pack_start(GTK_BOX(vbox), content, TRUE, TRUE, 0);

    app.board = gtk_drawing_area_new();
    gtk_widget_set_size_request(app.board, 720, 980);
    gtk_box_pack_start(GTK_BOX(content), app.board, TRUE, TRUE, 0);
    gtk_widget_add_events(app.board, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(app.board, "expose-event", G_CALLBACK(board_expose), NULL);
    g_signal_connect(app.board, "button-press-event", G_CALLBACK(board_button), NULL);

    side_panel = gtk_vbox_new(FALSE, 8);
    app.history_sidebar = side_panel;
    app.history_visible = TRUE;
    gtk_widget_set_size_request(side_panel, 260, 980);
    gtk_box_pack_start(GTK_BOX(content), side_panel, FALSE, FALSE, 0);

    history_frame = gtk_frame_new("Moves");
    gtk_box_pack_start(GTK_BOX(side_panel), history_frame, TRUE, TRUE, 0);
    history_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(history_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(history_frame), history_scroll);
    app.history_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app.history_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app.history_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app.history_view), GTK_WRAP_NONE);
    gtk_container_add(GTK_CONTAINER(history_scroll), app.history_view);
    app_apply_high_contrast(app.history_view);

    history_nav = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(side_panel), history_nav, FALSE, FALSE, 0);
    app.history_first_button = add_history_button(history_nav, "|<", G_CALLBACK(history_first_cb));
    app.history_prev_button = add_history_button(history_nav, "<", G_CALLBACK(history_prev_cb));
    app.history_next_button = add_history_button(history_nav, ">", G_CALLBACK(history_next_cb));
    app.history_latest_button = add_history_button(history_nav, ">|", G_CALLBACK(history_latest_cb));

    g_signal_connect(app.mode_combo, "changed", G_CALLBACK(mode_changed_cb), NULL);
    g_signal_connect(app.level_combo, "changed", G_CALLBACK(level_changed_cb), NULL);

    app_log("starting kindle-chinesechess");
    {
        GError *error = NULL;
        app.pikafish_available = pikafish_uci_start(&app.pikafish, &error);
        if (app.pikafish_available) {
            app_log("Pikafish started");
        } else {
            if (error != NULL) {
                app_log(error->message);
                g_error_free(error);
            }
            app_log("Pikafish unavailable; using embedded AI");
        }
    }
    update_ui();
    gtk_widget_show_all(app.window);
    gtk_window_present(GTK_WINDOW(app.window));
    maybe_schedule_ai();
    gtk_main();
    pikafish_uci_clear(&app.pikafish);
    clear_assets();
    return 0;
}
