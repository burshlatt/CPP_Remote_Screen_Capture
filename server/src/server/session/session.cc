#include <array>
#include <ctime>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <filesystem>

#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include "session.h"

namespace fs = std::filesystem;

Session::Session(UniqueFD&& client_fd, const std::string& host, uint16_t port) :
    _client_fd(std::move(client_fd)),
    _client_host(host),
    _client_port(std::to_string(port))
{}

int Session::GetClientFD() const noexcept {
    return _client_fd.Get();
}

std::string Session::GetClientHost() const noexcept {
    return _client_host;
}

std::string Session::GetClientPort() const noexcept {
    return _client_port;
}

bool Session::RecvAll(int fd) {
    constexpr size_t BUFFER_SIZE{4096};

    while (true) {
        std::array<char, BUFFER_SIZE> temp;
        ssize_t n{recv(fd, temp.data(), sizeof(temp), 0)};

        if (n > 0) {
            _buffer.insert(_buffer.end(), temp.data(), temp.data() + n);
        } else if (n == 0) {
            return false;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else if (errno == EINTR) {
                continue;
            }

            std::cerr << "recv() error from client: " << strerror(errno) << '\n';

            return false;
        }
    }

    return true;
}

bool Session::BufferHasSize(size_t size) const {
    return _buffer.size() >= size;
}

uint16_t Session::PeekUint16() const {
    if (!BufferHasSize(sizeof(uint16_t))) {
        throw std::runtime_error("Buffer too small to read uint16_t");
    }

    uint16_t value;
    std::memcpy(&value, _buffer.data(), sizeof(uint16_t));

    return ntohs(value);
}

uint32_t Session::PeekUint32() const {
    if (!BufferHasSize(sizeof(uint32_t))) {
        throw std::runtime_error("Buffer too small to read uint32_t");
    }

    uint32_t value;
    std::memcpy(&value, _buffer.data(), sizeof(uint32_t));

    return ntohl(value);
}

uint16_t Session::PopUint16() {
    uint16_t value{PeekUint16()};

    _buffer.erase(_buffer.begin(), _buffer.begin() + sizeof(uint16_t));

    return value;
}

uint32_t Session::PopUint32() {
    uint32_t value{PeekUint32()};

    _buffer.erase(_buffer.begin(), _buffer.begin() + sizeof(uint32_t));

    return value;
}

std::string Session::PopString(uint16_t str_len) {
    if (!BufferHasSize(str_len)) {
        throw std::runtime_error("Buffer too small to read string of length " + std::to_string(str_len));
    }

    std::string result_str(_buffer.begin(), _buffer.begin() + str_len);

    _buffer.erase(_buffer.begin(), _buffer.begin() + str_len);

    return result_str;
}

std::vector<uint8_t> Session::PopImg(uint32_t img_len) {
    if (!BufferHasSize(img_len)) {
        throw std::runtime_error("Buffer too small to read image of size " + std::to_string(img_len));
    }

    std::vector<uint8_t> bytes(_buffer.begin(), _buffer.begin() + img_len);

    _buffer.erase(_buffer.begin(), _buffer.begin() + img_len);

    return bytes;
}

bool Session::IsMessageComplete() const {
    if (!BufferHasSize(sizeof(uint32_t))) {
        return false;
    }
    
    return _buffer.size() >= PeekUint32();
}

Message Session::ParseMessage() {
    Message msg;
    msg.total_size = PopUint32();
    msg.len_hostname = PopUint16();
    msg.len_username = PopUint16();
    msg.img_size = PopUint32();
    msg.hostname = PopString(msg.len_hostname);
    msg.username = PopString(msg.len_username);
    msg.img_bytes = PopImg(msg.img_size);

    return msg;
}

std::string Session::GetCurrentTimestamp() {
    auto now{std::chrono::system_clock::now()};
    std::time_t now_time{std::chrono::system_clock::to_time_t(now)};
    std::tm* local_time{std::localtime(&now_time)};

    std::ostringstream oss;
    oss << std::put_time(local_time, "%Y-%m-%d %H:%M:%S");

    return oss.str();
}

std::string Session::GetStringFromHostPort() {
    std::string host{_client_host};

    host.erase(std::remove(host.begin(), host.end(), '.'), host.end());

    return host + "_" + _client_port;
}

void Session::SaveScreen() {
    Message msg;

    try {
        msg = ParseMessage();
    } catch (const std::runtime_error& ex) {
        std::cerr << ex.what() << '\n';

        return;
    }

    auto now{std::chrono::system_clock::now()};
    std::time_t time{std::chrono::system_clock::to_time_t(now)};
    std::tm tm{*std::localtime(&time)};

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");

    fs::path base{fs::path("screenshots") / fs::path(msg.hostname) / fs::path(msg.username)};

    fs::create_directories(base);

    std::string filename{oss.str() + "_" + GetStringFromHostPort() + ".png"};
    fs::path out_path{base / filename};
    std::ofstream file(out_path, std::ios::binary);

    file.write(reinterpret_cast<char*>(msg.img_bytes.data()), msg.img_bytes.size());
    file.close();

    std::cout<<"[INFO] [" << GetCurrentTimestamp() << "] [client: " << _client_host << ':' << _client_port << "] Saved image: \"" << out_path.string() << "\"\n";
}
