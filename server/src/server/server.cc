#include <atomic>
#include <csignal>
#include <cstring>
#include <iostream>

#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "server.h"

static volatile sig_atomic_t stop_flag = 0;

void signal_handler(int sig) {
    if (sig == SIGINT) {
        stop_flag = 1;
    }
}

Server::Server(const std::string& listen_port) :
    _listen_port(CheckPort(listen_port))
{}

int Server::CheckPort(const std::string& port) {
    int serv_port{std::stoi(port)};

    if (serv_port > 0 && serv_port <= 65535) {
        return serv_port;
    }

    throw std::invalid_argument("Invalid port: " + std::to_string(serv_port));
}

std::string Server::CheckHost(const std::string& host) {
    struct in_addr addr;

    if (inet_pton(AF_INET, host.c_str(), &addr) == 1) {
        return host;
    }

    throw std::invalid_argument("Invalid host: " + host);
}

void Server::SetupEpoll() {
    _epoll_fd = UniqueFD(ResourceFactory::MakeUniqueFD(epoll_create1(0)));
    
    if (!_epoll_fd.Valid()) {
        throw std::runtime_error("SetupEpoll(): " + std::string(strerror(errno)));
    }
}

void Server::SetupServerSocket() {
    _proxy_fd = UniqueFD(ResourceFactory::MakeUniqueFD(socket(AF_INET, SOCK_STREAM, 0)));

    if (!_proxy_fd.Valid()) {
        throw std::runtime_error("SetupServerSocket(): " + std::string(strerror(errno)));
    }

    int flags{fcntl(_proxy_fd.Get(), F_GETFL, 0)};
    fcntl(_proxy_fd.Get(), F_SETFL, flags | O_NONBLOCK);

    int opt{1};

    if (setsockopt(_proxy_fd.Get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        throw std::runtime_error("SetupServerSocket(): " + std::string(strerror(errno)));
    }

    struct sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(_listen_port);

    auto s_addr{reinterpret_cast<struct sockaddr*>(&server_addr)};

    if (bind(_proxy_fd.Get(), s_addr, sizeof(server_addr)) == -1) {
        throw std::runtime_error("SetupServerSocket(): " + std::string(strerror(errno)));
    }

    if (listen(_proxy_fd.Get(), SOMAXCONN) == -1) {
        throw std::runtime_error("SetupServerSocket(): " + std::string(strerror(errno)));
    }

    epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = _proxy_fd.Get();

    if (epoll_ctl(_epoll_fd.Get(), EPOLL_CTL_ADD, _proxy_fd.Get(), &event) == -1) {
        throw std::runtime_error("SetupServerSocket(): " + std::string(strerror(errno)));
    }
}

void Server::AcceptNewConnections() {
    while (true) {
        struct sockaddr_in client_addr = {};
        auto c_addr{reinterpret_cast<sockaddr*>(&client_addr)};
        socklen_t c_addr_len{sizeof(client_addr)};

        UniqueFD client_fd(ResourceFactory::MakeUniqueFD(accept(_proxy_fd.Get(), c_addr, &c_addr_len)));

        if (!client_fd.Valid()) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else if (errno == EINTR) {
                continue;
            } else {
                std::cerr << "accept(): " << strerror(errno) << '\n';

                break;
            }
        }

        int flags{fcntl(client_fd.Get(), F_GETFL, 0)};
        fcntl(client_fd.Get(), F_SETFL, flags | O_NONBLOCK);

        epoll_event event;
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = client_fd.Get();

        if (epoll_ctl(_epoll_fd.Get(), EPOLL_CTL_ADD, client_fd.Get(), &event) == -1) {
            std::cerr << "epoll_ctl(): " << strerror(errno) << '\n';

            continue;
        }

        std::string host(inet_ntoa(client_addr.sin_addr));
        uint16_t port{ntohs(client_addr.sin_port)};

        auto session{std::make_shared<Session>(std::move(client_fd), host, port)};

        _fd_session_ht[session->GetClientFD()] = session;

        std::cout << "[INFO] New connection! (client: " << host << ':' << port << ")\n";
    }
}

void Server::CloseSession(std::shared_ptr<Session> session) {
    int client_fd{session->GetClientFD()};

    epoll_ctl(_epoll_fd.Get(), EPOLL_CTL_DEL, client_fd, nullptr);

    _fd_session_ht.erase(client_fd);

    std::cout << "[INFO] Close connection. (client: " << session->GetClientHost() << ':' << session->GetClientPort() << ")\n";
}

void Server::HandleEvent(epoll_event& event) {
    int client_fd{event.data.fd};

    auto it{_fd_session_ht.find(client_fd)};

    if (it == _fd_session_ht.end()) {
        return;
    }

    auto& session{it->second};

    if (!session->RecvAll(client_fd)) {
        CloseSession(session);

        return;
    }

    if (session->IsMessageComplete()) {
        session->SaveScreen();
    }
}

void Server::EventLoop() {
    std::cout << "Waiting...\n";

    constexpr size_t MAX_EVENTS{1024};
    std::vector<epoll_event> events(MAX_EVENTS);

    while (!stop_flag) {
        int num_events{epoll_wait(_epoll_fd.Get(), events.data(), MAX_EVENTS, -1)};

        if (num_events == -1) {
            if (errno == EINTR) {
                continue;
            }

            throw std::runtime_error("epoll_wait(): " + std::string(strerror(errno)));
        }

        for (int i{}; i < num_events; ++i) {
            int fd{events[i].data.fd};

            if (fd == _proxy_fd.Get()) {
                AcceptNewConnections();
            } else {
                HandleEvent(events[i]);
            }
        }
    }
}

void Server::Run() {
    std::signal(SIGINT, signal_handler);

    SetupEpoll();
    SetupServerSocket();
    
    EventLoop();
}
