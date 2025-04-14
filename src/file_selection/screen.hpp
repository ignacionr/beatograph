#pragma once

#include "pch.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include <imgui.h>

#include "../registrar.hpp"
#include "../structural/text_command/host.hpp"

namespace file_selection
{
    class Screen
    {
    public:
        Screen(auto const &base_path): base_path_{base_path} { set_filter(filter_); }
        void render() noexcept {
            bool filter_dirty = false;
            if (base_path_.reserve(MAX_PATH); ImGui::InputText("Base Path", base_path_.data(), base_path_.capacity()))
            {
                base_path_ = base_path_.data();
                filter_dirty = true;
            }
            if (filter_.reserve(MAX_PATH); ImGui::InputText("Filter", filter_.data(), filter_.capacity()))
            {
                filter_ = filter_.data();
                filter_dirty = true;
            }
            if (command_format_.reserve(4000); ImGui::InputTextMultiline("Command Format", command_format_.data(), command_format_.capacity()))
            {
                command_format_ = command_format_.data();
            }
            if (output_file_name_format_.reserve(MAX_PATH); ImGui::InputText("Output File Name Format", output_file_name_format_.data(), output_file_name_format_.capacity()))
            {
                output_file_name_format_ = output_file_name_format_.data();
            }
            ImGui::Checkbox("Append Mode", &append_mode_);
            if (ImGui::Button("Unselect All")) {
                for (auto &[name, selected] : files_) {
                    selected = false;
                }
            }
            if (ImGui::SameLine(); ImGui::Button("Select All")) {
                for (auto &[name, selected] : files_) {
                    selected = true;
                }
            }
            if (ImGui::SameLine(); ImGui::Button("Show Contents")) {
                load_contents();
            }
            if (ImGui::SameLine(); ImGui::Button("Run")) {
                run_async();
            }
            if (filter_dirty) {
                set_filter(filter_);
            }
            if (!error_.empty()) {
                ImGui::TextColored(ImVec4{1.0f, 0.0f, 0.0f, 1.0f}, error_.c_str());
            }
            std::lock_guard lock(mutex_results_);
            for (auto &[name, selected] : files_) {
                ImGui::Checkbox(name.c_str(), &selected);
                if (selected) {
                    auto const &result = results_.find(name);
                    if (result != results_.end()) {
                        ImGui::Columns(2);
                        ImGui::TextUnformatted(result->second.first.c_str());
                        ImGui::NextColumn();
                        ImGui::TextUnformatted(result->second.second.c_str());
                        ImGui::Columns();
                    }
                }
            }
        }

        void add_directory(std::filesystem::directory_iterator const &dir) {
            for (auto const &entry : dir)
            {
                if (entry.is_regular_file() && entry.path().filename().string().contains(filter_))
                {
                    files_.emplace(entry.path().string(), true);
                }
                else if (entry.is_directory())
                {
                    add_directory(std::filesystem::directory_iterator{entry});
                }
            }
        }

        void run() {
            auto text_command_host = registrar::get<structural::text_command::host>({});
            for (auto &[name, selected] : files_) {
                if (selected) {
                    try {
                        auto file {std::ifstream{name}};
                        auto const &contents {std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}}};
                        {
                            std::lock_guard lock(mutex_results_);
                            results_[name].first = contents;
                        }
                        auto command = command_format_;
                        auto const file_contents_pos {command.find("${file_contents}")};
                        if (file_contents_pos != std::string::npos) {
                            command.replace(file_contents_pos, 16, contents);
                        }
                        auto output_file_name = output_file_name_format_;
                        auto const file_name_pos {output_file_name.find("${file_name}")};
                        if (file_name_pos != std::string::npos) {
                            output_file_name.replace(file_name_pos, 12, name);
                        }
                        auto result = (*text_command_host)(command);
                        {
                            std::ofstream output_file{output_file_name, append_mode_ ? std::ios_base::app : std::ios_base::trunc};
                            if (append_mode_) {
                                output_file << "\n";
                            }
                            output_file << result;
                        }
                        {
                            std::lock_guard lock(mutex_results_);
                            results_[name].second = result;
                        }
                    }
                    catch(std::exception const &e) {
                        std::lock_guard lock(mutex_results_);
                        results_[name].second = e.what();
                    }
                }
            }
        }

        void load_contents() {
            for (auto &[name, selected] : files_) {
                if (selected) {
                    try {
                        auto file {std::ifstream{name}};
                        auto const &contents {std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}}};
                        std::lock_guard lock(mutex_results_);
                        results_[name].first = contents;
                        auto output_file_name = output_file_name_format_;
                        auto const file_name_pos {output_file_name.find("${file_name}")};
                        if (file_name_pos != std::string::npos) {
                            output_file_name.replace(file_name_pos, 12, name);
                        }
                        if (std::filesystem::exists(output_file_name)) {
                            auto result_file {std::ifstream{output_file_name}};
                            results_[name].second = std::string{std::istreambuf_iterator<char>{result_file}, std::istreambuf_iterator<char>{}};
                        }
                    }
                    catch(std::exception const &e) {
                        std::lock_guard lock(mutex_results_);
                        results_[name].second = e.what();
                    }
                }
            }
        }

        void run_async() {
            thread_ = std::jthread{[this] { run(); }};
        }

        void set_filter(std::string const &filter) { 
            error_.clear();
            filter_ = filter;
            files_.clear();
            try {
                add_directory(std::filesystem::directory_iterator{base_path_});
            }
            catch (std::exception const &e) {
                error_ = e.what();
            }
        }
    private:
        std::string base_path_;
        std::string filter_ {"base.txt"};
        std::string command_format_ {
            "ai \n"
"--system \"You are an ArangoDB expert. The user will provide a query and its profile.\n"
"Come up with an optimized version. Only reply with the new query, which needs to be completely equivalent in results, but faster to execute.\n\""
"${file_contents}"};
        std::string output_file_name_format_ {"C:\\src\\DEVOPS-MONITORINGLOGGING\\_arangobench\\2161\\new-query-1.txt"};
        std::map<std::string, bool> files_;
        std::map<std::string, std::pair<std::string,std::string>> results_;
        std::string error_;
        std::mutex mutex_results_;
        std::jthread thread_;
        bool append_mode_{false};
    };
} // namespace file_selection