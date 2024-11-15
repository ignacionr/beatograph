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

std::string get_env_variable(std::string_view key) {
    char *val = nullptr;
    size_t len = 0;
    if (_dupenv_s(&val, &len, key.data()) || val == nullptr)
    {
        throw std::runtime_error(std::format("Error: {} environment variable not set.", key));
    }
    return std::string{val, len - 1}; // len returns the count of all copied bytes, including the terminator
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

        img_cache cache{"imgcache"};

        // get the Groq API key from GROQ_API_KEY
        auto groq_api_key = get_env_variable("GROQ_API_KEY");
        ignacionr::cppgpt gpt{groq_api_key, ignacionr::cppgpt::groq_base};

        toggl_client tc(get_env_variable("TOGGL_API_TOKEN"));
        toggl_screen ts(tc);

        jira::host jh {get_env_variable("JIRA_USER"), get_env_variable("JIRA_TOKEN")};
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

        rss::host rss_host;

        rss::screen rss_screen{rss_host,
            [&radio_host](std::string_view url) {
                radio_host.play(std::string{url});
            }, 
            cache
        };

        conversions::screen conv_screen{};

        cppgpt::screen gpt_screen;

        std::shared_ptr<screen_tabs> tabs;

        auto menu_tabs = [&tabs](std::string_view key){
            if (key == "Main") {
                if (ImGui::BeginMenu("Tabs")) {
                    if (ImGui::MenuItem("Jira", "Ctrl+J")) {
                        tabs->select_tab("Jira");
                    }
                    if (ImGui::MenuItem("Radio", "Ctrl+R")) {
                        tabs->select_tab("Radio");
                    }
                    ImGui::EndMenu();
                }
            }
        };
        auto menu_tabs_and = [&menu_tabs](std::function<void(std::string_view)>other) {
            return [menu_tabs, other](std::string_view key) {
                other(key);
                menu_tabs(key);
            };
        };

        tabs = std::make_shared<screen_tabs>(std::vector<screen_tabs::tab_t> {
            {"Local", [&local_screen] { local_screen.render(); }, menu_tabs},
            {"Backend Dev", [&dev_screen, &gpt] { dev_screen.render(gpt); }, menu_tabs},
            {"Data Offering", [&ds] { ds.render(); }, menu_tabs},
            {"ArangoDB", [&cr] { cr.render(); }, menu_tabs},
            {"Toggl", [&ts] { ts.render(); }, menu_tabs},
            {"Jira", [&js, &jh] { js->render(jh); }, 
                menu_tabs_and([&js](std::string_view item){ js->render_menu(item); }), 
                ImVec4(0.5f, 0.5f, 1.0f, 1.0f)},
            {"Calendar", [&cs] { cs.render(); }, menu_tabs},
            {"SSH Hosts", [&ssh_screen, &localhost] { ssh_screen.render(localhost); }, menu_tabs},
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
            }, menu_tabs},
            {"Radio", [&radio_screen, &rss_screen] {
            radio_screen->render();
            rss_screen.render();
            }, menu_tabs, ImVec4(0.05f, 0.5f, 0.05f, 1.0f)},
            {"Conversions", [&conv_screen] { conv_screen.render(); }, menu_tabs},
            {"AI", [&gpt_screen, &gpt] { gpt_screen.render(gpt); }, menu_tabs, ImVec4(0.75f, 0.75f, 0.75f, 1.0f)}
        });
        main_screen screen{tabs};

        // time to setup our fonts
        auto &io {ImGui::GetIO()};
        // io.Fonts->AddFontDefault();
        io.Fonts->AddFontFromFileTTF("assets/fonts/LiberationMono-Regular.ttf", 13.5f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
        io.Fonts->AddFontFromFileTTF("assets/fonts/Montserrat-Regular.ttf", 16.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
        io.Fonts->AddFontFromFileTTF("assets/fonts/Font90Icons-2ePo.ttf", 13.5f);

        radio_screen = std::make_unique<radio::screen>(radio_host, cache);
        js = std::make_unique<jira::screen>(cache);
        screen.run([&tabs]{
            if (ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
                if (ImGui::IsKeyPressed(ImGuiKey_J)) {
                    tabs->select_tab("Jira");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_R)) {
                    tabs->select_tab("Radio");
                }
            }
        });
        views::quitting(true);
    }
    std::cerr << "Terminating.\n";

    return 0;
}
