#pragma once

#include "pch.h"

#include <imgui.h>

#include "host.hpp"

#include "../../structural/views/cached_view.hpp"

namespace cloud::whatsapp
{
    struct screen
    {
        void render(host &h) {
            views::cached_view<nlohmann::json>("whatsapp", [&h] {
                return h.status();
            }, [&](nlohmann::json const &status) {
                auto const &status_string {status.at("state").get_ref<const std::string&>()};
                if (status_string == "CONNECTED") {
                    views::cached_view<nlohmann::json>("Chats", [&] {
                        return h.get_chats();
                    }, [&h](nlohmann::json const &chats) {
                        static std::string filter;
                        if (filter.reserve(256); ImGui::InputText("Filter", filter.data(), filter.capacity())) {
                            filter = filter.data();
                        }
                        for (auto const &chat : chats.at("chats")) {
                            auto const &name {chat.at("name").get_ref<const std::string&>()};
                            auto const &id_serialized {chat.at("id").at("_serialized").get_ref<const std::string&>()};
                            if (filter.empty() || name.find(filter) != std::string::npos) {
                                views::cached_view<nlohmann::json>(name, [&h, &id_serialized] {
                                    return h.fetch_messages(id_serialized);
                                }, [id_serialized, &h](nlohmann::json const &messages) {
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
                                    if (new_message.reserve(256); ImGui::InputText("New Message", new_message.data(), new_message.capacity(), ImGuiInputTextFlags_EnterReturnsTrue)) {
                                        new_message = new_message.data();
                                        // send the message
                                        h.send_message(id_serialized, new_message);
                                        new_message.clear();
                                    }
                                    ImGui::EndTable();
                                    if (ImGui::SmallButton("Copy Chat ID")) {
                                        ImGui::SetClipboardText(id_serialized.c_str());
                                    }
                                    ImGui::SameLine();
                                });
                            }
                        }
                        ImGui::Columns();
                    });
                }
                else {
                    ImGui::Text("Status: %s", status_string.c_str());
                }
            });
        }
    };
}
