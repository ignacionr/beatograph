#pragma once

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
        void render(nlohmann::json const &src, selector_t selector = {})
        {
            auto const &json = selector ? selector(src) : src;
            // use as an array
            if (json.is_array())
            {
                if (json.empty())
                {
                    ImGui::TextWrapped("Empty.");
                }
                else
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
                                    render(item[name]);
                                }
                            }
                        }
                        ImGui::EndTable();
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
                        render(item.value());
                    }
                    ImGui::EndTable();
                }
            }
            else
            {
                ImGui::TextUnformatted(json.dump(4).c_str());
            }
        }

        void render(std::string const &json_src, selector_t selector = {})
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
                render(*json_, selector);
            }
            else
            {
                ImGui::TextWrapped("Error parsing JSON: %s", json_.error().c_str());
            }
        }

    private:
        std::expected<nlohmann::json, std::string> json_;
        std::string last_source_;
    };
} // namespace views
