#ifndef CLIENT_CLIENT_SCREEN_GRABBER_UNIQUE_DISPLAY_UNIQUE_DISPLAY_H
#define CLIENT_CLIENT_SCREEN_GRABBER_UNIQUE_DISPLAY_UNIQUE_DISPLAY_H

#include <utility>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

class UniqueDisplay {
public:
    explicit UniqueDisplay(Display* disp);
    UniqueDisplay(UniqueDisplay&& other) noexcept;
    UniqueDisplay(const UniqueDisplay&) = delete;
    ~UniqueDisplay();

    UniqueDisplay& operator=(const UniqueDisplay&) = delete;
    UniqueDisplay& operator=(UniqueDisplay&& other) noexcept;

public:
    Display* Get() const noexcept;
    bool Valid() const noexcept;

private:
    Display* _disp;
};

#endif // CLIENT_CLIENT_SCREEN_GRABBER_UNIQUE_DISPLAY_UNIQUE_DISPLAY_H
