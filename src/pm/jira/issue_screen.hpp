#pragma once

#include <string>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <imgui.h>
#include "../../../external/cppgpt/cppgpt.hpp"
#include "../../structural/views/json.hpp"
#include "../../structural/views/cached_view.hpp"
#include "colors.hpp"
#include "user_screen.hpp"
#include "jira_content_render.hpp"
#include "../../registrar.hpp"
#include "../../hosting/http/fetch.hpp"
// #include "../../util/imgui_markdown.hpp"

namespace jira
{
    struct issue_screen
    {
        using context_actions_t = std::unordered_map<std::string, std::function<void(nlohmann::json::object_t const &)>>;

        issue_screen()
        {
        }

        void do_async(std::function<void()> fn)
        {
            // std::thread([fn]{
            try
            {
                fn();
            }
            catch (std::exception const &e)
            {
                std::cerr << "Error in async function: " << e.what() << "\n";
            }
            catch (...)
            {
                std::cerr << "Error in async function\n";
            }
            //}).detach();
        }

        bool render(nlohmann::json const &json, host &h, bool expanded = false, context_actions_t const &actions = {}, bool show_json_details = false, bool show_assignee = false)
        {
            bool request_requery = false;
            auto const &key{json.at("key").get_ref<const std::string &>()};
            ImGui::PushID(key.c_str());
            std::string_view color_name{"unknown"};
            static const nlohmann::json::json_pointer status_color_ptr{"/fields/status/statusCategory/colorName"};
            if (json.contains(status_color_ptr))
            {
                color_name = json.at(status_color_ptr).get_ref<std::string const&>();
            }
            ImGui::PushStyleColor(ImGuiCol_Header, color(color_name));
            static const nlohmann::json::json_pointer issue_type_name_ptr{"/fields/issuetype/name"};
            if (ImGui::CollapsingHeader(key.c_str(),
                                        expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0))
            {
                nlohmann::json::object_t const &fields{json.at("fields").get_ref<const nlohmann::json::object_t &>()};
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {20, 20});
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 255));
                ImGui::BeginChild("summary", {ImGui::GetColumnWidth(), 0}, ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY);
                ImGui::TextWrapped("%s\n \n", fields.at("summary").get_ref<const std::string &>().c_str());
                if (show_assignee)
                {
                    ImGui::SeparatorText("Asignee");
                    if (fields.find("assignee") != fields.end() && fields.at("assignee").is_object())
                    {
                        user_screen_.render(fields.at("assignee"));
                    }
                    else
                    {
                        ImGui::Text("Unassigned");
                    }
                }
                if (ImGui::SmallButton(ICON_MD_PERSON ICON_MD_JOIN_LEFT))
                {
                    do_async([&h, key]
                             { h.assign_issue_to_me(key); });
                    request_requery = true;
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    ImGui::SetTooltip("Assign to me");
                }
                if (ImGui::SameLine(); ImGui::SmallButton(ICON_MD_NO_ACCOUNTS))
                {
                    do_async([&h, key]
                             { h.unassign_issue(key); });
                    request_requery = true;
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    ImGui::SetTooltip("Unassign");
                }
                if (ImGui::SameLine(); ImGui::SmallButton(ICON_MD_DONE))
                {
                    do_async([&h, key]
                             { h.transition_issue(key, "31"); });
                    request_requery = true;
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    ImGui::SetTooltip("Resolve");
                }
                if (ImGui::SameLine(); ImGui::SmallButton(ICON_MD_WORK))
                {
                    do_async([&h, key]
                             { h.transition_issue(key, "21"); });
                    request_requery = true;
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    ImGui::SetTooltip("Start Progress");
                }
                if (ImGui::SmallButton(ICON_MD_WEB))
                {
                    auto const url = h.get_browse_url(key);
                    "open"_sfn(url);
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    ImGui::SetTooltip("Open in browser");
                }
                if (ImGui::SameLine(); ImGui::SmallButton(ICON_MD_CONTENT_COPY))
                {
                    auto const url = h.get_browse_url(key);
                    ImGui::SetClipboardText(url.c_str());
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    ImGui::SetTooltip("Copy URL to clipboard");
                }
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(255, 90, 90, 255));
                for (auto const &[action_name, action_fn] : actions)
                {
                    if (ImGui::SameLine(); ImGui::SmallButton(action_name.c_str()))
                    {
                        action_fn(json);
                        request_requery = true;
                    }
                }
                ImGui::PopStyleColor();
                ImGui::EndChild();
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();
                // are there subtasks?
                if (fields.contains("subtasks"))
                {
                    ImGui::Indent();
                    nlohmann::json::array_t const &subtasks{fields.at("subtasks").get_ref<const nlohmann::json::array_t &>()};
                    for (nlohmann::json const &subtask : subtasks)
                    {
                        request_requery |= render(subtask, h, false, actions, show_json_details, show_assignee);
                    }
                    ImGui::Unindent();
                }
                views::cached_view<nlohmann::json>("Comments", [&h, key]
                                                   { return h.get_issue_comments(key); }, 
                                                   [&h, key, this, show_json_details, &request_requery](nlohmann::json const &comments)
                                                   {
                    if (!comments.contains("comments")) {
                        return;
                    }
                    if (ImGui::BeginTable("comments", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersH))
                    {
                        ImGui::TableSetupColumn("Author", ImGuiTableColumnFlags_WidthFixed, 64);
                        ImGui::TableSetupColumn("Comment", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableHeadersRow();
                        for (nlohmann::json const &comment : comments.at("comments").get_ref<const nlohmann::json::array_t&>())
                        {
                            ImGui::TableNextColumn();
                            ImGui::Text("At: %s", comment.at("created").get_ref<const std::string &>().c_str());
                            if (comment.contains("author")) {
                                user_screen_.render(comment.at("author"));
                            }
                            else {
                                ImGui::Text("Unknown author");
                            }
                            ImGui::TableNextColumn();
                            jira::content::render(comment.at("body"));
                            if (show_json_details) {
                                ImGui::TextWrapped("%s", comment.at("body").dump().c_str());
                            }
                        }
                        ImGui::EndTable();
                    }

                    std::string comment_body;
                    if (comment_body.reserve(256); ImGui::InputText("Comment", comment_body.data(), comment_body.capacity(), ImGuiInputTextFlags_EnterReturnsTrue)) {
                        comment_body = comment_body.data();
                        do_async([&h, key, comment_body] { h.add_comment(key, comment_body); });
                        request_requery = true;
                    } });
                views::cached_view<std::string>("Summary", [&h, key, json]
                                                { 
                        auto gpt = registrar::get<ignacionr::cppgpt>({});
                        gpt->clear();
                        gpt->add_instructions("You are a professional Jira analyst and Product Owner. Following is an issue.");
                        // Compose the context with the issue and the comments
                        auto full_context = std::format("{}\n{}", json.dump(), h.get_issue_comments(key).dump());
                        gpt->add_instructions(full_context);
                        auto reply = gpt->sendMessage("Summarize this issue and its comments. What is the status of the issue? What is the next step?",
                            [](auto url, auto body, auto header_setter) {
                                http::fetch fetch;
                                return fetch.post(url, body, header_setter);
                            },
                            "user", "grok-2-latest");
                        return reply["choices"][0]["message"]["content"].get<std::string>(); }, [](std::string const &summary)
                                                { 
                                                    ImGui::TextWrapped("%s", summary.c_str()); 
                                                    // imgui_markdown_renderer markdown_renderer;
                                                    // markdown_renderer.boldFont = ImGui::GetIO().Fonts->Fonts[4];
                                                    // markdown_renderer.italicFont = ImGui::GetIO().Fonts->Fonts[3];
                                                    // markdown_renderer.headerFonts[0] = 
                                                    //     markdown_renderer.headerFonts[1] =
                                                    //     markdown_renderer.headerFonts[2] =
                                                    //     markdown_renderer.headerFonts[3] =
                                                    //     markdown_renderer.headerFonts[4] =
                                                    //     markdown_renderer.headerFonts[5] =
                                                    //     ImGui::GetIO().Fonts->Fonts[2];
                                                    // markdown_renderer.parse(summary);
                                                    // markdown_renderer.render_parsed();
                                                });
                if (show_json_details)
                {
                    ImGui::Indent();
                    if (ImGui::CollapsingHeader("All Details"))
                    {
                        json_view_.render(fields);
                    }
                    ImGui::Unindent();
                }
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
            return request_requery;
        }

    private:
        views::json json_view_;
        user_screen user_screen_;
    };
} // namespace jira
