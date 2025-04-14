#pragma once

#include <memory>

#include <imgui.h>

#include "../external/IconsMaterialDesign.h"
#include "host.hpp"

namespace git{
    struct screen {
        screen(std::shared_ptr<host> host) : host_{host} {}

        void render_list() {
            if (auto repos = host_->repos(); repos)
            {
                if (ImGui::BeginTable("Repos", 2))
                {
                    ImGui::TableSetupColumn("Repository");
                    ImGui::TableSetupColumn("##actions");
                    ImGui::TableHeadersRow();
                    for (auto const &repo : *repos)
                    {
                        ImGui::TableNextRow();
                        ImGui::PushID(repo.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(repo.c_str());
                        ImGui::TableNextColumn();
                        if (ImGui::SmallButton(ICON_MD_FOLDER_OPEN " Open"))
                        {
                            system(std::format("explorer \"{}\"", repo).c_str());
                        }
                        bool any_item_hovered {ImGui::IsItemHovered()};
                        if (ImGui::SameLine(); ImGui::SmallButton(ICON_MD_DOWNLOAD_FOR_OFFLINE " Checkout"))
                        {
                            host_->checkout(repo);
                        }
                        any_item_hovered |= ImGui::IsItemHovered();
                        if (ImGui::SameLine(); ImGui::SmallButton(ICON_MD_CODE " Open in VS Code"))
                        {
                            system(std::format("code \"{}\"", repo).c_str());
                        }
                        any_item_hovered |= ImGui::IsItemHovered();
                        if (any_item_hovered) {
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 0, 64));
                        }
                        ImGui::PopID();
                        // No need to pop style color
                    }
                    ImGui::EndTable();
                }
            }
            // refresh button
            if (ImGui::Button(ICON_MD_REFRESH " Refresh"))
            {
                host_->harvest();
            }
        }

        void render() noexcept {
            render_list();
            if (ImGui::CollapsingHeader("Clone Repository"))
            {
                if (repo_to_clone_.reserve(300); ImGui::InputTextWithHint("Repository URL", "https://github.com/ignacionr/beatograph", repo_to_clone_.data(), repo_to_clone_.capacity()))
                {
                    repo_to_clone_ = repo_to_clone_.data();
                    auto const last_slash = repo_to_clone_.find_last_of('/');
                    if (last_slash != std::string::npos)
                    {
                        clone_target_ = std::format("C:\\src\\{}", repo_to_clone_.substr(last_slash + 1));
                    }
                }
                if (clone_target_.reserve(300); ImGui::InputTextWithHint("Destination path",
                "C:\\src\\project-name", clone_target_.data(), clone_target_.capacity())) {
                    clone_target_ = clone_target_.data();
                }
                bool partial = ImGui::CollapsingHeader("Partial Shallow Download of a directory within the repository");
                if (partial)
                {
                    if (shallow_path_.reserve(300); ImGui::InputTextWithHint("Path to download", "src/include", shallow_path_.data(), shallow_path_.capacity()))
                    {
                        shallow_path_ = shallow_path_.data();
                    }
                }
                if (ImGui::Button(partial ? (ICON_MD_CLOUD_DOWNLOAD " Shallow Clone") : (ICON_MD_CLOUD_DOWNLOAD " Clone")))
                {
                    if (partial)
                    {
                        host_->shallow_partial_download(repo_to_clone_, shallow_path_, clone_target_);
                    }
                    else
                    {
                        host_->clone(repo_to_clone_, clone_target_);
                    }
                }
            }
        }
    private:
        std::shared_ptr<host> host_;
        std::string repo_to_clone_;
        std::string clone_target_;
        std::string shallow_path_;
    };
}