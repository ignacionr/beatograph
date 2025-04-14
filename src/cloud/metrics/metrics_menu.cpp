#include "metrics_menu.hpp"

void metrics_menu::render() noexcept
{
    // let's show an autocomplete text box
    static char input[256] = "";
    ImGui::InputText("Search", input, sizeof(input));

    // let's show the metrics that match the search
    std::string input_str{input};
    if (input_str.size() > 2)
    {
        if (last_search != input_str)
        {
            matches.clear();
            last_time = std::chrono::steady_clock::now();
            last_search = input_str;
        }
        else if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - last_time).count() > search_delay)
        {
            query(input_str);
            last_time = std::chrono::steady_clock::time_point::max();
        }

        if (ImGui::BeginChild("Matches", ImVec2(ImGui::GetWindowWidth() - 10, 200), 0, ImGuiWindowFlags_AlwaysVerticalScrollbar))
        {
            for (const auto &metric : matches)
            {
                if (selected_metric == metric)
                {
                    ImGui::SetScrollHereY();
                }
                if (ImGui::Selectable(metric->name.c_str(), selected_metric == metric))
                {
                    selected_metric = metric;
                }
            }
            if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)))
            {
                auto pos = std::ranges::find(matches, selected_metric);
                if (pos != matches.end() && pos != matches.begin())
                {
                    selected_metric = *std::prev(pos);
                }
                else
                {
                    selected_metric = matches.empty() ? nullptr : matches.front();
                }
            }
            else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)))
            {
                auto pos = std::ranges::find(matches, selected_metric);
                if (pos != matches.end() && std::next(pos) != matches.end())
                {
                    selected_metric = *std::next(pos);
                }
                else
                {
                    selected_metric = matches.empty() ? nullptr : matches.front();
                }
            }
            ImGui::EndChild();
        }
    }
}

void metrics_menu::query(std::string input)
{
    std::transform(input.begin(), input.end(), input.begin(), ::toupper);
    matches.clear();
    for (const auto &[metric, values] : model.metrics)
    {
        std::string capitalised = metric.name;
        std::transform(capitalised.begin(), capitalised.end(), capitalised.begin(), ::toupper);
        if (capitalised.find(input) != std::string::npos)
        {
            matches.push_back(&metric);
            if (matches.size() > max_matches)
            {
                break;
            }
        }
    }
}
