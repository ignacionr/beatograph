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
        nlohmann::json status() const
        {
            auto const url {std::format("http://localhost:3000/session/status/{}", session_id_)};
            auto const response {http::fetch{}(url)};
            return nlohmann::json::parse(response);
        }

        nlohmann::json get_chats() const
        {
            auto const url {std::format("http://localhost:3000/client/getChats/{}", session_id_)};
            auto const response {http::fetch{}(url)};
            return nlohmann::json::parse(response);
        }
        nlohmann::json fetch_messages(std::string_view chat_id) {
            auto const url {std::format("http://localhost:3000/chat/fetchMessages/{}", session_id_)};
            auto const payload {std::format("{{\"chatId\":\"{}\"}}", chat_id)};
            auto const response {http::fetch{}.post(url,payload, [](auto setheader) {
                setheader("Content-Type: application/json");
            })};
            return nlohmann::json::parse(response);
        }
        nlohmann::json send_message(std::string_view chat_id, std::string_view text) {
            auto const url {std::format("http://localhost:3000/client/sendMessage/{}", session_id_)};
            nlohmann::json payload {{"chatId", chat_id}, {"content", text}, {"contentType", "string"}};
            auto const response {http::fetch{}.post(url, payload.dump(), [](auto setheader) {
                setheader("Content-Type: application/json");
            })};
            return nlohmann::json::parse(response);
        }
        std::string session_id_ {"ignacio"};
    };
}