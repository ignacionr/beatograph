#pragma once

#include <unordered_map>
#include <functional>
#include <string>

#include <imgui.h>
#include <nlohmann/json.hpp>

namespace jira{
    struct content {
        static void render(nlohmann::json::object_t const &element) {
            static auto render_subcontent = [](nlohmann::json::object_t const &container) {
                for (auto const &subcontent : container.at("content").get<nlohmann::json::array_t>()) {
                    render(subcontent);
                }
            };
            static const std::unordered_map<std::string_view, std::function<void(nlohmann::json::object_t const &)>> renderers {
                {"doc", render_subcontent },
                {"text", [](nlohmann::json::object_t const &el){ 
                    ImGui::SameLine();
                    ImGui::TextUnformatted(el.at("text").get<std::string>().data()); }},
                {"paragraph", [](nlohmann::json::object_t const &el) {
                    ImGui::Text("");
                    render_subcontent(el);
                }},
                {"bulletList", render_subcontent},
                {"listItem", render_subcontent},
                {"hardBreak", [](nlohmann::json::object_t const &) { ImGui::Text(""); }},
                {"orderedList", render_subcontent},
                {"codeBlock", [](nlohmann::json::object_t const &el) {
                    // auto const language = el.at("attrs").at("language").get<std::string>();
                    auto const text = el.at("content").at(0).at("text").get<std::string>();
                    ImGui::Text("%s", text.data());
                }},
                {"panel", render_subcontent},
                {"table", render_subcontent},
                {"tableRow", render_subcontent},
                {"tableCell", render_subcontent},
                {"mediaGroup", render_subcontent},
                {"mediaSingle", render_subcontent},
                {"media", render_subcontent},
                {"mediaCard", render_subcontent},
                {"mediaInline", render_subcontent},
                {"mention", [](nlohmann::json::object_t const &el) {
                    auto const text = el.at("attrs").at("text").get<std::string>();
                    ImGui::SameLine();
                    ImGui::TextUnformatted(text.data());
                }},
                {"emoji", [](nlohmann::json::object_t const &el) {
                    auto const text = el.at("attrs").at("text").get<std::string>();
                    ImGui::SameLine();
                    ImGui::TextUnformatted(text.data());
                }}
            };
            auto const type = element.at("type").get<std::string>();
            if (auto const pos = renderers.find(type); pos != renderers.end()) {
                pos->second(element);
            }
        }
    };
}