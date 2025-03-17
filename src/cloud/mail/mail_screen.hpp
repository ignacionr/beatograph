#pragma once

#include <chrono>
#include <format>
#include <string>


#include "imap_host.hpp"
#include "../../structural/views/cached_view.hpp"
#include "message.hpp"
#include "../../registrar.hpp"
#include <cppgpt/cppgpt.hpp>

namespace cloud::mail {
    class screen {
    public:
        screen(std::shared_ptr<imap_host> host): host(host) {}
        void render() {
            views::cached_view<std::vector<std::shared_ptr<message>>>(
                "Messages",
                [this] {
                    host->connect();
                    std::vector<std::shared_ptr<message>> mails;
                    auto gpt = registrar::get<ignacionr::cppgpt>({});
                    auto do_post = [](auto a, auto b, auto c) {
                        http::fetch fetch;
                        return fetch.post(a, b, c);
                    };
                    std::vector<std::function<void()>> tasks;
                    for (auto uid : host->get_mail_uids()) {
                        mails.emplace_back(std::make_shared<message>(uid, host->get_mail_header(uid)));
                        // Start a new thread to get the body of the mail
                        auto mail = mails.back();
                        tasks.push_back([uid, mail, this, gpt, do_post] {
                            // Get the body of the mail
                            auto body = host->get_mail_body(uid);
                            // combine with the header to get the metadata
                            auto header_and_body = std::format("{}\n\n{}", mail->header(), body);
                            mail->get_metadata(header_and_body, gpt, do_post);
                        });
                    }
                    std::thread([tasks] () mutable {
                        auto retry = std::vector<std::function<void()>>{};
                        while (!tasks.empty()) {
                            retry.clear();
                            for (auto &task : tasks) {
                                try {
                                    task();
                                } catch (...) {
                                    retry.push_back(task);
                                }
                            }
                            tasks = retry;
                        }
                    }).detach();
                    return mails;
                },
                [this](std::vector<std::shared_ptr<message>> & mails) {
                    if (ImGui::BeginTable("Messages", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
                        ImGui::TableSetupColumn("From", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 100);
                        for (auto msg : mails) {
                            ImGui::PushID(static_cast<int>(msg->uid()));
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(msg->from().data(), msg->from().data() + msg->from().size());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(msg->title().data(), msg->title().data() + msg->title().size());
                            ImGui::TableNextColumn();
                            auto date_str = std::format("{:%Y-%m-%d %H:%M}", msg->date());
                            ImGui::TextUnformatted(date_str.data(), date_str.data() + date_str.size());

                            // Show metadata on a second row
                            ImGui::TableNextRow();
                            // using the message's urgency, decide the background color
                            if (msg->urgency() == 1) {
                                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImVec4{1.0f, 0.0f, 0.0f, 0.4f}));
                            } else if (msg->urgency() == 2) {
                                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImVec4{1.0f, 1.0f, 0.0f, 0.4f}));
                            }
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(msg->category().data(), msg->category().data() + msg->category().size());
                            ImGui::TableNextColumn();
                            ImGui::TextWrapped("%s", msg->summary().data());
                            ImGui::TableNextColumn();
                            for (auto const& tag : msg->tags()) {
                                ImGui::TextUnformatted(tag.data(), tag.data() + tag.size());
                            }
                            if (ImGui::SmallButton("Delete")) {
                                try {
                                    host->delete_message(msg->uid());
                                    mails.erase(std::remove_if(mails.begin(), mails.end(), [deleted_uid = msg->uid()](auto const& msg) {
                                        return msg->uid() == deleted_uid;
                                    }), mails.end());
                                    ImGui::PopID();
                                    break;
                                }
                                catch (...) {
                                    ImGui::PopID();
                                    ImGui::EndTable();
                                    throw;
                                }
                            }
                            ImGui::PopID();
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