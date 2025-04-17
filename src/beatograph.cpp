#pragma execution_character_set(push, "utf-8")
#include "../external/IconsMaterialDesign.h"
#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <numeric>
#include <print>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_map>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
using SOCKET = int;
#endif

#include <imgui/imgui.h>
#define STB_TRUETYPE_IMPLEMENTATION
#define STB_RECT_PACK_IMPLEMENTATION
// #include <imgui/imstb_truetype.h>
// #include <imgui/imstb_rectpack.h>

#include <stb/stb_rect_pack.h>
#include <stb/stb_truetype.h>

#include "main_screen.hpp"
#include "screen_tabs.hpp"
#include "cloud/metrics/metrics_parser.hpp"
#include "cloud/metrics/metrics_model.hpp"
#include "cloud/metrics/metrics_screen.hpp"
#include "pm/toggl/toggl_client.hpp"
#include "pm/toggl/toggl_screen.hpp"
#include "pm/calendar/uni_notice.hpp"
#include "hosting/host.hpp"
#include "hosting/host_local.hpp"
#include "hosting/local_screen.hpp"
#include "hosting/ssh/screen_all.hpp"
#include "../external/cppgpt/cppgpt.hpp"
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
#include "split_screen.hpp"
#include "tool_screen.hpp"
#include "structural/config/file_config.hpp"
#include "registrar.hpp"
#include "pm/toggl/login/host.hpp"
#include "cloud/github/login_host.hpp"
#include "util/clocks/pomodoro_screen.hpp"
#include "structural/text_command/host.hpp"
#include "structural/text_command/screen.hpp"
#include "hosting/telnet/host.hpp"
#include "cloud/whatsapp/screen.hpp"
#include "cloud/whatsapp/host.hpp"
#include "cloud/mail/imap_host.hpp"
#include "cloud/mail/mail_screen.hpp"
#include "file_selection/screen.hpp"
#include "factories.hpp"
#include "names.hpp"
#include "services/summarize.hpp"
#include "font_setup.hpp"
#include "panel_loader.hpp"
#include "service_utils.hpp"

#if defined(_WIN32)
int WinMain(HINSTANCE, HINSTANCE, LPSTR szArguments, int)
{
#else
int main(int argc, char *argv[])
{
#endif
    {
#if defined(_WIN32)
        std::string args {szArguments};
#else
        // get the arguments from the command line
        std::string args;
        for (int i = 1; i < argc; ++i) {
            args += argv[i];
            args += ' ';
        }
        if (!args.empty()) {
            args.pop_back();
        }
#endif
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
#if defined(_WIN32)
                WSADATA wsaData;
                if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                    throw std::runtime_error("WSAStartup failed");
                }
#endif
                // connect to localhost port 23
                SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#if defined(_WIN32)
                if (s == INVALID_SOCKET) {
                    throw std::runtime_error("socket failed");
                }
#else
                if (s == -1) {
                    throw std::runtime_error("socket failed");
                }
#endif
                sockaddr_in service;
                service.sin_family = AF_INET;
                if (inet_pton(AF_INET, "127.0.0.1", &service.sin_addr) <= 0) {
                    throw std::runtime_error("inet_pton failed");
                }
                service.sin_port = htons(hosting::telnet::host::port);
#if defined(_WIN32)
                if (connect(s, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
                    closesocket(s);
                    throw std::runtime_error("connect failed");
                }
#else 
                if (connect(s, (struct sockaddr*)&service, sizeof(service)) == -1) {
                    close(s);
                    throw std::runtime_error("connect failed");
                }
#endif
                // send the command
                std::string command {args};
                command += "\n";
#if defined(_WIN32)
                if (send(s, command.c_str(), static_cast<int>(command.size()), 0) == SOCKET_ERROR) {
                    closesocket(s);
                    throw std::runtime_error("send failed");
                }
#else
                if (send(s, command.c_str(), static_cast<int>(command.size()), 0) == -1) {
                    close(s);
                    throw std::runtime_error("send failed");
                }
#endif
                // receive the response
                std::string response;
                for (;;) {
                    char buffer[1024];
                    int bytesReceived = recv(s, buffer, sizeof(buffer), 0);
#if defined(_WIN32)
                    if (bytesReceived == SOCKET_ERROR) {
                        std::cerr << "recv failed" << std::endl;
                        closesocket(s);
                        return 1;
                    }
#else 
                    if (bytesReceived == -1) {
                        std::cerr << "recv failed" << std::endl;
                        close(s);
                        return 1;
                    }
#endif
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
                std::println("Error: {}", e.what());
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

        auto summarize_fn = std::make_shared<std::function<std::string(std::string_view)>>(services::summarize{});
        registrar::add({}, summarize_fn);

        auto open_fn = std::make_shared<std::function<void(std::string const &)>> ([localhost](std::string const &command) {
            localhost->open_content(command);
        });
        registrar::add("open", open_fn);

        gtts::host gtts_host{"./gtts_cache"};
        notify::host notify_host;
        auto notify_service = std::make_shared<std::function<void(std::string const &)>>([&notify_host](std::string const &text){ 
            notify_host(text); 
        });
        registrar::add("notify", notify_service);
        radio::host::init();

        auto say = [&gtts_host](std::string text) {
            try {
                radio::host notifications_radio_host{false};
                gtts_host.tts_job(text, [&notifications_radio_host](std::string_view file_produced) {
                    std::string file{file_produced.data(), file_produced.size()};
                    notifications_radio_host.play_sync(file);
                });
            }
            catch(const std::exception &e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        };
        registrar::add("say", std::make_shared<std::function<void(std::string_view)>>([&gtts_host, &say](std::string_view text) {
            say(std::string{text});
        }));

        bool quiet{fconfig->get("quiet").value_or("false") == "true"};
        notify_host.sink([&gtts_host, &quiet, &say](std::string_view text, std::string_view title)
                         { std::thread([&gtts_host, &quiet, text = std::string{text}, &say, title = std::string{title}]
                                       {
            if (!quiet) {
                auto full_text = std::format("{}: {}", title, text);
                say(full_text);
            }
        })
                               .detach(); });

        notify::screen notify_screen{notify_host};

        text_command_host->add_source(cloud::cppgpt::command::create_source(gpt));

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
        std::shared_ptr<tool_screen> tools;

        menu_tabs = [&tabs, &all_tabs, &loaded_panels, localhost, &menu_tabs, &screen](std::string_view key)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_F6)) {
                screen->contents()->show_right() = !screen->contents()->show_right();
            }
            if (key == "View") {
                ImGui::MenuItem("Tools Panel", "F6", &screen->contents()->show_right());
            }
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

        auto factories = screen_factories::map(menu_tabs, menu_tabs_and, notify_host, radio_host, localhost, cache, gpt, toggl_screens_by_id, ssh_all, mappings);
        // auto factories = screen_factories::map(menu_tabs, menu_tabs_and, notify_host, radio_host, localhost, cache, gpt, toggl_screens_by_id, ssh_all, mappings);

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

        tools = std::make_shared<tool_screen>(std::vector<group_t>{
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

        auto split = std::make_shared<split_screen>(
            [&tabs] { tabs->render(); }, 
            [&tools] { tools->render(); }, 
            [&tabs] (std::string_view item) { tabs->render_menu(item); }, 
            [&tools](std::string_view item) { tools->render_menu(item); });

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
                    tabs->select(names::radio_tab_name);
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
