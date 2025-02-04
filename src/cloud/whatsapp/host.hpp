#pragma once

#include "pch.h"

#include <format>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "../../hosting/http/fetch.hpp"
#include "../../hosting/local_mapping.hpp"

using namespace std::string_literals;
using namespace std::chrono_literals;

namespace cloud::whatsapp
{
    struct host
    {
        using mapping_ptr = std::shared_ptr<hosting::local::mapping>;
        host(mapping_ptr mapping) : mapping_{mapping} {}

        auto with_policy(auto fn)
        {
            auto result = decltype(fn()){};
            try
            {
                result = fn();
            }
            catch (std::runtime_error const &e)
            {
                if (e.what() == "Error: Couldn't connect to server"s)
                {
                    // try to reconnect
                    mapping_ = std::make_shared<hosting::local::mapping>(mapping_->mapped_port(), mapping_->hostname(), mapping_->localhost());
                    // give it one second before retrying
                    std::this_thread::sleep_for(1s);
                    result = fn();
                }
                throw;
            }
            return result;
        }

        nlohmann::json status()
        {
            return with_policy([this]
                               {
                auto const url {std::format("http://localhost:{}/session/status/{}", port(), session_id_)};
                auto const response {http::fetch{}(url)};
                return nlohmann::json::parse(response); });
        }

        nlohmann::json get_chats()
        {
            return with_policy([this]
                               {
                auto const url {std::format("http://localhost:{}/client/getChats/{}", port(), session_id_)};
                auto const response {http::fetch{160}(url)};
                return nlohmann::json::parse(response); });
        }
        nlohmann::json fetch_messages(std::string_view chat_id)
        {
            return with_policy([this, chat_id]
                               {
                auto const url {std::format("http://localhost:{}/chat/fetchMessages/{}", port(), session_id_)};
                auto const payload {std::format("{{\"chatId\":\"{}\"}}", chat_id)};
                auto const response {http::fetch{}.post(url,payload, [](auto setheader) {
                    setheader("Content-Type: application/json");
                })};
                return nlohmann::json::parse(response); });
        }
        nlohmann::json send_message(std::string_view chat_id, std::string_view text)
        {
            return with_policy([this, chat_id, text]
                               {
                auto const url {std::format("http://localhost:{}/client/sendMessage/{}", port(), session_id_)};
                nlohmann::json payload {{"chatId", chat_id}, {"content", text}, {"contentType", "string"}};
                auto const response {http::fetch{}.post(url, payload.dump(), [](auto setheader) {
                    setheader("Content-Type: application/json");
                })};
                return nlohmann::json::parse(response); });
        }
        std::string session_id_{"ignacio"};

        unsigned short port() const { return mapping_->local_port(); };

    private:
        mapping_ptr mapping_;
    };
}