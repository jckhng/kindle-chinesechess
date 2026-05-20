/*
 * Exact Chinese Chess PortMaster input handling.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef EXACT_CHINESE_CHESS_PORTMASTER_INPUT_H
#define EXACT_CHINESE_CHESS_PORTMASTER_INPUT_H

#include "game.h"

#include <SDL.h>

void input_handle_key(App *app, SDL_Keycode key);
void input_handle_controller_button(App *app, SDL_ControllerButtonEvent *button);
void input_handle_controller_button_up(App *app, SDL_ControllerButtonEvent *button);
void input_handle_controller_axis(App *app, SDL_ControllerAxisEvent *axis);
void input_open_controllers(App *app);
void input_close_controllers(App *app);

#endif
