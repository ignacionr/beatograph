#pragma once

struct metric_view_config {
    float min_value = 0.0f;
    float max_value = 100.0f;
    float concern_threshold = 90.0f;
    float warning_threshold = 80.0f;
    float normal_color[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    float warning_color[4] = {1.0f, 1.0f, 0.0f, 1.0f};
    float concern_color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
};
