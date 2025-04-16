#pragma once

#include <unordered_map>
#include <functional>
#include <string>

#include <imgui.h>
#include <nlohmann/json.hpp>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif

namespace jira{
    struct content {
        static void render(nlohmann::json::object_t const &element) noexcept {
            static auto render_subcontent = [](nlohmann::json::object_t const &container) {
                if (container.contains("content")) {
                    try {
                        for (auto const &subcontent : container.at("content").get_ref<const nlohmann::json::array_t&>()) {
                            render(subcontent);
                        }
                    }
                    catch (std::exception const &e) {
                        ImGui::TextUnformatted(e.what());
                    }
                }
            };
            static const std::unordered_map<std::string_view, std::function<void(nlohmann::json::object_t const &)>> renderers {
                {"doc", render_subcontent },
                {"text", [](nlohmann::json::object_t const &el){ 
                    ImGui::SameLine();
                    ImGui::TextUnformatted(el.at("text").get_ref<const std::string&>().c_str()); }},
                {"paragraph", [](nlohmann::json::object_t const &el) {
                    ImGui::NewLine();
                    render_subcontent(el);
                }},
                {"inlineCard", [](nlohmann::json::object_t const &el) {
                    ImGui::SameLine();
                    std::string const &url {el.at("attrs").at("url").get_ref<const std::string&>()};
                    if (ImGui::Selectable(url.c_str())) {
#if defined(_WIN32) || defined(_WIN64)
                        ::ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOW);
#else
                        system(std::format("xdg-open {}", url).c_str());
#endif
                    }
                }},
                {"bulletList", render_subcontent},
                {"listItem", render_subcontent},
                {"hardBreak", [](nlohmann::json::object_t const &) { ImGui::Text(""); }},
                {"orderedList", render_subcontent},
                {"codeBlock", [](nlohmann::json::object_t const &el) {
                    // auto const language = el.at("attrs").at("language").get<std::string>();
                    auto const &text = el.at("content").at(0).at("text").get_ref<const std::string&>();
                    ImGui::TextUnformatted(text.c_str());
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
                    auto const &text = el.at("attrs").at("text").get_ref<const std::string&>();
                    ImGui::SameLine();
                    ImGui::TextUnformatted(text.data());
                }},
                {"emoji", [](nlohmann::json::object_t const &el) {
                    auto const &text = el.at("attrs").at("text").get_ref<const std::string&>();
                    ImGui::SameLine();
                    ImGui::TextUnformatted(text.data());
                }}
            };
            auto const &type = element.at("type").get_ref<const std::string &>();
            if (auto const pos = renderers.find(type); pos != renderers.end()) {
                pos->second(element);
            }
        }
    };
}