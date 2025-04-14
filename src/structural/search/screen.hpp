#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <imgui.h>

#include "host.hpp"

namespace search {
    struct screen {
        using match_vector_t = std::vector<std::pair<std::string, action_t>>;

        screen(host &host) : host_(host) {}
        void render() noexcept {
            if (search_string_.reserve(256); ImGui::InputText("Search", search_string_.data(), search_string_.capacity())) {
                search_string_ = search_string_.data();
                auto new_matches = std::make_shared<match_vector_t>();
                matches_.store(new_matches);
                std::thread([this, new_matches] {
                    host_(search_string_, [new_matches](match_result_t result) {
                        new_matches->push_back(result);
                    });
                }).detach();
            }
            if (auto mm = matches_.load(); mm) {
                for (auto const &[match, action] : *mm) {
                    if (ImGui::Button(match.c_str())) {
                        action();
                    }
                }
            }
        }
    private:
        host &host_;
        std::string search_string_;
        std::atomic<std::shared_ptr<match_vector_t>> matches_;
    };
}