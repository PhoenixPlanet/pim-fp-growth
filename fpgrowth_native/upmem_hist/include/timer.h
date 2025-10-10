#ifndef TIMER_H
#define TIMER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <iostream>
#include <iomanip>

class Timer {
public:
    using Clock = std::chrono::steady_clock;
    using ns = std::chrono::nanoseconds;

    struct Record {
        std::string name;
        int64_t duration_ns;
    };

    static Timer& instance() {
        static Timer timer_instance;
        return timer_instance;
    }

    void start(const std::string& name) {
        if (running) {
            throw std::logic_error("Timer is already running");
        }
        running = true;
        current_name = name;
        start_time = Clock::now();
    }

    int64_t stop() {
        if (!running) {
            throw std::logic_error("Timer is not running");
        }
        auto end_time = Clock::now();
        auto duration = std::chrono::duration_cast<ns>(end_time - *start_time).count();
        if (records.find(*current_name) != records.end()) {
            records[*current_name].duration_ns += duration;
        } else {
            records[*current_name] = { *current_name, static_cast<int64_t>(duration) };
        }
        running = false;
        current_name.reset();
        start_time.reset();
        return static_cast<int64_t>(duration);
    }

    const std::unordered_map<std::string, Record>& get_records() const {
        return records;
    }

    void print_records() const {
        for (const auto& record : records) {
            std::cout << std::setw(30) << std::left << record.first
                      << ": " << std::setw(10) << std::right << record.second.duration_ns << " ns" << std::endl;
        }
    }

private:
    Timer() = default;
    ~Timer() = default;
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    bool running{false};
    std::optional<std::string> current_name;
    std::optional<Clock::time_point> start_time;
    std::unordered_map<std::string, Record> records;
};

#endif // TIMER_H