#pragma once

#include <atomic>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include <imgui.h>

#pragma execution_character_set("utf-8")
#include "../../../external/IconsMaterialDesign.h"

namespace views {
    template<typename cached_t>
    void cached_view(
        std::string const &name, std::function<cached_t()> const &factory, 
        std::function<void(cached_t &)> const &renderer,
        bool no_title = false,
        std::optional<std::chrono::system_clock::duration> autorefresh_seconds = std::nullopt,
        std::function<void(std::string_view)> const &on_error = {})
    {
        using cached_item_t = std::expected<cached_t, std::string>;
        using ptr_t = std::shared_ptr<cached_item_t>;
        using envelope_t = std::pair<std::atomic<ptr_t>, std::chrono::system_clock::time_point>;

        static std::unordered_map<int, envelope_t> cache;

        auto const item_id {ImGui::GetID(name.c_str())};
        auto reload = [&name, &factory, item_id, on_error] {
                cache[item_id].second = std::chrono::system_clock::now();
                cache[item_id].first.store(std::make_shared<cached_item_t>(std::unexpected("Loading...")));
                std::thread([factory, item_id, on_error] {
                    ptr_t contents;
                    try {
                        contents = std::make_shared<cached_item_t>(factory());
                    }
                    catch(std::exception const &ex) {
                        contents = std::make_shared<cached_item_t>(std::unexpected(ex.what()));
                        if (on_error) on_error(ex.what());
                    }
                    cache[item_id].first.store(contents);
                }).detach();
        };

        if (autorefresh_seconds.has_value()) {
            auto const now = std::chrono::system_clock::now();
            if (!cache.contains(item_id) || cache[item_id].second + autorefresh_seconds.value() < now) {
                reload();
            }
        }

        if (no_title || ImGui::CollapsingHeader(name.c_str())) {
            envelope_t &cached_copy{cache[item_id]};
            ptr_t item_ptr = cached_copy.first.load();
            ImGui::PushID(item_id);
            if (item_ptr == nullptr) {
                reload();
            }
            else {
                if (item_ptr->has_value()) {
                    renderer(item_ptr->value());
                }
                else {
                    auto const err{item_ptr->error()};
                    if (err == "Loading...") {
                        ImGui::TextUnformatted(ICON_MD_DOWNLOADING);
                    }
                    else {
                        constexpr ImVec4 red {ImVec4(1.0f, 0.0f, 0.0f, 1.0f)};
                        ImGui::TextColored(red, ICON_MD_ERROR " %s", err.c_str());
                    }
                }
                if (ImGui::SmallButton(ICON_MD_REFRESH)) {
                    cache.erase(item_id);
                }
            }
            ImGui::PopID();
        }
    }
}