#pragma once
#include <algorithm>
#include <chrono>
#include <list>
#include <ranges>
#include <string>
#include <imgui.h>
#include "metric.hpp"
#include "metrics_model.hpp"

struct metrics_menu {
    metrics_menu(metrics_model const &model) : model{model} {}

    void render();

    metric const *selected_metric = nullptr;
    int max_matches {100};
    int search_delay {500};
private:
    std::string last_search;
    std::list<const metric*> matches;
    std::chrono::steady_clock::time_point last_time;
    metrics_model const &model;
    
    void query(std::string input);
};
