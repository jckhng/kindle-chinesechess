/*
 * Kindle ChineseChess Pikafish UCI adapter
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PIKAFISH_UCI_H
#define PIKAFISH_UCI_H

#include <glib.h>

typedef void (*PikafishMoveCallback)(const char *move, gpointer user_data);

typedef struct {
    gchar *binary;
    GPid pid;
    gint stdin_fd;
    GIOChannel *stdout_channel;
    guint stdout_watch_id;
    GString *buffer;
    GString *moves;
    gboolean ready;
    gboolean waiting_for_move;
    gboolean dead;        /* set TRUE when the engine process exits */
    guint watchdog_id;    /* fallback timer armed while waiting for bestmove */
    guint child_watch_id; /* monitors child exit for diagnostic logging */
    gint movetime_ms;
    gint search_depth;    /* 0 means use movetime instead */
    gint watchdog_ms;
    PikafishMoveCallback move_callback;
    gpointer move_callback_data;
} PikafishUci;

void pikafish_uci_init(PikafishUci *engine, const char *binary);
gboolean pikafish_uci_start(PikafishUci *engine, GError **error);
void pikafish_uci_set_move_callback(PikafishUci *engine, PikafishMoveCallback callback, gpointer user_data);
void pikafish_uci_set_difficulty(PikafishUci *engine, gint preset);
void pikafish_uci_start_game(PikafishUci *engine);
void pikafish_uci_report_move(PikafishUci *engine, const char *move);
gboolean pikafish_uci_request_move(PikafishUci *engine);
void pikafish_uci_stop(PikafishUci *engine);
void pikafish_uci_clear(PikafishUci *engine);

#endif
