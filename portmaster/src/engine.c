/*
 * Exact Chinese Chess PortMaster UCI engine bridge.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine.h"
#include "game.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

void engine_init(UciEngine *engine) {
    memset(engine, 0, sizeof(*engine));
    engine->pid = -1;
    engine->in_fd = -1;
    engine->out_fd = -1;
    engine->movetime_ms = 450;
    engine->search_depth = 0;
    engine->multipv = 1;
}

void engine_stop(UciEngine *engine) {
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

bool engine_write(UciEngine *engine, const char *line) {
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

bool engine_start(UciEngine *engine) {
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

void engine_set_level(UciEngine *engine, XiangqiAiLevel level) {
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

bool engine_request_move(UciEngine *engine, const XiangqiGame *game, XiangqiAiLevel level, char *bestmove, size_t bestmove_len) {
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
            game_move_to_uci(&game->history[i], uci, sizeof(uci));
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
