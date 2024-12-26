#pragma once

#include <format>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "login_host.hpp"
#include "../http/fetch.hpp"

namespace github {
    struct host {
        void login_host(std::shared_ptr<github::login::host> login_host) {
            login_host_ = login_host;
        }

        nlohmann::json const& user() {
            if (user_.empty()) {
                user_ = fetch_user();
            }
            return user_;
        }

        nlohmann::json organizations() {
            return nlohmann::json::parse(fetch("https://api.github.com/user/orgs"));
        }

        nlohmann::json fetch_user() {
            return nlohmann::json::parse(fetch("https://api.github.com/user"));
        }

        nlohmann::json org_repos(std::string_view org) {
            return nlohmann::json::parse(fetch(std::format("https://api.github.com/orgs/{}/repos", org)));
        }

        nlohmann::json repo_workflows(std::string_view full_name) {
            return nlohmann::json::parse(fetch(std::format("https://api.github.com/repos/{}/actions/workflows", full_name)));
        }
    private:
        std::string fetch(const std::string &url) {
            http::fetch fetch;
            return fetch(url, [this](http::fetch::header_setter_t setheader) {
                setheader("Authorization", "Bearer " + login_host_->personal_token());
            });
        }

        std::shared_ptr<github::login::host> login_host_;
        nlohmann::json user_;
    };
}
