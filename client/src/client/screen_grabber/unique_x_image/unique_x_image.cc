#include "unique_x_image.h"

UniqueXImage::UniqueXImage(XImage* ximg) :
    _ximg(ximg)
{}

UniqueXImage::UniqueXImage(UniqueXImage&& other) noexcept :
    _ximg(std::exchange(other._ximg, nullptr))
{}

UniqueXImage::~UniqueXImage() {
    if (_ximg) {
        XDestroyImage(_ximg);
    }
}

UniqueXImage& UniqueXImage::operator=(UniqueXImage&& other) noexcept {
    if (this != &other) {
        if (_ximg) {
            XDestroyImage(_ximg);
        }

        _ximg = std::exchange(other._ximg, nullptr);
    }

    return *this;
}

XImage* UniqueXImage::Get() const noexcept {
    return _ximg;
}

bool UniqueXImage::Valid() const noexcept {
    return _ximg != nullptr;
}
