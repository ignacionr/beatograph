#pragma once

#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

void load_panels(auto tabs, auto &loaded_panels, auto localhost, auto menu_tabs)
{
    // remove previously loaded panels
    while (!loaded_panels.empty())
    {
        tabs->remove(loaded_panels.back());
        loaded_panels.pop_back();
    }
    std::filesystem::path panel_dir{"panels"};
    if (std::filesystem::exists(panel_dir) && std::filesystem::is_directory(panel_dir))
    {
        for (auto const &entry : std::filesystem::directory_iterator{panel_dir})
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
            {
                std::ifstream file{entry.path()};
                nlohmann::json panel;
                file >> panel;
                auto config = std::make_shared<panel::config>(
                    panel.at("contents").get<nlohmann::json::array_t>(), localhost);
                auto const &panel_name = panel.at("title").get_ref<std::string const &>();
                loaded_panels.push_back(panel_name);
                tabs->add({panel_name,
                           [config]
                           { config->render(); },
                           menu_tabs});
            }
        }
    }
}
