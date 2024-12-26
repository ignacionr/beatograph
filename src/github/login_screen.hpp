#pragma once

#include <string>

#include <imgui.h>

#include "login_host.hpp"
#include "../../external/IconsMaterialDesign.h"

namespace github::login {
    struct screen {
        screen(host& host): host_{host} {}
        bool render() {
            if (personal_token_.reserve(256); ImGui::InputText("Personal Token", personal_token_.data(), personal_token_.capacity())) {
                personal_token_ = personal_token_.data();
            }
            if (ImGui::Button(ICON_MD_SAVE " Save")) {
                host_.set_personal_token(personal_token_);
                return true;
            }
            return false;
        }
    private:
        host& host_;
        std::string personal_token_;
    };
}
