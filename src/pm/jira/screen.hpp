#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "../../imgcache.hpp"
#include "../../structural/views/cached_view.hpp"
#include "../../structural/views/json.hpp"
#include "../../structural/views/try_button.hpp"
#include "host.hpp"
#include "issue_screen.hpp"
#include "project_screen.hpp"
#include "user_screen.hpp"

namespace jira
{
    struct screen
    {
        using selector_t = std::function<std::vector<nlohmann::json::object_t>()>;

        screen() {
            search_text_.reserve(256);
            summary_text_.reserve(256);
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
            auto except_done{[&h] { return nlohmann::json::parse(h.get_assigned_issues()).at("issues").get<std::vector<nlohmann::json::object_t>>(); }};
            if (!selector_) {
                select (except_done);
            }
            // now present the tree of options to select issues from different grouppings
            if (ImGui::TreeNode("My Assigned Issues"))
            {
                views::try_button("Except Done", [&h, this, except_done] {
                    select(except_done);
                });
                views::try_button("All", [&h, this] {
                    select([&h] { return nlohmann::json::parse(h.get_assigned_issues(true)).at("issues").get<std::vector<nlohmann::json::object_t>>(); });
                });
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

        void render_list(host &h, issue_screen::context_actions_t const &actions = {}) {
            if (selected_issues_.empty()) {
                ImGui::Text("No issues selected.");
                return;
            }

            if (issue_lanes_) {
                if (ImGui::BeginChild("by_lane")) {
                    std::unordered_map<int, nlohmann::json::object_t const *> status_by_id;
                    // group by status id
                    std::map<int, std::vector<nlohmann::json::object_t const*>> by_status;
                    for (nlohmann::json::object_t const &issue : selected_issues_) {
                        auto const *status = &issue.at("fields").at("status").get_ref<const nlohmann::json::object_t&>();
                        int const status_id {std::stoi(status->at("id").get_ref<std::string const &>())};
                        if (status_by_id.find(status_id) == status_by_id.end()) {
                            status_by_id[status_id] = status;
                        }
                        by_status[status_id].push_back(&issue);
                    }
                    // show the lanes
                    ImGui::Columns(static_cast<int>(by_status.size()));
                    for (auto const &[status_id, issues] : by_status) {
                        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
                        ImGui::TextUnformatted(status_by_id.at(status_id)->at("name").get_ref<std::string const &>().c_str());
                        ImGui::PopFont();
                        for (auto const &issue : issues) {
                            if (issue_screen_.render(*issue, h, false, actions, show_json_details_, show_assignee_)) {
                                query();
                            }
                        }
                        ImGui::NextColumn();
                    }
                }
                ImGui::EndChild();
            }
            // present the selected issues
            else for (auto const &issue : selected_issues_)
            {
                if (issue_screen_.render(issue, h, false, actions, show_json_details_, show_assignee_)) {
                    query();
                    return;
                }
            }
        }

        void render_new_editor(host &h) {
            ImGui::Text(ICON_MD_CREATE  " New Issue");
            if (ImGui::SameLine(); ImGui::SmallButton(ICON_MD_CANCEL)) {
                editing_new_ = false;
                return;
            }
            views::cached_view<nlohmann::json>("Projects",
                [&h]() {
                    return nlohmann::json::parse(h.get_projects());
                },
                [this, &h](nlohmann::json const &all_projects) {
                    if (ImGui::BeginCombo("Project", selected_project_key_.empty() ? "Select Project" : selected_project_key_.c_str())) {
                        for (auto const &project : all_projects) {
                            auto const key {project.at("key").get_ref<std::string const &>()};
                            if (ImGui::Selectable(std::format("{} - {}", key, project.at("name").get_ref<std::string const &>()).c_str(), selected_project_key_ == key)) {
                                selected_project_key_ = key;
                                selected_project_id_ = project.at("id").get<std::string>();
                            }
                        }
                        ImGui::EndCombo();
                    }
                }, true);
            if (summary_text_.reserve(256); ImGui::InputText("Summary", summary_text_.data(), summary_text_.capacity())) {
                summary_text_ = summary_text_.data();
            }
            views::cached_view<nlohmann::json::array_t>("Issue Type",
                [&h]() {
                    return h.get_issue_types();
                },
                [this](nlohmann::json::array_t const &json_issue_types) {
                    if (ImGui::BeginCombo("Issue Type", issuetype_id_.empty() ? "Select Issue Type" : issuetype_name_.c_str())) {
                        for (auto const &issue_type : json_issue_types)
                        {
                            // determine if the scope applies
                            bool applicable = true;
                            auto const it_scope {issue_type.find("scope")};
                            if (it_scope != issue_type.end()) {
                                if (it_scope->at("type").get_ref<std::string const &>() == "PROJECT") {
                                    applicable = it_scope->at("project").at("id").get_ref<const std::string &>() == selected_project_id_;
                                }
                            }
                            if (applicable) {
                                auto const &name {issue_type.at("name").get_ref<std::string const &>()};
                                auto const &id {issue_type.at("id").get_ref<std::string const &>()};
                                if (ImGui::Selectable(std::format("{}", name).c_str(), issuetype_id_ == id)) {
                                    issuetype_id_ = issue_type.at("id").get<std::string>();
                                    issuetype_name_ = name;
                                }
                            }
                        }
                        ImGui::EndCombo();
                    }
                }, true);
            if (ImGui::CollapsingHeader("Sub-Tasks", ImGuiTreeNodeFlags_DefaultOpen)) {
                int i{0};
                for (auto &subtask : subtasks_) {
                    ImGui::PushID(i++);
                    if (subtask.reserve(256); ImGui::InputText("Summary", subtask.data(), subtask.capacity())) {
                        subtask = subtask.data();
                    }
                    if (ImGui::SameLine(); ImGui::SmallButton("-")) {
                        subtasks_.erase(std::next(subtasks_.begin(), i - 1));
                        ImGui::PopID();
                        return;
                    }
                    ImGui::PopID();
                }
                if (ImGui::SmallButton("  +  ")) {
                    subtasks_.push_back({});
                }
            }
            if (ImGui::Button("Create and Assign to Me")) {
                try {
                    auto issue {h.create_issue(summary_text_, selected_project_key_, issuetype_id_)};
                    h.assign_issue_to_me(issue.at("key").get<std::string>());
                    summary_text_.clear();
                    editing_new_ = false;
                    query();
                    last_error_.clear();
                }
                catch(std::exception const &e) {
                    last_error_ = e.what();
                }
            }
            if (!last_error_.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", last_error_.c_str());
            }
        }

        void render(host &h, issue_screen::context_actions_t const &actions = {})
        {
            // lock the selection_mutex_
            std::lock_guard lock(selection_mutex_);

            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() - 200);
            if (editing_new_) {
                render_new_editor(h);
            }
            else {
                if (ImGui::Button(ICON_MD_CREATE " New Issue")) {
                    editing_new_ = true;
                }
                ImGui::SameLine();
                ImGui::Checkbox("Show Lanes", &issue_lanes_);
                render_list(h, actions);
            }
            ImGui::NextColumn();
            selection_tree(h);
            ImGui::Columns();
        }

        void query() {
            if (!selector_) return;
            std::thread([this] {
                try {
                auto result = selector_();
                {
                    std::lock_guard lock(selection_mutex_);
                    selected_issues_ = result;
                }
                }
                catch(...) {
                    // TODO: handle appropriately
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
        bool editing_new_{false};
        bool issue_lanes_{true};
        std::string summary_text_;
        int selected_project_{-1};
        std::string selected_project_key_;
        std::string selected_project_id_;
        std::vector<std::string> subtasks_;
        std::string issuetype_id_;
        std::string issuetype_name_;
        std::string last_error_;
    };
}