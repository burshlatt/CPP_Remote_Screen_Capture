#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

#include "logger.h"

std::ostream& Logger::TypeToStream(MessageType type) {
    switch (type) {
        case MessageType::K_INFO:    return std::cout;
        case MessageType::K_ERROR:   return std::cerr;
        case MessageType::K_WARNING: return std::clog;
        default:                     return std::cout;
    }
}

std::string Logger::TypeToString(MessageType type) {
    switch (type) {
        case MessageType::K_INFO:    return "INFO";
        case MessageType::K_ERROR:   return "ERROR";
        case MessageType::K_WARNING: return "WARNING";
        default:                     return "UNKNOWN";
    }
}

std::string Logger::GetCurrentTimestamp(const char* mask) {
    auto now{std::chrono::system_clock::now()};
    std::time_t now_time{std::chrono::system_clock::to_time_t(now)};

    std::tm local_time{};
    localtime_r(&now_time, &local_time);

    std::ostringstream oss;
    oss << std::put_time(&local_time, mask);

    return oss.str();
}

void Logger::PrintInTerminal(MessageType type, const std::string& msg) {
    std::string type_str{TypeToString(type)};
    std::ostream& stream{TypeToStream(type)};

    std::string result_msg{"[" + type_str + "] [" + GetCurrentTimestamp() + "] " + msg};

    stream << result_msg << std::endl;
}
