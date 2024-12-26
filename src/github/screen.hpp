#pragma once

#include <memory>
#include <string>

#include <imgui.h>

#include "host.hpp"
#include "../registrar.hpp"
#include "login_host.hpp"
#include "login_screen.hpp"

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
                if (ImGui::Button(ICON_MD_SETTINGS " Configure")) {
                    config_mode_ = true;
                }
            }
        }
    private:
        std::shared_ptr<host> host_;
        std::shared_ptr<login::host> login_host_;
        std::unique_ptr<login::screen> login_screen_;
        std::string login_name_;
        bool config_mode_{};
    };
}
