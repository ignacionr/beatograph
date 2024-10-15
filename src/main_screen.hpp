#pragma once
#include <algorithm>
#include <chrono>
#include <string>
#include <ranges>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#include "metrics_model.hpp"
#include "metrics_menu.hpp"
#include "metric_view.hpp"

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
        gl_context = SDL_GL_CreateContext(window);
        glewInit();
        if (window == nullptr) {
            SDL_Log("Unable to create window: %s", SDL_GetError());
            return;
        }
        // create the ImGui context
        auto imgui_ctx = ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::SetCurrentContext(imgui_ctx);
        // initialize the ImGui SDL2 backend
        auto sdl_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
        ImGui_ImplOpenGL3_Init("#version 130");

        metrics_menu menu{model};
        // run the main loop
        SDL_Event event;
        bool running = true;
        metric_view view;
        while (running) {
            while (SDL_WaitEventTimeout(&event, 1000/60)) {
                ImGui_ImplSDL2_ProcessEvent(&event);
                if (event.type == SDL_QUIT) {
                    running = false;
                }
            }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            SDL_SetRenderDrawColor(sdl_renderer, 255,255,255, 255);
            SDL_RenderClear(sdl_renderer);
            auto const &display_size = io.DisplaySize;
            auto size = ImVec2(display_size.x, display_size.y);
            ImGui::SetNextWindowSize(size);
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::Begin("Available Metrics", nullptr, 
                ImGuiWindowFlags_MenuBar| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse
                | ImGuiWindowFlags_NoTitleBar);
            ImGui::BeginChild("MetricsMenu", ImVec2(ImGui::GetWindowWidth(), 230));
            menu.render();
            ImGui::EndChild();

            if (menu.selected_metric != nullptr) {
                float const height {ImGui::GetWindowHeight() - ImGui::GetCursorPosY() - 10};
                if (height > 0 && ImGui::BeginChild("MetricView", ImVec2(ImGui::GetWindowWidth() - 10, height))) {
                    view.render(*menu.selected_metric, model.metrics.at(*menu.selected_metric));
                    ImGui::EndChild();
                }
            }

            ImGui::End();
            ImGui::Render();

            glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
            glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            SDL_GL_SwapWindow(window);
        }
        // destroy the ImGui SDL2 backend
        ImGui_ImplSDL2_Shutdown();
    }
private:
    SDL_Window *window;
    SDL_GLContext gl_context;
};
