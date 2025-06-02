#pragma once
#include <atomic>
#include <chrono>
#include <format>
#include <map>
#include <memory>
#include <string>
#include "host_local.hpp"
#include "local_mapping.hpp"
#include "../cloud/metrics/from_url.hpp"
#include "../cloud/docker/host.hpp"

namespace hosting::ssh
{
    struct host
    {
        using properties_t = std::map<std::string, std::string>;
        using ptr = std::shared_ptr<host>;

        using hosts_by_name_t = std::map<std::string, std::weak_ptr<host>>;
        static ptr by_name(std::string const &name)
        {
            auto it = hosts_().find(name);
            if (it != hosts_().end() && !it->second.expired())
            {
                return it->second.lock();
            }
            auto res = std::shared_ptr<host>(new host{name});
            hosts_().emplace(name, res);
            return res;
        }
        static void destroy_all()
        {
            for (auto &h : hosts_())
            {
                if (!h.second.expired())
                {
                    h.second.reset();
                }
            }
        }

    private:
        host(std::string_view name) : name_{name} {
            ::hosting::local::host localhost{};
            resolve_from_ssh_conf(localhost);
        }

    public:
        void resolve_from_ssh_conf(::hosting::local::host &localhost)
        {
            std::string all_lines = localhost.execute_command(("ssh -G " + name_).c_str());
            std::string line;
            std::istringstream iss(all_lines);
            auto properties = std::make_shared<properties_t>();
            while (std::getline(iss, line))
            {
                auto pos = line.find(' ');
                if (pos != std::string::npos)
                {
                    properties->emplace(line.substr(0, pos), line.substr(pos + 1));
                }
            }
            {
                std::lock_guard<std::mutex> lock(properties_mutex_);
                properties_ = std::move(properties);
            }
        }

        void upload_file(std::string const &local_path, std::string const &remote_path, ::hosting::local::host &localhost)
        {
            localhost.execute_command(std::format("scp {} {}:{}", local_path, name_, remote_path).c_str());
        }

        std::string const &name() const { return name_; }

        properties_t const &properties() const
        {
            std::lock_guard<std::mutex> lock(properties_mutex_);
            return *properties_;
        }

        auto metrics(std::shared_ptr<::hosting::local::host> localhost)
        {
            auto const now = std::chrono::system_clock::now();
            if ((now - last_metrics_fetch_) > std::chrono::seconds(30))
            {
                std::thread{[this, localhost]
                            {
                                try
                                {
                                    fetch_metrics(localhost);
                                }
                                catch (std::exception const &e)
                                {
                                    std::cerr << "Failed to fetch metrics for " << name_ << ": " << e.what() << std::endl;
                                }
                            }}
                    .detach();
                last_metrics_fetch_ = now;
            }
            {
                std::lock_guard<std::mutex> lock(metrics_mutex_);
                return metrics_;
            }
        }

        void fetch_metrics(std::shared_ptr<::hosting::local::host> localhost)
        {
            {
                std::lock_guard<std::mutex> lock(nodeexporter_mapping_mutex_);
                if (!nodeexporter_mapping_)
                {
                    nodeexporter_mapping_ = std::make_shared<::hosting::local::mapping>(9100, name_, localhost);
                }
            }
            std::string url;
            {
                std::lock_guard<std::mutex> lock(nodeexporter_mapping_mutex_);
                url = std::format("http://localhost:{}/metrics", nodeexporter_mapping_->local_port());
            }
            auto metrics = metrics_from_url::fetch(url);
            {
                std::lock_guard<std::mutex> lock(metrics_mutex_);
                metrics_ = std::make_shared<metrics_model>(std::move(metrics));
            }
        }

        auto &docker()
        {
            if (!docker_host_)
            {
                docker_host_ = std::make_unique<docker::host>(name_);
            }
            return *docker_host_;
        }

        std::string execute_command(std::string const &command, std::shared_ptr<::hosting::local::host> localhost, bool use_sudo = true)
        {
            return docker().execute_command(command, localhost, use_sudo);
        }

        std::string get_os_release(std::shared_ptr<::hosting::local::host> localhost)
        {
            if (os_release_.empty())
            {
                os_release_ = execute_command("cat /etc/os-release", localhost);
            }
            return os_release_;
        }

    private:
        static hosts_by_name_t &hosts_()
        {
            static hosts_by_name_t hosts;
            return hosts;
        }

        std::string name_;
        std::string os_release_;
        mutable std::mutex properties_mutex_;
        std::shared_ptr<properties_t> properties_ = std::make_shared<properties_t>();
        mutable std::mutex nodeexporter_mapping_mutex_;
        std::shared_ptr<::hosting::local::mapping> nodeexporter_mapping_;
        mutable std::mutex metrics_mutex_;
        std::shared_ptr<metrics_model> metrics_;
        std::unique_ptr<docker::host> docker_host_;
        std::chrono::system_clock::time_point last_metrics_fetch_;
    };
}