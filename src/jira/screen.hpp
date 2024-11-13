#pragma once

#include <functional>
#include <vector>
#include <mutex>

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
        using selector_t = std::function<std::vector<nlohmann::json::object_t>()>;

        screen(img_cache &cache) : 
        user_screen_{cache}, issue_screen_{cache}, project_screen_{cache} {
            search_text_.reserve(256);
        }

        void render_menu(std::string_view item) {
            if (item == "View") {
                ImGui::SeparatorText("JIRA");
                if (ImGui::MenuItem("Show Details", nullptr, show_json_details_)) {
                    show_json_details_ = !show_json_details_;
                }
                if (ImGui::MenuItem("Show Assignee", nullptr, show_assignee_)) {
                    show_assignee_ = !show_assignee_;
                }
            }
        }

        void selection_tree(host &h) {
            // now present the tree of options to select issues from different grouppings
            if (ImGui::TreeNode("My Assigned Issues"))
            {
                if (ImGui::SmallButton("Except Done")) {
                    select([&h] { return nlohmann::json::parse(h.get_assigned_issues()).at("issues").get<std::vector<nlohmann::json::object_t>>(); });
                }
                if (ImGui::SmallButton("All")) {
                    select([&h] { return nlohmann::json::parse(h.get_assigned_issues(true)).at("issues").get<std::vector<nlohmann::json::object_t>>(); });
                }
                ImGui::TreePop();
            }
            views::cached_view<nlohmann::json::array_t>("Projects",
                [&h]() {
                    return nlohmann::json::parse(h.get_projects());
                },
                [this, &h](nlohmann::json::array_t const &json_projects) {
                    for (auto const &project : json_projects)
                    {
                        auto const key {project.at("key").get<std::string>()};
                        auto const key_and_name = std::format("{} - {}", 
                            key, 
                            project.at("name").get<std::string>());
                        if (ImGui::TreeNode(key_and_name.c_str()))
                        {
                            static const std::array<std::string_view, 3> status_categories {"To Do", "In Progress", "Done"};
                            for (auto const status_category : status_categories)
                            {
                                if (ImGui::SmallButton(status_category.data()))
                                {
                                    select([&h, key, status_category]{ return nlohmann::json::parse(h.search_by_project(key, status_category)).at("issues").get<std::vector<nlohmann::json::object_t>>();});
                                }
                            }
                            ImGui::TreePop();
                        }
                    }
                });
        }

        void render(host &h)
        {
            // lock the selection_mutex_
            std::lock_guard lock(selection_mutex_);

            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() - 200);
            // present the selected issues
            for (auto const &issue : selected_issues_)
            {
                if (issue_screen_.render(issue, h, false, show_json_details_, show_assignee_)) {
                    query();
                    return;
                }
            }
            ImGui::NextColumn();
            selection_tree(h);
            ImGui::Columns();
        }

        void query() {
            std::thread([this] {
                auto result = selector_();
                {
                    std::lock_guard lock(selection_mutex_);
                    selected_issues_ = result;
                }
            }).detach();
        }

        void select(selector_t selector) {
            selector_ = selector;
            query();
        }
    private:
        views::json json;
        issue_screen issue_screen_;
        user_screen user_screen_;
        std::string search_text_;
        project_screen project_screen_;
        std::vector<nlohmann::json::object_t> selected_issues_;
        selector_t selector_;
        std::mutex selection_mutex_;
        bool show_json_details_{false};
        bool show_assignee_{false};
    };
}