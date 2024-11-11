#pragma once

#include <algorithm>
#include <expected>
#include <numeric>
#include <string>
#include <set>
#include <nlohmann/json.hpp>
#include <imgui.h>

namespace views
{
    struct json
    {
        void render(std::string const &json_src)
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
                nlohmann::json const &json = json_.value();
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
                        if (ImGui::BeginTable("json", static_cast<int>(column_names.size())))
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
                                        ImGui::TextUnformatted(item[name].dump().c_str());
                                    }
                                }
                            }
                            ImGui::EndTable();
                        }
                    }
                }
                else
                {
                    ImGui::TextUnformatted(json.dump(4).c_str());
                }
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
