#pragma once

#include <format>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <optional>
#include <imgui.h>
#include <utf8h/utf8.h>

namespace views 
{
    struct state_t {
        bool value{false};
        std::optional<std::string> exception;
        operator bool() const {
            if (exception) return false;
            return value;
        }
        auto load(std::function<bool()> action) {
            exception.reset();
            try {
                value = action();
            }
            catch (std::exception const &e) {
                exception = e.what();
            }
        }
    };
    
    void Assertion(std::string_view title, std::function<bool()> assertion) 
    {
        static std::unordered_map<std::string, state_t> states;
        // get the full ImGui id
        auto const container{ImGui::GetItemID()};
        auto const full_id{std::format("{}##{}", title, container)};
        // is the assertion already in the map?
        if (states.find(full_id) == states.end()) {
            // if not, add it
            states[full_id].load(assertion);
        }
        // get the state
        auto &state{states[full_id]};
        // set the color
        auto const color{state ? ImVec4(0, 255, 0, 255) : ImVec4(255, 0, 0, 255)};
        // render the assertion
        ImGui::Text("%s", state ? "[x]" : "[ ]");
        ImGui::SameLine();
        ImGui::TextColored(color, "%s", title.data());
        ImGui::SameLine();
        if (state.exception) {
            ImGui::TextColored(ImVec4(255, 0, 0, 255), "Error: %s", state.exception->c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            state.load([&]{ return assertion(); });
        }
    }
} // namespace views