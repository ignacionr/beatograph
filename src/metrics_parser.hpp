#pragma once
#include <functional>
#include <string>
#include "metric_value.hpp"

struct metrics_parser {
    void operator()(std::string_view contents) {
        // Parse the contents line by line
        size_t start = 0;
        size_t end = 0;
        while (end != std::string::npos) {
            end = contents.find('\n', start);
            if (buffer.empty()) {
                parse_line(contents.substr(start, end - start));
            } else {
                buffer += contents.substr(start, end - start);
                parse_line(buffer);
                buffer.clear();
            }
            start = end + 1;
        }
        // save leftovers on the buffer string
        buffer += contents.substr(start);
    }

    void parse_line(std::string_view line) {
        // Parse the line and store the metric value
        if (line.empty()) {
            return;
        }

        enum class character_type {
            space,
            equal,
            quote,
            comma,
            hash,
            open_curly,
            close_curly,
            other
        };

        auto get_character_type = [](char c) -> character_type {
            switch (c) {
                case ' ':
                    return character_type::space;
                case '=':
                    return character_type::equal;
                case '"':
                    return character_type::quote;
                case ',':
                    return character_type::comma;
                case '#':
                    return character_type::hash;
                case '{':
                    return character_type::open_curly;
                case '}':
                    return character_type::close_curly;
                default:
                    return character_type::other;
            }
        };

        // Forward declaration of state function type
        using state = std::function<void*(char, character_type)>;

        enum class comment_type {
            none,
            type,
            help,
            unknown
        };
        comment_type comment = comment_type::none;
        std::string comment_name;
        std::string metric_name;
        std::string comment_value;
        std::string error_message;
        std::string metric_string_value;
        std::string current_tag_name;
        std::string current_tag_value;
        std::string current_metric_value;
        metric_value mv;

        state error_state = [&](char c, character_type t)  {
            return &error_state;
        };

        state read_metric_value_state = [&](char c, character_type t) {
            if (t == character_type::other) {
                metric_string_value.push_back(c);
                return &read_metric_value_state;
            }
            error_message = "Unexpected character in metric value";
            return &error_state;
        };

        std::reference_wrapper<state> after_metric_name {read_metric_value_state};
        state *read_tags_state_ptr;
        state metric_name_state = [&](char c, character_type t) {
            if (t == character_type::other) {
                metric_name.push_back(c);
                return &metric_name_state;
            }
            else if (t == character_type::open_curly) {
                return read_tags_state_ptr;
            }
            else if (t == character_type::space) {
                return &after_metric_name.get();
            }
            error_message = "Unexpected character in metric name";
            return &error_state;
        };
        
        state read_tags_value_state = [&](char c, character_type t) {
            if (t == character_type::other) {
                current_tag_value.push_back(c);
                return &read_tags_value_state;
            }
            else if (t == character_type::quote) {
                return &read_tags_value_state;
            }
            else if (t == character_type::comma || t == character_type::close_curly) {
                mv.labels[current_tag_name] = current_tag_value;
                current_tag_name.clear();
                current_tag_value.clear();
                return (t == character_type::comma) ? read_tags_state_ptr : &metric_name_state;
            }
            error_message = "Unexpected character in tag value";
            return &error_state;
        };

        state read_tags_state = [&](char c, character_type t) {
            if (t == character_type::close_curly) {
                return &metric_name_state;
            }
            else if (t == character_type::other) {
                current_tag_name.push_back(c);
                return &read_tags_state;
            }
            else if (t == character_type::equal) {
                return &read_tags_value_state;
            }
            error_message = "Unexpected character in tags, type: " + std::to_string((int)t) + " char: " + c;
            return &error_state;
        };
        read_tags_state_ptr = &read_tags_state;

        state read_comment_value_state = [&](char c, character_type t) {
            comment_value.push_back(c);
            return &read_comment_value_state;
        };
        
        state hash_state = [&](char c, character_type t) {
            if (t == character_type::space) {
                if (comment_name == "TYPE") {
                    comment = comment_type::type;
                    after_metric_name = read_comment_value_state;
                    return &metric_name_state;
                }
                else if (comment_name == "HELP") {
                    comment = comment_type::help;
                    after_metric_name = read_comment_value_state;
                    return &metric_name_state;
                }
                if (comment_name.empty()) {
                    return &hash_state; // ignore
                }
                error_message = "Unknown comment type";
                return &error_state;
            }
            else if (t == character_type::other) {
                comment_name.push_back(c);
                return &hash_state;
            }
            error_message = "Unexpected character after hash of type " + std::to_string((int)t);
            return &error_state;
        };
        state read_value = [&](char c, character_type t) {
            if (t == character_type::other) {
                return &read_value;
            }
            error_message = "Unexpected character after value";
            return &error_state;
        };

        state start_state = [&](char c, character_type t){
            if (t == character_type::hash) {
                comment = comment_type::unknown;
                return &hash_state;
            }
            else if (t == character_type::space) {
                return &start_state;
            }
            else if (t == character_type::other) {
                metric_name.push_back(c);
                return &metric_name_state;
            }
            error_message = "Unexpected character at start of line";
            return &error_state;
        };
    
        state *current_state = &start_state;
        for (char c : line) {
            character_type t = get_character_type(c);
            current_state = reinterpret_cast<state*>((*current_state)(c, t));
        }
        if (current_state == &error_state) {
            std::cerr << "Error parsing line: " << line << std::endl
                      << "Error message: " << error_message << std::endl;
            return;
        }
        
        if (comment == comment_type::type) {
            metric_type(metric_name, comment_value);
        }
        else if (comment == comment_type::help) {
            metric_help(metric_name, comment_value);
        }
        else {
            metric_metric_value(metric_name, {mv.labels, std::stod(metric_string_value)});
        }
    
    }

    std::function<void(std::string_view,std::string_view)> metric_type;
    std::function<void(std::string_view,std::string_view)> metric_help;
    std::function<void(std::string_view,metric_value)> metric_metric_value;
    std::string buffer;
};