#pragma once

#include "pch.h"

#include <format>
#include <string>

#include <nlohmann/json.hpp>

#include "../../hosting/http/fetch.hpp"

namespace cloud::whatsapp
{
    struct host
    {
        host(u_short port): port_{port} {}
        nlohmann::json status() const
        {
            auto const url {std::format("http://localhost:{}/session/status/{}", port_, session_id_)};
            auto const response {http::fetch{}(url)};
            return nlohmann::json::parse(response);
        }

        nlohmann::json get_chats() const
        {
            auto const url {std::format("http://localhost:{}/client/getChats/{}", port_, session_id_)};
            auto const response {http::fetch{}(url)};
            return nlohmann::json::parse(response);
        }
        nlohmann::json fetch_messages(std::string_view chat_id) {
            auto const url {std::format("http://localhost:{}/chat/fetchMessages/{}", port_, session_id_)};
            auto const payload {std::format("{{\"chatId\":\"{}\"}}", chat_id)};
            auto const response {http::fetch{}.post(url,payload, [](auto setheader) {
                setheader("Content-Type: application/json");
            })};
            return nlohmann::json::parse(response);
        }
        nlohmann::json send_message(std::string_view chat_id, std::string_view text) {
            auto const url {std::format("http://localhost:{}/client/sendMessage/{}", port_, session_id_)};
            nlohmann::json payload {{"chatId", chat_id}, {"content", text}, {"contentType", "string"}};
            auto const response {http::fetch{}.post(url, payload.dump(), [](auto setheader) {
                setheader("Content-Type: application/json");
            })};
            return nlohmann::json::parse(response);
        }
        std::string session_id_ {"ignacio"};
    private:
        u_short port_;
    };
}