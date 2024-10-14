#pragma once
#include <algorithm>
#include <chrono>
#include <string>
#include <ranges>
#include <SDL2/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

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
        window = SDL_CreateWindow("Beatograph", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, 
            SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI|SDL_WINDOW_SHOWN);
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
        std::string last_search;
        std::list<const metric*> matches;
        auto last_time = std::chrono::steady_clock::now();
        metric const *selected_metric = nullptr;
        while (running) {
            while (SDL_WaitEventTimeout(&event, 1000/60)) {
                ImGui_ImplSDL2_ProcessEvent(&event);
                if (event.type == SDL_QUIT) {
                    running = false;
                }
            }
            ImGui_ImplSDL2_NewFrame();
            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui::NewFrame();
            SDL_SetRenderDrawColor(sdl_renderer, 255,255,255, 255);
            SDL_RenderClear(sdl_renderer);

            ImGui::Begin("Available Metrics");
            // let's show an autocomplete text box
            static char input[256] = "";
            ImGui::InputText("Search", input, sizeof(input));
            bool const is_active {ImGui::IsItemActive()};

            // let's show the metrics that match the search
            std::string input_str{input};
            if (input_str.size() > 2) {
                if (last_search != input_str) {
                    matches.clear();
                    last_time = std::chrono::steady_clock::now();
                    last_search = input_str;
                }
                else if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - last_time).count() > 500) {
                    matches.clear();
                    for (const auto& [metric, values] : model.metrics) {
                        if (metric.name.find(input_str) != std::string::npos) {
                            matches.push_back(&metric);
                            if (matches.size() > 100) {
                                break;
                            }
                        }
                    }
                }
                int count{0};
                ImGui::BeginChild("Matches", ImVec2(0, 200), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_AlwaysVerticalScrollbar);
                for (const auto& metric : matches) {
                    if (selected_metric == metric) {
                        ImGui::SetScrollHereY();
                    }
                    if (ImGui::Selectable(metric->name.c_str(), selected_metric == metric)) {
                        selected_metric = metric;
                    }
                }
                ImGui::EndChild();
                if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow))) {
                    auto pos = std::ranges::find(matches, selected_metric);
                    if (pos != matches.end() && pos != matches.begin()) {
                        selected_metric = *std::prev(pos);
                    }
                    else {
                        selected_metric = matches.empty() ? nullptr : matches.front();
                    }
                }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow))) {
                    auto pos = std::ranges::find(matches, selected_metric);
                    if (pos != matches.end() && std::next(pos) != matches.end()) {
                        selected_metric = *std::next(pos);
                    }
                    else {
                        selected_metric = matches.empty() ? nullptr : matches.front();
                    }
                }
            }

            ImGui::End();
            ImGui::Render();
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), sdl_renderer);
            SDL_RenderPresent(sdl_renderer);
        }
        // destroy the ImGui SDL2 backend
        ImGui_ImplSDL2_Shutdown();
    }
private:
    SDL_Window *window;
};
