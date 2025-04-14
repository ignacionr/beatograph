#pragma once

#include <map>
#include <string>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <imgui.h>

#include "group_t.hpp"
#include "registrar.hpp"
#include "names.hpp"

#include "cloud/github/host.hpp"
#include "cloud/github/screen.hpp"
#include "pm/calendar/screen.hpp"
#include "util/clocks/pomodoro_screen.hpp"

#include "pm/toggl/toggl_client.hpp"
#include "pm/toggl/toggl_screen.hpp"

#include "pm/jira/host.hpp"
#include "pm/jira/screen.hpp"

#include "cloud/deel/host.hpp"
#include "cloud/deel/screen.hpp"


struct screen_factories {
    static std::map<std::string, std::function<group_t(nlohmann::json::object_t const &)>> map(auto &menu_tabs, auto &menu_tabs_and, auto &notify_host, auto &radio_host, auto &localhost, auto &cache, auto &gpt, auto &toggl_screens_by_id, auto &ssh_all, auto &mappings)
    {

        return {
        {"calendar",
            [&menu_tabs, localhost](nlohmann::json::object_t const &cal)
            { 
                auto calendar_host {std::make_shared<calendar::host>(localhost->resolve_environment(cal.at("endpoint")))};
                registrar::add(cal.at("title"), calendar_host);
                return group_t{cal.at("title"),
                                [cs = calendar::screen{calendar_host}]() mutable
                                { cs.render(); },
                                menu_tabs}; }},
        {"deel",
            [&menu_tabs](auto const &) {
                auto host = std::make_shared<cloud::deel::host<http::fetch>>();
                auto screen = std::make_shared<cloud::deel::screen>();
                return group_t{ICON_MD_ACCOUNT_BALANCE " Deel",
                               [screen, host] { screen->render(host); },
                               menu_tabs}; }},
        {"github",
         [&menu_tabs](nlohmann::json::object_t const &node)
         {
                auto host = std::make_shared<github::host>();
                auto screen = std::make_shared<github::screen>(host, node.at("login_name").get_ref<const std::string&>());
                return group_t{ICON_MD_CODE " GitHub", 
                    [screen] { screen->render(); },
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
                auto jh = std::make_shared<jira::host>(
                        localhost->resolve_environment(node.at("username")),
                        localhost->resolve_environment(node.at("token")),
                        localhost->resolve_environment(node.at("endpoint"))
                    );
                auto js = std::make_shared<jira::screen>(jh);
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
                return group_t {
                    node.at("title"), 
                    [actions, js, jh]() mutable { js->render(actions); },
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
             return group_t{names::radio_tab_name, [rss_screen = std::make_shared<media::rss::screen>(podcast_host, [&radio_host](std::string_view url)
                                                                                        { radio_host.play(std::string{url}); }, [localhost](std::string_view text)
                                                                                        { return localhost->execute_command(text, false); }),
                                             radio_screen = std::make_shared<radio::screen>(radio_host)]() mutable
                            {
                        radio_screen->render();
                        rss_screen->render(); },
                            menu_tabs, ImVec4(0.05f, 0.5f, 0.05f, 1.0f)};
         }},
         {"whatsapp", [&mappings, &cache, &menu_tabs](nlohmann::json::object_t const &) {
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
            "file-selection", [&menu_tabs](nlohmann::json::object_t const &config) {
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
            "email", [&menu_tabs, &localhost](nlohmann::json::object_t const& node) {
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
        }
};