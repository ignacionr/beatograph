#pragma once

#include <algorithm>
#include <memory>
#include <string>

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "host.hpp"
#include "../../../external/IconsMaterialDesign.h"
#include "../../registrar.hpp"
#include "../../structural/views/json.hpp"
#include "../../structural/views/cached_view.hpp"
#include "login_host.hpp"
#include "login_screen.hpp"
#include "repo_screen.hpp"

namespace github
{
    struct screen
    {
        screen(std::shared_ptr<host> h, std::string const &login_name) : host_{h}, login_name_{login_name}
        {
            try
            {
                login_host_ = registrar::get<login::host>(login_name);
            }
            catch (std::exception const &)
            {
                login_host_ = std::make_shared<login::host>();
                config_mode_ = true;
            }
            host_->login_host(login_host_);
        }

        void render()
        {
            if (config_mode_)
            {
                if (!login_screen_)
                {
                    login_screen_ = std::make_unique<login::screen>(*login_host_);
                }
                if (login_screen_->render())
                {
                    registrar::add<login::host>(login_name_, login_host_);
                    config_mode_ = false;
                }
            }
            else
            {
                try
                {
                    views::cached_view<nlohmann::json>("Organization", [this]
                                                       { return host_->organizations(); }, [this](nlohmann::json const &organizations)
                                                       {
                        if (ImGui::BeginCombo("Organizations", selected_org_login_.c_str())) {
                            try {
                                for (auto const &org : organizations) {
                                    if (ImGui::Selectable(org["login"].get_ref<const std::string&>().c_str())) {
                                        selected_org_login_ = org["login"].get<std::string>();
                                        repos_.clear();
                                        for (auto repo_json: host_->org_repos(selected_org_login_)) {
                                            repos_.emplace_back(repo::screen{std::move(repo_json), host_});
                                        }
                                    }
                                }
                                // append the user login to the list
                                auto const all_option {"All Repositories"};
                                if (ImGui::Selectable(all_option)) {
                                    selected_org_login_ = all_option;
                                    repos_.clear();
                                    for (auto repo_json: host_->user_repos()) {
                                        repos_.emplace_back(repo::screen{std::move(repo_json), host_});
                                    }
                                }
                                latest_error_.clear();
                            }
                            catch(std::exception const &e) {
                                latest_error_ = e.what();
                            }
                            ImGui::EndCombo();
                        } }, false);

                    if (ImGui::CollapsingHeader("Repositories"))
                    {
                        if (repo_filter_.reserve(256); ImGui::InputText("Filter", repo_filter_.data(), repo_filter_.capacity()))
                        {
                            repo_filter_ = repo_filter_.data();
                        }
                        for (auto &rs : repos_)
                        {
                            if (repo_filter_.empty() || rs.full_name().find(repo_filter_) != std::string::npos)
                            {
                                rs.render();
                            }
                        }
                    }

                    if (ImGui::CollapsingHeader("Add Repository"))
                    {
                        if (repo_name_.reserve(256); ImGui::InputText("Full Name", repo_name_.data(), repo_name_.capacity(), ImGuiInputTextFlags_EnterReturnsTrue))
                        {
                            repo_name_ = repo_name_.data();
                            try
                            {
                                repos_.emplace_back(repo::screen{host_->find_repo(repo_name_), host_});
                                latest_error_.clear();
                                repo_name_.clear();
                            }
                            catch (std::exception const &e)
                            {
                                latest_error_ = e.what();
                            }
                        }
                        if (!latest_error_.empty())
                        {
                            ImGui::Text("Error: %s", latest_error_.c_str());
                        }
                    }

                    views::cached_view<nlohmann::json>("User", [this]
                                                       { return host_->user(); }, [this](nlohmann::json const &user)
                                                       { json_.render(user); });
                }
                catch (std::exception const &e)
                {
                    ImGui::Text("Error: %s", e.what());
                }
                if (ImGui::Button(ICON_MD_SETTINGS " Configure"))
                {
                    config_mode_ = true;
                }
            }
        }

    private:
        std::shared_ptr<host> host_;
        std::shared_ptr<login::host> login_host_;
        std::unique_ptr<login::screen> login_screen_;
        views::json json_;
        std::string login_name_;
        std::string selected_org_login_;
        bool config_mode_{};
        std::vector<repo::screen> repos_;
        std::string repo_name_;
        std::string latest_error_;
        std::string repo_filter_;
    };
}
