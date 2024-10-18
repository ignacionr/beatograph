#pragma once

#include <SDL.h>
#include <SDL_image.h>
#include <GL/glew.h>
#include <imgui.h>
#include "metric.hpp"
#include "metric_value.hpp"
#include "metric_view_config.hpp"

struct metric_view {
    metric_view();
    void render(metric const &m, metric_view_config &config, std::list<metric_value> const &values);
private:
    unsigned const int gear_texture_id;
    bool show_settings{false};
};
