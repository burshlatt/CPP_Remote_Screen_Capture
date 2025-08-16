#ifndef COMMON_INCLUDE_UNIQUE_RESOURCE_H
#define COMMON_INCLUDE_UNIQUE_RESOURCE_H

#include <utility>

template<
    class T,
    class Deleter,
    T InvalidValue
> class UniqueResource {
public:
    explicit UniqueResource(T resource = InvalidValue, Deleter deleter = Deleter{})
        : _resource(resource), _deleter(deleter) {}

    UniqueResource(UniqueResource&& other) noexcept
        : _resource(std::exchange(other._resource, InvalidValue)), _deleter(std::move(other._deleter)) {}

    UniqueResource(const UniqueResource&) = delete;

    ~UniqueResource() {
        Reset();
    }

    UniqueResource& operator=(const UniqueResource&) = delete;

    UniqueResource& operator=(UniqueResource&& other) noexcept {
        if (this != &other) {
            Reset();

            _resource = std::exchange(other._resource, InvalidValue);
            _deleter = std::move(other._deleter);
        }

        return *this;
    }

public:
    T Get() const noexcept {
        return _resource;
    }

    bool Valid() const noexcept {
        return _resource != InvalidValue;
    }

    void Reset() noexcept {
        if (Valid()) {
            _deleter(_resource);
        }

        _resource = InvalidValue;
    }

private:
    T _resource;
    Deleter _deleter;
};

#endif // COMMON_INCLUDE_UNIQUE_RESOURCE_H
