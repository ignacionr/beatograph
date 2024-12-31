#pragma once

#include <chrono>

namespace clocks {
    struct pomodoro {
        pomodoro(
            std::chrono::system_clock::time_point start_time = std::chrono::system_clock::now(), 
            std::chrono::seconds duration = std::chrono::minutes{25})
            : start_time_{start_time}, duration_{duration} {}

        bool is_done(std::chrono::system_clock::time_point current_time) const
        {
            return current_time - start_time_ > duration_;
        }

        double percentaged_done(std::chrono::system_clock::time_point current_time) const
        {
            return std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time_).count() / static_cast<double>(duration_.count());
        }

        std::chrono::system_clock::time_point start_time() const
        {
            return start_time_;
        }

        std::chrono::system_clock::time_point end_time() const
        {
            return start_time_ + duration_;
        }

        std::chrono::seconds duration() const
        {
            return duration_;
        }

        void reset() {
            start_time_ = std::chrono::system_clock::now();
        }
    private:
        std::chrono::system_clock::time_point start_time_;
        std::chrono::seconds duration_;
    };
}