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

namespace Limit {
constexpr uint32_t MAX_MESSAGE_SIZE{1024 * 1024 * 10}; // 10 Mb
}

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
    while (true) {
        if (size_t type_len{sizeof(uint8_t)}; _message.type_vec.size() != type_len) {
            if (!FromReqToVec(_message.type_vec, type_len)) {
                return;
            }
        }

        if (size_t size_len{sizeof(uint32_t)}; _message.size_vec.size() != size_len) {
            if (!FromReqToVec(_message.size_vec, size_len)) {
                return;
            }
        }

        if (size_t msg_len{PeekUint32(_message.size_vec)}; _message.bytes_vec.size() != msg_len) {
            if (msg_len > Limit::MAX_MESSAGE_SIZE) {
                _logger.PrintInTerminal(
                    MessageType::K_WARNING,
                    "[client: " + _client_host + ":" + _client_port + "] message too large: " + std::to_string(msg_len)
                );

                _message.Clear();

                return;
            }

            if (!FromReqToVec(_message.bytes_vec, msg_len)) {
                return;
            }

            if (_message.bytes_vec.size() == msg_len) {            
                _messages.push(_message);

                _message.Clear();
            } else {
                return;
            }
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
            _logger.PrintInTerminal(MessageType::K_WARNING, "recv() error: connection closed by peer");

            return false;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else if (errno == EINTR) {
                continue;
            }

            _logger.PrintInTerminal(MessageType::K_WARNING, "recv() error: " + std::string(strerror(errno)));

            return false;
        }
    }

    return true;
}

bool Session::TrySend(int fd) {
    while (!_response.empty()) {
        ssize_t n{send(fd, _response.data(), _response.size(), MSG_NOSIGNAL)};
        
        if (n > 0) {
            _response.erase(_response.begin(), _response.begin() + n);
        } else if (n == 0) {
            _logger.PrintInTerminal(MessageType::K_WARNING, "send() error: connection closed by peer");
            
            return false;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return true;
            } else if (errno == EINTR) {
                continue;
            } else if (errno == EPIPE) {
                _logger.PrintInTerminal(MessageType::K_WARNING, "send() error: broken pipe (connection closed by client)");

                return false;
            }

            _logger.PrintInTerminal(MessageType::K_WARNING, "send() error: " + std::string(strerror(errno)));

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

    std::error_code ec;
    fs::create_directories(base, ec);

    if (ec) {
        _logger.PrintInTerminal(MessageType::K_ERROR, "create_directories() error: " + ec.message());

        return;
    }

    std::string filename{timestamp + "_" + GetStringFromHostPort() + ".png"};
    fs::path out_path{base / filename};
    std::ofstream file(out_path, std::ios::binary);

    if (!file) {
        _logger.PrintInTerminal(MessageType::K_ERROR, "open file failed: " + out_path.string());

        return;
    }

    file.write(reinterpret_cast<const char*>(msg.bytes_vec.data()), msg.bytes_vec.size());
    file.close();

    std::string log_msg{"[client: " + _client_host + ":" + _client_port + "] Saved image: \"" + out_path.string() + "\""};
    _logger.PrintInTerminal(MessageType::K_INFO, log_msg);
}

void Session::HandleImgMessage() {
    SaveScreen(_messages.front());

    _messages.pop();
}

bool Session::IsValidName(const std::string& name) {
    if (name.empty() || name.size() > 255) {
        return false;
    }

    for (char c : name) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_')) {
            return false;
        }
    }

    return true;
}

void Session::ParseAuthMessage(Message& msg) {
    uint16_t hostname_len{PopUint16(msg.bytes_vec)};

    if (hostname_len > Limit::MAX_MESSAGE_SIZE) {
        throw std::runtime_error("hostname too long");
    }

    _client_hostname = PopString(msg.bytes_vec, hostname_len);

    if (!IsValidName(_client_hostname)) {
        throw std::runtime_error("Invalid hostname");
    }
    
    uint16_t username_len{PopUint16(msg.bytes_vec)};

    if (username_len > Limit::MAX_MESSAGE_SIZE) {
        throw std::runtime_error("username too long");
    }

    _client_username = PopString(msg.bytes_vec, username_len);

    if (!IsValidName(_client_username)) {
        throw std::runtime_error("Invalid username");
    }
}

bool Session::HandleAuthRequest() {
    try {
        ParseAuthMessage(_messages.front());

        _messages.pop();

        return true;
    } catch (const std::runtime_error& ex) {
        _logger.PrintInTerminal(MessageType::K_WARNING, "[client: " + _client_host + ":" + _client_port + "] Authentication failed: " + std::string(ex.what()));

        _messages.pop();

        return false;
    }
}
