#ifndef SERVER_SERVER_SESSION_SESSION_H
#define SERVER_SERVER_SESSION_SESSION_H

#include <string>
#include <vector>

#include "resource_factory.h"

struct Message {
    uint32_t total_size;
    uint16_t len_hostname;
    uint16_t len_username;
    uint32_t img_size;
    std::string hostname;
    std::string username;
    std::vector<uint8_t> img_bytes;
};

class Session {
public:
    Session(UniqueFD&& client_fd, const std::string& host, uint16_t port);

public:
    int GetClientFD() const noexcept;
    std::string GetClientHost() const noexcept;
    std::string GetClientPort() const noexcept;

    bool IsMessageComplete() const;
    bool RecvAll(int fd);
    void SaveScreen();

private:
    bool BufferHasSize(size_t size) const;

    uint16_t PopUint16();
    uint32_t PopUint32();
    uint16_t PeekUint16() const;
    uint32_t PeekUint32() const;
    std::string PopString(uint16_t str_len);
    std::vector<uint8_t> PopImg(uint32_t img_len);

    Message ParseMessage();
    std::string GetCurrentTimestamp();
    std::string GetStringFromHostPort();

private:
    UniqueFD _client_fd;
    std::string _client_host;
    std::string _client_port;

    std::vector<uint8_t> _buffer;
};

#endif // SERVER_SERVER_SESSION_SESSION_H
