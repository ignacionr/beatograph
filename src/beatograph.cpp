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
#include "main_screen.hpp"
#include "screen_tabs.hpp"
#include "metrics/metrics_parser.hpp"
#include "metrics/metrics_model.hpp"
#include "metrics/metrics_screen.hpp"
#include "toggl/toggl_client.hpp"
#include "toggl/toggl_screen.hpp"
#include "jira/screen.hpp"
#include "calendar/screen.hpp"
#include "data_offering/screen.hpp"
#include "host/host.hpp"
#include "host/host_local.hpp"
#include "host/local_screen.hpp"
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


#if defined(_WIN32)
int WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#else
int main()
{
#endif
    {
        auto constexpr happy_bell_sound = "assets/mixkit-happy-bell-alert-601.wav";
        // auto constexpr impact_sound = "assets/mixkit-underground-explosion-impact-echo-1686.wav";

        img_cache cache{"imgcache"};
        // get the Groq API key from GROQ_API_KEY
        char *groq_env = nullptr;
        size_t len = 0;
        if (_dupenv_s(&groq_env, &len, "GROQ_API_KEY") || groq_env == nullptr)
        {
            std::cerr << "Error: GROQ_API_KEY environment variable not set." << std::endl;
            return 1;
        }
        std::string groq_api_key{groq_env, len - 1}; // len returns the count of all copied bytes, including the terminator
        ignacionr::cppgpt gpt{groq_api_key, ignacionr::cppgpt::groq_base};

        char *token_env = nullptr;
        len = 0;
        if (_dupenv_s(&token_env, &len, "TOGGL_API_TOKEN") || token_env == nullptr)
        {
            std::cerr << "Error: TOGGL_API_TOKEN environment variable not set." << std::endl;
            return 1;
        }
        std::string toggle_token{token_env, len - 1}; // len returns the count of all copied bytes, including the terminator
        toggl_client tc(toggle_token);
        toggl_screen ts(tc);

        jira_screen js;

        calendar_screen cs;

        host_local localhost;

        git_host git{localhost};
        dataoffering_screen ds{localhost, groq_api_key};

        cluster_report cr{localhost};
        ssh_screen ssh_screen;

        radio::host radio_host;
        radio_host.play_sync(happy_bell_sound);

        std::unique_ptr<radio::screen> radio_screen;

        dev_locked::screen dev_screen{localhost};

        rss::host rss_host;

        rss::screen rss_screen{rss_host,
            [&radio_host](std::string_view url) {
                radio_host.play(std::string{url});
            }, 
            cache
        };

        // radio::host announcements_host;
        // std::thread([&announcements_host] {
        //     auto say = [&announcements_host](std::string text) {
        //         static std::map<std::string_view, std::string_view> const replacements = {
        //             {" ", "%20"},
        //             {",", "%2C"},
        //             {".", "%2E"},
        //             {"!", "%21"},
        //             {"?", "%3F"},
        //             {":", "%3A"},
        //             {";", "%3B"},
        //             {"'", "%27"},
        //             {"\"", "%22"},
        //             {"&", "%26"}
        //         };
        //         for (auto const &[from, to] : replacements) {
        //             for (auto pos = text.find(from); pos != std::string::npos; pos = text.find(from, pos + to.size())) {
        //                 text.replace(pos, from.size(), to);
        //             }
        //         }
        //         announcements_host.play_sync(std::format("http://141.95.101.189:5000/tts?text={}", text));
        //     };
        //     say("Welcome to Beat-o-graph, your personal assistant for all things development and data offering, including RSS feeds and music streaming.");
        //     say("All systems are go.");
        //     for (;;) {
        //         // wait until next hour o'clock
        //         auto now = std::chrono::system_clock::now();
        //         auto next_hour = now + std::chrono::hours(1);
        //         auto next_hour_t = std::chrono::system_clock::to_time_t(next_hour);
        //         std::tm next_hour_tm;
        //         localtime_s(&next_hour_tm, &next_hour_t);
        //         next_hour_tm.tm_min = 0;
        //         next_hour_tm.tm_sec = 0;
        //         auto next_hour_tp = std::chrono::system_clock::from_time_t(std::mktime(&next_hour_tm));
        //         std::this_thread::sleep_until(next_hour_tp);
        //         // announce the hour
        //         auto hour = next_hour_tm.tm_hour;
        //         std::string text = std::format("It's {}{} o'clock.", hour % 12, hour >= 12 ? "pm" : "am");
        //         say(text);
        //     }
        // }).detach();

        // auto &up = views::state_updated();
        // up = [&announcements_host](views::state_t const &state) {
        //     announcements_host.stop();
        //     if (state) {
        //         announcements_host.play_sync(happy_bell_sound);
        //     }
        //     else {
        //         announcements_host.play_sync(impact_sound);
        //     }
        // };

        auto tabs = std::make_unique<screen_tabs>(std::vector<screen_tabs::tab_t> {
            {"Backend Dev", [&dev_screen] { dev_screen.render(); }},
            {"Data Offering", [&ds] { ds.render(); }},
            {"ArangoDB", [&cr] { cr.render(); }},
            {"Toggl", [&ts] { ts.render(); }},
            {"Jira", [&js] { js.render(); }},
            {"Calendar", [&cs] { cs.render(); }},
            {"Configured SSH Hosts", [&ssh_screen, &localhost] { ssh_screen.render(localhost); }},
             {"Git Repositories", [&git]
             {
                 ImGui::Text("Git Repositories");
                 if (auto repos = git.repos(); repos)
                 {
                     for (auto const &repo : *repos)
                     {
                         ImGui::Text("%s", repo.c_str());
                     }
                 }
             }},
             {"GitHub", [] { ImGui::Text("GitHub"); }},
             {"Bitbucket", [] { ImGui::Text("Bitbucket");}},
             {"Zookeeper", [] { ImGui::Text("Zookeeper"); }},
             {"RabbitMQ", [] { ImGui::Text("RabbitMQ"); }},
             {"GitHub", [] { ImGui::Text("GitHub"); }},
             {"Bitbucket", [] { ImGui::Text("Bitbucket"); }},
             {"Zookeeper", [] { ImGui::Text("Zookeeper"); }},
             {"RabbitMQ", [] { ImGui::Text("RabbitMQ"); }},
             {"Radio", [&radio_screen, &rss_screen] {
                radio_screen->render();
                rss_screen.render();
             }}
        });
        main_screen screen{std::move(tabs)};
        radio_screen = std::make_unique<radio::screen>(radio_host, cache);
        screen.run();
        views::quitting(true);
    }
    std::cerr << "Terminating.\n";

    return 0;
}
