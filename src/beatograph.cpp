#include "pch.h"
#include <algorithm>
#include <numeric>
#include <format>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <string>
#include <unordered_map>

#define IMGUI_USE_WCHAR32
#include <imgui.h>

#include "main_screen.hpp"
#include "screen_tabs.hpp"
#include "metrics/metrics_parser.hpp"
#include "metrics/metrics_model.hpp"
#include "metrics/metrics_screen.hpp"
#include "toggl/toggl_client.hpp"
#include "toggl/toggl_screen.hpp"
#include "jira/host.hpp"
#include "jira/screen.hpp"
#include "calendar/screen.hpp"
#include "data_offering/screen.hpp"
#include "hosting/host.hpp"
#include "hosting/host_local.hpp"
#include "hosting/local_screen.hpp"
#include "arangodb/cluster_report.hpp"
#include "ssh/screen_all.hpp"
#include <cppgpt/cppgpt.hpp>
#include "views/assertion.hpp"
#include "git/host.hpp"
#include "radio/host.hpp"
#include "radio/screen.hpp"
#include "dev-locked/screen.hpp"
#include "rss/host.hpp"
#include "rss/screen.hpp"
#include "imgcache.hpp"
#include "conversions/screen.hpp"
#include "clocks/screen.hpp"
#include "clocks/weather.hpp"

#pragma execution_character_set("utf-8")
#include "../external/IconsMaterialDesign.h"

std::string get_env_variable(std::string_view key)
{
    char *val = nullptr;
    size_t len = 0;
    if (_dupenv_s(&val, &len, key.data()) || val == nullptr)
    {
        throw std::runtime_error(std::format("Error: {} environment variable not set.", key));
    }
    return std::string{val, len - 1}; // len returns the count of all copied bytes, including the terminator
}

void setup_fonts()
{
    auto &io{ImGui::GetIO()};
    float baseFontSize = 13.5f;
    io.Fonts->AddFontFromFileTTF("assets/fonts/LiberationMono-Regular.ttf", baseFontSize, nullptr, io.Fonts->GetGlyphRangesCyrillic());

    float iconFontSize = baseFontSize * 3.0f / 3.0f;

    // merge in icons from Font Awesome
    static const ImWchar icons_ranges[] = { 
        static_cast<ImWchar>(ICON_MIN_MD), 
        static_cast<ImWchar>(ICON_MAX_MD), 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.GlyphOffset.y = 3;
    io.Fonts->AddFontFromFileTTF("assets/fonts/MaterialIcons-Regular.ttf", iconFontSize, &icons_config, icons_ranges);

    io.Fonts->AddFontFromFileTTF("assets/fonts/Montserrat-Regular.ttf", 16.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
}

void load_podcasts(auto &host) {
    std::ifstream file("podcasts.txt");
    // get all urls into a vector
    std::vector<std::string> urls;
    std::string line;
    while (std::getline(file, line))
    {
        urls.push_back(line);
    }
    host.add_feeds(urls);
}

#if defined(_WIN32)
int WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#else
int main()
{
#endif
    {
        // auto constexpr happy_bell_sound = "assets/mixkit-happy-bell-alert-601.wav";
        // auto constexpr impact_sound = "assets/mixkit-underground-explosion-impact-echo-1686.wav";
        auto static constexpr jira_tab_name {ICON_MD_TASK " Jira"};
        auto static constexpr radio_tab_name {ICON_MD_AUDIOTRACK " Radio"};

        img_cache cache{"imgcache"};

        weather::openweather_client weather_host{get_env_variable("OPENWEATHER_KEY")};

        // get the Groq API key from GROQ_API_KEY
        auto groq_api_key = get_env_variable("GROQ_API_KEY");
        ignacionr::cppgpt gpt{groq_api_key, ignacionr::cppgpt::groq_base};

        toggl::client tc(get_env_variable("TOGGL_API_TOKEN"));
        toggl::screen ts(tc);

        jira::host jh{get_env_variable("JIRA_USER"), get_env_variable("JIRA_TOKEN")};
        std::unique_ptr<jira::screen> js;

        calendar_screen cs;

        hosting::local::host localhost;
        hosting::local::screen local_screen{localhost};

        git_host git{localhost};
        dataoffering::screen ds{localhost, groq_api_key, get_env_variable("DATAOFFERING_TOKEN")};

        cluster_report cr{localhost};
        ssh_screen ssh_screen;

        radio::host radio_host;

        std::unique_ptr<radio::screen> radio_screen;

        dev_locked::screen dev_screen{localhost};

        rss::host podcast_host;
        load_podcasts(podcast_host);
        rss::screen rss_screen{podcast_host,
                               [&radio_host](std::string_view url)
                               {
                                   radio_host.play(std::string{url});
                               },
                               cache};

        conversions::screen conv_screen{};

        cppgpt::screen gpt_screen;

        clocks::screen clocks_screen{weather_host, cache};

        std::shared_ptr<screen_tabs> tabs;

        auto menu_tabs = [&tabs](std::string_view key)
        {
            if (key == "Main")
            {
                if (ImGui::BeginMenu("Tabs"))
                {
                    if (ImGui::MenuItem(jira_tab_name, "Ctrl+J"))
                    {
                        tabs->select_tab(jira_tab_name);
                    }
                    if (ImGui::MenuItem(radio_tab_name, "Ctrl+R"))
                    {
                        tabs->select_tab(radio_tab_name);
                    }
                    ImGui::EndMenu();
                }
            }
        };
        auto menu_tabs_and = [&menu_tabs](std::function<void(std::string_view)> other)
        {
            return [menu_tabs, other](std::string_view key)
            {
                other(key);
                menu_tabs(key);
            };
        };

        tabs = std::make_shared<screen_tabs>(std::vector<screen_tabs::tab_t> {
            {ICON_MD_COMPUTER, [&local_screen] { local_screen.render(); }, menu_tabs},
            {ICON_MD_GROUPS " Backend Dev", [&dev_screen, &gpt] { dev_screen.render(gpt); }, menu_tabs},
            {ICON_MD_GROUPS " Data Offering", [&ds] { ds.render(); }, menu_tabs},
            {ICON_MD_GROUPS " ArangoDB", [&cr] { cr.render(); }, menu_tabs},
            {ICON_MD_TASK " Toggl", [&ts] { ts.render(); }, menu_tabs},
            {jira_tab_name, [&js, &jh, &ts] { js->render(jh, {
                {"Start Toggl", [&ts](nlohmann::json const &entry) {
                    auto const activity_description {std::format("{} - {}", 
                        entry.at("fields").at("summary").get<std::string>(), 
                        entry.at("key").get<std::string>())};
                    ts.start_time_entry(activity_description);
                } }
            }); },
             menu_tabs_and([&js](std::string_view item)
                           { js->render_menu(item); }),
             ImVec4(0.5f, 0.5f, 1.0f, 1.0f)},
            {ICON_MD_CALENDAR_MONTH " Calendar", [&cs] { cs.render(); }, menu_tabs},
            {ICON_MD_SETTINGS_REMOTE " SSH Hosts", [&ssh_screen, &localhost] { ssh_screen.render(localhost); }, menu_tabs},
            {ICON_MD_CODE " Git Repositories", [&git]
             {
                 ImGui::Text("Git Repositories");
                 if (auto repos = git.repos(); repos)
                 {
                     for (auto const &repo : *repos)
                     {
                         ImGui::Text("%s", repo.c_str());
                     }
                 }
             },
             menu_tabs},
            {radio_tab_name, [&radio_screen, &rss_screen]
             {
                 radio_screen->render();
                 rss_screen.render();
             },
             menu_tabs, ImVec4(0.05f, 0.5f, 0.05f, 1.0f)},
            {ICON_MD_CURRENCY_EXCHANGE " Conversions", [&conv_screen]
             { conv_screen.render(); }, menu_tabs},
            {ICON_MD_WATCH " Clocks", [&clocks_screen] { clocks_screen.render(); }, 
             menu_tabs_and([&clocks_screen](auto i) {  clocks_screen.render_menu(i); })},
            {ICON_MD_CHAT_BUBBLE " AI", [&gpt_screen, &gpt]
             { gpt_screen.render(gpt); }, menu_tabs, ImVec4(0.75f, 0.75f, 0.75f, 1.0f)}});
        main_screen screen{tabs};

        setup_fonts();

        radio_screen = std::make_unique<radio::screen>(radio_host, cache);
        js = std::make_unique<jira::screen>(cache);
        screen.run([&tabs]
                   {
            if (ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
                if (ImGui::IsKeyPressed(ImGuiKey_J)) {
                    tabs->select_tab("Jira");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_R)) {
                    tabs->select_tab("Radio");
                }
            } });
        views::quitting(true);
    }
    std::cerr << "Terminating.\n";

    return 0;
}
