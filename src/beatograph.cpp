#include "pch.h"
#include <algorithm>
#include <numeric>
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
        std::unique_ptr<radio::screen> radio_screen;

        dev_locked::screen dev_screen{localhost};

        rss::host rss_host;
        rss_host.add_feed("https://www.spreaker.com/show/4209606/episodes/feed"); // los temas del día
        rss_host.add_feed("https://www.spreaker.com/show/6332631/episodes/feed"); // cultura líquida
        rss_host.add_feed("https://www.spreaker.com/show/5711490/episodes/feed"); // working class history
        rss_host.add_feed("https://www.spreaker.com/show/5634793/episodes/feed"); // dotnet rocks
        rss_host.add_feed("https://www.spreaker.com/show/6006838/episodes/feed"); // cine para pensar
        rss_host.add_feed("https://www.spreaker.com/show/5719641/episodes/feed"); // the digital decode
        rss_host.add_feed("https://www.spreaker.com/show/6102036/episodes/feed"); // adventures in DevOps
        rss_host.add_feed("https://www.spreaker.com/show/2576750/episodes/feed"); // compute this
        rss_host.add_feed("https://www.spreaker.com/show/4956890/episodes/feed"); // CP radio
        rss_host.add_feed("https://www.spreaker.com/show/3392139/episodes/feed"); // Internet Freakshows
        rss_host.add_feed("https://www.spreaker.com/show/6349862/episodes/feed"); // Llama Cast
        rss_host.add_feed("https://ankar.io/this-american-life-archive/TALArchive.xml"); // This American Life
        
        rss_host.add_feed("https://softwareengineeringdaily.com/feed/podcast/");
        rss::screen rss_screen{rss_host,
            [&radio_host](std::string_view url) {
                radio_host.play(std::string{url});
            }, 
            cache
        };

        auto tabs = std::make_unique<screen_tabs>(std::vector<screen_tabs::tab_t>{
            {"Backend Dev", [&dev_screen]
             {
                dev_screen.render();
             }},
            {"Data Offering", [&ds]
             { ds.render(); }},
            {"ArangoDB", [&cr]
             { cr.render(); }},
            {"Toggl", [&ts]
             { ts.render(); }},
            {"Jira", [&js]
             { js.render(); }},
            {"Calendar", [&cs]
             { cs.render(); }},
            {"Configured SSH Hosts", [&ssh_screen, &localhost]
             { ssh_screen.render(localhost); }},
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
             {"GitHub", [] {
                 ImGui::Text("GitHub");
             }},
             {"Bitbucket", [] {
                 ImGui::Text("Bitbucket");
             }},
             {"Zookeeper", [] {
                 ImGui::Text("Zookeeper");
             }},
             {"RabbitMQ", [] {
                 ImGui::Text("RabbitMQ");
             }},
             {"GitHub", [] {
                 ImGui::Text("GitHub");
             }},
             {"Bitbucket", [] {
                 ImGui::Text("Bitbucket");
             }},
             {"Zookeeper", [] {
                 ImGui::Text("Zookeeper");
             }},
             {"RabbitMQ", [] {
                 ImGui::Text("RabbitMQ");
             }},
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
