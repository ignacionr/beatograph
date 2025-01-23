#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace structural::text_command {

    struct command_source {
        std::function<void(std::string const &partial, std::function<void(std::string const &)> callback)> partial_list;
        std::function<bool(std::string const &name)> execute;
    };

    struct host {
        using action_t = std::function<void()>;
        
        void add_command(std::string const &name, action_t action) {
            std::lock_guard<std::mutex> lock(mutex_);
            commands[name] = action;
        }

        void add_source(command_source&& source) {
            std::lock_guard<std::mutex> lock(mutex_);
            sources.push_back(std::move(source));
        }

        void operator()(std::string const &name) {
            if (auto it = commands.find(name); it != commands.end()) {
                it->second();
            }
            else {
                for (auto const &source : sources) {
                    if (source.execute(name)) {
                        return;
                    }
                }
                throw std::runtime_error("Unknown command");
            }
        }

        void partial_list(std::string const &partial, std::function<void(std::string const &)> callback) {
            auto lower_partial = partial;
            std::transform(lower_partial.begin(), lower_partial.end(), lower_partial.begin(), ::tolower);
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto const &[name, _] : commands) {
                auto lower_name = name;
                std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
                if (lower_name.find(lower_partial) != std::string::npos) {
                    callback(name);
                }
            }
            // also try the sources
            for (auto const &source : sources) {
                source.partial_list(partial, callback);
            }
        }
    private:
        std::map<std::string, action_t> commands;
        std::vector<command_source> sources;
        std::mutex mutex_;
    };
}