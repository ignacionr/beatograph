#include <stdexcept>

#include "metric_view.hpp"
#include "metric_view_config.hpp"
#include "metric_view_config_screen.hpp"

#include "../../when_available.hpp"

using namespace std::string_literals;


static void CheckOpenGLError()
{
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
    {
        throw std::runtime_error("OpenGL error: "s + std::to_string(err));
    }
}

static unsigned int LoadTexture(const char *file_path)
{
    CheckOpenGLError();
    SDL_Surface *surface = IMG_Load(file_path);
    if (surface == nullptr)
    {
        throw std::runtime_error("Failed to load image: "s + IMG_GetError());
    }

    unsigned int texture{0};
    glGenTextures(1, &texture);
    CheckOpenGLError();

    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    SDL_FreeSurface(surface);
    return texture;
}

void metric_view::render(metric const &m, metric_view_config &config, std::list<metric_value> const &values) noexcept
{
    ImGui::Text("Name: %s", m.name.c_str());
    ImGui::Text("%s", m.help.c_str());
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 46);
    if (ImGui::ImageButton("GearBtn", reinterpret_cast<void *>(static_cast<uintptr_t>(gear_texture_id)), ImVec2(26, 26)))
    {
        // toggle settings
        show_settings = !show_settings;
    }
    if (show_settings)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        if (ImGui::BeginChild("Settings", ImVec2(0, 0), ImGuiChildFlags_Border)) {
            metric_view_config_screen config_screen{config};
            config_screen.render();
            ImGui::EndChild();
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
    }
    else
    {
        for (metric_value const &v : values)
        {
            ImGui::Text("%s: %f", m.type.c_str(), v.value);
            for (auto const &[label_name, label_value] : v.labels)
            {
                ImGui::Text("\t%s: %s", label_name.c_str(), label_value.c_str());
            }
            ImGui::Separator();
        }
    }
}

metric_view::metric_view()
{
    Repository<SDL_Renderer*>::when_available([this](SDL_Renderer*) {
        gear_texture_id = LoadTexture(gear_icon);
    });
}
