#pragma once

#include <chrono>
#include <format>
#include <string>

#include "../../util/conversions/rfc2047.hpp"

#include "imap_host.hpp"
#include "../../structural/views/cached_view.hpp"

namespace cloud::mail {
    class screen {
        class message {
        public:
            message(std::string_view header): header_{std::string{header}} {
                title_ = conversions::decode_rfc2047(get_header_field(header, "Subject"));
                from_ = conversions::decode_rfc2047(get_header_field(header, "From"));
                auto date_str = get_header_field(header, "Date");
                std::tm tm = {};
                std::istringstream ss{date_str};
                ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M %z");
                date_ = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            }

            static std::string get_header_field(std::string_view header, std::string_view field) {
                auto marker = std::format("\r\n{}: ", field);
                auto start {header.find(marker)};
                if (start == std::string_view::npos) {
                    return {};
                }
                start += marker.size();
                auto end = header.find("\r\n", start);
                return std::string{header.substr(start, end - start)};
            }

            std::string const &title() const { return title_; }
            std::string const &from() const { return from_; }
            std::chrono::system_clock::time_point date() const { return date_; }
        private:
            std::string header_;
            std::string title_;
            std::string from_;
            std::chrono::system_clock::time_point date_;
        };
    public:
        screen(std::shared_ptr<imap_host> host): host(host) {}
        void render() {
            views::cached_view<std::vector<message>>(
                "Messages",
                [this] {
                    host->connect();
                    std::vector<message> mails;
                    for (auto uid : host->get_mail_uids()) {
                        mails.push_back(message{host->get_mail_header(uid)});
                    }
                    return mails;
                },
                [](std::vector<message> const& mails) {
                    if (ImGui::BeginTable("Messages", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
                        ImGui::TableSetupColumn("From", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 100);
                        ImGui::TableHeadersRow();
                        for (auto const& msg : mails) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(msg.from().data(), msg.from().data() + msg.from().size());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(msg.title().data(), msg.title().data() + msg.title().size());
                            ImGui::TableNextColumn();
                            auto date_str = std::format("{:%Y-%m-%d %H:%M}", msg.date());
                            ImGui::TextUnformatted(date_str.data(), date_str.data() + date_str.size());
                        }
                        ImGui::EndTable();
                    }
                },
                true
            );
        }
    private:
        std::shared_ptr<imap_host> host;
    };
}