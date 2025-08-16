#ifndef SERVER_SERVER_SERVER_h
#define SERVER_SERVER_SERVER_h

#include <string>
#include <vector>
#include <memory>
#include <string_view>
#include <unordered_map>

#include <sys/epoll.h>

#include "session.h"
#include "resource_factory.h"

class Server {
public:
    Server(const std::string& listen_port);

    void Run();

private:
    void SetupEpoll();
    void SetupServerSocket();
    void EventLoop();
    void UpdateEpollEvents(int fd, uint32_t events);
    void AcceptNewConnections();
    void CloseSession(std::shared_ptr<Session> session);
    void HandleEvent(epoll_event& event);

    int CheckPort(const std::string& port);
    std::string CheckHost(const std::string& host);

private:
    int _listen_port;

    UniqueFD _proxy_fd{};
    UniqueFD _epoll_fd{};

    std::unordered_map<int, std::shared_ptr<Session>> _fd_session_ht;
};

#endif // SERVER_SERVER_SERVER_h
