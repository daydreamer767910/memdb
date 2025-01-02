#include "logger.hpp"

Logger& Logger::get_instance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void Logger::set_log_file(const std::string& file_path) {
    if (log_file_.is_open()) {
        log_file_.close();
    }
    log_file_.open(file_path, std::ios::out | std::ios::app);
    if (!log_file_) {
        throw std::ios_base::failure("Failed to open log file: " + file_path);
    }
}


void Logger::enable_console_output(bool enable) {
    console_output_ = enable;
}

std::string Logger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    std::time_t seconds = millis / 1000;
    std::tm tm = *std::localtime(&seconds);
    std::stringstream ss;

    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setw(3) << std::setfill('0') << (millis % 1000);  // 毫秒部分

    return ss.str();
}

std::string Logger::format_message(const std::string& format_str, const std::initializer_list<std::string>& args) {
    std::ostringstream oss;
    size_t index = 0;
    auto arg_it = args.begin();

    for (size_t i = 0; i < format_str.size(); ++i) {
        if (format_str[i] == '{' && i + 1 < format_str.size() && format_str[i + 1] == '}' && arg_it != args.end()) {
            oss << *arg_it++;
            ++i; // Skip the next '}'
        } else {
            oss << format_str[i];
        }
    }

    return oss.str();
}


void Logger::output_log(const std::string& message, LogLevel level) {
    std::string prefix;
    switch (level) {
        case LogLevel::INFO:    prefix = "[INFO] "; break;
        case LogLevel::WARNING: prefix = "[WARNING] "; break;
        case LogLevel::ERROR:   prefix = "[ERROR] "; break;
    }
    std::ostringstream oss;
    oss << "PID[" << std::this_thread::get_id() << "]";
    std::string threadIdStr = oss.str();
    const auto& msg = get_timestamp() + threadIdStr + prefix + message;
    //std::cout << "sendmsg:" << msg << std::endl;
    sendmsg(msg);
}

void Logger::on_msg(const std::variant<std::string>& msg) {
    std::visit([this](auto&& message) {
        using T = std::decay_t<decltype(message)>;
        if constexpr (std::is_same_v<T, std::string>) {
            if (console_output_) {
                std::cout << message << std::endl;
            }

            if (log_file_.is_open()) {
                log_file_ << message << std::endl;
            }
        }
    }, msg);
}



