#pragma once
#include <algorithm>
#include <chrono>
#include <memory>
#include <ranges>
#include <string>

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#include "when_available.hpp"

template <typename screen_t>
struct main_screen
{
    main_screen(std::unique_ptr<screen_t> &&contained_screen) : screen{std::move(contained_screen)}
    {
#if defined(IMGUI_IMPL_OPENGL_ES2)
        // GL ES 2.0 + GLSL 100
        const char *glsl_version = "#version 100";
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
        // GL 3.2 Core + GLSL 150
        const char *glsl_version = "#version 150";
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
        // GL 3.0 + GLSL 130
        const char *glsl_version = "#version 130";
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

        // initialize SDL2
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            throw std::runtime_error(std::string("Unable to initialize SDL: ") + SDL_GetError());
        }
        // create a main window
        window = SDL_CreateWindow("Beatograph", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN);
        gl_context = SDL_GL_CreateContext(window);
        SDL_GL_MakeCurrent(window, gl_context);
        SDL_GL_SetSwapInterval(1); // Enable vsync

        // initialize ImGui
        IMGUI_CHECKVERSION();
        auto imgui_ctx = ImGui::CreateContext();
        ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
        ImGui_ImplOpenGL3_Init(glsl_version);

        glewInit();
        if (window == nullptr)
        {
            throw std::runtime_error(std::string("Unable to create window: ") + SDL_GetError());
        }
        // create the ImGui context
        ImGui::SetCurrentContext(imgui_ctx);
        // initialize the ImGui SDL2 backend
        sdl_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (sdl_renderer == nullptr)
        {
            throw std::runtime_error(std::string("Unable to create renderer: ") + SDL_GetError());
        }
        Repository<SDL_Renderer *>::onSubsystemAvailable(sdl_renderer);
    }
    ~main_screen()
    {
        // destroy the ImGui SDL2 backend
        ImGui_ImplSDL2_Shutdown();
        if (window != nullptr)
        {
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
    }

    void run()
    {

        // run the main loop
        SDL_Event event;
        bool running = true;
        while (running)
        {
            while (SDL_WaitEventTimeout(&event, 1000 / 60))
            {
                if (event.type == SDL_QUIT)
                {
                    running = false;
                }
                ImGui_ImplSDL2_ProcessEvent(&event);
                // Handle window resize event
                if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    int width = event.window.data1;
                    int height = event.window.data2;
                    glViewport(0, 0, width, height);
                    do_frame();
                }
            }
            do_frame();
        }
    }

private:
    void do_frame()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        SDL_SetRenderDrawColor(sdl_renderer, 255, 255, 255, 255);
        SDL_RenderClear(sdl_renderer);
        ImGuiIO &io{ImGui::GetIO()};
        auto const &display_size = io.DisplaySize;
        auto size = ImVec2(display_size.x, display_size.y);
        ImGui::SetNextWindowSize(size);
        ImGui::SetNextWindowPos(ImVec2(0, 0));


        ImGui::Begin("Available Metrics", nullptr,
                     ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
        screen->render();
        ImGui::End();
        ImGui::Render();

        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    SDL_Window *window;
    SDL_GLContext gl_context;
    SDL_Renderer *sdl_renderer;
    std::unique_ptr<screen_t> screen;
};
