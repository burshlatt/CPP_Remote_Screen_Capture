#ifndef CLIENT_CLIENT_CLIENT_H
#define CLIENT_CLIENT_CLIENT_H

#include <string>
#include <cstdint>

#include "resource_factory.h"
#include "screen_grabber.h"
#include "logger.h"

class Client {
public:
    Client(const std::string& s_host_port, unsigned timeout_sec = 10);

public:
    void Run();

private:
    void SetupHostname();
    void SetupUsername();

    uint16_t ParsePort(const std::string& s_host_port);
    std::string ParseHost(const std::string& s_host_port);

private:
    void WaitLoop();
    void SendLoop();
    void SetupSocket();

    template<typename T>
    void InsertToVector(std::vector<uint8_t>& buffer, T num);
    
    std::vector<uint8_t> CreateMessage();

    size_t SendAll(const std::vector<uint8_t>& data);
    
private:
    std::string _s_host_port;
    std::string _server_host;
    uint16_t _server_port;
    unsigned _timeout_sec;

    std::string _hostname;
    std::string _username;

    Logger _logger;

    UniqueFD _server_fd;

    ScreenGrabber _screen_grabber;
};

#endif // CLIENT_CLIENT_CLIENT_H
