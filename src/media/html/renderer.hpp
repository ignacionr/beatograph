#pragma once

#include <algorithm>
#include <format>
#include <functional>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
#include <set>

#include <imgui.h>

namespace media::html
{
    struct renderer
    {
        template<typename T>
        struct html_element_type {
            std::string tag_name;
            std::function<void(T const &)> render_prolog;
            std::function<void(T const &)> render_epilog;
        };

        struct html_element: html_element_type<html_element> {
            html_element(html_element_type &type) : type(type) {}

            std::string tag_name() const { return type.tag_name; }
            std::unordered_map<std::string, std::string> attributes;
            std::vector<std::shared_ptr<html_element>> children;
            html_element* parent {nullptr};
            html_element_type& type;

            void render() noexcept {
                type.render_prolog(*this);
                for (auto const &child : children) {
                    child->render();
                }
                type.render_epilog(*this);
            }
        };

        static std::function<void()> create_renderer(std::string const &html)
        {
            std::function<void()> render = []{};

            std::set<html_element_type<html_element>> element_types {
                {"html", {}, {}},
                {"table", [](html_element const &el) {
                        // what is the largest count of "td" inside the "tr" elements?
                        int max_columns = 0;
                        for (auto const &child : el.children) {
                            if (child->tag_name() == "tr") {
                                // count the number of "td" elements
                                int columns = std::accummulate(child->children.begin(), child->children.end(), 0, [](size_t count, auto const &el) {
                                    return count + (el->tag_name() == "td" ? 1 : 0);
                                });
                                max_columns = std::max(max_columns, columns);
                            }
                        }
                        ImGui::BeginTable("table", max_columns);
                    }, [](html_element const &){
                        ImGui::EndTable();
                    }},
                {"tr", [](html_element const &){ ImGui::TableNextRow(); }, {}},
                {"td", [](html_element const &){ ImGui::TableNextColumn(); }, {}}
            };

            auto root = std::make_shared<html_element>(*element_types.find({"html"}));
            auto current = root.get();
            std::string buffer;
            enum class char_type { lt, gt, slash, other };
            enum class status { text, tag, closing_tag } s = status::text;
            
            auto process_text = [&buffer] () {
                // TODO: keep the independent text
                buffer.clear();
            };

            auto process_tag = [&] () {
                if (s == status::closing_tag) {
                    // make sure we are closing the current tag
                    if (buffer != current->tag_name()) {
                        throw std::runtime_error(std::format("Invalid closing tag {} when current is {}", buffer, current->tag_name()));
                    }
                    current = current->parent;
                }
                else {
                    // obtain the tag name (first word)
                    auto space_pos = buffer.find(' ');
                    auto tag_name = buffer.substr(0, space_pos);
                    auto pos = element_types.find({tag_name});
                    if (pos != element_types.end()) {
                        auto new_element = std::make_shared<html_element>(*pos);
                        new_element->parent = current;
                        current->children.push_back(new_element);
                        current = new_element.get();
                    }
                }
                buffer.clear();
            };
            auto add_to_buffer = [&buffer, &s](char c) -> status {
                if (!std::isspace(c) || (!buffer.empty() && std::isspace(buffer.back()))) {
                    buffer += c;
                }
                return s;
            };

            static std::unordered_map<std::pair<status, char_type>, std::function<status(char)>> const actions {
                {{status::text, char_type::lt}, [process_text](char c) {
                    process_text();
                    return status::tag; }},
                {{status::text, char_type::gt}, add_to_buffer},
                {{status::text, char_type::slash}, add_to_buffer},
                {{status::text, char_type::other}, add_to_buffer},
                {{status::tag, char_type::lt}, add_to_buffer},
                {{status::tag, char_type::gt}, [process_tag](char c) { 
                    process_tag(); 
                    return status::text; }},
                {{status::tag, char_type::slash}, [&buffer, add_to_buffer](char c) {
                    if (buffer.empty()) 
                        return status::closing_tag; 
                    else {
                         return add_to_buffer(c); 
                    } }},
                {{status::tag, char_type::other}, [](char c) { return status::tag; }},
                {{status::closing_tag, char_type::gt}, [process_tag](char c) { 
                    process_tag();
                    return status::text; }},
                {{status::closing_tag, char_type::other}, add_to_buffer}
            };
            for (char c: html) {
                char_type ct = char_type::other;
                if (c == '<') {
                    ct = char_type::lt;
                }
                else if (c == '>') {
                    ct = char_type::gt;
                }
                else if (c == '/') {
                    ct = char_type::slash;
                }
                auto pos = actions.find({s, ct});
                if (pos == actions.end()) {
                    throw std::runtime_error("Invalid character in html");
                }
                s = pos->second(c);
            }
        }
        void render(std::string const &html) noexcept
        {
            if (html_ != html) {
                render_ = create_renderer(html);
            }
            render_();
        }
    private:
        std::string html_;
        std::function<void()> render_ {[]{}};
    };
}
