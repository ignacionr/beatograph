#pragma once

#include <functional>
#include <optional>
#include <string>
#include <atomic>
#include <memory>
#include <expected>
#include <unordered_map>
#include <imgui.h>

namespace views {
    template<typename cached_t>
    void cached_view(std::string const &name, std::function<cached_t()> const &factory, 
    std::function<void(cached_t const &)> const &renderer) 
    {
        using cached_item_t = std::expected<cached_t, std::string>;
        using ptr_t = std::shared_ptr<cached_item_t>;
        using envelope_t = std::atomic<ptr_t>;

        static std::unordered_map<int, envelope_t> cache;

        if (ImGui::CollapsingHeader(name.c_str())) {
            auto const item_id {ImGui::GetID(name.c_str())};
            envelope_t cached_copy{cache[item_id].load()};
            ptr_t item_ptr = cached_copy.load();
            if (item_ptr == nullptr) {
                cache[item_id].store(std::make_shared<cached_item_t>(std::unexpected("Loading...")));
                std::thread([name, factory, item_id] {
                    ptr_t contents;
                    try {
                        contents = std::make_shared<cached_item_t>(factory());
                    }
                    catch(std::exception const &ex) {
                        contents = std::make_shared<cached_item_t>(std::unexpected(ex.what()));
                    }
                    cache[item_id].store(contents);
                }).detach();
            }
            else {
                if (item_ptr->has_value()) {
                    renderer(item_ptr->value());
                }
                else {
                    auto const err{item_ptr->error()};
                    if (err == "Loading...") {
                        ImGui::TextUnformatted(err.c_str());
                    }
                    else {
                        constexpr ImVec4 red {ImVec4(1.0f, 0.0f, 0.0f, 1.0f)};
                        ImGui::TextColored(red, "Error: %s", err.c_str());
                    }
                }
                if (ImGui::Button("Refresh")) {
                    cache.erase(item_id);
                }
            }
        }
    }
}