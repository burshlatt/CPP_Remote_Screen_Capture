#ifndef SERVER_SERVER_SERVER_h
#define SERVER_SERVER_SERVER_h

#include <string>
#include <vector>
#include <memory>
#include <string_view>
#include <unordered_map>

#include <sys/epoll.h>

#include "session.h"
#include "unique_fd.h"

/**
 * @class Server
 * @brief TCP-прокси сервер для обработки подключений и переадресации запросов на PostgreSQL.
 *
 * Сервер реализует работу с epoll, управляет клиентскими подключениями,
 * логирует SQL-запросы и обрабатывает отказ от SSL.
 */
class Server {
public:
    /**
     * @brief Конструктор сервера.
     * @param port Порт, на котором запускается сервер.
     */
    Server(const std::string& listen_port);

    /**
     * @brief Запускает сервер: инициализация, биндинг и основной цикл событий.
     */
    void Run();

private:
    /**
     * @brief Создаёт epoll.
     */
    void SetupEpoll();

    /**
     * @brief Создаёт и настраивает сокет для прослушивания клиентских подключений.
     */
    void SetupServerSocket();

    /**
     * @brief Основной цикл обработки событий через epoll.
     */
    void EventLoop();

    /**
     * @brief Установка флагов для дескриптора.
     * @param fd Дескриптор.
     * @param events флаги.
     */
    void UpdateEpollEvents(int fd, uint32_t events);

    /**
     * @brief Принимает новые входящие подключения.
     */
    void AcceptNewConnections();

    /**
     * @brief Закрывает соединение с клиентом и PostgreSQL.
     * @param session Сессия для закрытия.
     */
    void CloseSession(std::shared_ptr<Session> session);

    /**
     * @brief Обрабатывает событие epoll.
     * @param event Структура события.
     */
    void HandleEvent(epoll_event& event);

    int CheckPort(const std::string& port);
    std::string CheckHost(const std::string& host);

private:
    int _listen_port; ///< Порт для прослушивания.

    UniqueFD _proxy_fd{}; ///< RAII-обертка над дескриптором сокета прокси-сервера.
    UniqueFD _epoll_fd{}; ///< RAII-обертка над дескриптором epoll.

    std::unordered_map<int, std::shared_ptr<Session>> _fd_session_ht;
};

#endif // SERVER_SERVER_SERVER_h
