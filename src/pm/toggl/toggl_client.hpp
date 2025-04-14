#pragma once

#include <format>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "../../util/conversions/date_time.hpp"
#include "login/host.hpp"

namespace toggl
{
    class client
    {
    public:

        struct error: public std::runtime_error {
            error(std::string const &what): std::runtime_error{what} {}
        };

        struct config_error: error {
            config_error(std::string const &what): error{what} {}
        };

        struct no_login_set: config_error {
            no_login_set(): config_error{"No login set."} {}
        };

        using notifier_t = std::function<void(std::string_view)>;

        client(notifier_t notifier)
            : baseUrl("https://api.track.toggl.com/api/v9/"), notifier_{notifier}
        {
        }

        void set_login(std::shared_ptr<login::host> login) {
            login_ = login;
        }

        auto getTimeEntries()
        {
            auto str = performRequest(std::format("{}me/time_entries", baseUrl), "GET");
            return nlohmann::json::parse(str);
        }

        void check_today(int seconds_target = 9 * 3600)
        {
            try
            {
                auto entries = getTimeEntries();
                // sum today's entries
                auto const today = std::chrono::system_clock::now();
                auto const today_start = std::chrono::floor<std::chrono::days>(today);
                auto const today_end = today_start + std::chrono::hours(24);
                auto today_entries = entries | 
                    std::views::filter([today_start, today_end](const auto &entry) {
                        auto const start = conversions::date_time::from_string(entry["start"]);
                        return start >= today_start && start < today_end;
                    });
                auto const today_seconds {std::accumulate(today_entries.begin(), 
                    today_entries.end(), 
                    0ll, 
                    [](auto acc, auto const &entry) {
                        auto duration = entry["duration"].template get<long long>();
                        return acc + 
                            (duration < 0 ? 
                                std::chrono::system_clock::now().time_since_epoch().count() 
                                / 1000000ll - entry["start"].template get<long long>() 
                                : duration); 
                    })};
                if (today_seconds < 1)
                {
                    notifier_("No time entries today.");
                }
                else if (today_seconds >= seconds_target)
                {
                    notifier_("Today's target reached.");
                }
            }
            catch (std::exception const &e)
            {
                notifier_(std::format("Failed to get time entries: {}", e.what()));
            }
        }

        std::string startTimeEntry(long long workspace_id, const std::string_view description_text)
        {
        nlohmann::json json = {
            {"created_with", "Beat-o-graph"},
            {"description", description_text},
            {"duration", -1},
            {"start", std::format("{:%FT%T}Z", std::chrono::system_clock::now())},
            {"tags", {"adhoc"}},
            {"workspace_id", workspace_id}
        };
        return performRequest(
                std::format("{}workspaces/{}/time_entries", baseUrl, workspace_id),
                "POST",
                json.dump());
        }

        std::string stopTimeEntry(auto &entry)
        {
            auto url = std::format("{}workspaces/{}/time_entries/{}",
                                   baseUrl,
                                   entry["workspace_id"].template get<long long>(),
                                   entry["id"].template get<long long>());
            auto data = std::format("{{\"stop\":\"{}\"}}",
                                    std::format("{:%FT%T}Z", std::chrono::system_clock::now()));
            return performRequest(url, "PUT", data);
        }

        void deleteTimeEntry(auto &entry)
        {
            auto url = std::format("{}workspaces/{}/time_entries/{}",
                                   baseUrl,
                                   entry["workspace_id"].template get<long long>(),
                                   entry["id"].template get<long long>());
            performRequest(url, "DELETE");
        }

    private:
        std::string baseUrl;
        std::shared_ptr<login::host> login_;

        std::string performRequest(const std::string &url, const std::string &method, const std::string &data = "")
        {
            if (!login_) throw no_login_set{};

            CURL *curl = curl_easy_init();
            if (curl)
            {
                struct curl_slist *headers = nullptr;
                headers = curl_slist_append(headers, login_->header().c_str());
                headers = curl_slist_append(headers, "Accept: application/json");
                if (!data.empty())
                {
                    headers = curl_slist_append(headers, "Content-Type: application/json");
                }
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                if (method != "GET")
                {
                    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
                }
                if (!data.empty())
                {
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
                }
                else if (method != "GET")
                {
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
                }
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
                std::string response;
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                try
                {
                    CURLcode res = curl_easy_perform(curl);
                    if (res != CURLE_OK)
                    {
                        throw std::runtime_error("Failed to perform request: " + std::string(curl_easy_strerror(res)));
                    }
                    // obtain the response code
                    long response_code;
                    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
                    if (response_code != 200)
                    {
                        throw std::runtime_error(std::format("Request failed with code: {} and message {}", response_code, response));
                    }
                }
                catch (...)
                {
                    curl_slist_free_all(headers);
                    curl_easy_cleanup(curl);
                    throw;
                }
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                return response;
            }
            throw std::runtime_error("Failed to initialize cURL");
        }
        static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
        {
            ((std::string *)userp)->append((char *)contents, size * nmemb);
            return size * nmemb;
        }
        notifier_t notifier_;
    };
}