#pragma once

#include <SDL2/SDL.h>
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/backends/imgui_impl_sdlrenderer2.h>

#include "metrics_model.hpp"

struct main_screen{
    metrics_model model;

    main_screen(metrics_model model) : model{model} {
        // initialize SDL2
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
            return;
        }
        // initialize ImGui
        IMGUI_CHECKVERSION();
    }
    ~main_screen() {
        if (window != nullptr) {
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
    }

    void run() {
        // create a main window
        window = SDL_CreateWindow("Beatograph", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, 0);
        if (window == nullptr) {
            SDL_Log("Unable to create window: %s", SDL_GetError());
            return;
        }
        // create the ImGui context
        auto imgui_ctx = ImGui::CreateContext();
        ImGui::SetCurrentContext(imgui_ctx);
        // initialize the ImGui SDL2 backend
        auto sdl_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        ImGui_ImplSDL2_InitForSDLRenderer(window, sdl_renderer);
        // run the main loop
        SDL_Event event;
        bool running = true;
        while (running) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                }
            }
        }
    }
private:
    SDL_Window *window;
};
