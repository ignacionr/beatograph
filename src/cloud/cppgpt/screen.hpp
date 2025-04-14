#pragma once

#include <functional>
#include <memory>
#include <string>

#include <imgui.h>
#include "../external/cppgpt/cppgpt.hpp"

namespace cppgpt {
    struct screen {
        void render(std::shared_ptr<ignacionr::cppgpt> gpt, std::function<void(std::string_view)> say) noexcept 
        {
            if (prompt.reserve(2500); ImGui::InputTextMultiline("##prompt", prompt.data(), prompt.capacity())) {
                prompt = prompt.data();
            }
            std::string result;
            if (ImGui::SmallButton("Run") || ImGui::IsKeyChordPressed(ImGuiKey_Enter | ImGuiMod_Ctrl)) {
                try {
                    result = gpt->sendMessage(prompt, 
                    [](auto a, auto b, auto c){
                        http::fetch fetch;
                        return fetch.post(a, b, c);
                    },
                    "user", "grok-2-latest")["choices"][0]["message"]["content"];
                }
                catch(std::exception &e) {
                    result = e.what();
                }
                say(result);
                prompt.clear();
            }
            else {
                // read from the last conversation
                for (auto const &message : gpt->get_conversation()) {
                    if (message["role"] == "user") {
                        ImGui::TextWrapped("You: %s", message.at("content").get_ref<const std::string&>().c_str());
                    }
                    else if (message["role"] == "assistant") {
                        ImGui::TextWrapped("AI: %s", message.at("content").get_ref<const std::string&>().c_str());
                    }
                }
            }
            if (ImGui::SmallButton("Clear")) {
                gpt->clear();
            }
            if (!result.empty()) {
                ImGui::TextWrapped("%s", result.c_str());
            }
        }
    private:
        std::string prompt;
    };
}