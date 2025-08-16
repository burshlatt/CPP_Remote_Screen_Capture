#include <condition_variable>
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

std::mutex mutex;
std::condition_variable cond;
std::atomic<bool> stop_flag{false};

void signal_handler(int sig) {
    if (sig == SIGINT) {
        stop_flag = true;
        cond.notify_all();
    }
}

Client::Client(const std::string& s_host_port, unsigned timeout_sec) :
    _server_host(ParseHost(s_host_port)),
    _server_port(ParsePort(s_host_port)),
    _timeout_sec(timeout_sec)
{}

uint16_t Client::ParsePort(const std::string& s_host_port) {
    auto port_beg_it{s_host_port.begin() + s_host_port.find(":") + 1};
    std::string port_str(port_beg_it, s_host_port.end());

    int serv_port{std::stoi(port_str)};

    if (serv_port > 0 && serv_port <= 65535) {
        return serv_port;
    }

    throw std::invalid_argument("Invalid port: " + port_str);
}

std::string Client::ParseHost(const std::string& s_host_port) {
    auto host_end_it{s_host_port.begin() + s_host_port.find(":")};
    std::string host_str(s_host_port.begin(), host_end_it);

    struct in_addr addr;

    if (inet_pton(AF_INET, host_str.c_str(), &addr) == 1) {
        return host_str;
    }

    throw std::invalid_argument("Invalid host: " + host_str);
}

std::string Client::GetHostname() const {
    static std::string cached;

    if (!cached.empty()) {
        return cached;
    }

    char buf[256];

    if (gethostname(buf, sizeof(buf)) == 0) {
        return cached = std::string(buf);
    }

    std::cerr << "Failed to get hostname\n";

    return "unknown-host";
}

std::string Client::GetUsername() const {
    static std::string cached;

    if (!cached.empty()) {
        return cached;
    }

    struct passwd *pw{getpwuid(getuid())};

    if (pw) {
        return cached = std::string(pw->pw_name);
    }

    std::cerr << "Failed to get username\n";

    return "unknown-user";
}

UniqueFD Client::SetupSocket() {
    UniqueFD fd(ResourceFactory::MakeUniqueFD(socket(AF_INET, SOCK_STREAM, 0)));

    if (!fd.Valid()) {
        throw std::runtime_error("SetupSocket(): " + std::string(strerror(errno)));
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_server_port);

    if (inet_pton(AF_INET, _server_host.c_str(), &addr.sin_addr) != 1) {
        throw std::invalid_argument("Invalid server address");
    }

    if (connect(fd.Get(), (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        throw std::runtime_error("SetupSocket(): " + std::string(strerror(errno)));
    }

    std::cout << "[INFO] Connected! (server: " << _server_host << ':' << _server_port << ")\n";

    return fd;
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

    std::string hostname{GetHostname()};
    std::string username{GetUsername()};

    InsertToVector<uint16_t>(buffer, hostname.size());
    InsertToVector<uint16_t>(buffer, username.size());

    int width{};
    int height{};
    std::vector<uint8_t> img_bytes;

    if (!_screen_grabber.GrabAsPNG(img_bytes, width, height)) {
        throw std::runtime_error("Failed to grab screen");
    }

    InsertToVector<uint32_t>(buffer, img_bytes.size());

    buffer.insert(buffer.end(), hostname.begin(), hostname.end());
    buffer.insert(buffer.end(), username.begin(), username.end());
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

std::string Client::GetCurrentTimestamp() {
    auto now{std::chrono::system_clock::now()};
    std::time_t now_time{std::chrono::system_clock::to_time_t(now)};
    std::tm* local_time{std::localtime(&now_time)};

    std::ostringstream oss;
    oss << std::put_time(local_time, "%Y-%m-%d %H:%M:%S");

    return oss.str();
}

void Client::Run() {
    std::signal(SIGINT, signal_handler);

    _server_fd = SetupSocket();

    while (!stop_flag) {
        std::vector<uint8_t> bytes(CreateMessage());

        try {
            size_t sent{SendAll(bytes)};

            std::cout << "[INFO] [" << GetCurrentTimestamp() << "] Sent " << sent << " bytes to " << _server_host  << ":" << _server_port << '\n';
        } catch (const std::runtime_error& ex) {
            std::cerr << ex.what() << '\n';

            return;
        }

        std::unique_lock<std::mutex> lock(mutex);

        cond.wait_for(lock, std::chrono::seconds(_timeout_sec), [] {
            return stop_flag.load();
        });
    }
}
