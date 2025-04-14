#pragma once

#include <memory>
#include <string>

#include <imgui.h>

#include "host.hpp"

namespace toggl::login
{
    struct screen{
        screen(std::shared_ptr<host> host): host_{host} {}

        std::shared_ptr<host> host() const { return host_; }

        void render() noexcept {
            if (token_.reserve(200); ImGui::InputText("Toggl Personal Token", token_.data(), token_.capacity())) {
                token_ = token_.data();
                host_->set_token(token_);
            }
        }
    private:
        std::shared_ptr<toggl::login::host> host_;
        std::string token_;
    };
}
