#pragma once

#include <algorithm>
#include <format>
#include <fstream>
#include <map>
#include <string>

#include <Windows.h>
#include <Shlobj.h>
#include <Shlobj_core.h>
#include "../host_local.hpp"
#include "../host.hpp"
#include "../ssh_screen.hpp"
#include "../../registrar.hpp"
#include "../../structural/text_command/host.hpp"

namespace ssh
{
    struct screen_all
    {
        screen_all()
        {
            read_hosts();
            auto text_host = registrar::get<structural::text_command::host>({});
            text_host->add_source({
                [this](std::string_view partial, auto callback) {
                    if (partial.starts_with("ssh")) {
                        for (auto const &[name, host] : hosts) {
                            if (partial.size() < 5 || name.find(partial.substr(4)) != std::string::npos)
                            {
                                callback(std::format("ssh {}", name));
                            }
                        }
                    }
                },
                [this](std::string_view command) -> bool {
                    if (command.starts_with("ssh ")) {
                        auto name = command.substr(4);
                        if (auto it = std::find_if(hosts.begin(), hosts.end(), [name](auto const &entry) -> bool {return name == entry.first;}); 
                            it != hosts.end()) {
                            // use the Windows API with a simple shell command
                            if (auto result = reinterpret_cast<long long>(ShellExecuteA(NULL, "open", "cmd", std::format("/c {}", command).c_str(), NULL, SW_SHOW)); result <= 32)
                            {
                                throw std::runtime_error(std::format("ShellExecute failed with error code {}", result));
                            }
                            return true;
                        }
                    }
                    return false;
                }
            });
        }
        
        void read_hosts() {
            // get the Windows user home directory
            char home[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, home)))
            {
                std::ifstream file(std::format("{}/.ssh/config", home));
                if (file.is_open())
                {
                    std::string line;
                    while (std::getline(file, line))
                    {
                        if (line.starts_with("Host "))
                        {
                            auto name = line.substr(5);
                            hosts.emplace(name, hosting::ssh::host::by_name(name));
                        }
                    }
                }
            }
        }
        void render(std::shared_ptr<hosting::local::host> localhost)
        {
            for (auto const &[name, host] : hosts)
            {
                if (ImGui::CollapsingHeader(name.c_str()))
                {
                    host_screen_.render(host, localhost);
                }
            }
            if (ImGui::SmallButton(ICON_MD_REFRESH " Refresh"))
            {
                read_hosts();
            }
        }

    private:
        std::map<std::string, hosting::ssh::host::ptr> hosts;
        hosting::ssh::screen host_screen_;
    };
}