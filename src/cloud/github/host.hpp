#pragma once

#include <format>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "../../hosting/http/fetch.hpp"
#include "login_host.hpp"

namespace github {
    struct host {
        void login_host(std::shared_ptr<github::login::host> login_host) {
            login_host_ = login_host;
        }

        nlohmann::json const& user() const {
            if (user_.empty()) {
                user_ = fetch_user();
            }
            return user_;
        }

        nlohmann::json organizations() const {
            return fetch("https://api.github.com/user/orgs");
        }

        nlohmann::json fetch_user() const {
            return fetch("https://api.github.com/user");
        }

        nlohmann::json org_repos(std::string_view org) const {
            return fetch(std::format("https://api.github.com/orgs/{}/repos", org));
        }

        nlohmann::json user_repos() const {
            return fetch_all("https://api.github.com/user/repos");
        }

        nlohmann::json find_repo(std::string_view full_name) const {
            return fetch(std::format("https://api.github.com/repos/{}", full_name));
        }

        nlohmann::json repo_workflows(std::string_view full_name) const {
            return fetch(std::format("https://api.github.com/repos/{}/actions/workflows", full_name));
        }

        http::fetch::header_client_t header_client() const {
            return [bearer_header = std::format("Authorization: Bearer {}", login_host_->personal_token())](auto setheader) {
                setheader(bearer_header);
            };
        }

        std::string fetch_string(const std::string &url) const {
            http::fetch fetch;
            auto const source { fetch(url, header_client()) };
            return source;
        }

        nlohmann::json fetch(const std::string &url) const {
            return nlohmann::json::parse(fetch_string(url));
        }

        nlohmann::json fetch_all(const std::string &url) const {
            static auto constexpr page_size {100};
            nlohmann::json::array_t result;
            for (int page = 1; ; ++page) {
                auto const source { fetch(std::format("{}?per_page={}&page={}", url, page_size, page)) };
                if (source.empty()) {
                    break;
                }
                result.insert(result.end(), source.begin(), source.end());
                if (source.size() < page_size) {
                    break;
                }
            }
            return result;
        }
        
    private:
        std::shared_ptr<github::login::host> login_host_;
        mutable nlohmann::json user_;
    };
}
