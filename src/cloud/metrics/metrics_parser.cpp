#include "pch.h"
#include <stdexcept>
#include "metrics_parser.hpp"

void metrics_parser::operator()(std::string_view contents)
{
    // Parse the contents line by line
    size_t start = 0;
    size_t end = 0;
    while (end != std::string::npos)
    {
        end = contents.find('\n', start);
        if (end != std::string_view::npos) {
            if (buffer.empty())
            {
                parse_line(contents.substr(start, end - start));
            }
            else
            {
                buffer += contents.substr(start, end - start);
                parse_line(buffer);
                buffer.clear();
            }
            start = (end == std::string::npos) ? contents.size() : (end + 1);
        }
    }
    // save leftovers on the buffer string
    buffer += contents.substr(start);
}

enum class character_type
{
    other,
    space,
    equal,
    quote,
    comma,
    hash,
    open_curly,
    close_curly,
};

struct character_classifier {
    character_type character_types_[255] {character_type::other};
    character_classifier() {
        character_types_[' '] = character_type::space;
        character_types_['='] = character_type::equal;
        character_types_['"'] = character_type::quote;
        character_types_[','] = character_type::comma;
        character_types_['#'] = character_type::hash;
        character_types_['{'] = character_type::open_curly;
        character_types_['}'] = character_type::close_curly;
    }
    character_type operator()(char c) const {
        return character_types_[c];
    }
};

void metrics_parser::parse_line(std::string_view line)
{
    // Parse the line and store the metric value
    if (line.empty())
    {
        return;
    }

    static character_classifier get_character_type;

    // Forward declaration of state function type
    using state = std::function<void *(char, character_type)>;

    enum class comment_type
    {
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
    mv.timestamp = sample_time;

    state error_state = [&](char, character_type)
    {
        return &error_state;
    };

    state read_metric_value_state = [&](char c, character_type t)
    {
        if (t == character_type::other)
        {
            metric_string_value.push_back(c);
            return &read_metric_value_state;
        }
        error_message = "Unexpected character in metric value";
        return &error_state;
    };

    std::reference_wrapper<state> after_metric_name{read_metric_value_state};
    state *read_tags_state_ptr;
    state metric_name_state = [&](char c, character_type t)
    {
        if (t == character_type::other)
        {
            metric_name.push_back(c);
            return &metric_name_state;
        }
        else if (t == character_type::open_curly)
        {
            return read_tags_state_ptr;
        }
        else if (t == character_type::space)
        {
            return &after_metric_name.get();
        }
        error_message = "Unexpected character in metric name";
        return &error_state;
    };

    bool in_quotes;
    state read_tags_value_state = [&](char c, character_type t)
    {
        if (t == character_type::quote)
        {
            if (in_quotes)
            {
                mv.labels[current_tag_name] = current_tag_value;
                return read_tags_state_ptr;
            }
            in_quotes = true;
            return &read_tags_value_state;
        }
        current_tag_value.push_back(c);
        return &read_tags_value_state;
    };

    state read_tags_state = [&](char c, character_type t)
    {
        if (t == character_type::close_curly)
        {
            return &metric_name_state;
        }
        else if (t == character_type::other)
        {
            current_tag_name.push_back(c);
            return &read_tags_state;
        }
        else if (t == character_type::comma)
        {
            current_tag_name.clear();
            return &read_tags_state;
        }
        else if (t == character_type::equal)
        {
            in_quotes = false;
            return &read_tags_value_state;
        }
        error_message = "Unexpected character in tags, type: " + std::to_string((int)t) + " char: " + c;
        return &error_state;
    };
    read_tags_state_ptr = &read_tags_state;

    state read_comment_value_state = [&](char c, character_type)
    {
        comment_value.push_back(c);
        return &read_comment_value_state;
    };

    state hash_state = [&](char c, character_type t)
    {
        if (t == character_type::space)
        {
            if (comment_name == "TYPE")
            {
                comment = comment_type::type;
                after_metric_name = read_comment_value_state;
                return &metric_name_state;
            }
            else if (comment_name == "HELP")
            {
                comment = comment_type::help;
                after_metric_name = read_comment_value_state;
                return &metric_name_state;
            }
            if (comment_name.empty())
            {
                return &hash_state; // ignore
            }
            error_message = "Unknown comment type";
            return &error_state;
        }
        else if (t == character_type::other)
        {
            comment_name.push_back(c);
            return &hash_state;
        }
        error_message = "Unexpected character after hash of type " + std::to_string((int)t);
        return &error_state;
    };

    state read_value = [&](char, character_type t)
    {
        if (t == character_type::other)
        {
            return &read_value;
        }
        error_message = "Unexpected character after value";
        return &error_state;
    };

    state start_state = [&](char c, character_type t)
    {
        if (t == character_type::hash)
        {
            comment = comment_type::unknown;
            return &hash_state;
        }
        else if (t == character_type::space)
        {
            return &start_state;
        }
        else if (t == character_type::other)
        {
            metric_name.push_back(c);
            return &metric_name_state;
        }
        error_message = "Unexpected character at start of line";
        return &error_state;
    };

    state *current_state = &start_state;
    for (char c : line)
    {
        character_type t = get_character_type(c);
        current_state = reinterpret_cast<state *>((*current_state)(c, t));
    }
    if (current_state == &error_state)
    {
        throw std::runtime_error("Error parsing line: " + std::string(line) + "\nError message: " + error_message);
    }

    if (comment == comment_type::type)
    {
        metric_type(metric_name, comment_value);
    }
    else if (comment == comment_type::help)
    {
        metric_help(metric_name, comment_value);
    }
    else
    {
        mv.value = std::stod(metric_string_value);
        metric_metric_value(metric_name, std::move(mv));
    }
}
