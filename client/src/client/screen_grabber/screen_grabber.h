#ifndef CLIENT_CLIENT_SCREEN_GRABBER_SCREEN_GRABBER_H
#define CLIENT_CLIENT_SCREEN_GRABBER_SCREEN_GRABBER_H

#include <vector>
#include <cstdint>
#include <utility>

#include "logger.h"
#include "resource_factory.h"

/**
 * @brief Класс исключения для ошибок захвата экрана.
 * 
 * Исключение выбрасывается при возникновении ошибок в процессе захвата экрана.
 * Содержит дополнительную информацию об ошибке через метод what().
 */
class grabber_error : public std::exception {
public:
    /**
     * @brief Конструктор с сообщением об ошибке.
     * @param info Текст ошибки.
     */
    grabber_error(std::string info) :
        _info(info)
    {}

public:
    /**
     * @brief Возвращает сообщение об ошибке.
     * @return const char* Текст ошибки (null-terminated строка).
     */
    const char* what() const noexcept override {
        return _info.c_str();
    }

private:
    std::string _info; ///< Хранит текст ошибки.
};

/**
 * @brief Класс для захвата содержимого экрана и кодирования в PNG.
 * 
 * Обеспечивает весь процесс захвата экрана в X11:
 * подключение к дисплею, захват изображения, конвертацию формата и кодирование в PNG.
 */
class ScreenGrabber {
public:
    /**
     * @brief Захватывает экран и кодирует его в PNG.
     * 
     * @param[out] out_png Вектор для данных PNG изображения.
     * @param[out] out_w Ширина захваченного изображения.
     * @param[out] out_h Высота захваченного изображения.
     * @throw grabber_error При ошибках в процессе захвата.
     * 
     * Основной метод, выполняющий весь процесс захвата:
     * 1. Подключение к X11 дисплею
     * 2. Получение атрибутов экрана
     * 3. Захват изображения
     * 4. Конвертация в RGB
     * 5. Кодирование в PNG
     */
    void GrabAsPNG(std::vector<uint8_t>& out_png, int& out_w, int& out_h);

private:
    /**
     * @brief Устанавливает соединение с X11 дисплеем.
     * @return UniqueDisplay RAII-обертка для соединения с дисплеем.
     * @throw grabber_error При неудачном подключении.
     */
    UniqueDisplay OpenDisplay();
    
    /**
     * @brief Получает атрибуты экрана по умолчанию.
     * @param[in] disp Соединение с X11 дисплеем.
     * @param[out] gwa Атрибуты окна.
     * @throw grabber_error При ошибке получения атрибутов.
     */
    void GetScreenAttributes(Display* disp, XWindowAttributes& gwa);
    
    /**
     * @brief Захватывает содержимое экрана как XImage.
     * @param[in] disp Соединение с X11 дисплеем.
     * @param[in] root Корневое окно для захвата.
     * @param[in] width Ширина области захвата.
     * @param[in] height Высота области захвата.
     * @return UniqueXImage RAII-обертка для захваченного изображения.
     * @throw grabber_error При ошибке захвата изображения.
     */
    UniqueXImage CaptureImage(Display* disp, Window root, int width, int height);
    
    /**
     * @brief Конвертирует XImage в RGB пиксельные данные.
     * @param[in] img Исходное XImage для конвертации.
     * @param[in] width Ширина изображения.
     * @param[in] height Высота изображения.
     * @return std::vector<uint8_t> Пиксельные данные в RGB (3 байта на пиксель).
     * @throw grabber_error При неподдерживаемом формате пикселей.
     */
    std::vector<uint8_t> ConvertToRGB(XImage* img, int width, int height);
    
    /**
     * @brief Кодирует RGB данные в PNG формат.
     * @param[in] pixels RGB пиксельные данные.
     * @param[in] width Ширина изображения.
     * @param[in] height Высота изображения.
     * @param[out] out_png Результирующие PNG данные.
     */
    void EncodePNG(const std::vector<uint8_t>& pixels, int width, int height, std::vector<uint8_t>& out_png);
    
private:
    Logger _logger; ///< Экземпляр логгера для записи ошибок.
};

#endif // CLIENT_CLIENT_SCREEN_GRABBER_SCREEN_GRABBER_H
