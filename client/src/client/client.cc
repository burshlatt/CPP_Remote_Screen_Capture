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

Client::Client(const std::string& s_host, uint16_t s_port, unsigned timeout_sec) :
    _server_host(s_host),
    _server_port(s_port),
    _timeout_sec(timeout_sec)
{}

void Client::SetupHostname() {
    char buffer[256];

    if (gethostname(buffer, sizeof(buffer)) == 0) {
        _hostname = std::string(buffer);
    } else {
        _hostname = "unknown-host";
    }
}

void Client::SetupUsername() {
    struct passwd* pw{getpwuid(getuid())};

    if (pw) {
        _username = std::string(pw->pw_name);
    } else {
        _username = "unknown-user";
    }
}

void Client::SetupSocket() {
    _server_fd = ResourceFactory::MakeUniqueFD(socket(AF_INET, SOCK_STREAM, 0));

    if (!_server_fd.Valid()) {
        throw std::runtime_error("socket() error: " + std::string(strerror(errno)));
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_server_port);

    if (inet_pton(AF_INET, _server_host.c_str(), &addr.sin_addr) != 1) {
        throw std::runtime_error("inet_pton() error.");
    }

    if (connect(_server_fd.Get(), (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        throw std::runtime_error("connect(): " + std::string(strerror(errno)));
    }

    _logger.PrintInTerminal(MessageType::K_INFO, "Connected! (server: " + _server_host + ":" + std::to_string(_server_port) + ")");
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

std::vector<uint8_t> Client::CreateAuthenticationRequest() {
    std::vector<uint8_t> buffer;

    InsertToVector<uint8_t>(buffer, 'A');
    InsertToVector<uint32_t>(buffer, 0);
    InsertToVector<uint16_t>(buffer, _hostname.size());
    buffer.insert(buffer.end(), _hostname.begin(), _hostname.end());
    InsertToVector<uint16_t>(buffer, _username.size());
    buffer.insert(buffer.end(), _username.begin(), _username.end());

    constexpr uint32_t TYPE_SIZE{1};
    constexpr uint32_t LEN_SIZE{4};

    uint32_t total_size{static_cast<uint32_t>(buffer.size()) - TYPE_SIZE - LEN_SIZE};
    uint32_t net_total_size{htonl(total_size)};

    std::memcpy(buffer.data() + TYPE_SIZE, &net_total_size, sizeof(net_total_size));

    return buffer;
}

std::vector<uint8_t> Client::CreateImgMessage() {
    int width{};
    int height{};
    std::vector<uint8_t> img_bytes;

    _screen_grabber.GrabAsPNG(img_bytes, width, height);

    std::vector<uint8_t> buffer;
    buffer.reserve(sizeof(uint8_t) + sizeof(uint32_t) + img_bytes.size());

    InsertToVector<uint8_t>(buffer, 'I');
    InsertToVector<uint32_t>(buffer, img_bytes.size());
    buffer.insert(buffer.end(), img_bytes.begin(), img_bytes.end());

    return buffer;
}

ssize_t Client::SendAll(const std::vector<uint8_t>& data) {
    size_t total_sent{0};
    size_t data_size{data.size()};

    while (total_sent < data_size) {
        ssize_t sent{send(_server_fd.Get(), data.data() + total_sent, data_size - total_sent, MSG_NOSIGNAL)};
        
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EPIPE) {
                throw std::runtime_error("send() error: broken pipe (connection closed by server)");
            }

            throw std::runtime_error("send() error: " + std::string(strerror(errno)));
        } else if (sent == 0) {
            throw std::runtime_error("send() error: connection closed by peer");
        }

        total_sent += static_cast<size_t>(sent);
    }

    return static_cast<ssize_t>(total_sent);
}

ssize_t Client::RecvAll(uint8_t* buffer, size_t total_bytes) {
    size_t bytes_read{0};

    while (bytes_read < total_bytes) {
        ssize_t ret{recv(_server_fd.Get(), buffer + bytes_read, total_bytes - bytes_read, 0)};

        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }

            throw std::runtime_error("recv() error: " + std::string(strerror(errno)));
        } else if (ret == 0) {
            throw std::runtime_error("recv() error: connection closed by peer");
        }

        bytes_read += static_cast<size_t>(ret);
    }

    return static_cast<ssize_t>(bytes_read);
}

bool Client::TryAuthenticate() {
    std::vector<uint8_t> auth_req(CreateAuthenticationRequest());

    uint8_t auth_resp{0};

    try {
        SendAll(auth_req);
        RecvAll(&auth_resp, sizeof(auth_resp));

        if (auth_resp == 'Y') {
            _logger.PrintInTerminal(MessageType::K_INFO, "Authentication was successful!");

            return true;
        }
    } catch (const std::runtime_error& ex) {
        _logger.PrintInTerminal(MessageType::K_ERROR, std::string(ex.what()));
    }

    return false;
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
            std::vector<uint8_t> bytes(CreateImgMessage());
            
            ssize_t sent{SendAll(bytes)};
                
            _logger.PrintInTerminal(MessageType::K_INFO, "Image sent to server.");
        } catch (const grabber_error& ex) {
            _logger.PrintInTerminal(MessageType::K_WARNING, ex.what());
        }

        WaitLoop();
    }
}

void Client::Run() {
    std::signal(SIGINT, signal_handler);

    SetupHostname();
    SetupUsername();

    try {
        SetupSocket();

        if (TryAuthenticate()) {
            SendLoop();
        }
    } catch (const std::runtime_error& ex) {
        _logger.PrintInTerminal(MessageType::K_ERROR, ex.what());
    }
}
