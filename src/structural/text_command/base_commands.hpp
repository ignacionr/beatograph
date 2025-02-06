#pragma once

#include <functional>
#include <string>

namespace structural::text_command {
    template <typename source_t>
    struct base_commands {
        static source_t echo() {
            return {
                [](std::string const &partial, std::function<void(std::string const &)> callback) {
                    if (partial.starts_with("echo")) {
                        callback("echo ...");
                    }
                },
                [](std::string const &command) -> std::optional<std::string> {
                    if (command.starts_with("echo ")) {
                        return command.substr(5);
                    }
                    return std::nullopt;
                }
            };
        }
    };
} // namespace structural::text_command