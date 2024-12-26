#pragma once

#include <memory>
#include <string>

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "host.hpp"
#include "../views/cached_view.hpp"
#include "../views/json.hpp"

namespace github::repo {
    struct screen {
        screen(nlohmann::json::object_t &&repo, std::shared_ptr<host> host): repo_{repo}, host_{host} {}
        void render() {
            auto const& repo_name {repo_.at("name").get_ref<const std::string&>()};
            ImGui::PushID(repo_name.c_str());
            ImGui::TextUnformatted(repo_name.data(), repo_name.data() + repo_name.size());
            views::cached_view<nlohmann::json>("Workflows",
                [this] { return host_->repo_workflows(repo_.at("full_name").get_ref<const std::string&>()); },
                [](nlohmann::json const &workflows) {
                    static views::json json_view;
                    json_view.render(workflows);
                }
            );
            ImGui::PopID();
        }   
    private:
        nlohmann::json::object_t repo_;
        std::shared_ptr<host> host_;
    };
}
