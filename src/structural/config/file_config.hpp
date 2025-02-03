#pragma once

#include <filesystem>
#include <string>
#include <map>
#include <fstream>

namespace config
{
    struct file_config
    {
        file_config(std::filesystem::path p) : path_{p}
        {
            std::ifstream input{p};
            std::string line;
            while (std::getline(input, line))
            {
                auto pos = line.find('=');
                if (pos != std::string::npos)
                {
                    properties_[line.substr(0, pos)] = line.substr(pos + 1);
                }
            }
        }
        void save() const
        {
            std::ofstream output{path_};
            for (auto const &[key, value] : properties_)
            {
                output << key << '=' << value << '\n';
            }
        }

        void set(std::string const &key, std::optional<std::string> const &value) {
            if (value) {
                properties_[key] = *value;
            }
            else {
                properties_.erase(key);
            }
        }

        std::optional<std::string> get(std::string const& key) const {
            auto const it {properties_.find(key)};
            if (it != properties_.end()) {
                return it->second;
            }
            return {};
        }

        void scan_level(std::string_view name_base, auto sink, bool greedy = false) const {
            std::string last_level;
            for (auto const &kv: properties_) {
                if (kv.first.starts_with(name_base)) {
                    auto v = kv.first.substr(name_base.size());
                    if (greedy) {
                        sink(v);
                    }
                    else {
                        v = v.substr(0, v.find('.'));
                        if (last_level != v) {
                            sink(v);
                        }
                    }
                }
            }
        }

    private:
        std::filesystem::path path_;
        std::map<std::string, std::string> properties_;
    };
}

#include "../key_value.hpp"
static_assert(KeyValue<config::file_config>);
