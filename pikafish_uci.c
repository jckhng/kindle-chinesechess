/*
 * Exact Chinese Chess Pikafish UCI adapter
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pikafish_uci.h"

#include <signal.h>
#include <string.h>
#include <unistd.h>

static void write_line(PikafishUci *engine, const char *line) {
    gchar *message;
    gsize length;

    if (engine->stdin_fd < 0 || line == NULL) {
        return;
    }
    g_printerr("[pikafish->] %s\n", line);
    message = g_strdup_printf("%s\n", line);
    length = strlen(message);
    if (write(engine->stdin_fd, message, length) < 0) {
    }
    g_free(message);
}

static void free_gstring(gpointer data) {
    if (data != NULL) {
        g_string_free((GString *)data, TRUE);
    }
}

static void configure_engine(PikafishUci *engine) {
    write_line(engine, "setoption name Threads value 1");
    write_line(engine, "setoption name Hash value 16");
    write_line(engine, "setoption name Ponder value false");
    write_line(engine, "isready");
}

static void pikafish_child_exited(GPid pid, gint status, gpointer user_data) {
    PikafishUci *engine = (PikafishUci *)user_data;
    engine->child_watch_id = 0;
    if (WIFSIGNALED(status))
        g_printerr("[pikafish] killed by signal %d\n", WTERMSIG(status));
    else if (WIFEXITED(status))
        g_printerr("[pikafish] exited with code %d\n", WEXITSTATUS(status));
    g_spawn_close_pid(pid);
}

static void cancel_watchdog(PikafishUci *engine) {
    if (engine->watchdog_id != 0) {
        g_source_remove(engine->watchdog_id);
        engine->watchdog_id = 0;
    }
}

static gboolean pikafish_watchdog_cb(gpointer user_data) {
    PikafishUci *engine = (PikafishUci *)user_data;
    engine->watchdog_id = 0;
    if (!engine->waiting_for_move)
        return FALSE;
    g_printerr("[pikafish] watchdog fired — no bestmove in time, triggering fallback\n");
    engine->waiting_for_move = FALSE;
    engine->dead = TRUE;
    if (engine->move_callback != NULL)
        engine->move_callback(NULL, engine->move_callback_data);
    return FALSE;
}

static void process_line(PikafishUci *engine, const char *line) {
    g_printerr("[pikafish<-] %s\n", line);

    if (g_str_has_prefix(line, "uciok")) {
        configure_engine(engine);
        return;
    }
    if (g_str_has_prefix(line, "readyok")) {
        engine->ready = TRUE;
        return;
    }
    if (g_str_has_prefix(line, "bestmove ")) {
        gchar **tokens = g_strsplit(line, " ", 0);
        cancel_watchdog(engine);
        engine->waiting_for_move = FALSE;
        if (tokens[1] != NULL && strcmp(tokens[1], "(none)") != 0 && engine->move_callback != NULL) {
            engine->move_callback(tokens[1], engine->move_callback_data);
        }
        g_strfreev(tokens);
    }
}

static void parse_buffer(PikafishUci *engine) {
    while (TRUE) {
        gchar *newline = strchr(engine->buffer->str, '\n');
        gchar *line;
        if (newline == NULL)
            break;
        *newline = '\0';
        line = g_strdup(engine->buffer->str);
        g_string_erase(engine->buffer, 0, (newline - engine->buffer->str) + 1);
        g_strstrip(line);
        if (line[0] != '\0')
            process_line(engine, line);
        g_free(line);
    }
}

static gboolean read_cb(GIOChannel *source, GIOCondition condition, gpointer user_data) {
    PikafishUci *engine = (PikafishUci *)user_data;
    gchar chunk[512];
    gsize bytes_read;
    GIOStatus status;
    gboolean got_hup = !!(condition & (G_IO_HUP | G_IO_ERR | G_IO_NVAL));

    /* Always drain all available data before handling HUP.  When the process
     * exits, GLib may deliver G_IO_IN|G_IO_HUP together.  Checking HUP first
     * would discard all buffered output (e.g. the full UCI option block). */
    do {
        bytes_read = 0;
        status = g_io_channel_read_chars(source, chunk, sizeof(chunk) - 1, &bytes_read, NULL);
        if (status == G_IO_STATUS_NORMAL && bytes_read > 0) {
            chunk[bytes_read] = '\0';
            g_string_append_len(engine->buffer, chunk, bytes_read);
            parse_buffer(engine);
        }
    } while (status == G_IO_STATUS_NORMAL && bytes_read > 0);

    if (status == G_IO_STATUS_EOF || got_hup) {
        gboolean was_waiting = engine->waiting_for_move;
        cancel_watchdog(engine);
        engine->ready = FALSE;
        engine->waiting_for_move = FALSE;
        engine->dead = TRUE;
        g_printerr("[pikafish] engine process died (HUP/ERR)\n");
        if (was_waiting && engine->move_callback != NULL)
            engine->move_callback(NULL, engine->move_callback_data);
        return FALSE;
    }

    return TRUE;
}

void pikafish_uci_init(PikafishUci *engine, const char *binary) {
    memset(engine, 0, sizeof(*engine));
    engine->binary = g_strdup(binary);
    engine->stdin_fd = -1;
    engine->buffer = g_string_new("");
    engine->moves = g_string_new("");
    pikafish_uci_set_difficulty(engine, 1);
}

gboolean pikafish_uci_start(PikafishUci *engine, GError **error) {
    gchar *argv[2];
    gint stdout_fd = -1;
    gchar **envp = NULL;
    gchar *bin_dir = NULL;

    /* Pikafish is built with max GLIBC_2.17 and statically linked libm/libstdc++/libgcc.
     * It only needs the Kindle's system glibc (libpthread, librt, libc).  Pointing
     * LD_LIBRARY_PATH at our bundled Docker libs would load a mismatched glibc and
     * crash immediately.  Explicitly unset it so the system linker uses its own paths.
     *
     * Set the working directory to the binary's dir so Pikafish finds pikafish.nnue
     * next to the binary (relative-path lookup used by Pikafish's NNUE loader). */
    bin_dir = g_path_get_dirname(engine->binary);
    envp = g_get_environ();
    envp = g_environ_unsetenv(envp, "LD_LIBRARY_PATH");
    g_printerr("[pikafish] starting in dir: %s\n", bin_dir);

    argv[0] = engine->binary;
    argv[1] = NULL;
    if (!g_spawn_async_with_pipes(bin_dir,
                                  argv,
                                  envp,
                                  G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                                  NULL,
                                  NULL,
                                  &engine->pid,
                                  &engine->stdin_fd,
                                  &stdout_fd,
                                  NULL,   /* inherit parent stderr → goes to log file */
                                  error)) {
        g_strfreev(envp);
        g_free(bin_dir);
        return FALSE;
    }
    g_strfreev(envp);
    g_free(bin_dir);
    engine->child_watch_id = g_child_watch_add(engine->pid, pikafish_child_exited, engine);
    engine->stdout_channel = g_io_channel_unix_new(stdout_fd);
    g_io_channel_set_encoding(engine->stdout_channel, NULL, NULL);
    g_io_channel_set_flags(engine->stdout_channel, G_IO_FLAG_NONBLOCK, NULL);
    engine->stdout_watch_id = g_io_add_watch(engine->stdout_channel, G_IO_IN | G_IO_HUP | G_IO_ERR, read_cb, engine);
    write_line(engine, "uci");
    return TRUE;
}

void pikafish_uci_set_move_callback(PikafishUci *engine, PikafishMoveCallback callback, gpointer user_data) {
    engine->move_callback = callback;
    engine->move_callback_data = user_data;
}

void pikafish_uci_set_difficulty(PikafishUci *engine, gint preset) {
    switch (preset) {
    case 0:
        /* Kid-friendly: shallow one-ply search. Pikafish does not expose
         * UCI_Elo/UCI_LimitStrength, so search limits are the reliable knob. */
        engine->search_depth = 1;
        engine->movetime_ms = 120;
        engine->watchdog_ms = 2000;
        break;
    case 2:
        engine->search_depth = 0;
        engine->movetime_ms = 1400;
        engine->watchdog_ms = engine->movetime_ms;
        break;
    case 1:
    default:
        engine->search_depth = 2;
        engine->movetime_ms = 300;
        engine->watchdog_ms = 3000;
        break;
    }
}

void pikafish_uci_start_game(PikafishUci *engine) {
    g_string_assign(engine->moves, "");
    engine->waiting_for_move = FALSE;
    write_line(engine, "ucinewgame");
    write_line(engine, "isready");
}

void pikafish_uci_report_move(PikafishUci *engine, const char *move) {
    if (move == NULL || move[0] == '\0') {
        return;
    }
    if (engine->moves->len > 0) {
        g_string_append_c(engine->moves, ' ');
    }
    g_string_append(engine->moves, move);
}

gboolean pikafish_uci_request_move(PikafishUci *engine) {
    gchar *position;
    gchar *go_line;

    if (engine->dead || !engine->ready || engine->waiting_for_move) {
        return FALSE;
    }
    position = engine->moves->len > 0 ? g_strdup_printf("position startpos moves %s", engine->moves->str)
                                      : g_strdup("position startpos");
    write_line(engine, position);
    g_free(position);
    engine->waiting_for_move = TRUE;
    if (engine->search_depth > 0) {
        go_line = g_strdup_printf("go depth %d", engine->search_depth);
    } else {
        go_line = g_strdup_printf("go movetime %d", engine->movetime_ms);
    }
    write_line(engine, go_line);
    g_free(go_line);

    /* Arm a watchdog: requested search budget + 8 s grace period before fallback. */
    cancel_watchdog(engine);
    engine->watchdog_id = g_timeout_add(engine->watchdog_ms + 8000, pikafish_watchdog_cb, engine);

    return TRUE;
}

void pikafish_uci_stop(PikafishUci *engine) {
    cancel_watchdog(engine);
    if (engine->child_watch_id != 0) {
        g_source_remove(engine->child_watch_id);
        engine->child_watch_id = 0;
    }
    if (engine->stdin_fd >= 0) {
        write_line(engine, "quit");
        close(engine->stdin_fd);
        engine->stdin_fd = -1;
    }
    if (engine->stdout_watch_id != 0) {
        g_source_remove(engine->stdout_watch_id);
        engine->stdout_watch_id = 0;
    }
    if (engine->stdout_channel != NULL) {
        g_io_channel_shutdown(engine->stdout_channel, TRUE, NULL);
        g_io_channel_unref(engine->stdout_channel);
        engine->stdout_channel = NULL;
    }
    if (engine->pid != 0) {
        kill(engine->pid, SIGTERM);
        /* child_watch_id already cancelled above; close pid ourselves. */
        g_spawn_close_pid(engine->pid);
        engine->pid = 0;
    }
    engine->ready = FALSE;
    engine->waiting_for_move = FALSE;
}

void pikafish_uci_clear(PikafishUci *engine) {
    pikafish_uci_stop(engine);
    g_clear_pointer(&engine->buffer, free_gstring);
    g_clear_pointer(&engine->moves, free_gstring);
    g_clear_pointer(&engine->binary, g_free);
}
