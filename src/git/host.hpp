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
#include "../registrar.hpp"
#include "../structural/text_command/host.hpp"

namespace git{
struct host
{
    host(std::shared_ptr<hosting::local::host> localhost, std::string_view root) : localhost_{localhost}, root_{root}
    {
        async_harvest();
        registrar::get<structural::text_command::host>({})->add_source({
            [this](std::string const &partial, std::function<void(std::string const &)> callback) {
                auto repos = m_repos.load();
                for (auto const &repo : *repos)
                {
                    if (repo.find(partial) != std::string::npos)
                    {
                        callback(std::format("open {} with VSCode", repo));
                    }
                }
            },
            [](std::string const &name) -> bool {
                if (name.starts_with("open ") && name.ends_with(" with VSCode"))
                {
                    auto const repo = name.substr(5, name.size() - 5 - 12);
                    auto const command = std::format("code \"{}\"", repo);
                    std::system(command.c_str());
                    return true;
                }
                return false;
            }
        });
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
                        // avoid going any deeper, since we don't want to open submodules separately
                        return;
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

    void harvest()
    {
        std::shared_ptr<std::vector<std::string>> repos = std::make_shared<std::vector<std::string>>();
        enumerate_repos(root_,
            [repos](std::filesystem::path repo)
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
        return localhost_->execute_command(command.c_str());
    }

    std::string clone(std::string_view repo, std::string_view target_path)
    {
        std::string const command {std::format("git clone \"{}\" \"{}\"", repo, target_path)};
        auto const result = localhost_->execute_command(command.c_str());
        if (result.find("fatal") != std::string::npos)
        {
            throw std::runtime_error(result);
        }
        auto repos = std::make_shared<std::vector<std::string>>(*m_repos.load());
        repos->push_back(std::string{target_path});
        m_repos.store(repos);
        return result;
    }

    std::string checkout(std::string_view repo, std::string_view commit = "HEAD")
    {
        std::string const command {std::format("git --git-dir=\"{}/.git\" --work-tree=\"{}\" checkout {}", repo, repo, commit)};
        return localhost_->execute_command(command.c_str());
    }

    std::string shallow_partial_download(std::string_view repo, std::string_view partial_path, std::string_view target_path) {
        std::string const command {std::format("git clone --depth 1 --filter=blob:none --no-checkout \"{}\" \"{}\"", repo, target_path)};
        auto const result = localhost_->execute_command(command.c_str());
        if (result.find("fatal") != std::string::npos)
        {
            throw std::runtime_error(result);
        }
        std::string const command2 {std::format("git --git-dir=\"{}/.git\" --work-tree=\"{}\" sparse-checkout init --cone", target_path, target_path)};
        auto const result2 = localhost_->execute_command(command2.c_str());
        if (result2.find("fatal") != std::string::npos)
        {
            throw std::runtime_error(result2);
        }
        std::string const command3 {std::format("git --git-dir=\"{}/.git\" --work-tree=\"{}\" sparse-checkout set \"{}\"", target_path, target_path, partial_path)};
        auto const result3 = localhost_->execute_command(command3.c_str());
        if (result3.find("fatal") != std::string::npos)
        {
            throw std::runtime_error(result3);
        }
        // Manually checkout files after setting sparse paths
        std::string const command4 {std::format(
            "git --git-dir=\"{}/.git\" --work-tree=\"{}\" checkout",
            target_path, target_path)};
        auto const result4 = localhost_->execute_command(command4.c_str());
        if (result4.find("fatal") != std::string::npos) {
            throw std::runtime_error(result4);
        }
        return result4;
    }

private:
    std::shared_ptr<hosting::local::host> localhost_;
    std::atomic<std::shared_ptr<std::vector<std::string>>> m_repos;
    std::string root_;
};
}
