#pragma once

#include <string>
#include <ranges>
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
        ImGui_ImplSDLRenderer2_Init(sdl_renderer);
        ImGui_ImplSDL2_InitForSDLRenderer(window, sdl_renderer);
        // run the main loop
        SDL_Event event;
        bool running = true;
        while (running) {
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL2_ProcessEvent(&event);
                if (event.type == SDL_QUIT) {
                    running = false;
                }
            }
            ImGui_ImplSDL2_NewFrame(window);
            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui::NewFrame();
            SDL_SetRenderDrawColor(sdl_renderer, 255,255,255, 255);
            SDL_RenderClear(sdl_renderer);
            // render the main screen
            ImGui::Begin("Beatograph");

            // let's show an autocomplete text box
            static char input[256] = "";
            ImGui::InputText("Search", input, sizeof(input));

            // let's show the metrics that match the search
            std::string input_str{input};
            if (input_str.size() > 2) {
                int count{0};
                for (const auto& [metric, values] : model.metrics) {
                    if (metric.name.find(input_str) != std::string::npos) {
                        if (ImGui::Selectable(metric.name.c_str())) {
                            // do something
                        }
                        if (++count > 15) {
                            break;
                        }
                    }
                }
            }

            ImGui::End();
            ImGui::Render();
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
            SDL_RenderPresent(sdl_renderer);
        }
        // destroy the ImGui SDL2 backend
        ImGui_ImplSDL2_Shutdown();
    }
private:
    SDL_Window *window;
};
