#pragma once

#include "../../pch.h"

#include <algorithm>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "base_commands.hpp"

namespace structural::text_command {

    struct command_source {
        std::function<void(std::string const &partial, std::function<void(std::string const &)> callback)> partial_list;
        std::function<std::optional<std::string>(std::string const &name)> execute;
    };

    struct host {
        using action_t = std::function<void()>;

        host() {
            add_source(base_commands<command_source>::echo());
            try {
                register_custom_url_protocol();
            }
            catch(...) {
                // ignore, maybe we're not elevated
            }
        }
        
        void add_command(std::string const &name, action_t action) {
            std::lock_guard<std::mutex> lock(mutex_);
            commands[name] = action;
        }

        void add_source(command_source&& source) {
            std::lock_guard<std::mutex> lock(mutex_);
            sources.push_back(std::move(source));
        }

        std::string operator()(std::string const &name) {
            if (auto it = commands.find(name); it != commands.end()) {
                it->second();
                return "OK";
            }
            else {
                for (auto const &source : sources) {
                    auto result = source.execute(name);
                    if (result.has_value()) {
                        return result.value();
                    }
                }
                throw std::runtime_error("Unknown command");
            }
        }

        void partial_list(std::string const &partial, std::function<void(std::string const &)> callback) {
            auto lower_partial = partial;
            std::transform(lower_partial.begin(), lower_partial.end(), lower_partial.begin(), ::tolower);
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto const &[name, _] : commands) {
                auto lower_name = name;
                std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
                if (lower_name.find(lower_partial) != std::string::npos) {
                    callback(name);
                }
            }
            // also try the sources
            for (auto const &source : sources) {
                source.partial_list(partial, callback);
            }
        }

#if defined(_WIN32) || defined(_WIN64)
        struct win_error: public std::runtime_error {
            win_error(std::string_view prefix, LSTATUS code): std::runtime_error("Win32 error") {
                LPVOID lpMsgBuf;
                ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                              NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
                description_ = std::format("{}: {}", prefix, lpMsgBuf);
            }
            virtual const char* what() const noexcept override {
                return description_.c_str();
            }
        private:
            std::string description_;
        };
#endif

        bool register_custom_url_protocol() {
#if defined(_WIN32) || defined(_WIN64)
            // use the Windows Registry to register a beatograph: protocol schema
            HKEY hkey_beatograph;
            auto ret = ::RegCreateKey(HKEY_CLASSES_ROOT, TEXT("beatograph"), &hkey_beatograph);
            if (ret != ERROR_SUCCESS) {
                // get the error code and text
                return false;
            }
            // create a default value
            ret = ::RegSetValueEx(hkey_beatograph, nullptr, 0, REG_SZ, (const BYTE*)"URL:beatograph Protocol", 24);
            if (ret != ERROR_SUCCESS) {
                // get the error code and text
                ::RegCloseKey(hkey_beatograph);
                return false;
            }
            // an empty string value "URL Protocol"
            ret = ::RegSetValueEx(hkey_beatograph, TEXT("URL Protocol"), 0, REG_SZ, (const BYTE*)"", 0);
            if (ret != ERROR_SUCCESS) {
                // get the error code and text
                throw win_error{"RegSetValueEx failed", ret};
            }
            // create shell\\open\\command
            HKEY hkey_shell;
            ret = ::RegCreateKey(hkey_beatograph, TEXT("shell"), &hkey_shell);
            if (ret != ERROR_SUCCESS) {
                // get the error code and text
                throw win_error{"RegCreateKey failed", ret};
            }
            HKEY hkey_open;
            ret = ::RegCreateKey(hkey_shell, TEXT("open"), &hkey_open);
            if (ret != ERROR_SUCCESS) {
                // get the error code and text
                throw win_error{"RegCreateKey failed", ret};
            }
            HKEY khey_open_command;
            ret = ::RegCreateKey(hkey_open, TEXT("command"), &khey_open_command);
            if (ret != ERROR_SUCCESS) {
                // get the error code and text
                throw win_error{"RegCreateKey failed", ret};
            }
            // Get the running module's path
            TCHAR szPath[MAX_PATH];
            if (!::GetModuleFileName(nullptr, szPath, MAX_PATH)) {
                throw win_error{"GetModuleFileName failed", static_cast<LSTATUS>(::GetLastError())};
            }
            // create the default value
            std::string command = std::format("\"{}\" \"%1\"", szPath);
            ret = ::RegSetValueEx(khey_open_command, nullptr, 0, REG_SZ, (const BYTE*)command.c_str(), static_cast<DWORD>(command.size()));
            if (ret != ERROR_SUCCESS) {
                // get the error code and text
                throw win_error{"RegSetValueEx failed", ret};
            }
            return true;
#else
            // obtain the current executable file name
            char buffer[PATH_MAX];
            if (readlink("/proc/self/exe", buffer, sizeof(buffer)) == -1) {
                throw std::runtime_error("Failed to get the current executable path");
            }
            std::string executable_path(buffer);

            // register the protocol on Linux
            // create a .desktop file in ~/.local/share/applications
            std::string home_dir = std::getenv("HOME");
            std::string desktop_file = std::format("{}/.local/share/applications/beatograph.desktop", home_dir);
            {
                std::ofstream file(desktop_file, std::ios::out | std::ios::trunc);
                if (!file.is_open()) {
                    throw std::runtime_error("Failed to open file: " + desktop_file);
                }
                file << "[Desktop Entry]\n";
                file << "Name=Beatograph\n";
                file << "Exec=" << executable_path << " %u\n";
                file << "Type=Application\n";
                file << "Terminal=false\n";
                file << "MimeType=x-scheme-handler/beatograph\n";
                file << "NoDisplay=true\n";
                file << "X-GNOME-Autostart-enabled=true\n";
                file << "X-GNOME-Autostart-Phase=Applications\n";
                file << "X-GNOME-Autostart-Notify=true\n";
                file << "X-GNOME-Autostart-Delay=0\n";
                file << "X-GNOME-Autostart-Condition=GSettings org.gnome.desktop.interface enable-hot-corners\n";
            }
            // now register the protocol
            system("update-desktop-database ~/.local/share/applications");
            system("xdg-mime install --mode user --novendor ~/.local/share/applications/beatograph.desktop");
            system("xdg-mime default beatograph.desktop x-scheme-handler/beatograph");
            return true;
#endif
        }
    private:
        std::map<std::string, action_t> commands;
        std::vector<command_source> sources;
        std::mutex mutex_;
    };
}