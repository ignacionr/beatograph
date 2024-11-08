#pragma once

#include <format>
#include <functional>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <optional>
#include <imgui.h>
#include <utf8h/utf8.h>

namespace views 
{
    struct state_t 
    {
        bool value{false};
        bool waiting{false};
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
            waiting = false;
        }
    };

    using state_updated_fn = std::function<void(state_t const &)>;
    
    static state_updated_fn &state_updated() {
        static state_updated_fn state_updated = [](state_t const &) {};
        return state_updated;
    }

    static bool &quitting() {
        static bool quitting{false};
        return quitting;
    }

    static void quitting(bool value) {
        quitting() = value;
    }
    
    void Assertion(std::string_view title, std::function<bool()> assertion) 
    {
        static std::unordered_map<int, state_t> states;
        static std::queue<std::function<void()>> updates;
        static std::thread update_thread([] {
            while (!quitting()) {
                if (updates.empty()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
                updates.front()();
                updates.pop();
            }
        });
        static bool dettached {false};
        if (!dettached) {
            update_thread.detach();
            dettached = true;
        }
        auto id = ImGui::GetID(title.data(), title.data() + title.size());
        ImGui::PushID(id);
        // is the assertion already in the map?
        if (states.find(id) == states.end()) {
            // if not, add it
            states[id].waiting = true;
            updates.push([id, assertion] {
                states[id].load([assertion] { return assertion(); });
                state_updated()(states[id]);
            });
        }
        // get the state
        auto &state{states[id]};
        // set the color
        auto const color{state.waiting ? ImVec4(128, 128, 128, 128) :  (state ? ImVec4(0, 255, 0, 255) : ImVec4(255, 0, 0, 255))};
        // render the assertion
        ImGui::TextUnformatted(state ? "[x]" : "[ ]");
        ImGui::SameLine();
        ImGui::TextColored(color, "%s", title.data());
        if (state.exception) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(255, 0, 0, 255), "Error: %s", state.exception->c_str());
        }
        if (!state.waiting) {
            // check that the user cursor is within the bounds of this item (ignore the button)
            auto const cursor = ImGui::GetCursorScreenPos();
            auto const size = ImGui::GetItemRectSize();
            auto const mouse = ImGui::GetMousePos();
            if (mouse.x >= cursor.x && mouse.x <= cursor.x + 20 && mouse.y >= cursor.y && mouse.y <= cursor.y + size.y) {
                if (ImGui::Button("Refresh")) {
                    state.load([&]{ return assertion(); });
                }
                }
        }
        ImGui::PopID();
    }
} // namespace views