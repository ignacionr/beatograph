#pragma once
#include <algorithm>
#include <chrono>
#include <memory>
#include <ranges>
#include <string>
#include <thread>

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#include "when_available.hpp"

template <typename screen_t>
struct main_screen
{
    main_screen(std::shared_ptr<screen_t> contained_screen) : screen_{contained_screen}
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
        window = SDL_CreateWindow("Beat-o-Graph", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                      SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN);
        // set the icon from the resources (IDI_ICON1)
        auto icon = SDL_LoadBMP("assets/beatograph-icon.bmp");
        if (icon != nullptr)
        {
            SDL_SetWindowIcon(window, icon);
            SDL_FreeSurface(icon);
        }
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
        Repository<SDL_Renderer *>::available(sdl_renderer);

        auto &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
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

    void set_title(std::string_view title)
    {
        if (title.size() > 4 && title[0] < 0 && title[3] == ' ') {
            // remove the leading icon
            title.remove_prefix(4);
        }
        SDL_SetWindowTitle(window, std::format("{} - Beatograph", title).c_str());
    }

    auto &contents()
    {
        return screen_;
    }

    void run(std::function<void(std::string_view)> notifier, std::function<void()> pre_frame = []{})
    {
        // run the main loop
        SDL_Event event;
        running = true;
        while (running.load())
        {
            bool some_event{false};
            while (SDL_PollEvent(&event))
            {
                some_event = true;
                if (event.type == SDL_QUIT)
                {
                    running.store(false);
                }
                ImGui_ImplSDL2_ProcessEvent(&event);
            }
            static int wait_count{0};
            wait_count++;
            wait_count %= 60;
            if (some_event || wait_count == 0) {
                auto &io = ImGui::GetIO();
                if (io.DeltaTime < (1.0f / 60.0f)) {
                    // If the delta time is too small, we assume that the window is not visible
                    // and we don't want to waste CPU cycles on rendering.
                    std::this_thread::sleep_for(std::chrono::milliseconds(15));
                }    
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplSDL2_NewFrame();
                ImGui::NewFrame();
                try {
                    do_frame(pre_frame);
                }
                catch (std::exception const &e) {
                    notifier(e.what());
                }
                catch (...) {
                    notifier("Unknown exception");
                }
                ImGui::Render();

                auto const ds {ImGui::GetIO().DisplaySize};
                glViewport(0, 0, (int)ds.x, (int)ds.y);
                glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
                glClear(GL_COLOR_BUFFER_BIT);
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

                SDL_GL_SwapWindow(window);
            }
            else {
                // no events, sleep for a while
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
            }
        }
    }

    void quit() {
        running.store(false);
    }

    auto renderer() const
    {
        return sdl_renderer;
    }

private:
    void do_frame(auto pre_frame)
    {
        ImGuiIO &io{ImGui::GetIO()};
        auto const &display_size = io.DisplaySize;
        auto size = ImVec2(display_size.x, display_size.y);
        ImGui::SetNextWindowSize(size);
        ImGui::SetNextWindowPos({0, 0});
        if (ImGui::Begin("Beat-o-Graph", nullptr,
                         ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar))
        {
            pre_frame();
            auto const flags = SDL_GetWindowFlags(window);
            auto const is_fullscreen {flags & SDL_WINDOW_FULLSCREEN_DESKTOP};
            auto toggle_fullscreen = [&] {
                if (is_fullscreen)
                {
                    SDL_SetWindowFullscreen(window, 0);
                }
                else
                {
                    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                }
            };

            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Exit"))
                    {
                        running.store(false);
                    }
                    screen_->render_menu("File");
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("View")) {
                    if (ImGui::MenuItem("Stats", nullptr, show_stats))
                    {
                        show_stats = !show_stats;
                    }
                    #if defined (WIN32)
                    if (ImGui::MenuItem("Debug")) {
                        // attach a console, so I can check what comes out of std::cerr
                        AllocConsole();
                        FILE *out, *err;
                        freopen_s(&out, "CONOUT$", "w", stderr);
                        freopen_s(&err, "CONOUT$", "w", stdout);
                    }
                    #endif // WIN32
                    if (ImGui::MenuItem("Full Screen", "F11", is_fullscreen))
                    {
                        toggle_fullscreen();
                    }
                    screen_->render_menu("View");
                    ImGui::EndMenu();
                }
                screen_->render_menu("Main");
                ImGui::EndMenuBar();
            }

            // run shortcuts
            if (ImGui::IsKeyPressed(ImGuiKey_F11))
            {
                toggle_fullscreen();
            }
            
            if (show_stats)
            {
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                            1000.0f / io.Framerate, io.Framerate);
            }
            screen_->render();
            ImGui::End();
        }

    }

    SDL_Window *window;
    SDL_GLContext gl_context;
    SDL_Renderer *sdl_renderer;
    std::shared_ptr<screen_t> screen_;
    std::atomic<bool> running{false};
    bool show_stats{false};
};
