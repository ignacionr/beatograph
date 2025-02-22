#pragma once

#include "pch.h"

#include <imgui.h>

#include "host.hpp"

#include "../../structural/views/cached_view.hpp"
#include "classifier.hpp"
#include "autotext.hpp"

namespace cloud::whatsapp
{
    struct screen
    {
        void render(host &h, auto &img_cache) {
            if (try_connect_) {
                try {
                    long long const texture_id {img_cache.load_texture_from_url(h.get_qr_image_url(), {}, ".png", std::chrono::minutes{1})};
                    ImGui::Image(reinterpret_cast<ImTextureID>(texture_id), ImVec2{345.0f, 345.0f});
                }
                catch(const std::exception &e) {
                    ::MessageBoxA(nullptr, e.what(), "Error", MB_ICONERROR);
                }
            }
            views::cached_view<nlohmann::json>("whatsapp", [&h] {
                return h.status();
            }, [&](nlohmann::json const &status) {
                auto const &status_string {status.at("state").get_ref<const std::string&>()};
                if (status_string == "CONNECTED") {
                    try_connect_ = false;
                    views::cached_view<nlohmann::json>("Chats", [&] {
                        return h.get_chats();
                    }, [&h, this](nlohmann::json const &chats) {
                        static std::string filter;
                        if (filter.reserve(256); ImGui::InputText("Filter", filter.data(), filter.capacity())) {
                            filter = filter.data();
                        }
                        for (auto const &chat : chats.at("chats")) {
                            std::optional<unsigned int> const unread_count {
                                chat.contains("unread_count") ? std::optional<unsigned int>{chat.at("unread_count").get<unsigned int>()} : std::nullopt
                            };
                            auto const &id_serialized {chat.at("id").at("_serialized").get_ref<const std::string&>()};
                            auto const &name {chat.contains("name") ? 
                                chat.at("name").get_ref<const std::string&>() :
                                id_serialized};
                            if (filter.empty() || name.find(filter) != std::string::npos) {
                                auto const classification {classifier_.classify(id_serialized)};
                                if (classification == "work") {
                                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4{1.0f, 0.5f, 0.0f, 0.5f});
                                }
                                else if (classification == "personal") {
                                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4{1.0f, 1.0f, 0.0f, 0.5f});
                                }
                                else {
                                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4{0.5f, 0.5f, 1.5f, 0.5f});
                                }
                                views::cached_view<nlohmann::json>(std::format("{}{}##{}", name, 
                                    unread_count.has_value() ? std::format(" ({})", unread_count.value()) : "",
                                    id_serialized), [&h, &id_serialized] {
                                    return h.fetch_messages(id_serialized);
                                }, [id_serialized, &h, this, classification](nlohmann::json const &messages) {
                                    if (!classification.has_value()) {
                                        classifier_.classify(id_serialized, messages);
                                    }
                                    if (ImGui::BeginTable("Messages", 2)) {
                                        ImGui::TableSetupColumn("From", ImGuiTableColumnFlags_WidthFixed, 93.0f);
                                        ImGui::TableSetupColumn("Body", ImGuiTableColumnFlags_WidthStretch);
                                        ImGui::TableHeadersRow();
                                        for (auto const &message : messages.at("messages")) {
                                            ImGui::TableNextRow();
                                            ImGui::TableNextColumn();
                                            ImGui::TextUnformatted(message.at("from").get_ref<const std::string&>().c_str());
                                            ImGui::TableNextColumn();
                                            ImGui::TextUnformatted(message.at("body").get_ref<const std::string&>().c_str());
                                        }
                                    }
                                    static std::string new_message;
                                    if (new_message.reserve(256); ImGui::InputText("New Message", new_message.data(), new_message.capacity())) {
                                        new_message = new_message.data();
                                    }
                                    if (ImGui::SameLine(); ImGui::Button("Send")) {
                                        h.send_message(id_serialized, new_message);
                                        new_message.clear();
                                    }
                                    ImGui::EndTable();
                                    if (ImGui::BeginCombo("##Classification", classification.value_or("unclassified").c_str(), ImGuiComboFlags_WidthFitPreview)) {
                                        if (ImGui::Selectable("work")) {
                                            classifier_.set(id_serialized, "work");
                                        }
                                        if (ImGui::Selectable("personal")) {
                                            classifier_.set(id_serialized, "personal");
                                        }
                                        if (ImGui::Selectable("unclassified")) {
                                            classifier_.set(id_serialized, "unclassified");
                                        }
                                        ImGui::EndCombo();
                                    }
                                    if (ImGui::SameLine(); ImGui::SmallButton("Say hello")) {
                                        try {
                                            auto msg = autotext().say_hello(messages, new_message);
                                            h.send_message(id_serialized, msg);
                                            new_message.clear();
                                        }
                                        catch(const std::exception &e) {
                                            ::MessageBoxA(nullptr, e.what(), "Error", MB_ICONERROR);
                                        }
                                    }
                                    if (ImGui::SameLine(); ImGui::SmallButton("Reply last")) {
                                        try {
                                            auto msg = autotext().reply_last(messages, new_message);
                                            h.send_message(id_serialized, msg);
                                            new_message.clear();
                                        }
                                        catch(const std::exception &e) {
                                            ::MessageBoxA(nullptr, e.what(), "Error", MB_ICONERROR);
                                        }
                                    }
                                    if (ImGui::SameLine(); ImGui::SmallButton("Reply from Clipboard")) {
                                        try {
                                            auto msg = autotext().reply_last(messages, new_message, true);
                                            h.send_message(id_serialized, msg);
                                            new_message.clear();
                                        }
                                        catch(const std::exception &e) {
                                            ::MessageBoxA(nullptr, e.what(), "Error", MB_ICONERROR);
                                        }
                                    }
                                    if (ImGui::SameLine(); ImGui::SmallButton("Copy Chat ID")) {
                                        ImGui::SetClipboardText(id_serialized.c_str());
                                    }
                                    ImGui::SameLine();
                                });
                                ImGui::PopStyleColor();
                            }
                        }
                        ImGui::Columns();
                    }, true);
                }
                else {
                    ImGui::Text("Status: %s", status_string.c_str());
                }
            }, 
            true,
            std::nullopt,
            [this](std::string_view error) {
                if (error == "session_not_connected") {
                    try_connect_ = true;
                }
            });
        }
    private:
        classifier classifier_;
        bool try_connect_{false};
    };
}
