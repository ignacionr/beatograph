#pragma once

#include <atomic>
#include <filesystem>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "../hosting/host_local.hpp"

struct git_host
{
    git_host(hosting::local::host &localhost, std::string_view root) : localhost_{localhost}
    {
        async_harvest(root);
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

    void harvest(std::string const &root)
    {
        std::shared_ptr<std::vector<std::string>> repos = std::make_shared<std::vector<std::string>>();
        enumerate_repos(root,
            [repos](std::filesystem::path repo)
                            { repos->push_back(repo.string()); });
        m_repos.store(repos);
    }

    void async_harvest(std::string_view root)
    {
        std::thread([this, root_str = std::string{root}]()
                    { harvest(root_str); })
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
    hosting::local::host &localhost_;
    std::atomic<std::shared_ptr<std::vector<std::string>>> m_repos;
};
