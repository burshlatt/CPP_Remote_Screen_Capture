#ifndef COMMON_INCLUDE_LOGGER_H
#define COMMON_INCLUDE_LOGGER_H

#include <string>

enum MessageType {
    K_INFO,
    K_ERROR,
    K_WARNING
};

class Logger {
public:
    void PrintInTerminal(MessageType type, const std::string& msg);
    std::string GetCurrentTimestamp(const char* mask = "%Y-%m-%d %H:%M:%S");

private:
    std::string TypeToString(MessageType type);
    std::ostream& TypeToStream(MessageType type);
};

#endif // COMMON_INCLUDE_LOGGER_H
