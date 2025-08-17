#ifndef COMMON_INCLUDE_RESOURCE_FACTORY_H
#define COMMON_INCLUDE_RESOURCE_FACTORY_H

#include <functional>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "unique_resource.h"

/**
 * @brief Псевдоним для уникального файлового дескриптора
 * 
 * Автоматически закрывает файловый дескриптор с помощью close()
 */
using UniqueFD = UniqueResource<int, std::function<void(int)>, -1>;

/**
 * @brief Псевдоним для уникального XImage
 * 
 * Автоматически освобождает XImage с помощью XDestroyImage()
 */
using UniqueXImage = UniqueResource<XImage*, std::function<void(XImage*)>, nullptr>;

/**
 * @brief Псевдоним для уникального соединения с X-сервером (Display)
 * 
 * Автоматически закрывает соединение с помощью XCloseDisplay()
 */
using UniqueDisplay = UniqueResource<Display*, std::function<void(Display*)>, nullptr>;

/**
 * @brief Фабрика для создания RAII-объектов X11 ресурсов
 * 
 * Предоставляет статические методы для создания объектов, автоматически 
 * управляющих временем жизни ресурсов X11.
 */
class ResourceFactory {
public:
    /**
     * @brief Создает UniqueFD для управления файловым дескриптором
     * @param fd Файловый дескриптор
     * @return UniqueFD, который закроет дескриптор при разрушении
     */
    static UniqueFD MakeUniqueFD(int fd);

    /**
     * @brief Создает UniqueXImage для управления XImage
     * @param img Указатель на XImage
     * @return UniqueXImage, который уничтожит изображение при разрушении
     */
    static UniqueXImage MakeUniqueXImage(XImage* img);

    /**
     * @brief Создает UniqueDisplay для управления соединением с X-сервером
     * @param disp Указатель на Display
     * @return UniqueDisplay, который закроет соединение при разрушении
     */
    static UniqueDisplay MakeUniqueDisplay(Display* disp);
};

#endif // COMMON_INCLUDE_RESOURCE_FACTORY_H
