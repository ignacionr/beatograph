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
        host_screen hs;

        auto tabs = std::make_unique<screen_tabs>(std::vector<screen_tabs::tab_t>{
            {"Backend Dev", [&hs, &localhost]
             {
                hs.render(host::by_name("dev-locked"), localhost);
                if (ImGui::CollapsingHeader("Status")) {
                    views::Assertion("dev1_container is running", [&localhost] {
                        return host::by_name("dev-locked")->docker().is_container_running("dev1_container", localhost);
                    });
                    ImGui::Indent();
                    views::Assertion("xrdp is running for dev1", [&localhost] {
                        return host::by_name("dev-locked")->docker().is_process_running("dev1_container", "xrdp", localhost);
                    });
                    views::Assertion("dev1 can log in for gitservice usage with ssh", [&localhost] {
                        auto const result = host::by_name("dev-locked")->docker().execute_command("su - dev1 -c 'ssh -T git@gitservice'", "dev1_container", localhost, false);
                        return result.find("Welcome to git-server-docker!") != std::string::npos;
                    });
                    ImGui::Unindent();
                    views::Assertion("dev2_container is running", [&localhost] {
                        return host::by_name("dev-locked")->docker().is_container_running("dev2_container", localhost);
                    });
                    ImGui::Indent();
                    views::Assertion("xrdp is running for dev2", [&localhost] {
                        return host::by_name("dev-locked")->docker().is_process_running("dev2_container", "xrdp", localhost);
                    });
                    views::Assertion("dev2 can log in for gitservice usage with ssh", [&localhost] {
                        auto const result = host::by_name("dev-locked")->docker().execute_command("su - dev2 -c 'ssh -T git@gitservice'", "dev2_container", localhost, false);
                        return result.find("Welcome to git-server-docker!") != std::string::npos;
                    });
                    ImGui::Unindent();
                    views::Assertion("dev3_container is running", [&localhost] {
                        return host::by_name("dev-locked")->docker().is_container_running("dev3_container", localhost);
                    });
                    ImGui::Indent();
                    views::Assertion("xrdp is running for dev3", [&localhost] {
                        return host::by_name("dev-locked")->docker().is_process_running("dev3_container", "xrdp", localhost);
                    });
                    views::Assertion("dev3 can log in for gitservice usage with ssh", [&localhost] {
                        auto const result = host::by_name("dev-locked")->docker().execute_command("su - dev3 -c 'ssh -T git@gitservice'", "dev3_container", localhost, false);
                        return result.find("Welcome to git-server-docker!") != std::string::npos;
                    });
                    ImGui::Unindent();
                    views::Assertion("The Git service container is running", [&localhost] {
                        return host::by_name("dev-locked")->docker().is_container_running("gitservice", localhost);
                    });
                    ImGui::Indent();
                    ImGui::Unindent();
                    views::Assertion("The externalizer service is running", [&localhost] {
                        return host::by_name("dev-locked")->docker().is_container_running("socat-proxy", localhost);
                    });
                    ImGui::Indent();
                    ImGui::Unindent();
                    views::Assertion("The pipeline runner service is running", [&localhost] {
                        return host::by_name("dev-locked")->docker().is_container_running("pipeline-runner", localhost);
                    });
                    ImGui::Indent();
                    views::Assertion("root on the pipeline runner container can log in for gitservice usage with ssh", [&localhost] {
                        auto const result = host::by_name("dev-locked")->docker().execute_command("ssh -T git@gitservice", "pipeline-runner", localhost, false);
                        return result.find("Welcome to git-server-docker!") != std::string::npos;
                    });
                    ImGui::Unindent();
                }
                if (ImGui::CollapsingHeader("Corrective Actions")) {
                    static std::string seed_result;
                    if (ImGui::Button("Seed Containers")) {
                        seed_result = host::by_name("dev-locked")->docker().execute_command(
                            "cd /home/ubuntu/arangodb-infra/dev-locked && ./seed.sh", localhost, false);
                    }
                    if (!seed_result.empty()) {
                        ImGui::Text("%s", seed_result.c_str());
                    }
                }
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
             {"Radio", [&radio_screen] {
                    radio_screen->render();
             }}
        });
        main_screen screen{std::move(tabs)};
        radio_screen = std::make_unique<radio::screen>(radio_host);
        screen.run();
        views::quitting(true);
    }
    std::cerr << "Terminating.\n";

    return 0;
}
