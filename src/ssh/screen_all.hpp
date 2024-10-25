#pragma once

#include <format>
#include <fstream>
#include <map>
#include <string>
#include <Windows.h>
#include <Shlobj.h>
#include <Shlobj_core.h>
#include "../host/host_local.hpp"
#include "../host/host.hpp"
#include "../host/screen.hpp"

struct ssh_screen {
    ssh_screen() {
        // get the Windows user home directory
        char home[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, home))) {
            std::ifstream file(std::format("{}/.ssh/config", home));
            if (file.is_open()) {
                std::string line;
                while (std::getline(file, line)) {
                    if (line.starts_with("Host ")) {
                        auto name = line.substr(5);
                        hosts.emplace(name, host::by_name(name));
                    }
                }
            }
        }
    }
    void render(host_local &localhost) {
        for (auto const &[name, host] : hosts) {
            if (ImGui::CollapsingHeader(name.c_str())) {
                host->resolve_from_ssh_conf(localhost);
                host_screen_.render(host, localhost);
            }
        }
    }
private:
    std::map<std::string, host::ptr> hosts;
    host_screen host_screen_;
};
