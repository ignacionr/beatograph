#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "../../../external/cppgpt/cppgpt.hpp"
#include "../../registrar.hpp"
#include "../../hosting/http/fetch.hpp"
#include "../../structural/config/file_config.hpp"

namespace cloud::whatsapp
{
    struct classifier
    {
        classifier() {
            // load existing classifications
            auto config = registrar::get<config::file_config>({});
            config->scan_level("whatsapp.classifications.", [this, config](auto const &subkey) {
                cache_[subkey] = config->get(std::format("whatsapp.classifications.{}", subkey)).value();
            }, true);
        }
        std::string classify(std::string const &chat_id, nlohmann::json const &sample)
        {
            std::lock_guard lock{mutex_};
            if (auto it = cache_.find(chat_id); it != cache_.end())
            {
                return it->second;
            }

            // try to classify the message
            std::string result;
            try {
                auto gpt = registrar::get<ignacionr::cppgpt>({})->new_conversation();
                gpt.add_instructions("You are a message classifier. Classify the following sample as one of the following: work, personal, unclassified. Respond with the one-word classification only.");
                result = gpt.sendMessage(sample.dump(), [](auto a, auto b, auto c) { return http::fetch().post(a, b, c); }).at("choices").at(0).at("message").at("content").get<std::string>();
                // make lowercase
                for (auto &c : result) {
                    c = static_cast<char>(std::tolower(c));
                }
                set(chat_id, result);
            }
            catch(const std::exception &) {
                result = "unclassified";
            }
            cache_[chat_id] = result;
            return result;
        }

        void set(std::string const &chat_id, std::string const &result) {
            if (result != "unclassified") {
                auto config = registrar::get<config::file_config>({});
                config->set(std::format("whatsapp.classifications.{}", chat_id), result);
                config->save();
                cache_[chat_id] = result;
            }
        }

        std::optional<std::string> classify(std::string const &chat_id)
        {
            std::lock_guard lock{mutex_};
            if (auto it = cache_.find(chat_id); it != cache_.end())
            {
                return it->second;
            }
            return std::nullopt;
        }

        std::unordered_map<std::string, std::string> cache_;
        std::mutex mutex_;
    };
} // namespace cloud::whatsapp
