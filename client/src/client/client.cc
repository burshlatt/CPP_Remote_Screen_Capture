#include <iostream>
#include <cstring>
#include <iomanip>
#include <csignal>
#include <atomic>
#include <vector>
#include <chrono>
#include <thread>

#include <pwd.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "client.h"

std::atomic<bool> stop_flag{false};

void signal_handler(int sig) {
    if (sig == SIGINT) {
        stop_flag.store(true, std::memory_order_relaxed);
    }
}

Client::Client(const std::string& s_host_port, unsigned timeout_sec) :
    _s_host_port(s_host_port),
    _server_host(ParseHost(_s_host_port)),
    _server_port(ParsePort(_s_host_port)),
    _timeout_sec(timeout_sec)
{}

uint16_t Client::ParsePort(const std::string& s_host_port) {
    auto pos{s_host_port.find(":")};
    
    if (pos != std::string::npos) {
        auto port_beg_it{s_host_port.begin() + pos + 1};
        std::string port_str(port_beg_it, s_host_port.end());

        try {
            int serv_port{std::stoi(port_str)};

            if (serv_port > 0 && serv_port <= 65535) {
                return serv_port;
            }
        } catch (...) {
            _logger.PrintInTerminal(MessageType::K_ERROR, "Invalid port.");

            throw;
        }
    }

    throw std::invalid_argument("Invalid port.");
}

std::string Client::ParseHost(const std::string& s_host_port) {
    auto pos{s_host_port.find(":")};
    
    if (pos != std::string::npos) {
        auto host_end_it{s_host_port.begin() + pos};
        std::string host_str(s_host_port.begin(), host_end_it);

        struct in_addr addr;

        if (inet_pton(AF_INET, host_str.c_str(), &addr) == 1) {
            return host_str;
        }
    }

    throw std::invalid_argument("Invalid host.");
}

void Client::SetupHostname() {
    char buf[256];

    if (gethostname(buf, sizeof(buf)) == 0) {
        _hostname = std::string(buf);
    } else {
        _hostname = "unknown-host";
    }
}

void Client::SetupUsername() {
    struct passwd *pw{getpwuid(getuid())};

    if (pw) {
        _username = std::string(pw->pw_name);
    } else {
        _username = "unknown-user";
    }
}

void Client::SetupSocket() {
    _server_fd = ResourceFactory::MakeUniqueFD(socket(AF_INET, SOCK_STREAM, 0));

    if (!_server_fd.Valid()) {
        throw std::runtime_error("SetupSocket(): " + std::string(strerror(errno)));
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_server_port);

    if (inet_pton(AF_INET, _server_host.c_str(), &addr.sin_addr) != 1) {
        throw std::invalid_argument("Invalid server address");
    }

    if (connect(_server_fd.Get(), (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        throw std::runtime_error("SetupSocket(): " + std::string(strerror(errno)));
    }

    _logger.PrintInTerminal(MessageType::K_INFO, "Connected! (server: " + _s_host_port + ")");
}

template<typename T>
void Client::InsertToVector(std::vector<uint8_t>& buffer, T num) {
    static_assert(std::is_integral<T>::value, "T must be integral");

    T net_num;

    if constexpr (sizeof(T) == 1) {
        net_num = num;
    } else if constexpr (sizeof(T) == 2) {
        net_num = htons(static_cast<uint16_t>(num));
    } else if constexpr (sizeof(T) == 4) {
        net_num = htonl(static_cast<uint32_t>(num));
    } else {
        static_assert(sizeof(T) <= 4, "Unsupported type size");
    }

    uint8_t bytes[sizeof(T)];
    std::memcpy(bytes, &net_num, sizeof(T));

    buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
}

std::vector<uint8_t> Client::CreateMessage() {
    std::vector<uint8_t> buffer;
    buffer.reserve(1024 * 1024);

    InsertToVector<uint32_t>(buffer, 0);

    InsertToVector<uint16_t>(buffer, _hostname.size());
    InsertToVector<uint16_t>(buffer, _username.size());

    int width{};
    int height{};
    std::vector<uint8_t> img_bytes;

    _screen_grabber.GrabAsPNG(img_bytes, width, height);

    InsertToVector<uint32_t>(buffer, img_bytes.size());

    buffer.insert(buffer.end(), _hostname.begin(), _hostname.end());
    buffer.insert(buffer.end(), _username.begin(), _username.end());
    buffer.insert(buffer.end(), img_bytes.begin(), img_bytes.end());

    uint32_t total_size{static_cast<uint32_t>(buffer.size())};
    uint32_t net_total_size{htonl(total_size)};
    std::memcpy(buffer.data(), &net_total_size, sizeof(net_total_size));

    return buffer;
}

size_t Client::SendAll(const std::vector<uint8_t>& data) {
    size_t total_sent{0};
    size_t data_size{data.size()};

    while (total_sent < data_size) {
        ssize_t sent{send(_server_fd.Get(), data.data() + total_sent, data_size - total_sent, 0)};
        
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }

            throw std::runtime_error("SendAll(): send failed: " + std::string(strerror(errno)));
        } else if (sent == 0) {
            throw std::runtime_error("SendAll(): connection closed by peer");
        }

        total_sent += static_cast<size_t>(sent);
    }

    return total_sent;
}

void Client::WaitLoop() {
    for (unsigned i{0}; i < _timeout_sec; ++i) {
        if (stop_flag.load(std::memory_order_relaxed)) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Client::SendLoop() {
    while (!stop_flag.load(std::memory_order_relaxed)) {
        try {
            std::vector<uint8_t> bytes(CreateMessage());
            
            try {
                size_t sent{SendAll(bytes)};

                _logger.PrintInTerminal(MessageType::K_INFO, "Sent " + std::to_string(sent) + " bytes to " + _s_host_port);
            } catch (const std::runtime_error& ex) {
                _logger.PrintInTerminal(MessageType::K_ERROR, ex.what());

                return;
            }
        } catch (const std::runtime_error& ex) {
            _logger.PrintInTerminal(MessageType::K_WARNING, ex.what());
        }

        WaitLoop();
    }
}

void Client::Run() {
    std::signal(SIGINT, signal_handler);

    SetupSocket();
    SetupHostname();
    SetupUsername();
    SendLoop();
}
