#pragma once

#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <imgui.h>

#include "../hosting/host.hpp"
#include "../hosting/ssh_screen.hpp"
#include "../views/assertion.hpp"
#include "../views/cached_view.hpp"

namespace panel {
    struct config {
        using fn_t = std::function<void()>;

        config(nlohmann::json::array_t const &panel, hosting::local::host &localhost)
        {
            render_ = render_to_cache(panel, localhost);
        }

        void render() const {
            render_();
        }

    private:
        fn_t render_to_cache(nlohmann::json const &panel, hosting::local::host &localhost) 
        {
            std::unordered_map<std::string_view, std::function<fn_t(nlohmann::json const &)>> renderers {
                {"ssh-host", [&localhost, this] (nlohmann::json const &element) -> fn_t {
                    // render ssh host
                    auto host = hosting::ssh::host::by_name(element.at("name").get<std::string>());
                    return [host, &localhost, this]{ssh_screen_.render(host, localhost);};
                }},
                {"group", [&localhost, this] (nlohmann::json const &element) -> fn_t {
                    fn_t content_render = render_to_cache(element.at("contents"), localhost);
                    return [content_render, title = element.at("title").get<std::string>()]{
                        if (ImGui::CollapsingHeader(title.c_str())) {
                            ImGui::Indent();
                            content_render();
                            ImGui::Unindent();
                        }
                    };
                }},
                {"container-command-output", [&localhost] (nlohmann::json const &element) -> fn_t {
                    auto const host_name = element.at("host").get<std::string>();
                    auto const container_name = element.at("container").get<std::string>();
                    auto const command = element.at("command").get<std::string>();
                    auto const title = element.at("title").get<std::string>();
                    return [command, host_name, container_name, &localhost, title]{
                        views::cached_view<std::string>(title,
                        [host_name, command, container_name, &localhost]{ return hosting::ssh::host::by_name(host_name)->docker()
                        .execute_command(command, container_name, localhost, false);},
                        [](std::string const &output) {
                            ImGui::TextWrapped("%s", output.c_str());
                        });
                    };
                }},
                {"ssh-command-output", [&localhost] (nlohmann::json const &element) -> fn_t {
                    auto const host_name = element.at("host").get<std::string>();
                    auto const command = element.at("command").get<std::string>();
                    auto const title = element.at("title").get<std::string>();
                    return [command, host_name, &localhost, title]{
                        views::cached_view<std::string>(title,
                        [host_name, command, &localhost]{ return hosting::ssh::host::by_name(host_name)->execute_command(command, localhost);},
                        [](std::string const &output) {
                            ImGui::TextWrapped("%s", output.c_str());
                        });
                    };
                }},
                {"map-port", [&localhost, this] (nlohmann::json const &element) -> fn_t {
                    auto const host_name = element.at("host").get<std::string>();
                    auto const port = element.at("port").get<int>();
                    auto const title = element.at("title").get<std::string>();
                    return [host_name, port, &localhost, title, this]{
                        if (ImGui::Button(title.c_str())) {
                            mappings_.push_back(std::make_unique<hosting::local::mapping>(port, host_name, localhost));
                        }
                    };
                }},
                {"assertion", [&localhost](nlohmann::json const &element) -> fn_t {
                    // render assertion
                    std::function<bool()> test = []{ return false; };
                    auto const test_element = element.at("test").get<nlohmann::json::object_t>();
                    auto const type_name = test_element.at("type").get<std::string>();
                    std::unordered_map<std::string, fn_t> tests {
                        {"docker-container-running", [&test_element, &localhost, &test]{
                            auto const container_name = test_element.at("container").get<std::string>();
                            auto const host_name = test_element.at("host").get<std::string>();
                            test = [container_name, host_name, &localhost]{
                                return hosting::ssh::host::by_name(host_name)->docker()
                                .is_container_running(container_name, localhost);
                            };
                        }},
                        {"docker-process-running", [&test_element, &localhost, &test]{
                            auto const process_name = test_element.at("process").get<std::string>();
                            auto const host_name = test_element.at("host").get<std::string>();
                            auto const container_name = test_element.at("container").get<std::string>();
                            test = [process_name, host_name, container_name, &localhost] {
                                return hosting::ssh::host::by_name(host_name)->docker()
                                .is_process_running(container_name, process_name, localhost);
                            };
                        }},
                        {"container-command", [&test_element, &localhost, &test]{
                            auto const host_name = test_element.at("host").get<std::string>();
                            auto const container_name = test_element.at("container").get<std::string>();
                            auto const command = test_element.at("command").get<std::string>();
                            std::function<bool(std::string_view)> string_test = [](std::string_view result){ return !result.empty(); };
                            if (test_element.contains("should-contain")) {
                                auto content = test_element.at("should-contain").get<std::string>();
                                string_test = [content](std::string_view actual) -> bool {
                                    return actual.find(content) != std::string::npos;
                                };
                            }
                            else if (test_element.contains("should-not-contain")) {
                                auto content = test_element.at("should-not-contain").get<std::string>();
                                string_test = [content](std::string_view actual) -> bool {
                                    return actual.find(content) == std::string::npos;
                                };
                            }
                            test = [command, host_name, container_name, &localhost, string_test]{
                                return string_test(hosting::ssh::host::by_name(host_name)->docker()
                                .execute_command(command, container_name, localhost, false));
                            };
                        }}
                    };
                    if (auto it = tests.find(type_name); it != tests.end()) {
                        it->second();
                    }
                    return [title = element.at("title").get<std::string>(), test]{
                        views::Assertion(title, test);
                    };
                }},
                {"shell-open", [](nlohmann::json const &element) {
                    // execute locally
                    auto command = element.at("command").get<std::string>();
                    return [command, title = element.at("title").get<std::string>()]{
                        if (ImGui::Button(title.c_str())) {
                            ShellExecuteA(nullptr, "open", command.c_str(), nullptr, nullptr, SW_SHOW);
                        }
                    };
                }},
                {"shell-execute", [](nlohmann::json const &element) {
                    // execute locally
                    auto command = element.at("command").get<std::string>();
                    auto args = element.at("args").get<std::string>();
                    return [command, args, title = element.at("title").get<std::string>()]{
                        if (ImGui::Button(title.c_str())) {
                            ShellExecuteA(nullptr, nullptr, command.c_str(), args.c_str(), nullptr, SW_SHOW);
                        }
                    };
                }}
            };

            fn_t result = []{};
            for (auto const &element : panel) {
                auto type = element.at("type").get<std::string>();
                if (auto it = renderers.find(type); it != renderers.end()) {
                    auto previous_result = result;
                    auto action = it->second(element);
                    result = [previous_result, action] { previous_result(); action(); };
                }
            }
            return result;
        }

        std::function<void()> render_;
        hosting::ssh::screen ssh_screen_;
        std::vector<std::unique_ptr<hosting::local::mapping>> mappings_;
    };
}