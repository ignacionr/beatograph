#pragma once

#include <memory>
#include <string>

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "host.hpp"
#include "../registrar.hpp"
#include "login_host.hpp"
#include "login_screen.hpp"
#include "../../external/IconsMaterialDesign.h"
#include "../views/json.hpp"
#include "../views/cached_view.hpp"

namespace github {
    struct screen {
        screen(std::shared_ptr<host> h, std::string const &login_name) : host_{h}, login_name_{login_name} {
            try {
                login_host_ = registrar::get<login::host>(login_name);
            }
            catch(std::exception const &) {
                login_host_ = std::make_shared<login::host>();
                config_mode_ = true;
            }
            host_->login_host(login_host_);
        }

        void render() {
            if (config_mode_) {
                if (!login_screen_) {
                    login_screen_ = std::make_unique<login::screen>(*login_host_);
                }
                if (login_screen_->render()) {
                    registrar::add<login::host>(login_name_, login_host_);
                    config_mode_ = false;
                }
            }
            else {
                try {
                    views::cached_view<nlohmann::json>("Organizations", [this] {
                        return host_->organizations();
                    },
                    [this](nlohmann::json const &organizations) {
                        if (ImGui::BeginCombo("Organizations", selected_org_login_.c_str())) {
                            for (auto const &org : organizations) {
                                if (ImGui::Selectable(org["login"].get_ref<const std::string&>().c_str())) {
                                    selected_org_login_ = org["login"].get<std::string>();
                                    repos_ = host_->org_repos(selected_org_login_);
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }, false);

                    if (!repos_.empty()) {
                        json_.render(repos_);
                    }

                    views::cached_view<nlohmann::json>("User", [this] {
                        return host_->user();
                    },
                    [this](nlohmann::json const &user) {
                        json_.render(user);
                    });
                }
                catch(std::exception const &e) {
                    ImGui::Text("Error: %s", e.what());
                }
                if (ImGui::Button(ICON_MD_SETTINGS " Configure")) {
                    config_mode_ = true;
                }
            }
        }
    private:
        std::shared_ptr<host> host_;
        std::shared_ptr<login::host> login_host_;
        std::unique_ptr<login::screen> login_screen_;
        views::json json_;
        std::string login_name_;
        std::string selected_org_login_;
        bool config_mode_{};
        nlohmann::json repos_;
    };
}
