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

void load_metrics_file(metrics_model &model, std::string_view filename)
{
    // obtain the last modification time of the file
    std::filesystem::path path(filename);
    auto last_write_time = std::filesystem::last_write_time(path);

    std::ifstream file(path);
    std::string line;
    metrics_parser parser;
    parser.sample_time = std::chrono::system_clock::now() + (last_write_time - std::filesystem::file_time_type::clock::now());
    parser.metric_help = [&](const std::string_view &name, const std::string_view &help)
    {
        model.set_help(name, help);
    };
    parser.metric_type = [&](const std::string_view &name, const std::string_view &type)
    {
        model.set_type(name, type);
    };
    parser.metric_metric_value = [&](std::string_view name, metric_value &&value)
    {
        model.add_value(name, std::move(value));
    };
    while (std::getline(file, line))
    {
        parser(line);
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
        metrics_model model;

#if defined(_DEBUG)
    // attach a console, so I can check what comes out of std::cerr
    AllocConsole();
    FILE *stream;
    freopen_s(&stream, "CONOUT$", "w", stderr);
#endif

        metrics_screen ms(model);
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

        dataoffering_screen ds{localhost, groq_api_key};
        host_local_screen local_screen{localhost};

        cluster_report cr{localhost};
        ssh_screen ssh_screen;

        auto tabs = std::make_unique<screen_tabs>(std::vector<screen_tabs::tab_t>{
            {"This Computer", [&local_screen]
             { local_screen.render(); }},
            {"Data Offering", [&ds]
             { ds.render(); }},
            {"ArangoDB", [&cr]
             { cr.render(); }},
            {"Metrics", [&ms]
             { ms.render(); }},
            {"Toggl", [&ts]
             { ts.render(); }},
            {"Jira", [&js]
             { js.render(); }},
            {"Calendar", [&cs]
             { cs.render(); }},
            {"Configured SSH Hosts", [&ssh_screen, &localhost]
             { ssh_screen.render(localhost); }},
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
        });
        main_screen screen{std::move(tabs)};
        screen.run();
        views::quitting(true);
    }
    std::cerr << "Terminating.\n";

    return 0;
}
