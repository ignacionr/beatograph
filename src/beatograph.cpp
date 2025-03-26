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
#include <stdexcept>
#include <unordered_map>

#include <imgui.h>

#include "main_screen.hpp"
#include "screen_tabs.hpp"
#include "cloud/metrics/metrics_parser.hpp"
#include "cloud/metrics/metrics_model.hpp"
#include "cloud/metrics/metrics_screen.hpp"
#include "pm/toggl/toggl_client.hpp"
#include "pm/toggl/toggl_screen.hpp"
#include "pm/jira/host.hpp"
#include "pm/jira/screen.hpp"
#include "pm/calendar/screen.hpp"
#include "pm/calendar/uni_notice.hpp"
#include "hosting/host.hpp"
#include "hosting/host_local.hpp"
#include "hosting/local_screen.hpp"
#include "hosting/ssh/screen_all.hpp"
#include <cppgpt/cppgpt.hpp>
#include "structural/views/assertion.hpp"
#include "git/host.hpp"
#include "git/screen.hpp"
#include "media/radio/host.hpp"
#include "media/radio/screen.hpp"
#include "media/rss/host.hpp"
#include "media/rss/screen.hpp"
#include "imgcache.hpp"
#include "util/conversions/screen.hpp"
#include "util/clocks/screen.hpp"
#include "util/clocks/weather.hpp"
#include "hosting/notify/host.hpp"
#include "hosting/notify/screen.hpp"
#include "structural/panel/config.hpp"
#include "cloud/cppgpt/screen.hpp"
#include "cloud/cppgpt/command.hpp"
#include "cloud/gtts/host.hpp"
#include "report/host.hpp"
#include "group_t.hpp"
#include "split_screen.hpp"
#include "tool_screen.hpp"
#include "structural/config/file_config.hpp"
#include "registrar.hpp"
#include "pm/toggl/login/host.hpp"
#include "cloud/github/host.hpp"
#include "cloud/github/screen.hpp"
#include "cloud/github/login_host.hpp"
#include "util\clocks\pomodoro_screen.hpp"
#include "structural/text_command/host.hpp"
#include "structural/text_command/screen.hpp"
#include "hosting/telnet/host.hpp"
#include "cloud/whatsapp/screen.hpp"
#include "cloud/whatsapp/host.hpp"
#include "cloud/mail/imap_host.hpp"
#include "cloud/mail/mail_screen.hpp"
#include "file_selection/screen.hpp"

void setup_fonts()
{
    auto &io{ImGui::GetIO()};
    float baseFontSize = 13.5f;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());   // Default ASCII
    builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());  // Cyrillic
    builder.AddChar(static_cast<ImWchar>(0x1F4C5));  // Unicode Calendar 📅 (U+1F4C5)
    builder.AddChar(static_cast<ImWchar>(0x2019));   // Special apostrophe (Right Single Quotation Mark ’ U+2019)
    builder.AddChar(static_cast<ImWchar>(0x2603));   // Unicode Snowman ☃ (U+2603)

    static ImVector<ImWchar> glyphRanges;
    builder.BuildRanges(&glyphRanges);
    io.Fonts->AddFontFromFileTTF("assets/fonts/NotoSansMono-Regular.ttf", baseFontSize, nullptr, glyphRanges.Data);

    float iconFontSize = baseFontSize * 3.0f / 3.0f;

    // merge in icons from Font Awesome
    static const ImWchar icons_ranges[] = {
        static_cast<ImWchar>(ICON_MIN_MD),
        static_cast<ImWchar>(ICON_MAX_MD), 0};
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.GlyphOffset.y = 3;
    io.Fonts->AddFontFromFileTTF("assets/fonts/MaterialIcons-Regular.ttf", iconFontSize, &icons_config, icons_ranges);

    // font[1] for important content text
    io.Fonts->AddFontFromFileTTF("assets/fonts/Montserrat-Regular.ttf", 16.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
    // font[2] for titles
    io.Fonts->AddFontFromFileTTF("assets/fonts/Montserrat-Bold.ttf", 23.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
}

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

template <typename T>
void load_services(auto config, auto key_base)
{
    config->scan_level(key_base, [config, key_base](auto const &subkey)
                       {
        auto login = std::make_shared<T>();
        login->load_from([config, key_base, subkey](std::string_view k){ return config->get(std::format("{}{}.{}", key_base, subkey,k)); });
        registrar::add<T>(subkey, login); });
}

template <typename T>
void save_services(auto config, auto key_base)
{
    registrar::all<T>([config, key_base](std::string const &key, auto login)
                      { login->save_to([config, key, key_base](std::string_view k, std::string v)
                                       { config->set(std::format("{}{}.{}", key_base, key, k), v); }); });
}

#if defined(_WIN32)
int WinMain(HINSTANCE, HINSTANCE, LPSTR szArguments, int)
{
#else
int main()
{
#endif
    {
        std::string args {szArguments};
        if (!args.empty()) {
            try {
                if (args.front() == '"') {
                    args = args.substr(1);
                    if (args.back() == '"') {
                        args.pop_back();
                    }
                }
                if (!args.starts_with("beatograph:")) {
                    throw std::runtime_error(std::format("Invalid protocol: {}", args));
                }
                else args = args.substr(11);
                // replace every %20 with a space
                for (auto pos {args.find("%20")}; pos != std::string::npos; pos = args.find("%20", pos)) {
                    args.replace(pos, 3, " ");
                }
                // initialize sockets
                WSADATA wsaData;
                if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                    throw std::runtime_error("WSAStartup failed");
                }
                // connect to localhost port 23
                SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (s == INVALID_SOCKET) {
                    throw std::runtime_error("socket failed");
                }
                sockaddr_in service;
                service.sin_family = AF_INET;
                if (inet_pton(AF_INET, "127.0.0.1", &service.sin_addr) <= 0) {
                    throw std::runtime_error("inet_pton failed");
                }
                service.sin_port = htons(23);
                if (connect(s, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
                    closesocket(s);
                    throw std::runtime_error("connect failed");
                }
                // send the command
                std::string command {args};
                command += "\n";
                if (send(s, command.c_str(), static_cast<int>(command.size()), 0) == SOCKET_ERROR) {
                    closesocket(s);
                    throw std::runtime_error("send failed");
                }
                // receive the response
                std::string response;
                for (;;) {
                    char buffer[1024];
                    int bytesReceived = recv(s, buffer, sizeof(buffer), 0);
                    if (bytesReceived == SOCKET_ERROR) {
                        std::cerr << "recv failed" << std::endl;
                        closesocket(s);
                        return 1;
                    }
                    else if (bytesReceived == 0) {
                        break;
                    }
                    response += std::string(buffer, bytesReceived);
                    if (response.back() == '\n') {
                        response.pop_back();
                        break;
                    }
                }
            }
            catch(std::exception const &e) {
                ::MessageBoxA(nullptr, e.what(), "Error", MB_OK|MB_ICONERROR);
            }
            return 0;
        }

        auto fconfig{std::make_shared<config::file_config>("./beatograph.ini")};
        registrar::add({}, fconfig);

        // load toggl_logins
        static std::string_view constexpr toggl_login_base{"toggl_login."};
        static std::string_view constexpr github_login_base{"github_login."};

        load_services<toggl::login::host>(fconfig, toggl_login_base);
        load_services<github::login::host>(fconfig, github_login_base);

        // auto constexpr happy_bell_sound = "assets/mixkit-happy-bell-alert-601.wav";
        // auto constexpr impact_sound = "assets/mixkit-underground-explosion-impact-echo-1686.wav";
        auto static constexpr radio_tab_name{ICON_MD_AUDIOTRACK " Radio"};

        auto localhost = std::make_shared<hosting::local::host>();
        registrar::add({}, localhost);

        auto cache{std::make_shared<img_cache>("imgcache")};
        registrar::add({}, cache);

        auto text_command_host = std::make_shared<structural::text_command::host>();
        registrar::add({}, text_command_host);

        text_command_host->add_source({
            [](std::string const &, std::function<void(std::string const &)> callback) {
            },
            [](std::string const &command) -> std::optional<std::string> {
                if (command.starts_with("auth/")) {
                    auto const &auth = command.substr(5);
                    if (auto const pos = auth.find('/'); pos != std::string::npos) {
                        auto const &service = auth.substr(0, pos);
                        auto const &token = auth.substr(pos + 1);
                        auto env_variable = std::format("{}_TOKEN", service);
                        auto localhost = registrar::get<hosting::local::host>({});
                        localhost->set_env_variable(env_variable, token);
                        return env_variable;
                    }
                }
                return std::nullopt;
            }
        });

        // get the Groq API key from GROQ_API_KEY
        auto grok_api_key = localhost->get_env_variable("GROK_API_KEY");
        auto gpt = std::make_shared<ignacionr::cppgpt> (grok_api_key, ignacionr::cppgpt::grok_base);
        registrar::add({}, gpt);

        gtts::host gtts_host{"./gtts_cache"};
        notify::host notify_host;
        auto notify_service = std::make_shared<std::function<void(std::string_view)>>([&notify_host](std::string_view text){ 
            notify_host(text); 
        });
        registrar::add("notify", notify_service);
        radio::host::init();
        bool quiet{fconfig->get("quiet").value_or("false") == "true"};
        notify_host.sink([&gtts_host, &quiet](std::string_view text, std::string_view title)
                         { std::thread([&gtts_host, &quiet, text = std::string{text}, title = std::string{title}]
                                       {
            if (!quiet) {
            try {
                radio::host notifications_radio_host{false};
                gtts_host.tts_job(std::format("{}: {}", title, text), [&notifications_radio_host](std::string_view file_produced) {
                    std::string file{file_produced.data(), file_produced.size()};
                    notifications_radio_host.play_sync(file);
                });
            }
            catch(const std::exception &e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }} })
                               .detach(); });

        notify::screen notify_screen{notify_host};

        text_command_host->add_source(cloud::cppgpt::command::create_source(gpt, *notify_service));

        radio::host radio_host;
        constexpr std::string_view lastplayed_key{"radio.last_played"};
        if (auto last_played = fconfig->get(std::string{lastplayed_key}); last_played)
        {
            long long last_played_time {};
            if (auto last_played_time_str = fconfig->get("radio.last_run"); last_played_time_str) {
                last_played_time = std::stoll(*last_played_time_str);
            }
            radio_host.play(*last_played, std::chrono::milliseconds(last_played_time));
        }

        conversions::screen conv_screen{};

        cppgpt::screen gpt_screen;

        std::shared_ptr<screen_tabs> tabs;

        std::vector<std::string> loaded_panels;

        std::vector<group_t> all_tabs;
        std::function<void(std::string_view)> menu_tabs;
        std::shared_ptr<main_screen<split_screen>> screen;
        menu_tabs = [&tabs, &all_tabs, &loaded_panels, localhost, &menu_tabs, &screen](std::string_view key)
        {
            if (key == "Main")
            {
                if (ImGui::GetIO().KeyAlt && ImGui::IsKeyPressed(ImGuiKey_T))
                {
                    ImGui::OpenPopup("Tabs");
                }
                if (ImGui::BeginMenu("Tabs"))
                {
                    for (auto const &tab : all_tabs)
                    {
                        if (ImGui::MenuItem(tab.name.c_str()))
                        {
                            tabs->select(tab.name);
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Reload Panels"))
                    {
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

        auto set_quiet = [&quiet, fconfig](bool q)
        {
            quiet = q;
            fconfig->set("quiet", quiet ? "true" : "false");
        };

        text_command_host->add_command("Quiet Notifications", [set_quiet, &quiet]
                                       { set_quiet(true); });
        text_command_host->add_command("Unquiet (audible) Notifications", [set_quiet, &quiet]
                                       { set_quiet(false); });

        all_tabs = {
            {ICON_MD_NOTIFICATIONS " Notifications", [&notify_screen]
             { notify_screen.render(); }, menu_tabs_and([set_quiet, &quiet, &fconfig](std::string_view item)
                                                        {
                                                            if (item == "Main") {
                                                                if (ImGui::BeginMenu("Settings"))
                                                                {
                                                                    if (ImGui::Checkbox("Quiet", &quiet)) {
                                                                        set_quiet(quiet);
                                                                    }
                                                                    ImGui::EndMenu();
                                                                }
                                                            }
                                                         })}};

        nlohmann::json all_tabs_json;
        // load from "beatograph.json"
        if (std::filesystem::exists("beatograph.json"))
        {
            std::ifstream file{"beatograph.json"};
            file >> all_tabs_json;
        }

        std::unordered_map<std::string, std::shared_ptr<toggl::screen>> toggl_screens_by_id;

        auto quitting = std::make_shared<std::function<bool()>>([]{ return views::quitting(); });
        registrar::add("quitting", quitting);

        auto telnet_host = std::make_shared<hosting::telnet::host>();
        try {
            telnet_host->run(*quitting, [text_command_host](std::string_view command) -> std::string
            {
                std::string response;
                try {
                    (*text_command_host)(std::string{command});
                    response = "OK";
                }
                catch (std::exception const &e) {
                    response = std::format("Error: {}", e.what());
                }
                return response;
            });
        }
        catch(std::exception const &e) {
            notify_host(std::format("Telnet Host: {}", e.what()));
        }

        // set up port mappings, if there are any
        std::unordered_map<std::string, std::shared_ptr<hosting::local::mapping>> mappings;
        if (all_tabs_json.contains("mappings")) {
            for (auto const &mapping : all_tabs_json.at("mappings")) {
                auto const port = mapping.at("port").get<u_short>();
                auto const host = mapping.at("host").get_ref<std::string const &>();
                mappings.emplace(mapping.at("name").get<std::string>(), std::make_shared<hosting::local::mapping>(port, host, localhost));
            }
        }


        ssh::screen_all ssh_all{};

        std::map<std::string, std::function<group_t(nlohmann::json::object_t const &)>> factories = {
            {"github",
             [&menu_tabs](nlohmann::json::object_t const &node)
             {
                    auto host = std::make_shared<github::host>();
                    auto screen = std::make_shared<github::screen>(host, node.at("login_name").get_ref<const std::string&>());
                    return group_t{ICON_MD_CODE " GitHub", 
                        [screen] { screen->render(); },
                        menu_tabs}; }},
            {"calendar",
             [&menu_tabs, localhost](nlohmann::json::object_t const &cal)
             { 
                auto calendar_host {std::make_shared<calendar::host>(localhost->resolve_environment(cal.at("endpoint")))};
                registrar::add(cal.at("title"), calendar_host);
                return group_t{cal.at("title"),
                              [cs = calendar::screen{calendar_host}]() mutable
                              { cs.render(); },
                              menu_tabs}; }},
            {"pomodoro",
                [&menu_tabs](nlohmann::json::object_t const &pomodoro_item) {
                    return group_t{
                        pomodoro_item.at("title"),
                        [pom = clocks::pomodoro_screen(std::make_shared<clocks::pomodoro>())] () mutable -> void {
                            pom.render();
                        },
                        menu_tabs
                    };
                }
            },
            {"ssh-hosts",
             [&menu_tabs, localhost, &ssh_all](nlohmann::json::object_t const &)
             { return group_t{ICON_MD_SETTINGS_REMOTE " SSH Hosts",
                              [localhost, &ssh_all]
                              { 
                                ssh_all.render(localhost);
                            },
                              menu_tabs}; }},
            {"toggl",
             [&menu_tabs, localhost, &notify_host, &toggl_screens_by_id](nlohmann::json::object_t const &node)
             {
                    auto toggle_client {std::make_shared<toggl::client>(
                            [&notify_host](std::string_view text) { notify_host(text, "Toggl"); })};
                    std::string login_name = node.contains("login_name") ? node.at("login_name").get<std::string>() : std::string{};
                    try {
                        toggle_client->set_login(registrar::get<toggl::login::host>(login_name));
                    }
                    catch(std::exception const &e) {
                        notify_host(std::format("Toggl: {}", e.what()));
                    }
                    auto ts {
                        std::make_shared<toggl::screen>(toggle_client, 
                        static_cast<int>(node.at("daily_goal").get<float>() * 3600),
                        login_name)};
                    if (node.contains("id")) {
                        toggl_screens_by_id[node.at("id")] = ts;
                    }
                    return group_t {
                        node.at("title"),
                        [ts]() mutable {
                            ts->render();
                        },
                        menu_tabs}; }},
            {"jira",
             [localhost, &menu_tabs_and, &toggl_screens_by_id](nlohmann::json::object_t const &node) mutable
             {
                    auto js = std::make_shared<jira::screen>();
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
                            localhost->resolve_environment(node.at("username")),
                            localhost->resolve_environment(node.at("token")),
                            localhost->resolve_environment(node.at("endpoint"))
                         );
                    return group_t{
                        node.at("title"), 
                        [actions, js, jh]() mutable { js->render(*jh, actions); },
                        menu_tabs_and([js](std::string_view item)
                           { js->render_menu(item); }),
                         ImVec4(0.5f, 0.5f, 1.0f, 1.0f)}; }},
            {"git-repositories",
             [&cache, localhost, &menu_tabs](nlohmann::json::object_t const &node) mutable
             { return group_t{
                   ICON_MD_CODE " Git Repositories",
                   [git_screen = std::make_shared<git::screen>(std::make_shared<git::host>(localhost, node.at("root")))]
                   {
                       git_screen->render();
                   },
                   menu_tabs}; }},
            {"radio",
             [&menu_tabs, &notify_host, &radio_host, localhost](nlohmann::json::object_t const &)
             {
                 auto podcast_host = std::make_shared<media::rss::host>([localhost](std::string_view command) -> std::string
                                                                 { return localhost->execute_command(command); });
                 return group_t{radio_tab_name, [rss_screen = std::make_shared<media::rss::screen>(podcast_host, [&radio_host](std::string_view url)
                                                                                            { radio_host.play(std::string{url}); }, [localhost](std::string_view text)
                                                                                            { return localhost->execute_command(text, false); }),
                                                 radio_screen = std::make_shared<radio::screen>(radio_host)]() mutable
                                {
                            radio_screen->render();
                            rss_screen->render(); },
                                menu_tabs, ImVec4(0.05f, 0.5f, 0.05f, 1.0f)};
             }},
             {"whatsapp", [&](nlohmann::json::object_t const &) {
                 auto host = std::make_shared<cloud::whatsapp::host>(mappings.at("whatsapp"));
                 registrar::add({}, host);
                 auto screen = std::make_shared<cloud::whatsapp::screen>();
                 return group_t{ICON_MD_CHAT_BUBBLE " WhatsApp",
                                [host, screen, &cache] {
                                    screen->render(*host, *cache);
                                },
                                menu_tabs};
             }},
             {
                "file-selection", [&](nlohmann::json::object_t const &config) {
                    std::string start_path;
                    if (config.contains("start_path")) {
                        start_path = config.at("start_path").get_ref<std::string const &>();
                    }
                    auto screen = std::make_shared<file_selection::Screen>(start_path);
                    return group_t{ICON_MD_FOLDER " File Selection",
                                   [screen] {
                                       screen->render();
                                   },
                                   menu_tabs};
                }
             },
             {
                "email", [&](nlohmann::json::object_t const& node) {
                    auto host = std::make_shared<cloud::mail::imap_host>(
                        localhost->resolve_environment(node.at("host")),
                        localhost->resolve_environment(node.at("username")),
                        localhost->resolve_environment(node.at("password")));
                    auto screen = std::make_shared<cloud::mail::screen>(host);
                    return group_t{ICON_MD_EMAIL " Email",
                                   [screen] {
                                        screen->render();
                                   },
                                   menu_tabs};
                }
             },
            {"world-clocks",
             [&menu_tabs_and, localhost, &notify_host, gpt](nlohmann::json::object_t const &node)
             {
                 auto weather_host = std::make_shared<weather::openweather_client>(localhost->resolve_environment(node.at("openweather_key")));
                 auto host = std::make_shared<clocks::host>();
                 host->set_weather_client(weather_host);
                 try
                 {
                     auto const local_data{localhost->get_my_ip_and_geolocation()};
                     host->add_city(std::format("{}, {}",
                                                local_data.at("city").get_ref<std::string const &>(),
                                                local_data.at("region").get_ref<std::string const &>()));
                 }
                 catch (const std::exception &e)
                 {
                     notify_host(e.what(), "Clocks");
                 }
                 for (auto const &city : node.at("cities"))
                 {
                    if (city.is_string()) {
                        host->add_city(city.get_ref<std::string const &>());
                    }
                    else {
                        ImVec4 background_color{0.0f, 0.0f, 0.0f, 0.0f};
                        // "background_color": "#e0e000"
                        if (city.contains("background_color")) {
                            auto const &color = city.at("background_color").get_ref<std::string const &>();
                            background_color = ImVec4{std::stoi(color.substr(1, 2), nullptr, 16) / 255.0f,
                                                      std::stoi(color.substr(3, 2), nullptr, 16) / 255.0f,
                                                      std::stoi(color.substr(5, 2), nullptr, 16) / 255.0f,
                                                      0.25f};
                        }
                        host->add_city(city.at("place").get_ref<std::string const &>(), background_color);
                    }
                 }
                 auto clocks_screen = std::make_shared<clocks::screen>(
                     host,
                     std::move(gpt->new_conversation()),
                     [&notify_host](std::string_view text)
                     { notify_host(text, "Clocks"); });

                 return group_t{ICON_MD_WATCH " Clocks", [clocks_screen, &menu_tabs_and]
                                { clocks_screen->render(); },
                                menu_tabs_and([clocks_screen](auto i)
                                              { clocks_screen->render_menu(i); })};
             }}};

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

        tabs = std::make_shared<screen_tabs>(all_tabs, [&screen](std::string_view tab_name)
                                             { screen->set_title(tab_name); });

        text_command_host->add_source({
            [&tabs](std::string const &partial, std::function<void(std::string const &)> callback) {
                if (partial.starts_with("focus")) {
                    tabs->tab_names([&partial,&callback](std::string_view name) {
                        if (partial.size() < 7 || name.contains(partial.substr(6))) {
                            callback(std::format("Focus on {}", name));
                        }
                        return true;
                    });
                }
            },
            [&tabs](std::string const &command) -> std::optional<std::string> {
                std::optional<std::string> result{};
                tabs->tab_names([&command, &result, &tabs](std::string_view name) {
                    if (command == std::format("Focus on {}", name)) {
                        tabs->select(name);
                        result = name;
                        return false;
                    }
                    return true;
                });
                return result;
            }
        });

        structural::text_command::screen text_command_screen{*text_command_host};

        auto tools = std::make_shared<tool_screen>(std::vector<group_t>{
            {ICON_MD_CALENDAR_TODAY " Current Events", [uni_notice = std::make_shared<calendar::uni_notice>()]{ uni_notice->render(); }, menu_tabs},
            {ICON_MD_CURRENCY_EXCHANGE " Conversions", [&conv_screen]
             { conv_screen.render(); }, menu_tabs},
            {ICON_MD_CHAT_BUBBLE " AI", [&gpt_screen, gpt, &notify_host]
             { gpt_screen.render(gpt, [&notify_host](std::string_view text)
                                 { notify_host(text, "AI"); }); },
             menu_tabs,
             ImVec4(0.75f, 0.75f, 0.75f, 1.0f)},
            {ICON_MD_KEYBOARD_COMMAND " Commands", [&text_command_screen]
             { text_command_screen.render(); }, menu_tabs},
            {ICON_MD_COMPUTER " Local Host", [&menu_tabs, ls = hosting::local::screen{localhost}]
             { ls.render(); }, menu_tabs},
             { ICON_MD_CIRCLE " Pomodoro",
                        [pom = clocks::pomodoro_screen(std::make_shared<clocks::pomodoro>())] () mutable -> void {
                            pom.render();
                        },
                        menu_tabs}
            });

        auto split = std::make_shared<split_screen>([&tabs]
                                                    { tabs->render(); }, [tools]
                                                    { tools->render(); }, [&tabs](std::string_view item)
                                                    { tabs->render_menu(item); }, [&tools](std::string_view item)
                                                    { tools->render_menu(item); });

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

        text_command_host->add_command("Quit", [screen]
                            { screen->quit(); });
        
        screen->run(
            [&notify_host](std::string_view text)
            { notify_host(text, "Main"); },
            [&tabs, &tools]
            {
            if (ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
                // if (ImGui::IsKeyPressed(ImGuiKey_J)) {
                //     tabs->select(jira_tab_name);
                // }
                if (ImGui::IsKeyPressed(ImGuiKey_R)) {
                    tabs->select(radio_tab_name);
                }
                else if (ImGui::IsKeyDown(ImGuiMod_Shift) && ImGui::IsKeyPressed(ImGuiKey_P)) {
                    tools->select(ICON_MD_KEYBOARD_COMMAND " Commands");
                }
            } });

        // save toggl_logins
        save_services<toggl::login::host>(fconfig, toggl_login_base);
        save_services<github::login::host>(fconfig, github_login_base);

        auto const last_played_value {radio_host.last_played()};
        fconfig->set(std::string{lastplayed_key}, last_played_value);
        if (last_played_value) {
            long long const run {radio_host.current_run().count() - 5000};
            if (run > 0) {
                fconfig->set("radio.last_run", std::to_string(run));
            }
            else {
                fconfig->set("radio.last_run", {});
            }
        }
        fconfig->save();

        views::quitting(true);
    }
    std::cerr << "Terminating.\n";

    return 0;
}
