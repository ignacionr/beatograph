#pragma once

#include <format>
#include <fstream>
#include <map>
#include <string>
#include <Windows.h>
#include <Shlobj.h>
#include <Shlobj_core.h>
#include "../hosting/host_local.hpp"
#include "../hosting/host.hpp"
#include "../hosting/ssh_screen.hpp"

namespace ssh
{
    struct screen_all
    {
        screen_all()
        {
            read_hosts();
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
        void render(hosting::local::host &localhost)
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