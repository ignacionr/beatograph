#pragma once

#include "pch.h"

#include <nlohmann/json.hpp>
#include <utf8h/utf8.h>


#include "../../../external/cppgpt/cppgpt.hpp"
#include "../../registrar.hpp"
#include "../../hosting/http/fetch.hpp"

namespace cloud::whatsapp {
    struct autotext {

        static std::string ANSIToUTF8(const std::string& ansiStr) {
            // Step 1: Convert ANSI to Wide Character (UTF-16)
            int wideCharLen = MultiByteToWideChar(CP_ACP, 0, ansiStr.c_str(), -1, NULL, 0);
            if (wideCharLen == 0) {
                // Handle error
                return std::string();
            }
            std::wstring wideStr(wideCharLen, 0);
            MultiByteToWideChar(CP_ACP, 0, ansiStr.c_str(), -1, &wideStr[0], wideCharLen);

            // Step 2: Convert Wide Character (UTF-16) to UTF-8
            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, NULL, 0, NULL, NULL);
            if (utf8Len == 0) {
                // Handle error
                return std::string();
            }
            std::string utf8Str(utf8Len, 0);
            WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, &utf8Str[0], utf8Len, NULL, NULL);

            return utf8Str;
        }

        nlohmann::json reduce_sample(nlohmann::json const &sample, int count = 20) const {
            nlohmann::json reduced_sample = sample;
            auto &messages = reduced_sample.at("messages").get_ref<nlohmann::json::array_t &>();
            if (messages.size() > count) {
                messages.erase(messages.begin(), messages.end() - count);
            }
            return reduced_sample;
        }

        std::string say_hello(nlohmann::json const &sample, const std::string &new_message) const {
            auto gpt = registrar::get<ignacionr::cppgpt>({})->new_conversation();
            gpt.add_instructions("You are a professional of Public Relations. Create a witty short greeting that adjusts to the language, style and tone of the conversation that the user gives you. Reply with the greeting only.");
            if (!new_message.empty()) {
                gpt.add_instructions(std::format("Use the following instructions (copy or adapt the message): {}", new_message));
            }
            return gpt.sendMessage(reduce_sample(sample).dump(), [](auto a, auto b, auto c) { return http::fetch().post(a, b, c); }).at("choices").at(0).at("message").at("content").get<std::string>();
        }

        std::string reply_last(nlohmann::json const &sample, const std::string &new_message, bool consume_clipboard = false) const {
            auto gpt = registrar::get<ignacionr::cppgpt>({})->new_conversation();
            gpt.add_instructions("You are a friendly chat user. Create a fresh short reply to the last message of the conversation that the user gives you, that adjusts to the language, style and tone, and the contents of the conversation; never repeat previous messages. Reply with the text only.");
            std::string clipboard;
            if (consume_clipboard) {
                if (OpenClipboard(nullptr)) {
                    HANDLE hData = GetClipboardData(CF_TEXT);
                    size_t cbData = GlobalSize(hData);
                    char *pszText = static_cast<char*>(GlobalLock(hData));
                    if (pszText != nullptr) {
                        std::string ansi_clipboard(pszText, cbData);
                        GlobalUnlock(hData);
                        clipboard = ANSIToUTF8(ansi_clipboard);
                        gpt.add_instructions(std::format("Use the following information to generate the reply: {}", clipboard));
                    }
                    CloseClipboard();
                }
            }
            if (!new_message.empty()) {
                gpt.add_instructions(std::format("Use the following instructions (copy or adapt the message): {}", new_message));
            }
            return gpt.sendMessage(reduce_sample(sample).dump(), [](auto a, auto b, auto c) { return http::fetch().post(a, b, c); }).at("choices").at(0).at("message").at("content").get<std::string>();
        }
    };
}