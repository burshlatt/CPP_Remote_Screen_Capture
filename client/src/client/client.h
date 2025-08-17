#ifndef CLIENT_CLIENT_CLIENT_H
#define CLIENT_CLIENT_CLIENT_H

#include <string>
#include <cstdint>

#include "resource_factory.h"
#include "screen_grabber.h"
#include "logger.h"

/**
 * @brief Клиент для отправки скриншотов на сервер.
 * 
 * Класс реализует:
 * - Подключение к серверу по TCP/IP
 * - Аутентификацию (с передачей имени хоста и пользователя)
 * - Периодический захват экрана и отправку изображений
 * - Обработку сигнала SIGINT для корректного завершения
 */
class Client {
public:
    /**
     * @brief Конструктор клиента
     * @param s_host IP-адрес или доменное имя сервера
     * @param s_port Порт сервера
     * @param timeout_sec Интервал между отправкой скриншотов (по умолчанию 10 сек)
     */
    Client(const std::string& s_host, uint16_t s_port, unsigned timeout_sec = 10);

public:
    /**
     * @brief Запуск основного цикла работы клиента
     * @throws std::runtime_error при критических ошибках подключения
     */
    void Run();

private:
    /// Определяет имя текущего хоста (заполняет _hostname)
    void SetupHostname();
    
    /// Определяет имя текущего пользователя (заполняет _username)
    void SetupUsername();

private:
    /**
     * @brief Ожидание с проверкой флага остановки
     * @note Выполняет sleep с проверкой stop_flag каждую секунду
     */
    void WaitLoop();
    
    /// Основной цикл отправки скриншотов
    void SendLoop();
    
    /**
     * @brief Установка соединения с сервером
     * @throws std::runtime_error при ошибках socket()/connect()
     */
    void SetupSocket();
    
    /**
     * @brief Попытка аутентификации на сервере
     * @return true если аутентификация прошла успешно
     */
    bool TryAuthenticate();

    /**
     * @brief Вставляет число в сетевом порядке байт (big-endian) в буфер
     * @tparam T Целочисленный тип (uint8_t, uint16_t или uint32_t)
     * @param buffer Целевой буфер
     * @param num Число для вставки
     */
    template<typename T>
    void InsertToVector(std::vector<uint8_t>& buffer, T num);
    
    /// Создает сообщение с изображением экрана
    std::vector<uint8_t> CreateImgMessage();
    
    /// Формирует запрос аутентификации
    std::vector<uint8_t> CreateAuthenticationRequest();
    
    /**
     * @brief Отправка всех данных через сокет
     * @param data Буфер для отправки
     * @return Количество отправленных байт
     * @throws std::runtime_error при ошибках send()
     */
    ssize_t SendAll(const std::vector<uint8_t>& data);
    
    /**
     * @brief Получение точного количества байт из сокета
     * @param buffer Буфер для приема
     * @param total_bytes Требуемое количество байт
     * @return Количество полученных байт
     * @throws std::runtime_error при ошибках recv()
     */
    ssize_t RecvAll(uint8_t* buffer, size_t total_bytes);
    
private:
    std::string _server_host;      ///< Адрес сервера
    uint16_t _server_port;         ///< Порт сервера
    unsigned _timeout_sec;         ///< Таймаут между отправками (в секундах)

    std::string _hostname;         ///< Имя текущего хоста
    std::string _username;         ///< Имя текущего пользователя

    Logger _logger;                ///< Логгер для вывода сообщений

    UniqueFD _server_fd;           ///< Дескриптор сокета сервера

    ScreenGrabber _screen_grabber; ///< Захватчик экрана
};

#endif // CLIENT_CLIENT_CLIENT_H
