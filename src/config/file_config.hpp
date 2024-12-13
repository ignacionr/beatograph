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

        void set(std::string const &key, std::string const &value) {
            properties_[key] = value;
        }

        std::string const &get(std::string const& key) const {
            return properties_.at(key);
        }

        void scan_level(std::string_view name_base, auto sink) {
            std::string last_level;
            for (auto const &kv: properties_) {
                if (kv.first.starts_with(name_base)) {
                    auto v = kv.first.substr(name_base.size());
                    v = v.substr(0, v.find('.'));
                    if (last_level != v) {
                        sink(v);
                    }
                }
            }
        }

    private:
        std::filesystem::path path_;
        std::map<std::string, std::string> properties_;
    };
}
