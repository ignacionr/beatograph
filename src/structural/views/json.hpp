#pragma once

#include "../external/IconsMaterialDesign.h"

#include <algorithm>
#include <expected>
#include <functional>
#include <numeric>
#include <string>
#include <set>
#include <nlohmann/json.hpp>
#include <imgui.h>

namespace views
{
    struct json
    {
        using selector_t = std::function<nlohmann::json(nlohmann::json const &)>;

        struct additional_action {
            std::string label;
            std::function<void(nlohmann::json const &)> action;
        };

        void render(nlohmann::json const &json, std::vector<additional_action> const &actions = {}, selector_t selector = {}) const noexcept
        {
            render_internal(json);
            if (ImGui::SmallButton(ICON_MD_CONTENT_COPY " Copy"))
            {
                ImGui::LogToClipboard();
                ImGui::LogText(json.dump(4).c_str());
                ImGui::LogFinish();
            }
            for (auto const &action : actions)
            {
                ImGui::SameLine();
                if (ImGui::SmallButton(action.label.c_str()))
                {
                    action.action(json);
                }
            }
            ImGui::SameLine();
        }

        void render_internal(nlohmann::json const &src, selector_t selector = {}) const
        {
            auto const &json = selector ? selector(src) : src;
            // use as an array
            if (json.is_array())
            {
                if (json.empty())
                {
                    ImGui::TextWrapped("Empty.");
                }
                else if (json[0].is_object())
                {
                    std::set<std::string> column_names;
                    for (nlohmann::json::object_t const &row : json)
                    {
                        for (const auto &item : row)
                        {
                            column_names.insert(item.first);
                        }
                    }
                    if (ImGui::BeginTable("json", static_cast<int>(column_names.size()), 
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg ))
                    {
                        for (auto const &name : column_names)
                        {
                            ImGui::TableSetupColumn(name.c_str());
                        }
                        ImGui::TableHeadersRow();
                        for (auto const &item : json)
                        {
                            for (auto const &name : column_names)
                            {
                                ImGui::TableNextColumn();
                                if (item.find(name) != item.end())
                                {
                                    render_internal(item[name]);
                                }
                            }
                        }
                        ImGui::EndTable();
                    }
                }
                else {
                    for (auto const &item : json)
                    {
                        render_internal(item);
                    }
                }
            }
            else if (json.is_object()) {
                // show the properties on a vertical table
                if (ImGui::BeginTable("json", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg))
                {
                    for (auto const &item : json.items())
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(item.key().c_str());
                        ImGui::TableNextColumn();
                        render_internal(item.value());
                    }
                    ImGui::EndTable();
                }
            }
            else if (json.is_string()) {
                ImGui::TextUnformatted(json.get<std::string>().c_str());
            }
            else
            {
                ImGui::TextUnformatted(json.dump(4).c_str());
            }
        }

        void render(std::string const &json_src, selector_t selector = {}) const noexcept
        {
            if (last_source_ != json_src)
            {
                try
                {
                    json_ = nlohmann::json::parse(json_src);
                }
                catch (std::exception const &e)
                {
                    json_ = std::unexpected(e.what());
                }
                last_source_ = json_src;
            }
            if (json_)
            {
                render_internal(*json_, selector);
            }
            else
            {
                ImGui::TextWrapped("Error parsing JSON: %s", json_.error().c_str());
            }
        }

    private:
        std::expected<nlohmann::json, std::string> mutable json_;
        std::string mutable last_source_;
    };
} // namespace views
