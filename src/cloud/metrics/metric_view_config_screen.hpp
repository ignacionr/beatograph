#pragma once

#include <string>
#include <imgui.h>
#include "metric_view_config.hpp"

struct metric_view_config_screen {
    metric_view_config_screen(metric_view_config &config) : config(config) {}
    void render() noexcept;
private:
    metric_view_config &config;
};
