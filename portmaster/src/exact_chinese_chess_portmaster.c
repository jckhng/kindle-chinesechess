/*
 * Exact Chinese Chess PortMaster SDL2 frontend.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine.h"
#include "game.h"
#include "input.h"
#include "ui.h"

#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv) {
    App app;
    SDL_Event event;
    Uint32 last_ai_tick = 0;
    Uint32 input_ready_tick = 0;
    bool dirty = true;
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

    ui_log_sdl_drivers();

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
    ui_warp_mouse_to_pointer(&app);
    input_open_controllers(&app);
    ui_load_assets(&app);
    if (engine_start(&app.engine)) {
        set_status(&app, "PIKAFISH READY");
    }
    input_ready_tick = SDL_GetTicks() + 800;

    while (app.running) {
        int events_this_frame = 0;
        int wait_ms = 250;
        bool have_event;
        if (dirty) {
            wait_ms = 0;
        } else if (app.pointer_vx != 0 || app.pointer_vy != 0) {
            wait_ms = 33;
        } else if (app.ai_pending) {
            wait_ms = 50;
        }
        have_event = SDL_WaitEventTimeout(&event, wait_ms) == 1;
        while (events_this_frame < 32 && (have_event || SDL_PollEvent(&event))) {
            events_this_frame++;
            have_event = false;
            switch (event.type) {
            case SDL_QUIT:
                app.running = false;
                dirty = true;
                break;
            case SDL_KEYDOWN:
                input_handle_key(&app, event.key.keysym.sym);
                dirty = true;
                break;
            case SDL_KEYUP:
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                if (SDL_GetTicks() >= input_ready_tick) {
                    input_handle_controller_button(&app, &event.cbutton);
                    dirty = true;
                }
                break;
            case SDL_CONTROLLERBUTTONUP:
                if (SDL_GetTicks() >= input_ready_tick) {
                    input_handle_controller_button_up(&app, &event.cbutton);
                    dirty = true;
                }
                break;
            case SDL_CONTROLLERAXISMOTION:
                if (SDL_GetTicks() >= input_ready_tick) {
                    input_handle_controller_axis(&app, &event.caxis);
                    dirty = true;
                }
                break;
            case SDL_MOUSEMOTION:
                if (SDL_GetTicks() >= input_ready_tick) {
                    app.pointer_visible = true;
                    app.pointer_x = event.motion.x;
                    app.pointer_y = event.motion.y;
                    ui_clamp_pointer(&app);
                    dirty = true;
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (SDL_GetTicks() >= input_ready_tick) {
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        bool had_pointer = app.pointer_visible;
                        if (!had_pointer) {
                            app.pointer_x = event.button.x;
                            app.pointer_y = event.button.y;
                            ui_clamp_pointer(&app);
                        }
                        app.pointer_visible = true;
                        ui_pointer_click(&app, app.pointer_x, app.pointer_y);
                        dirty = true;
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
            dirty = true;
        }
        if (ui_update_pointer(&app)) {
            dirty = true;
        }
        if (dirty) {
            ui_draw_board(&app);
            SDL_RenderPresent(app.renderer);
            dirty = false;
        }
    }

    ui_destroy_assets(&app);
    engine_stop(&app.engine);
    input_close_controllers(&app);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    return 0;
}
