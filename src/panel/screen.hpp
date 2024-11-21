#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "../hosting/host.hpp"
#include "../hosting/ssh_screen.hpp"
#include "../views/assertion.hpp"

namespace panel {
    struct screen {
        void render(nlohmann::json::array_t const &panel, hosting::local::host &localhost) {
            std::unordered_map<std::string_view, std::function<void(nlohmann::json const &)>> renderers {
                {"ssh-host", [&localhost](nlohmann::json const &element) {
                    // render ssh host
                    auto host = hosting::ssh::host::by_name(element.at("name").get<std::string>());
                    hosting::ssh::screen{}.render(host, localhost);
                }},
                {"group", [&localhost, this](nlohmann::json const &element) {
                    if (ImGui::CollapsingHeader(element.at("title").get<std::string>().c_str())) {
                        ImGui::Indent();
                        render(element.at("contents"), localhost);
                        ImGui::Unindent();
                    }
                }},
                {"assertion", [&localhost](nlohmann::json const &element) {
                    // render assertion
                    std::function<bool()> test = []{ return false; };
                    auto const test_element = element.at("test").get<nlohmann::json::object_t>();
                    auto const type_name = test_element.at("type").get<std::string>();
                    std::unordered_map<std::string, std::function<void()>> tests {
                        {"docker-container-running", [&test_element, &localhost, &test]{
                            auto const container_name = test_element.at("container").get<std::string>();
                            auto const host_name = test_element.at("host").get<std::string>();
                            test = [container_name, host_name, &localhost]{
                                return hosting::ssh::host::by_name(host_name)->docker()
                                .is_container_running(container_name, localhost);
                            };
                        }},
                        {"process-running", [&test_element, &localhost, &test]{
                            auto const process_name = test_element.at("process").get<std::string>();
                            auto const host_name = test_element.at("host").get<std::string>();
                            auto const container_name = test_element.at("container").get<std::string>();
                            test = [process_name, host_name, container_name, &localhost]{
                                return hosting::ssh::host::by_name(host_name)->docker()
                                .is_process_running(container_name, process_name, localhost);
                            };
                        }},
                        {"container-command", [&test_element, &localhost, &test]{
                            auto const host_name = test_element.at("host").get<std::string>();
                            auto const container_name = test_element.at("container").get<std::string>();
                            auto const command = test_element.at("command").get<std::string>();
                            auto string_test = [content = test_element.at("should-contain").get<std::string>()](std::string const &actual) {
                                return actual.find(content) != std::string::npos;
                            };
                            test = [command, host_name, container_name, &localhost, string_test]{
                                return string_test(hosting::ssh::host::by_name(host_name)->docker()
                                .execute_command(command, container_name, localhost, false));
                            };
                        }}

                    };
                    if (auto it = tests.find(type_name); it != tests.end()) {
                        it->second();
                    }
                    views::Assertion(
                        element.at("title").get<std::string>(), test);
                }},
                {"shell-open", [](nlohmann::json const &element) {
                    // execute locally
                    auto command = element.at("command").get<std::string>();
                    if (ImGui::Button(element.at("title").get<std::string>().c_str())) {
                        ShellExecuteA(nullptr, "open", command.c_str(), nullptr, nullptr, SW_SHOW);
                    }
                }},
                {"shell-execute", [](nlohmann::json const &element) {
                    // execute locally
                    auto command = element.at("command").get<std::string>();
                    auto args = element.at("args").get<std::string>();
                    if (ImGui::Button(element.at("title").get<std::string>().c_str())) {
                        ShellExecuteA(nullptr, nullptr, command.c_str(), args.c_str(), nullptr, SW_SHOW);
                    }
                }}
            };
            for (auto const &element : panel) {
                auto type = element.at("type").get<std::string>();
                if (auto it = renderers.find(type); it != renderers.end()) {
                    it->second(element);
                }
            }
        }
    };
}