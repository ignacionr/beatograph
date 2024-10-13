#pragma once
#include <list>
#include <string>
#include <map>
#include "metric_value.hpp"

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

    void add_value(std::string_view name, const metric_value& value) {
        auto pos = metrics.find(metric{std::string{name}});
        if (pos == metrics.end()) {
            metrics.insert({metric{std::string{name}}, {value}});
        }
        else {
            pos->second.push_back(value);
        }
    }

    metrics_map_t metrics;
};
