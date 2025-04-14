#pragma once

#include <string>

#include <nlohmann/json.hpp>
#include <imgui.h>

#include "../../imgcache.hpp"
#include "../../registrar.hpp"

namespace jira
{
    struct user_screen
    {
        user_screen() {}

        void render(nlohmann::json::object_t const &json_user) noexcept {
            // obtain the avatar url
            std::string avatar_url {"https://fakeimg.pl/48x48/cccccc/909090?text=User&font=lobster"};
            if (json_user.contains("avatarUrls")) {
                auto const &avatar_urls = json_user.at("avatarUrls");
                if (avatar_urls.contains("48x48")) {
                    avatar_url = avatar_urls.at("48x48").get<std::string>();
                }
            }
            try {
                ImGui::Image(reinterpret_cast<void *>(static_cast<uintptr_t>(
                        cache_->load_texture_from_url(avatar_url))),
                    ImVec2{48,48});
                ImGui::SameLine();
            }
            catch(...) {
                // tough luck
            }

            ImGui::TextUnformatted(json_user.at("displayName").get<std::string>().c_str());
            if (json_user.contains("emailAddress")) {
                ImGui::SameLine();
                ImGui::TextUnformatted(json_user.at("emailAddress").get<std::string>().c_str());
            }
            if (!json_user.at("active").get<bool>()) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "(Inactive)");
            }
        }
    private:
        std::shared_ptr<img_cache> cache_ = registrar::get<img_cache>({});
    };
} // namespace jira
