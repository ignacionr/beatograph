#pragma once

#include <memory>
#include <string>

#include "../registrar.hpp"
#include <cppgpt/cppgpt.hpp>

namespace services {
    struct summarize {
        std::string operator()(std::string_view contents) {
            // summarize the contents using gpt
            auto conversation = gpt_->new_conversation();
            conversation.add_instructions("You are a qualified expert. Summarize the contents of the following in English; avoid technicallities related to it is encoded/presented, go for the essence of the content, including the author. Add your own opinion about it at the end of the report.");
            auto const response = conversation.sendMessage(contents,[](auto a, auto b, auto c) { return http::fetch{240}.post(a, b, c); });
            return response["choices"][0]["message"]["content"].get_ref<std::string const &>();
        }
    private:
        std::shared_ptr<ignacionr::cppgpt> gpt_ = registrar::get<ignacionr::cppgpt>({});
    };
}