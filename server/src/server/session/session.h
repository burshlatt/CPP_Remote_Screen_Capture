#ifndef SERVER_SERVER_SESSION_SESSION_H
#define SERVER_SERVER_SESSION_SESSION_H

#include <queue>
#include <vector>
#include <string>

#include "logger.h"
#include "resource_factory.h"

/**
 * @brief Структура для хранения сообщения
 * 
 * Содержит данные сообщения в виде векторов байт:
 * - тип сообщения
 * - размер данных
 * - сами данные
 */
struct Message {
    std::vector<uint8_t> type_vec;   ///< Вектор байт типа сообщения (1 байт)
    std::vector<uint8_t> size_vec;   ///< Вектор байт размера данных (4 байта)
    std::vector<uint8_t> bytes_vec;  ///< Вектор байт данных сообщения

    /**
     * @brief Очистить все поля сообщения
     */
    void Clear() {
        type_vec.clear();
        size_vec.clear();
        bytes_vec.clear();
    }
};

/**
 * @brief Класс для управления клиентской сессией
 * 
 * Обрабатывает сетевые сообщения, управляет передачей данных,
 * сохраняет скриншоты и логирует события сессии.
 */
class Session {
public:
    /**
     * @brief Конструктор сессии
     * @param client_fd Уникальный файловый дескриптор клиентского сокета
     * @param host IP-адрес клиента
     * @param port Порт клиента
     */
    Session(UniqueFD&& client_fd, const std::string& host, const std::string& port);

public:
    /**
     * @brief Получить файловый дескриптор клиента
     * @return Дескриптор сокета клиента
     */
    int GetClientFD() const noexcept;

    /**
     * @brief Получить IP-адрес клиента
     * @return Строка с IP-адресом
     */
    std::string GetClientHost() const noexcept;

    /**
     * @brief Получить порт клиента
     * @return Строка с номером порта
     */
    std::string GetClientPort() const noexcept;

    /**
     * @brief Получить тип текущего сообщения
     * @return Тип сообщения (1 байт)
     * @throw std::runtime_error Если сообщение неполное
     */
    uint8_t GetMessageType() const;

    /**
     * @brief Попытаться получить данные от клиента
     * @param fd Файловый дескриптор для чтения
     * @return true если соединение активно, false если разрыв
     */
    bool TryRecv(int fd);

    /**
     * @brief Попытаться отправить данные клиенту
     * @param fd Файловый дескриптор для записи
     * @return true если отправка успешна, false при ошибке
     */
    bool TrySend(int fd);

    /**
     * @brief Отправить ответ на аутентификацию
     * @param fd Файловый дескриптор
     * @param ok Результат аутентификации
     * @return true если отправка успешна, false при ошибке
     */
    bool SendAuthResponse(int fd, bool ok);

    /**
     * @brief Разобрать полученные данные в сообщения
     */
    void ParseMessage();

    /**
     * @brief Проверить пустоту буфера отправки
     * @return true если буфер пуст
     */
    bool SendBufferEmpty() const;

    /**
     * @brief Проверить наличие полного сообщения
     * @return true если есть готовое сообщение
     */
    bool IsMessageComplete() const;

    /**
     * @brief Обработать сообщение с изображением
     */
    void HandleImgMessage();

    /**
     * @brief Обработать запрос аутентификации
     * @return true если аутентификация успешна
     */
    bool HandleAuthRequest();

private:
    /**
     * @brief Прочитать uint8_t из буфера (без извлечения)
     * @param buffer Входной буфер данных
     * @return Прочитанное значение
     * @throw std::runtime_error Если буфер слишком мал
     */
    uint8_t PeekUint8(const std::vector<uint8_t>& buffer) const;

    /**
     * @brief Прочитать uint16_t из буфера (без извлечения)
     * @param buffer Входной буфер данных
     * @return Прочитанное значение (конвертируется из сетевого порядка)
     * @throw std::runtime_error Если буфер слишком мал
     */
    uint16_t PeekUint16(const std::vector<uint8_t>& buffer) const;

    /**
     * @brief Прочитать uint32_t из буфера (без извлечения)
     * @param buffer Входной буфер данных
     * @return Прочитанное значение (конвертируется из сетевого порядка)
     * @throw std::runtime_error Если буфер слишком мал
     */
    uint32_t PeekUint32(const std::vector<uint8_t>& buffer) const;

    /**
     * @brief Извлечь uint8_t из буфера
     * @param buffer Буфер данных (будет модифицирован)
     * @return Извлеченное значение
     * @throw std::runtime_error Если буфер слишком мал
     */
    uint8_t PopUint8(std::vector<uint8_t>& buffer);

    /**
     * @brief Извлечь uint16_t из буфера
     * @param buffer Буфер данных (будет модифицирован)
     * @return Извлеченное значение (конвертируется из сетевого порядка)
     * @throw std::runtime_error Если буфер слишком мал
     */
    uint16_t PopUint16(std::vector<uint8_t>& buffer);

    /**
     * @brief Извлечь uint32_t из буфера
     * @param buffer Буфер данных (будет модифицирован)
     * @return Извлеченное значение (конвертируется из сетевого порядка)
     * @throw std::runtime_error Если буфер слишком мал
     */
    uint32_t PopUint32(std::vector<uint8_t>& buffer);

    /**
     * @brief Извлечь строку из буфера
     * @param buffer Буфер данных (будет модифицирован)
     * @param str_len Длина извлекаемой строки
     * @return Извлеченная строка
     * @throw std::runtime_error Если буфер слишком мал
     */
    std::string PopString(std::vector<uint8_t>& buffer, uint16_t str_len);

    /**
     * @brief Сгенерировать строку идентификатора из хоста и порта
     * @return Строка в формате "ip_port" (например "192168011_8080")
     */
    std::string GetStringFromHostPort();

    /**
     * @brief Сохранить скриншот из сообщения в файл
     * @param msg Сообщение содержащее изображение
     * 
     * Сохраняет в папку screenshots/<hostname>/<username>/
     * с именем файла <timestamp>_<host_port>.png
     */
    void SaveScreen(const Message& msg);

    /**
     * @brief Разобрать сообщение аутентификации
     * @param msg Сообщение для разбора
     * 
     * Извлекает hostname и username из сообщения,
     * сохраняет их в полях класса
     */
    void ParseAuthMessage(Message& msg);

    /**
     * @brief Перенести данные из запроса в вектор
     * @param vec Целевой вектор
     * @param len Количество байт для переноса
     * @return true если данных хватило, false если нет
     */
    bool FromReqToVec(std::vector<uint8_t>& vec, size_t len);

private:
    UniqueFD _client_fd;            ///< Дескриптор клиентского сокета
    std::string _client_host;       ///< IP-адрес клиента
    std::string _client_port;       ///< Порт клиента
    std::string _client_hostname;   ///< Имя хоста клиента
    std::string _client_username;   ///< Имя пользователя клиента

    Message _message;               ///< Текущее обрабатываемое сообщение
    std::queue<Message> _messages;  ///< Очередь готовых сообщений
    std::vector<uint8_t> _request;  ///< Буфер входящих данных
    std::vector<uint8_t> _response; ///< Буфер исходящих данных

    Logger _logger;                 ///< Логгер для записи событий
};

#endif // SERVER_SERVER_SESSION_SESSION_H
