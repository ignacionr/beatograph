#pragma once

#include <vector>

#include <nlohmann/json.hpp>

#include "host.hpp"
#include "../views/cached_view.hpp"
#include "../views/json.hpp"
#include "issue_screen.hpp"
#include "user_screen.hpp"
#include "../imgcache.hpp"
#include "project_screen.hpp"

namespace jira
{
    struct screen
    {
        screen(img_cache &cache) : 
        user_screen_{cache}, issue_screen_{cache}, project_screen_{cache} {
            search_text_.reserve(256);
        }

        void render(host &h)
        {
            views::cached_view<nlohmann::json::object_t>("My Main Project",
                [&h]() {
                    return nlohmann::json::parse(h.get_project(10026));
                },
                [this, &h](nlohmann::json::object_t const &json_content) {
                    project_screen_.render(json_content, h);
                });
            ImGui::InputText("Search", search_text_.data(), search_text_.capacity());
            {
                search_text_.resize(std::strlen(search_text_.data()));
                views::cached_view<nlohmann::json::array_t>("Search Results",
                    [&h, &search_text = search_text_] {
                        nlohmann::json result = nlohmann::json::parse(h.search_issues_summary(search_text));
                        if (result.contains("errorMessages")) {
                            throw std::runtime_error(result.at("errorMessages").dump());
                        }
                        return result.at("issues");
                    },
                    [this, &h](nlohmann::json::array_t const &json_content) {
                        if (json_content.empty())
                        {
                            ImGui::Text("No results found.");
                        }
                        else for (auto const &issue : json_content)
                        {
                            issue_screen_.render(issue, h);
                        }
                    });
            }
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
                [this, &h](json_array_t const& json_content) {
                    for (nlohmann::json const &issue : json_content) {
                        issue_screen_.render(issue, h, true);
                    }
                },
                true);
        }
    private:
        views::json json;
        issue_screen issue_screen_;
        user_screen user_screen_;
        std::string search_text_;
        project_screen project_screen_;
    };
}