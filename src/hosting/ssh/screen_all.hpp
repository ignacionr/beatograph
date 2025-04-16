#pragma once

#include <algorithm>
#include <format>
#include <fstream>
#include <map>
#include <string>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#include <Shlobj.h>
#include <Shlobj_core.h>
#endif

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
                [this](std::string_view command) -> std::optional<std::string> {
                    if (command.starts_with("ssh ")) {
                        auto name = command.substr(4);
                        if (auto it = std::find_if(hosts.begin(), hosts.end(), [name](auto const &entry) -> bool {return name == entry.first;}); 
                            it != hosts.end()) {
                            // use the Windows API with a simple shell command
                            if (auto result = reinterpret_cast<long long>(ShellExecuteA(NULL, "open", "cmd", std::format("/c {}", command).c_str(), NULL, SW_SHOW)); result <= 32)
                            {
                                throw std::runtime_error(std::format("ShellExecute failed with error code {}", result));
                            }
                            return "OK";
                        }
                    }
                    return std::nullopt;
                }
            });
        }

        static const std::string configure_file_path() {
            // get the Windows user home directory
            char home[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, home)))
            {
                return std::format("{}/.ssh/config", home);
            }
            // obtain the error description
            LPSTR messageBuffer = nullptr;
            auto error = GetLastError();
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
            std::string message(messageBuffer);
            LocalFree(messageBuffer);
            throw std::runtime_error(std::format("SHGetFolderPath failed with error code {}: {}", error, message));
        }
        
        void read_hosts() {
            std::ifstream file(configure_file_path());
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

        void render(std::shared_ptr<hosting::local::host> localhost) noexcept
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
            if (ImGui::SameLine(); ImGui::SmallButton(ICON_MD_EDIT " Edit"))
            {
                last_error_.clear();
                // use the Windows API with a simple shell command
                if (auto result = reinterpret_cast<long long>(ShellExecuteA(NULL, "open", "code", configure_file_path().c_str(), NULL, SW_SHOW)); result <= 32)
                {
                    last_error_ = std::format("ShellExecute failed with error code {}", result);
                }
            }
            if (!last_error_.empty())
            {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", last_error_.c_str());
            }
        }

    private:
        std::map<std::string, hosting::ssh::host::ptr> hosts;
        hosting::ssh::screen host_screen_;
        std::string last_error_;
    };
}