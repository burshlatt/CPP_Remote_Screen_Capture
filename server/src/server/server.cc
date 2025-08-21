#include <atomic>
#include <csignal>
#include <cstring>
#include <iostream>

#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "server.h"

std::atomic<bool> stop_flag{false};

void signal_handler(int sig) {
    if (sig == SIGINT) {
        stop_flag.store(true, std::memory_order_relaxed);
    }
}

Server::Server(uint16_t listen_port) :
    _listen_port(listen_port)
{}

void Server::SetupServerSocket() {
    _server_fd = UniqueFD(ResourceFactory::MakeUniqueFD(socket(AF_INET, SOCK_STREAM, 0)));

    if (!_server_fd.Valid()) {
        throw std::runtime_error("socket(): " + std::string(strerror(errno)));
    }

    int flags{fcntl(_server_fd.Get(), F_GETFL, 0)};
    fcntl(_server_fd.Get(), F_SETFL, flags | O_NONBLOCK);

    int opt{1};

    if (setsockopt(_server_fd.Get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        throw std::runtime_error("setsockopt(): SO_REUSEADDR failed: " + std::string(strerror(errno)));
    }

    if (setsockopt(_server_fd.Get(), SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
        _logger.PrintInTerminal(MessageType::K_WARNING, "setsockopt(): SO_REUSEPORT not supported: " + std::string(strerror(errno)));
    }

    if (setsockopt(_server_fd.Get(), SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) == -1) {
        _logger.PrintInTerminal(MessageType::K_WARNING, "setsockopt(): SO_KEEPALIVE not supported: " + std::string(strerror(errno)));
    }

    struct sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(_listen_port);

    auto s_addr{reinterpret_cast<struct sockaddr*>(&server_addr)};

    if (bind(_server_fd.Get(), s_addr, sizeof(server_addr)) == -1) {
        throw std::runtime_error("bind(): " + std::string(strerror(errno)));
    }

    if (listen(_server_fd.Get(), SOMAXCONN) == -1) {
        throw std::runtime_error("listen(): " + std::string(strerror(errno)));
    }
}

void Server::SetupEpoll() {
    _epoll_fd = UniqueFD(ResourceFactory::MakeUniqueFD(epoll_create1(0)));
    
    if (!_epoll_fd.Valid()) {
        throw std::runtime_error("epoll_create1(): " + std::string(strerror(errno)));
    }

    epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = _server_fd.Get();

    if (epoll_ctl(_epoll_fd.Get(), EPOLL_CTL_ADD, _server_fd.Get(), &event) == -1) {
        throw std::runtime_error("epoll_ctl(): " + std::string(strerror(errno)));
    }
}

void Server::AcceptNewConnections() {
    while (true) {
        struct sockaddr_in client_addr = {};
        auto c_addr{reinterpret_cast<sockaddr*>(&client_addr)};
        socklen_t c_addr_len{sizeof(client_addr)};

        UniqueFD client_fd(ResourceFactory::MakeUniqueFD(accept(_server_fd.Get(), c_addr, &c_addr_len)));

        if (!client_fd.Valid()) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else if (errno == EINTR) {
                continue;
            } else {
                _logger.PrintInTerminal(MessageType::K_WARNING, "accept() error: " + std::string(strerror(errno)));

                break;
            }
        }

        int flags{fcntl(client_fd.Get(), F_GETFL, 0)};
        fcntl(client_fd.Get(), F_SETFL, flags | O_NONBLOCK);

        epoll_event event;
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = client_fd.Get();

        if (epoll_ctl(_epoll_fd.Get(), EPOLL_CTL_ADD, client_fd.Get(), &event) == -1) {
            _logger.PrintInTerminal(MessageType::K_WARNING, "epoll_ctl() error: " + std::string(strerror(errno)));

            continue;
        }

        char host_buf[INET_ADDRSTRLEN]{};

        if (!inet_ntop(AF_INET, &client_addr.sin_addr, host_buf, sizeof(host_buf))) {
            _logger.PrintInTerminal(MessageType::K_WARNING, "inet_ntop() failed");

            continue;
        }

        std::string host(host_buf);
        std::string port{std::to_string(ntohs(client_addr.sin_port))};

        auto session{std::make_shared<Session>(std::move(client_fd), host, port)};

        _fd_session_ht[session->GetClientFD()] = session;

        _logger.PrintInTerminal(MessageType::K_INFO, "New connection! (client: " + host + ":" + port + ")");
    }
}

void Server::CloseSession(std::shared_ptr<Session> session) {
    int client_fd{session->GetClientFD()};
    std::string host(session->GetClientHost());
    std::string port{session->GetClientPort()};

    epoll_ctl(_epoll_fd.Get(), EPOLL_CTL_DEL, client_fd, nullptr);

    _fd_session_ht.erase(client_fd);

    _logger.PrintInTerminal(MessageType::K_INFO, "Close connection. (client: " + host + ":" + port + ")");
}

void Server::UpdateEpollEvents(int fd, uint32_t events) {
    epoll_event event;
    event.data.fd = fd;
    event.events = events;

    if (epoll_ctl(_epoll_fd.Get(), EPOLL_CTL_MOD, fd, &event) == -1) {
        throw std::runtime_error("epoll_ctl(): " + std::string(strerror(errno)));
    }
}

bool Server::HandleOutEvent(epoll_event& event, std::shared_ptr<Session> session) {
    int client_fd{event.data.fd};

    if (!session->TrySend(client_fd)) {
        return false;
    }

    if (session->SendBufferEmpty()) {
        UpdateEpollEvents(client_fd, EPOLLIN | EPOLLET);
    }

    return true;
}

bool Server::HandleInEvent(epoll_event& event, std::shared_ptr<Session> session) {
    int client_fd{event.data.fd};

    if (!session->TryRecv(client_fd)) {
        return false;
    }

    session->ParseMessage();

    if (session->IsMessageComplete()) {
        uint8_t msg_type{session->GetMessageType()};

        if (msg_type == 'A') {
            bool ok{session->HandleAuthRequest()};

            if (!session->SendAuthResponse(client_fd, ok)) {
                return false;
            }

            if (!session->SendBufferEmpty()) {
                UpdateEpollEvents(client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
            }
        } else if (msg_type == 'I') {
            session->HandleImgMessage();
        }
    }

    return true;
}

void Server::HandleEvent(epoll_event& event) {
    int client_fd{event.data.fd};

    auto it{_fd_session_ht.find(client_fd)};

    if (it == _fd_session_ht.end()) {
        return;
    }

    auto& session{it->second};

    if (event.events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP)) {
        CloseSession(session);
        
        return;
    }

    if (event.events & EPOLLOUT) {
        if (!HandleOutEvent(event, session)) {
            CloseSession(session);

            return;
        }
    }

    if (event.events & EPOLLIN) {
        if (!HandleInEvent(event, session)) {
            CloseSession(session);

            return;
        }
    }
}

void Server::EventLoop() {
    _logger.PrintInTerminal(MessageType::K_INFO, "Waiting...");

    constexpr size_t MAX_EVENTS{1024};
    std::vector<epoll_event> events(MAX_EVENTS);

    while (!stop_flag.load(std::memory_order_relaxed)) {
        int num_events{epoll_wait(_epoll_fd.Get(), events.data(), MAX_EVENTS, -1)};

        if (num_events == -1) {
            if (errno == EINTR) {
                continue;
            }

            throw std::runtime_error("epoll_wait(): " + std::string(strerror(errno)));
        }

        for (int i{}; i < num_events; ++i) {
            int fd{events[i].data.fd};

            if (fd == _server_fd.Get()) {
                AcceptNewConnections();
            } else {
                HandleEvent(events[i]);
            }
        }
    }
}

void Server::Run() {
    std::signal(SIGINT, signal_handler);

    try {
        SetupServerSocket();
        SetupEpoll();
        EventLoop();
    } catch (const std::runtime_error& ex) {
        _logger.PrintInTerminal(MessageType::K_ERROR, ex.what());
    }
}
