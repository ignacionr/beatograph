#pragma once

#include <memory>
#include <functional>
#include <unordered_map>

#include "../../structural/text_command/host.hpp"
#include "../../../external/cppgpt/cppgpt.hpp"
#include "../../hosting/http/fetch.hpp"

namespace cloud::cppgpt {
    struct command {
        static structural::text_command::command_source create_source(std::shared_ptr<ignacionr::cppgpt> gpt) {
            return {
                [](std::string const &partial, std::function<void(std::string const &)> callback) {
                    if (partial.starts_with("ai")) {
                        callback("ai ...");
                    }
                },
                [&gpt](std::string const &command) -> std::optional<std::string> {
                    if (command.starts_with("ai ")) {
                        auto message = command.substr(3);
                        if (!message.empty() && message != "...") {
                            auto g = gpt->new_conversation();
                            auto reply = g.sendMessage(message,
                                [](auto a, auto b, auto c) { return http::fetch{240}.post(a, b, c); });
                            return (reply.at("choices").at(0).at("message").at("content").get_ref<std::string const &>());
                        }
                        return std::string{};
                    }
                    return std::nullopt;
                }
            };
        };

        struct command_arguments {
            std::string system;
            std::string message;
        };

        static command_arguments parse_arguments(std::string original) {
            command_arguments result;
            auto const system_start = original.find("--system \"");
            if (system_start != std::string::npos) {
                // find the ending quote
                auto const system_end = original.find('"', system_start + 10);
                if (system_end != std::string::npos) {
                    result.system = original.substr(system_start + 10, system_end - system_start - 10);
                    original.erase(system_start, system_end - system_start + 1);
                }
            }
            auto const message_start = original.find("--message \"");
            if (message_start != std::string::npos) {
                // find the ending quote
                auto const message_end = original.find('"', message_start + 11);
                if (message_end != std::string::npos) {
                    result.message = original.substr(message_start + 11, message_end - message_start - 11);
                    original.erase(message_start, message_end - message_start + 1);
                }
            }
            else {
                // if there is no message, then the rest of the string is the message
                result.message = original;
            }
            return result;
        }
    };
}
