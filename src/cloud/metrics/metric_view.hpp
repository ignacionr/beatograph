#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GL/glew.h>
#include <imgui.h>
#include "metric.hpp"
#include "metric_value.hpp"
#include "metric_view_config.hpp"

struct metric_view {
    static constexpr const char *gear_icon {"assets/gear.png"};
    metric_view();
    void render(metric const &m, metric_view_config &config, std::list<metric_value> const &values) noexcept;
private:
    unsigned int gear_texture_id;
    bool show_settings{false};
};
