#pragma once

#include <atomic>
#include <filesystem>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "../host/host_local.hpp"

struct git_host
{
    git_host(host_local &localhost) : localhost_{localhost}
    {
        async_harvest();
    }

    static void enumerate_drives(auto sink)
    {
        char buffer[256];
        DWORD result = GetLogicalDriveStringsA(sizeof(buffer), buffer);

        if (result == 0)
        {
            throw std::runtime_error("Error: Could not retrieve logical drive strings.");
        }

        char *drive = buffer;
        while (*drive)
        {
            std::string_view drive_letter{drive};
            sink(drive_letter);
            drive += drive_letter.size() + 1; // Move to the next drive string
        }
    }

    static void enumerate_repos(std::filesystem::path path, auto sink)
    {
        try
        {
            for (auto &entry : std::filesystem::directory_iterator(path))
            {
                if (entry.is_directory())
                {
                    std::filesystem::path entry_path = entry.path();
                    if (entry_path.filename() == ".git")
                    {
                        sink(entry_path.parent_path());
                    }
                    else
                    {
                        enumerate_repos(entry_path, sink);
                    }
                }
            }
        }
        catch (...)
        {
            // Ignore errors (most will be access denied, others we also don't care about)
        }
    }

    static void enumerate_all_repos(auto sink)
    {
        enumerate_drives([&sink](std::string_view drive_letter)
                         { enumerate_repos(drive_letter, sink); });
    }

    void harvest()
    {
        std::shared_ptr<std::vector<std::string>> repos = std::make_shared<std::vector<std::string>>();
        enumerate_all_repos([repos](std::filesystem::path repo)
                            { repos->push_back(repo.string()); });
        m_repos.store(repos);
    }

    void async_harvest()
    {
        std::thread([this]()
                    { harvest(); })
            .detach();
    }

    auto repos() const
    {
        return m_repos.load();
    }

    std::string get_log(std::string_view repo)
    {
        std::string command = std::format("git --git-dir=\"{}/.git\" log --pretty=format:\"%h %ad %ae %s\" --date=short --graph --all --stat", repo);
        return localhost_.execute_command(command.c_str());
    }

private:
    host_local &localhost_;
    std::atomic<std::shared_ptr<std::vector<std::string>>> m_repos;
};
