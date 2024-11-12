#pragma once

#include <vector>

#include "host.hpp"
#include "../views/cached_view.hpp"
#include "../views/json.hpp"
#include "issue_screen.hpp"

namespace jira
{
    struct screen
    {
        void render(host &h)
        {
            views::cached_view<std::string>("My Profile",
                [&h](){ return h.me(); },
                [this](std::string const& json_content) {
                    json.render(json_content);
                }
            );
            using json_array_t = std::vector<nlohmann::json::object_t>;
            views::cached_view<json_array_t>("My Assigned Issues",
                [&h]() {
                    json_array_t issues;
                    auto const issues_response {h.get_assigned_issues()};
                    std::cerr << issues_response << std::endl;
                    nlohmann::json obj = nlohmann::json::parse(issues_response);
                    nlohmann::json::array_t arr_issues = obj["issues"].get<nlohmann::json::array_t>();
                    for (nlohmann::json const &issue : arr_issues) {
                        issues.push_back(issue);
                    }
                    return issues;
                },
                [this](json_array_t const& json_content) {
                    for (nlohmann::json const &issue : json_content) {
                        issue_screen_.render(issue);
                    }
                }
            );
        }
    private:
        views::json json;
        issue_screen issue_screen_;
    };
}