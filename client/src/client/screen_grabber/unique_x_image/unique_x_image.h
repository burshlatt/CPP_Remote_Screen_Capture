#ifndef CLIENT_CLIENT_SCREEN_GRABBER_UNIQUE_X_IMAGE_UNIQUE_X_IMAGE_H
#define CLIENT_CLIENT_SCREEN_GRABBER_UNIQUE_X_IMAGE_UNIQUE_X_IMAGE_H

#include <utility>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

struct UniqueXImage {
public:
    explicit UniqueXImage(XImage* ximg);
    UniqueXImage(UniqueXImage&& other) noexcept;
    UniqueXImage(const UniqueXImage&) = delete;
    ~UniqueXImage();

    UniqueXImage& operator=(const UniqueXImage&) = delete;
    UniqueXImage& operator=(UniqueXImage&& other) noexcept;

public:
    XImage* Get() const noexcept;
    bool Valid() const noexcept;

private:
    XImage* _ximg;
};

#endif // CLIENT_CLIENT_SCREEN_GRABBER_UNIQUE_X_IMAGE_UNIQUE_X_IMAGE_H
