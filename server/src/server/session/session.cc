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

Session::Session(UniqueFD&& client_fd, const std::string& host, const std::string& port) :
    _client_fd(std::move(client_fd)),
    _client_host(host),
    _client_port(port)
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

uint8_t Session::GetMessageType() const {
    return PeekUint8(_messages.front().type_vec);
}

uint8_t Session::PeekUint8(const std::vector<uint8_t>& buffer) const {
    if (buffer.size() < sizeof(uint8_t)) {
        throw std::runtime_error("Buffer too small to read uint8_t");
    }

    return buffer.front();
}

uint16_t Session::PeekUint16(const std::vector<uint8_t>& buffer) const {
    if (buffer.size() < sizeof(uint16_t)) {
        throw std::runtime_error("Buffer too small to read uint16_t");
    }

    uint16_t value;
    std::memcpy(&value, buffer.data(), sizeof(uint16_t));

    return ntohs(value);
}

uint32_t Session::PeekUint32(const std::vector<uint8_t>& buffer) const {
    if (buffer.size() < sizeof(uint32_t)) {
        throw std::runtime_error("Buffer too small to read uint32_t");
    }

    uint32_t value;
    std::memcpy(&value, buffer.data(), sizeof(uint32_t));

    return ntohl(value);
}

uint8_t Session::PopUint8(std::vector<uint8_t>& buffer) {
    uint8_t value{PeekUint8(buffer)};

    buffer.erase(buffer.begin(), buffer.begin() + sizeof(uint8_t));

    return value;
}

uint16_t Session::PopUint16(std::vector<uint8_t>& buffer) {
    uint16_t value{PeekUint16(buffer)};

    buffer.erase(buffer.begin(), buffer.begin() + sizeof(uint16_t));

    return value;
}

uint32_t Session::PopUint32(std::vector<uint8_t>& buffer) {
    uint32_t value{PeekUint32(buffer)};

    buffer.erase(buffer.begin(), buffer.begin() + sizeof(uint32_t));

    return value;
}

std::string Session::PopString(std::vector<uint8_t>& buffer, uint16_t str_len) {
    if (buffer.size() < str_len) {
        throw std::runtime_error("Buffer too small to read string of length " + std::to_string(str_len));
    }

    std::string result_str(buffer.begin(), buffer.begin() + str_len);

    buffer.erase(buffer.begin(), buffer.begin() + str_len);

    return result_str;
}

bool Session::FromReqToVec(std::vector<uint8_t>& vec, size_t len) {
    if (_request.size() < len) {
        return false;
    }

    vec.insert(vec.end(), _request.begin(), _request.begin() + len);
    _request.erase(_request.begin(), _request.begin() + len);

    return true;
}

void Session::ParseMessage() {
    if (size_t type_len{sizeof(uint8_t)}; _message.type_vec.size() != type_len) {
        bool status{FromReqToVec(_message.type_vec, type_len)};

        if (!status) {
            return;
        }
    }

    if (size_t size_len{sizeof(uint32_t)}; _message.size_vec.size() != size_len) {
        bool status{FromReqToVec(_message.size_vec, size_len)};

        if (!status) {
            return;
        }
    }

    if (size_t msg_len{PeekUint32(_message.size_vec)}; _message.bytes_vec.size() != msg_len) {
        bool status{FromReqToVec(_message.bytes_vec, msg_len)};

        if (status && _message.bytes_vec.size() == msg_len) {            
            _messages.push(_message);

            _message.Clear();
        }
    }
}

bool Session::TryRecv(int fd) {
    constexpr size_t BUFFER_SIZE{4096};

    while (true) {
        char temp[BUFFER_SIZE];

        ssize_t n{recv(fd, temp, sizeof(temp), 0)};

        if (n > 0) {
            _request.insert(_request.end(), temp, temp + n);
        } else if (n == 0) {
            return false;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else if (errno == EINTR) {
                continue;
            }

            _logger.PrintInTerminal(MessageType::K_ERROR, "recv() error: " + std::string(strerror(errno)));

            return false;
        }
    }

    return true;
}

bool Session::TrySend(int fd) {
    while (!_response.empty()) {
        ssize_t n{send(fd, _response.data(), _response.size(), 0)};
        
        if (n > 0) {
            _response.erase(_response.begin(), _response.begin() + n);
        } else if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return true;
            } else if (errno == EINTR) {
                continue;
            }

            _logger.PrintInTerminal(MessageType::K_ERROR, "send() error: " + std::string(strerror(errno)));

            return false;
        }
    }

    return true;
}

bool Session::SendAuthResponse(int fd, bool ok) {
    char resp{ok ? 'Y' : 'N'};

    if (!_response.empty()) { 
        _response[0] = static_cast<uint8_t>(resp);
    } else {
        _response.push_back(static_cast<uint8_t>(resp));
    }

    return TrySend(fd);
}

bool Session::SendBufferEmpty() const {
    return _response.empty();
}

bool Session::IsMessageComplete() const {
    return !_messages.empty();
}

std::string Session::GetStringFromHostPort() {
    std::string host{_client_host};

    host.erase(std::remove(host.begin(), host.end(), '.'), host.end());

    return host + "_" + _client_port;
}

void Session::SaveScreen(const Message& msg) {
    std::string timestamp{_logger.GetCurrentTimestamp("%Y%m%d_%H%M%S")};

    fs::path base{fs::path("screenshots") / fs::path(_client_hostname) / fs::path(_client_username)};

    fs::create_directories(base);

    std::string filename{timestamp + "_" + GetStringFromHostPort() + ".png"};
    fs::path out_path{base / filename};
    std::ofstream file(out_path, std::ios::binary);

    file.write(reinterpret_cast<const char*>(msg.bytes_vec.data()), msg.bytes_vec.size());
    file.close();

    std::string log_msg{"[client: " + _client_host + ":" + _client_port + "] Saved image: \"" + out_path.string() + "\""};
    _logger.PrintInTerminal(MessageType::K_INFO, log_msg);
}

void Session::HandleImgMessage() {
    SaveScreen(_messages.front());

    _messages.pop();
}

void Session::ParseAuthMessage(Message& msg) {
    uint16_t hostname_len{PopUint16(msg.bytes_vec)};
    _client_hostname = PopString(msg.bytes_vec, hostname_len);
    
    uint16_t username_len{PopUint16(msg.bytes_vec)};
    _client_username = PopString(msg.bytes_vec, username_len);
}

bool Session::HandleAuthRequest() {
    try {
        ParseAuthMessage(_messages.front());

        _messages.pop();

        return true;
    } catch (const std::runtime_error& ex) {
        _logger.PrintInTerminal(MessageType::K_WARNING, "[client: " + _client_host + ":" + _client_port + "] Authentication failed: " + std::string(ex.what()));

        return false;
    }
}
