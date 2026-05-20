/*
 * Exact Chinese Chess PortMaster input handling.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "input.h"
#include "ui.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>

void input_handle_key(App *app, SDL_Keycode key) {
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

void input_handle_controller_button(App *app, SDL_ControllerButtonEvent *button) {
    if (button->state != SDL_PRESSED) {
        return;
    }
    switch (button->button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: input_handle_key(app, SDLK_UP); break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: input_handle_key(app, SDLK_DOWN); break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: input_handle_key(app, SDLK_LEFT); break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: input_handle_key(app, SDLK_RIGHT); break;
    case SDL_CONTROLLER_BUTTON_A:
        if (app->pointer_visible && ui_pointer_over_panel(app)) {
            ui_pointer_click(app, app->pointer_x, app->pointer_y);
        } else {
            input_handle_key(app, SDLK_RETURN);
        }
        break;
    case SDL_CONTROLLER_BUTTON_B:
        if (app->open_dropdown != DROPDOWN_NONE) {
            app->open_dropdown = DROPDOWN_NONE;
        } else {
            input_handle_key(app, SDLK_ESCAPE);
        }
        break;
    case SDL_CONTROLLER_BUTTON_X: input_handle_key(app, SDLK_u); break;
    case SDL_CONTROLLER_BUTTON_Y: input_handle_key(app, SDLK_n); break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: input_handle_key(app, SDLK_COMMA); break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
        app->pointer_visible = true;
        ui_pointer_click(app, app->pointer_x, app->pointer_y);
        break;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
        app->pointer_visible = true;
        ui_pointer_click(app, app->pointer_x, app->pointer_y);
        break;
    case SDL_CONTROLLER_BUTTON_BACK:
        app->back_down = true;
        input_handle_key(app, SDLK_q);
        break;
    case SDL_CONTROLLER_BUTTON_START:
        app->start_down = true;
        if (app->back_down) {
            input_handle_key(app, SDLK_q);
        } else {
            app->side_page = (SidePage)(((int)app->side_page + 1) % 3);
            app->open_dropdown = DROPDOWN_NONE;
        }
        break;
    default:
        break;
    }
}

void input_handle_controller_button_up(App *app, SDL_ControllerButtonEvent *button) {
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

void input_handle_controller_axis(App *app, SDL_ControllerAxisEvent *axis) {
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
            input_handle_key(app, SDLK_HOME);
        } else if (axis->value < 8000) {
            left_trigger_down = false;
        }
        return;
    }
    if (axis->axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
        if (axis->value > 22000 && !right_trigger_down) {
            right_trigger_down = true;
            input_handle_key(app, SDLK_PERIOD);
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
            input_handle_key(app, SDLK_LEFT);
            next_axis_tick = now + 180;
        } else if (axis->value > 22000) {
            input_handle_key(app, SDLK_RIGHT);
            next_axis_tick = now + 180;
        }
    } else if (axis->axis == SDL_CONTROLLER_AXIS_LEFTY) {
        if (axis->value < -22000) {
            input_handle_key(app, SDLK_UP);
            next_axis_tick = now + 180;
        } else if (axis->value > 22000) {
            input_handle_key(app, SDLK_DOWN);
            next_axis_tick = now + 180;
        }
    }
}

void input_open_controllers(App *app) {
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

void input_close_controllers(App *app) {
    int i;
    for (i = 0; i < app->controller_count; i++) {
        if (app->controllers[i]) {
            SDL_GameControllerClose(app->controllers[i]);
        }
    }
    app->controller_count = 0;
}
