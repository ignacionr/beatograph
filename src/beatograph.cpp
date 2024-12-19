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
#include "git/screen.hpp"
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
#include "group_t.hpp"
#include "split_screen.hpp"
#include "tool_screen.hpp"
#include "config/file_config.hpp"
#include "registrar.hpp"
#include "toggl/login/host.hpp"

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

void load_podcasts(auto host, auto sink) {
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
    host->add_feeds(urls, sink, []{ return !views::quitting(); });
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

template<typename T>
void load_services(auto config, auto key_base) {
    config->scan_level(key_base, [config, key_base](auto const &subkey) {
        auto login = std::make_shared<T>();
        login->load_from([config, key_base, subkey](std::string_view k){ return config->get(std::format("{}{}.{}", key_base, subkey,k)); });
        registrar::add<T>(subkey, login);
    });
}

template<typename T>
void save_services(auto config, auto key_base) {
    registrar::all<T>([config, key_base](std::string const &key, auto login) {
        login->save_to([config, key, key_base](std::string_view k, std::string v) {
            config->set(std::format("{}{}.{}", key_base, key, k), v);
        });
    });
}


#if defined(_WIN32)
int WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#else
int main()
{
#endif
    {
        auto fconfig {std::make_shared<config::file_config>("./beatograph.ini")};

        // load toggl_logins
        static std::string_view constexpr toggl_login_base {"toggl_login."};
        load_services<toggl::login::host>(fconfig, toggl_login_base);

        // auto constexpr happy_bell_sound = "assets/mixkit-happy-bell-alert-601.wav";
        // auto constexpr impact_sound = "assets/mixkit-underground-explosion-impact-echo-1686.wav";
        auto static constexpr radio_tab_name {ICON_MD_AUDIOTRACK " Radio"};

        hosting::local::host localhost;

        img_cache cache{"imgcache"};

        // get the Groq API key from GROQ_API_KEY
        auto groq_api_key = localhost.get_env_variable("GROQ_API_KEY");
        ignacionr::cppgpt gpt{groq_api_key, ignacionr::cppgpt::groq_base};

        gtts::host gtts_host{"./gtts_cache"};
        notify::host notify_host;
        radio::host::init();
        notify_host.sink([&gtts_host](std::string_view text, std::string_view title) {
            std::thread([&gtts_host, text=std::string{text}, title=std::string{title}] {
            try {
                radio::host notifications_radio_host;
                gtts_host.tts_job(std::format("{}: {}", title, text), [&notifications_radio_host](std::string_view file_produced) {
                    std::string file{file_produced.data(), file_produced.size()};
                    notifications_radio_host.play_sync(file);
                });
            }
            catch(const std::exception &e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }}).detach();
        });

        notify::screen notify_screen{notify_host};

        radio::host radio_host;
        constexpr std::string_view lastplayed_key {"radio.last_played"};
        if (auto last_played = fconfig->get(std::string{lastplayed_key}); last_played)
        {
            radio_host.play(*last_played);
        }

        conversions::screen conv_screen{};

        cppgpt::screen gpt_screen;

        std::shared_ptr<screen_tabs> tabs;

        std::vector<std::string> loaded_panels;

        std::vector<group_t> all_tabs;
        std::function<void(std::string_view)> menu_tabs;
        std::shared_ptr<main_screen<split_screen>> screen;
        menu_tabs = [&tabs, &all_tabs, &loaded_panels, &localhost, &menu_tabs, &screen](std::string_view key)
        {
            if (key == "Main")
            {
                if (ImGui::BeginMenu("Tabs"))
                {
                    for(auto const &tab : all_tabs)
                    {
                        if (ImGui::MenuItem(tab.name.c_str()))
                        {
                            tabs->select(tab.name);
                        }
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

        all_tabs = {
            {ICON_MD_NOTIFICATIONS " Notifications", [&notify_screen] { notify_screen.render(); }, menu_tabs}
        };

        nlohmann::json all_tabs_json;
        // load from "beatograph.json"
        if (std::filesystem::exists("beatograph.json"))
        {
            std::ifstream file{"beatograph.json"};
            file >> all_tabs_json;
        }

        std::unordered_map<std::string, std::shared_ptr<toggl::screen>> toggl_screens_by_id;

        std::map<std::string, std::function<group_t(nlohmann::json::object_t const&)>> factories = {
            {"calendar",
                [&menu_tabs, &localhost] (nlohmann::json::object_t const& cal) {
                    return group_t{cal.at("title"), 
                        [cs = calendar::screen{std::make_shared<calendar::host>(localhost.resolve_environment(cal.at("endpoint")))}]() mutable { cs.render(); }, 
                        menu_tabs};}},
            {"ssh-hosts",
                [&menu_tabs, &localhost] (nlohmann::json::object_t const&) {
                    return group_t{ICON_MD_SETTINGS_REMOTE " SSH Hosts", 
                        [&localhost] { ssh::screen_all{}.render(localhost); },
                    menu_tabs};}},
            {"toggl",
                [&menu_tabs, &localhost, &notify_host, &toggl_screens_by_id](nlohmann::json::object_t const& node) {
                    auto ts {std::make_shared<toggl::screen>(std::make_shared<toggl::client>(
                            [&notify_host](std::string_view text) { notify_host(text, "Toggl"); }), 
                            static_cast<int>(node.at("daily_goal").get<float>() * 3600))};
                    if (node.contains("id")) {
                        toggl_screens_by_id[node.at("id")] = ts;
                    }
                    return group_t {
                        node.at("title"),
                        [ts]() mutable {
                            ts->render();
                        },
                        menu_tabs};}},
            {"jira",
                [&cache, &localhost, &menu_tabs_and, &toggl_screens_by_id] (nlohmann::json::object_t const& node) mutable {
                    auto js = std::make_shared<jira::screen>(cache);
                    jira::issue_screen::context_actions_t actions;
                    if (node.contains("integrate")) {
                        for (std::string const &action : node.at("integrate")) {
                            actions[ICON_MD_PUNCH_CLOCK " Start Toggl"] = [js, ts = toggl_screens_by_id.at(action)](nlohmann::json const &entry) {
                                auto const activity_description {std::format("{} - {}", 
                                    entry.at("fields").at("summary").get<std::string>(), 
                                    entry.at("key").get<std::string>())};
                                ts->start_time_entry(activity_description);
                            };
                        }
                    }
                    auto jh = std::make_shared<jira::host>(
                            localhost.resolve_environment(node.at("username")),
                            localhost.resolve_environment(node.at("token")),
                            localhost.resolve_environment(node.at("endpoint"))
                         );
                    return group_t{
                        node.at("title"), 
                        [actions, js, jh]() mutable { js->render(*jh, actions); },
                        menu_tabs_and([&js](std::string_view item)
                           { js->render_menu(item); }),
                         ImVec4(0.5f, 0.5f, 1.0f, 1.0f)};}},
             {"git-repositories",
                [&cache, &localhost, &menu_tabs] (nlohmann::json::object_t const& node) mutable {
                    return group_t {
                        ICON_MD_CODE " Git Repositories", 
                    [git_screen = std::make_shared<git::screen>(std::make_shared<git::host>(localhost, node.at("root")))]
                    {
                        git_screen->render();
                    },
                    menu_tabs};}},
            {"radio",
                [&menu_tabs, &notify_host, &radio_host, &cache, &localhost] (nlohmann::json::object_t const& ) {
                    auto podcast_host = std::make_shared<rss::host>([&localhost](std::string_view command) -> std::string {
                        return localhost.execute_command(command);
                    });
                    load_podcasts(podcast_host, [&notify_host](auto text){ notify_host(text, "RSS"); });
                    return group_t {radio_tab_name, [
                        rss_screen = std::make_shared<rss::screen>(podcast_host,
                                        [&radio_host](std::string_view url)
                                        {
                                            radio_host.play(std::string{url});
                                        },
                                        cache,
                                        [&localhost](std::string_view text) {
                                            return localhost.execute_command(text, false); }),
                        radio_screen = std::make_shared<radio::screen>(radio_host, cache)
                        ]() mutable
                        {
                            radio_screen->render();
                            rss_screen->render();
                        },
                        menu_tabs, ImVec4(0.05f, 0.5f, 0.05f, 1.0f)};
                }},
            {"world-clocks",
                [&cache, &menu_tabs_and, &localhost, &notify_host, &gpt](nlohmann::json::object_t const &node) {
                    auto weather_host = std::make_shared<weather::openweather_client>(localhost.resolve_environment(node.at("openweather_key")));
                    auto host = std::make_shared<clocks::host>();
                    host->set_weather_client(weather_host);
                    try {
                        auto const local_data {localhost.get_my_ip_and_geolocation()};
                        host->add_city(std::format("{}, {}", 
                            local_data.at("city").get_ref<std::string const &>(),
                            local_data.at("region").get_ref<std::string const &>()));
                    }
                    catch(const std::exception &e) {
                        notify_host(e.what(), "Clocks");
                    }
                    for (std::string const &city : node.at("cities")) {
                        host->add_city(city);
                    }
                    auto clocks_screen = std::make_shared<clocks::screen>(
                        host,
                        cache, 
                        []{ return views::quitting(); }, 
                        std::move(gpt.new_conversation()), 
                        [&notify_host](std::string_view text) { notify_host(text, "Clocks"); });

                    return group_t{ICON_MD_WATCH " Clocks", [clocks_screen, &menu_tabs_and] { clocks_screen->render(); }, 
             menu_tabs_and([clocks_screen](auto i) {  clocks_screen->render_menu(i); })};
                }}
        };

        for (nlohmann::json::object_t const &tab : all_tabs_json.at("tabs"))
        {
            auto const &name = tab.at("type").get_ref<std::string const &>();
            if (!factories.contains(name))
            {
                std::cerr << "Unknown tab: " << name << std::endl;
                continue;
            }
            auto const &factory = factories.at(name);
            all_tabs.push_back(factory(tab));
        }

        tabs = std::make_shared<screen_tabs>(all_tabs, [&screen](std::string_view tab_name) { screen->set_title(tab_name); });

        auto tools = std::make_shared<tool_screen>(std::vector<group_t> {
            {ICON_MD_CURRENCY_EXCHANGE " Conversions", [&conv_screen]
             { conv_screen.render(); }, menu_tabs},
            {ICON_MD_CHAT_BUBBLE " AI", [&gpt_screen, &gpt, &notify_host]
             {  gpt_screen.render(gpt, [&notify_host](std::string_view text) { 
                    notify_host(text, "AI");
              }); },
                menu_tabs, 
                ImVec4(0.75f, 0.75f, 0.75f, 1.0f)},
            {ICON_MD_COMPUTER " Local Host", [&menu_tabs, ls = hosting::local::screen{localhost}]
             { ls.render(); }, menu_tabs}
        });
        
        auto split = std::make_shared<split_screen>([&tabs]{ tabs->render(); }, [tools]{ tools->render(); });

        screen = std::make_shared<main_screen<split_screen>>(split);

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

        screen->run(
            [&notify_host](std::string_view text) { notify_host(text, "Main"); },
            [&tabs] {
            if (ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
                // if (ImGui::IsKeyPressed(ImGuiKey_J)) {
                //     tabs->select(jira_tab_name);
                // }
                if (ImGui::IsKeyPressed(ImGuiKey_R)) {
                    tabs->select(radio_tab_name);
                }
            } });

        // save toggl_logins
        save_services<toggl::login::host>(fconfig, toggl_login_base);

        fconfig->set(std::string{lastplayed_key}, radio_host.last_played());
        fconfig->save();

        views::quitting(true);
    }
    std::cerr << "Terminating.\n";

    return 0;
}
