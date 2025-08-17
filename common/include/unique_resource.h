#ifndef COMMON_INCLUDE_UNIQUE_RESOURCE_H
#define COMMON_INCLUDE_UNIQUE_RESOURCE_H

#include <utility>

/**
 * @brief Уникальный ресурс с автоматическим управлением временем жизни.
 *
 * Класс обеспечивает безопасное владение ресурсом, автоматически вызывая
 * указанный удалитель при выходе из области видимости или сбросе.
 *
 * @tparam T Тип ресурса (например, дескриптор файла, указатель и т.д.)
 * @tparam Deleter Тип функтора для освобождения ресурса
 * @tparam InvalidValue Значение, обозначающее невалидный ресурс
 */
template<
    class T,
    class Deleter,
    T InvalidValue
> class UniqueResource {
public:
    /**
     * @brief Конструктор
     * @param resource Ресурс для управления (по умолчанию InvalidValue)
     * @param deleter Функтор для освобождения ресурса (по умолчанию конструктор по умолчанию)
     */
    explicit UniqueResource(T resource = InvalidValue, Deleter deleter = Deleter{})
        : _resource(resource), _deleter(deleter) {}

    /**
     * @brief Перемещающий конструктор
     * @param other Другой объект UniqueResource для перемещения
     * @note После перемещения исходный объект будет содержать InvalidValue
     */
    UniqueResource(UniqueResource&& other) noexcept
        : _resource(std::exchange(other._resource, InvalidValue)), _deleter(std::move(other._deleter)) {}

    /// Копирование запрещено
    UniqueResource(const UniqueResource&) = delete;

    /**
     * @brief Деструктор - автоматически вызывает Reset()
     */
    ~UniqueResource() {
        Reset();
    }

    /// Копирующее присваивание запрещено
    UniqueResource& operator=(const UniqueResource&) = delete;

    /**
     * @brief Перемещающее присваивание
     * @param other Другой объект UniqueResource для перемещения
     * @return Ссылка на текущий объект
     * @note Перед перемещением текущий ресурс будет освобожден
     */
    UniqueResource& operator=(UniqueResource&& other) noexcept {
        if (this != &other) {
            Reset();

            _resource = std::exchange(other._resource, InvalidValue);
            _deleter = std::move(other._deleter);
        }

        return *this;
    }

public:
    /**
     * @brief Получить управляемый ресурс
     * @return Текущее значение ресурса
     */
    T Get() const noexcept {
        return _resource;
    }

    /**
     * @brief Проверить валидность ресурса
     * @return true если ресурс валиден (не равен InvalidValue), иначе false
     */
    bool Valid() const noexcept {
        return _resource != InvalidValue;
    }

    /**
     * @brief Освободить ресурс (если он валиден) и установить InvalidValue
     * @note Безопасно вызывать многократно
     */
    void Reset() noexcept {
        if (Valid()) {
            _deleter(_resource);
        }

        _resource = InvalidValue;
    }

private:
    T _resource;      ///< Управляемый ресурс
    Deleter _deleter; ///< Функтор для освобождения ресурса
};

#endif // COMMON_INCLUDE_UNIQUE_RESOURCE_H
