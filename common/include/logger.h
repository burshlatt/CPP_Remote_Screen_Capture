#ifndef COMMON_INCLUDE_LOGGER_H
#define COMMON_INCLUDE_LOGGER_H

#include <string>

/**
 * @brief Типы сообщений для логирования
 */
enum MessageType {
    K_INFO,   ///< Информационное сообщение
    K_ERROR,  ///< Сообщение об ошибке
    K_WARNING ///< Предупреждение
};

/**
 * @brief Класс для логирования сообщений с временными метками
 * 
 * Обеспечивает вывод форматированных сообщений в соответствующие потоки вывода
 * с указанием типа сообщения и временной метки.
 */
class Logger {
public:
    /**
     * @brief Выводит сообщение в терминал с форматированием
     * @param type Тип сообщения (из перечисления MessageType)
     * @param msg Текст сообщения
     * 
     * Формат вывода: [TYPE] [TIMESTAMP] message
     * 
     * @note Для разных типов сообщений используются разные потоки:
     *       - INFO -> std::cout
     *       - ERROR -> std::cerr
     *       - WARNING -> std::clog
     */
    void PrintInTerminal(MessageType type, const std::string& msg);
    
    /**
     * @brief Генерирует текущую временную метку
     * @param mask Формат строки времени (по умолчанию "%Y-%m-%d %H:%M:%S")
     * @return Строка с отформатированным временем
     */
    std::string GetCurrentTimestamp(const char* mask = "%Y-%m-%d %H:%M:%S");

private:
    /**
     * @brief Преобразует тип сообщения в строку
     * @param type Тип сообщения
     * @return Строковое представление типа
     */
    std::string TypeToString(MessageType type);
    
    /**
     * @brief Возвращает поток вывода для типа сообщения
     * @param type Тип сообщения
     * @return Ссылка на соответствующий поток вывода
     */
    std::ostream& TypeToStream(MessageType type);
};

#endif // COMMON_INCLUDE_LOGGER_H
