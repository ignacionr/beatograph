#pragma once

#include <string>
#include <imgui.h>
#include "../external/cppgpt/cppgpt.hpp"

namespace cppgpt {
    struct screen {
        void render(ignacionr::cppgpt &gpt) {
            if (prompt.reserve(256); ImGui::InputTextMultiline("##prompt", prompt.data(), prompt.capacity())) {
                prompt.resize(std::strlen(prompt.data()));
            }
            if (ImGui::SmallButton("Run")) {
                try {
                    result = gpt.sendMessage(prompt, "user", "llama-3.2-90b-text-preview")["choices"][0]["message"]["content"];
                }
                catch(std::exception &e) {
                    result = e.what();
                }
            }
            if (ImGui::SmallButton("Clear")) {
                gpt.clear();
            }
            if (!result.empty()) {
                ImGui::TextWrapped(result.c_str());
            }
        }
    private:
        std::string prompt;
        std::string result;
    };
}