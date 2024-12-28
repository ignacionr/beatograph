#pragma once
#include <functional>
#include <list>
#include <map>
#include <numeric>
#include <optional>
#include <string>
#include "metric_value.hpp"
#include "metric_view_config.hpp"

struct metrics_model {
    using metrics_map_t = std::map<metric, std::list<metric_value>>;

    void modify_key(std::string_view metric_name, auto modification) {
        auto node = metrics.extract(metric{std::string{metric_name}});
        if (node.empty()) {
            auto key = metric{std::string{metric_name}};
            modification(key);
            metrics[key] = {};
        }
        else {
            modification(node.key());
            metrics.insert(std::move(node));
        }
    }

    void set_help(std::string_view name, std::string_view help) {
        modify_key(name, [help](auto& key) {
            key.help = help;
        });
    }

    void set_type(std::string_view name, std::string_view type) {
        modify_key(name, [type](auto& key) {
            key.type = type;
        });
    }

    void add_value(std::string_view name, metric_value&& value) {
        auto pos = metrics.find(metric{std::string{name}});
        if (pos == metrics.end()) {
            metrics.insert({metric{std::string{name}}, {std::move(value)}});
        }
        else {
            pos->second.emplace_back(std::move(value));
        }
    }

    std::optional<double> sum(std::string_view key) const {
        auto pos = metrics.find({std::string{key}});
        if (pos != metrics.end()) {
            return std::accumulate(pos->second.begin(), pos->second.end(), 0.0, [](auto acc, auto const& mv) { return acc + mv.value; });
        }
        else {
            return std::nullopt;
        }
    }

    metrics_map_t metrics;
    std::map<const metric*, metric_view_config> views;
};
