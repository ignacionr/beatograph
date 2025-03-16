#pragma once

#include <string>

#include "../../util/conversions/rfc2047.hpp"

#include "imap_host.hpp"
#include "../../structural/views/cached_view.hpp"

namespace cloud::mail {
    class screen {
        class message {
        public:
            message(std::string_view header): header_{std::string{header}} {
                auto start = header.find("\r\nSubject: ") + 11;
                auto end = header.find("\r\n", start);
                title_ = conversions::decode_rfc2047(header.substr(start, end - start));
            }

            void render() const {
                ImGui::TextUnformatted(title_.data(), title_.data() + title_.size());
            }
        private:
            std::string header_;
            std::string title_;
        };
    public:
        screen(std::shared_ptr<imap_host> host): host(host) {}
        void render() {
            views::cached_view<std::vector<message>>(
                "Messages",
                [this] {
                    host->connect();
                    // host->list_mailboxes([&mailboxes](std::string_view mailbox) {
                    //     mailboxes.push_back(std::string{mailbox});
                    // });
                    // host->select_mailbox("INBOX");
                    std::vector<message> mails;
                    for (auto uid : host->get_mail_uids()) {
                        mails.push_back(message{host->get_mail_header(uid)});
                    }
                    return mails;
                },
                [](std::vector<message> const& mails) {
                    for (auto const& msg : mails) {
                        msg.render();
                    }
                }
            );
        }
    private:
        std::shared_ptr<imap_host> host;
    };
}