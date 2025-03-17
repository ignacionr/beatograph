#pragma once

#include <chrono>
#include <set>
#include <string>
#include "../../util/conversions/rfc2047.hpp"

#include <cppgpt/cppgpt.hpp>

namespace cloud::mail
{
    class message
    {
    public:
        message(long long uid, std::string_view header) : uid_{uid}, header_{std::string{header}}
        {
            title_ = conversions::decode_rfc2047(get_header_field(header, "Subject"));
            from_ = conversions::decode_rfc2047(get_header_field(header, "From"));
            auto date_str = get_header_field(header, "Date");
            std::tm tm = {};
            std::istringstream ss{date_str};
            ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M %z");
            date_ = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        }

        static std::string get_header_field(std::string_view header, std::string_view field)
        {
            auto marker = std::format("\r\n{}: ", field);
            auto start{header.find(marker)};
            if (start == std::string_view::npos)
            {
                return {};
            }
            start += marker.size();
            auto end = header.find("\r\n", start);
            return std::string{header.substr(start, end - start)};
        }

        void get_metadata(std::string_view body, std::shared_ptr<ignacionr::cppgpt> gpt, auto do_post) {
            auto conversation = gpt->new_conversation();
            conversation.add_instructions(R"(You are a mail reader. Given the following email, extract the metadata into a JSON object of the form {"urgency": 1(highest)..3(lowest), "category": "primary"|"promotions"|"social"|"updates"|"forums", "summary": <full summary of the message>, "tags": ["topic1", ..., "topicN"]}.)");
            metadata_ = conversation.sendMessage(body, do_post)["choices"][0]["message"]["content"];

            // parse as a JSON object (remove the ```json prefix and suffix)
            auto start = metadata_.find("{");
            auto end = metadata_.rfind("}");
            metadata_ = metadata_.substr(start, end - start + 1);

            // parse as JSON
            auto metadata = nlohmann::json::parse(metadata_);
            if (metadata.contains("urgency")) {
                urgency_ = metadata["urgency"].get<int>();
            }
            if (metadata.contains("category")) {
                category_ = metadata["category"].get<std::string>();
            }
            if (metadata.contains("summary")) {
                summary_ = metadata["summary"].get<std::string>();
            }
            if (metadata.contains("tags")) {
                for (auto const &tag : metadata["tags"]) {
                    tags_.insert(tag.get<std::string>());
                }
            }
        }

        std::string const &metadata() const { return metadata_; }

        std::string const &title() const { return title_; }
        std::string const &from() const { return from_; }
        std::string const &header() const { return header_; }
        std::chrono::system_clock::time_point date() const { return date_; }

        int urgency() const { return urgency_; }
        std::string const &category() const { return category_; }
        std::string const &summary() const { return summary_; }
        std::set<std::string> const &tags() const { return tags_; }
        long long uid() const { return uid_; }

    private:
        long long uid_;
        std::string header_;
        std::string title_;
        std::string from_;
        std::string metadata_;
        int urgency_{0};
        std::string category_;
        std::string summary_;
        std::set<std::string> tags_;
        std::chrono::system_clock::time_point date_;
    };
}