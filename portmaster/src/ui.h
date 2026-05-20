/*
 * Exact Chinese Chess PortMaster SDL UI.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef EXACT_CHINESE_CHESS_PORTMASTER_UI_H
#define EXACT_CHINESE_CHESS_PORTMASTER_UI_H

#include "game.h"

#include <stdbool.h>

void ui_log_sdl_drivers(void);
void ui_load_assets(App *app);
void ui_destroy_assets(App *app);
void ui_clamp_pointer(App *app);
void ui_warp_mouse_to_pointer(App *app);
bool ui_update_pointer(App *app);
void ui_pointer_click(App *app, int x, int y);
bool ui_pointer_over_panel(const App *app);
void ui_draw_board(App *app);

#endif
