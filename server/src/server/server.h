#ifndef SERVER_SERVER_SERVER_h
#define SERVER_SERVER_SERVER_h

#include <string>
#include <vector>
#include <memory>
#include <string_view>
#include <unordered_map>

#include <sys/epoll.h>

#include "logger.h"
#include "session.h"
#include "resource_factory.h"

/**
 * @brief Класс TCP-сервера с использованием epoll
 * 
 * Реализует асинхронный сервер с обработкой множества соединений.
 *
 * @section protocol Протокол сообщений:
 * 
 * 1. Аутентификация (клиент -> сервер):
 *    - Формат: 
 *      - 'A'
 *      - [4 байта: размер данных (размер всего что идет дальше)]
 *      - [2 байта: длина имени устройства]
 *      - [имя устройства] 
 *      - [2 байта: длина имени пользователя]
 *      - [имя пользователя]
 * 
 * 2. Ответ на аутентификацию (сервер -> клиент):
 *    - Успех: 'Y'
 *    - Ошибка: 'N'
 * 
 * 3. Передача изображения (клиент -> сервер):
 *    - Формат:
 *      - 'I'
 *      - [4 байта размер данных]
 *      - [бинарные данные изображения]
 */
class Server {
public:
    /**
     * @brief Конструктор сервера
     * @param listen_port Порт для прослушивания входящих соединений
     */
    explicit Server(uint16_t listen_port);

public:
    /**
     * @brief Запуск основного цикла сервера
     * 
     * Последовательность работы:
     * 1. Настройка серверного сокета
     * 2. Инициализация epoll
     * 3. Вход в цикл обработки событий
     * 
     * @note Обрабатывает сигнал SIGINT
     * @throw std::runtime_error При ошибках инициализации
     */
    void Run();

private:
    /**
     * @brief Инициализация epoll
     * @throw std::runtime_error При ошибках создания epoll
     */
    void SetupEpoll();

    /**
     * @brief Настройка серверного сокета
     * @throw std::runtime_error При ошибках:
     * - создания сокета
     * - установки опций
     * - bind/listen
     */
    void SetupServerSocket();

    /**
     * @brief Основной цикл обработки событий
     * 
     * Использует epoll_wait для мультиплексирования ввода-вывода.
     * Обрабатывает до 1024 событий за один вызов.
     * 
     * @throw std::runtime_error При ошибках epoll_wait
     */
    void EventLoop();

    /**
     * @brief Обновить маску событий для файлового дескриптора
     * @param fd Файловый дескриптор
     * @param events Новая маска событий (EPOLLIN/EPOLLOUT и др.)
     * @throw std::runtime_error При ошибках epoll_ctl
     */
    void UpdateEpollEvents(int fd, uint32_t events);

    /**
     * @brief Прием новых подключений
     * 
     * В бесконечном цикле принимает соединения, пока accept
     * не вернет EAGAIN/EWOULDBLOCK. Для каждого соединения:
     * - Устанавливает non-blocking режим
     * - Добавляет в epoll
     * - Создает Session
     * - Добавляет в хеш-таблицу активных сессий
     */
    void AcceptNewConnections();

    /**
     * @brief Закрытие сессии
     * @param session Сессия для закрытия
     * 
     * Удаляет сессию из epoll и хеш-таблицы,
     * логирует событие закрытия.
     */
    void CloseSession(std::shared_ptr<Session> session);

    /**
     * @brief Обработка события записи
     * @param event Событие epoll
     * @param session Сессия для обработки
     * @return true если сессия жива, false если нужно закрыть
     */
    bool HandleOutEvent(epoll_event& event, std::shared_ptr<Session> session);

    /**
     * @brief Обработка события чтения
     * @param event Событие epoll
     * @param session Сессия для обработки
     * @return true если сессия жива, false если нужно закрыть
     * 
     * Обрабатывает два типа сообщений:
     * - 'A' (аутентификация)
     * - 'I' (изображение)
     */
    bool HandleInEvent(epoll_event& event, std::shared_ptr<Session> session);

    /**
     * @brief Обработка одного события epoll
     * @param event Событие для обработки
     * 
     * Определяет тип события (ошибка, чтение, запись)
     * и вызывает соответствующий обработчик.
     */
    void HandleEvent(epoll_event& event);

private:
    uint16_t _listen_port;                                            ///< Порт прослушивания
    
    Logger _logger;                                                   ///< Логгер сервера

    UniqueFD _epoll_fd{};                                             ///< Дескриптор epoll
    UniqueFD _server_fd{};                                            ///< Серверный сокет

    std::unordered_map<int, std::shared_ptr<Session>> _fd_session_ht; ///< Активные сессии (fd -> Session)
};

#endif // SERVER_SERVER_SERVER_h
