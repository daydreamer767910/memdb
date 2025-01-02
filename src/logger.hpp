#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <iomanip>
#include <ctime>
#include "threadbase.hpp"

class Logger: public ThreadBase<std::string> {
public:
    enum class LogLevel {
        INFO,
        WARNING,
        ERROR
    };

    static Logger& get_instance();

    void set_log_file(const std::string& file_path);
    void enable_console_output(bool enable);

    template <typename... Args>
    void log(LogLevel level, const std::string& format_str, Args&&... args) {
        std::initializer_list<std::string> arguments = {to_string(args)...};
        std::string formatted_message = format_message(format_str, arguments);
        output_log(formatted_message, level);
    }

    void on_msg(const std::variant<std::string>& msg) override;

private:
    Logger() = default;
    ~Logger();

    std::ofstream log_file_;
    bool console_output_ = true;

    template <typename T>
    std::string to_string(T&& arg) {
        std::ostringstream oss;
        oss << std::forward<T>(arg);
        return oss.str();
    }
    std::string get_timestamp();
    std::string format_message(const std::string& format_str, const std::initializer_list<std::string>& args);
    void output_log(const std::string& message, LogLevel level);
};

#endif // LOGGER_HPP
