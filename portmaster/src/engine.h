/*
 * Exact Chinese Chess PortMaster UCI engine bridge.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef EXACT_CHINESE_CHESS_PORTMASTER_ENGINE_H
#define EXACT_CHINESE_CHESS_PORTMASTER_ENGINE_H

#include "../../xiangqi_engine.h"

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

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

void engine_init(UciEngine *engine);
void engine_stop(UciEngine *engine);
bool engine_write(UciEngine *engine, const char *line);
bool engine_start(UciEngine *engine);
void engine_set_level(UciEngine *engine, XiangqiAiLevel level);
bool engine_request_move(UciEngine *engine, const XiangqiGame *game, XiangqiAiLevel level, char *bestmove, size_t bestmove_len);

#endif
