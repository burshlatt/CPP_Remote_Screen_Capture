#ifndef COMMON_INCLUDE_INPUT_PARSER_H
#define COMMON_INCLUDE_INPUT_PARSER_H

#include <vector>
#include <string>
#include <cstdint>
#include <getopt.h>
#include <unordered_map>

/**
 * @brief Тип программы (сервер или клиент)
 */
enum ProgramType {
    K_SERVER, ///< Серверная часть приложения
    K_CLIENT  ///< Клиентская часть приложения
};

/**
 * @brief Класс для разбора аргументов командной строки
 *
 * Парсит входные аргументы в зависимости от типа программы (сервер/клиент).
 * Для сервера обязателен параметр --port, для клиента --srv и --period.
 * Выбрасывает исключения при невалидных аргументах или отсутствии обязательных параметров.
 */
class InputParser {
public:
    /**
     * @brief Конструктор парсера
     * @param type Тип программы (сервер или клиент)
     */
    explicit InputParser(ProgramType type);

public:
    /**
     * @brief Получить хост сервера (только для клиента)
     * @return IP-адрес сервера в строковом формате
     */
    std::string GetHost() const noexcept;

    /**
     * @brief Получить порт (для сервера - порт прослушивания, для клиента - порт сервера)
     * @return Номер порта
     */
    uint16_t GetPort() const noexcept;

    /**
     * @brief Получить период (только для клиента)
     * @return Период в миллисекундах
     */
    unsigned GetPeriod() const noexcept;

    /**
     * @brief Разобрать аргументы командной строки
     * @param argc Количество аргументов
     * @param argv Массив аргументов
     * @throw std::invalid_argument При невалидных аргументах или отсутствии обязательных параметров
     *
     * @note Форматы аргументов:
     *       Для сервера: --port <номер_порта>
     *       Для клиента: --srv <ip:порт> --period <интервал_мс>
     */
    void Parse(int argc, char *argv[]);

private:
    /**
     * @brief Инициализировать структуры для разбора аргументов
     */
    void InitStructs();

    /**
     * @brief Разобрать аргумент --srv (только для клиента)
     * @param arg Аргумент в формате "ip:port"
     * @throw std::invalid_argument При невалидном формате
     */
    void ParseSrv(char* arg);

    /**
     * @brief Разобрать аргумент --port (только для сервера)
     * @param arg Номер порта (1-65535)
     * @throw std::invalid_argument При невалидном порте
     */
    void ParsePort(char* arg);

    /**
     * @brief Разобрать аргумент --period (только для клиента)
     * @param arg Период в миллисекундах (>0)
     * @throw std::invalid_argument При невалидном периоде
     */
    void ParsePeriod(char* arg);

    /**
     * @brief Обработать опцию сервера
     * @param opt_index Индекс обрабатываемой опции
     */
    void HandleServerOption(int opt_index);

    /**
     * @brief Обработать опцию клиента
     * @param opt_index Индекс обрабатываемой опции
     */
    void HandleClientOption(int opt_index);

private:
    ProgramType _prog_type;                                    ///< Тип программы (сервер/клиент)
    std::string _host;                                         ///< Хост сервера (для клиента)
    uint16_t _port;                                            ///< Порт
    unsigned _period;                                          ///< Период (для клиента)
    std::vector<option> _long_options;                         ///< Структуры long options для getopt_long
    std::unordered_map<std::string, bool> _option_enabled_ht;  ///< Хеш-таблица обработанных опций
};

#endif // COMMON_INCLUDE_INPUT_PARSER_H
