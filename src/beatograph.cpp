#include "pch.h"
#pragma execution_character_set("utf-8")
#include "../external/IconsMaterialDesign.h"
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
#include "hosting/host.hpp"
#include "hosting/host_local.hpp"
#include "hosting/local_screen.hpp"
#include "ssh/screen_all.hpp"
#include <cppgpt/cppgpt.hpp>
#include "views/assertion.hpp"
#include "git/host.hpp"
#include "radio/host.hpp"
#include "radio/screen.hpp"
#include "rss/host.hpp"
#include "rss/screen.hpp"
#include "imgcache.hpp"
#include "conversions/screen.hpp"
#include "clocks/screen.hpp"
#include "clocks/weather.hpp"
#include "notify/host.hpp"
#include "notify/screen.hpp"
#include "panel/config.hpp"
#include "cppgpt/screen.hpp"
#include "gtts/host.hpp"
#include "report/host.hpp"

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

    // font[1] for important content text
    io.Fonts->AddFontFromFileTTF("assets/fonts/Montserrat-Regular.ttf", 16.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
    // font[2] for titles
    io.Fonts->AddFontFromFileTTF("assets/fonts/Montserrat-Bold.ttf", 23.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
}

void load_podcasts(auto &host, auto sink) {
    std::ifstream file("podcasts.txt");
    // get all urls into a vector
    std::vector<std::string> urls;
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line[0] != '#'){
            urls.push_back(line);
        }
    }
    host.add_feeds(urls, sink, []{ return !views::quitting(); });
}

std::optional<std::string> load_last_played() {
    std::ifstream file("last_played.txt");
    if (file.is_open()) {
        std::string line;
        std::getline(file, line);
        return line;
    }
    return std::nullopt;
}

void save_last_played(std::string_view url) {
    std::ofstream file("last_played.txt");
    file << url;
}

void load_panels(auto tabs, auto &loaded_panels, auto &localhost, auto menu_tabs) {
    // remove previously loaded panels
    while (!loaded_panels.empty()) {
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
                                [config] { 
                                config->render(); },
                                menu_tabs});
            }
        }
    }
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

        hosting::local::host localhost;

        img_cache cache{"imgcache"};

        weather::openweather_client weather_host{localhost.get_env_variable("OPENWEATHER_KEY")};

        // get the Groq API key from GROQ_API_KEY
        auto groq_api_key = localhost.get_env_variable("GROQ_API_KEY");
        ignacionr::cppgpt gpt{groq_api_key, ignacionr::cppgpt::groq_base};

        gtts::host gtts_host{"./gtts_cache"};
        notify::host notify_host;
        radio::host::init();
        radio::host notifications_radio_host;
        notify_host.sink([&gtts_host, &notifications_radio_host](std::string_view text, std::string_view title) {
            try {
                gtts_host.tts_job(std::format("{}: {}", title, text), [&notifications_radio_host](std::string_view file_produced) {
                    notifications_radio_host.play_sync(std::string{file_produced.data(), file_produced.size()});
                });
            }
            catch(const std::exception &e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        });

        notify::screen notify_screen{notify_host};

        toggl::client tc(localhost.get_env_variable("TOGGL_API_TOKEN"), [&notify_host](std::string_view text) { notify_host(text, "Toggl"); });
        toggl::screen ts(tc);

        jira::host jh{localhost.get_env_variable("JIRA_USER"), localhost.get_env_variable("JIRA_TOKEN")};
        std::unique_ptr<jira::screen> js;

        calendar::host calendar_host{localhost.get_env_variable("CALENDAR_DELEGATE_URL")};
        calendar::screen cs{calendar_host};

        hosting::local::screen local_screen{localhost};

        git_host git{localhost};

        ssh_screen ssh_screen;

        radio::host radio_host;
        if (auto last_played = load_last_played(); last_played)
        {
            radio_host.play(*last_played);
        }

        std::unique_ptr<radio::screen> radio_screen;

        rss::host podcast_host;
        load_podcasts(podcast_host, [&notify_host](auto text){ notify_host(text, "RSS"); });
        rss::screen rss_screen{podcast_host,
                               [&radio_host](std::string_view url)
                               {
                                   radio_host.play(std::string{url});
                               },
                               cache};

        conversions::screen conv_screen{};

        cppgpt::screen gpt_screen;

        clocks::screen clocks_screen{weather_host, cache, []{ return views::quitting(); }};

        std::shared_ptr<screen_tabs> tabs;

        std::vector<std::string> loaded_panels;
        std::function<void(std::string_view)> menu_tabs;
        menu_tabs = [&tabs, &loaded_panels, &localhost, &menu_tabs](std::string_view key)
        {
            if (key == "Main")
            {
                if (ImGui::BeginMenu("Tabs"))
                {
                    if (ImGui::MenuItem(jira_tab_name, "Ctrl+J"))
                    {
                        tabs->select(jira_tab_name);
                    }
                    if (ImGui::MenuItem(radio_tab_name, "Ctrl+R"))
                    {
                        tabs->select(radio_tab_name);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Reload Panels")) {
                        load_panels(tabs, loaded_panels, localhost, menu_tabs);
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
            {ICON_MD_TASK " Toggl", [&ts] { ts.render(); }, menu_tabs},
            {jira_tab_name, [&js, &jh, &ts] { js->render(jh, {
                {ICON_MD_PUNCH_CLOCK " Start Toggl", [&ts](nlohmann::json const &entry) {
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
                         ImGui::TextUnformatted(repo.c_str());
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
             { gpt_screen.render(gpt); }, menu_tabs, ImVec4(0.75f, 0.75f, 0.75f, 1.0f)},
            {"Notifications", [&notify_screen] { notify_screen.render(); }, menu_tabs}
        });

        main_screen screen{tabs};

        // enumerate the ./panels directory
        load_panels(tabs, loaded_panels, localhost, menu_tabs);

        report::host report_host{localhost};
        std::jthread report_thread{[&report_host, &notify_host]
        {
            while (!views::quitting())
            {
                try
                {
                    auto contents = report_host();
                    notify_host(contents, "Report");
                }
                catch (const std::exception &e)
                {
                    notify_host(e.what(), "Report");
                }
                for (int i = 0; i < 3600 && !views::quitting(); ++i)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }
        }};

        setup_fonts();

        radio_screen = std::make_unique<radio::screen>(radio_host, cache);
        js = std::make_unique<jira::screen>(cache);
        screen.run([&tabs]
                   {
            if (ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
                if (ImGui::IsKeyPressed(ImGuiKey_J)) {
                    tabs->select(jira_tab_name);
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_R)) {
                    tabs->select(radio_tab_name);
                }
            } });

        if (auto last_played = radio_host.last_played(); last_played) {
            save_last_played(*last_played);
        }
        else {
            // remove the last played file
            std::filesystem::remove("last_played.txt");
        }

        views::quitting(true);
    }
    std::cerr << "Terminating.\n";

    return 0;
}
