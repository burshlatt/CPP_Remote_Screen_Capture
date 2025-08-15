#include "unique_display.h"

UniqueDisplay::UniqueDisplay(Display* disp) :
    _disp(disp)
{}

UniqueDisplay::UniqueDisplay(UniqueDisplay&& other) noexcept :
    _disp(std::exchange(other._disp, nullptr))
{}

UniqueDisplay::~UniqueDisplay() {
    if (_disp) {
        XCloseDisplay(_disp);
    }
}

UniqueDisplay& UniqueDisplay::operator=(UniqueDisplay&& other) noexcept {
    if (this != &other) {
        if (_disp) {
            XCloseDisplay(_disp);
        }

        _disp = std::exchange(other._disp, nullptr);
    }

    return *this;
}

Display* UniqueDisplay::Get() const noexcept {
    return _disp;
}

bool UniqueDisplay::Valid() const noexcept {
    return _disp != nullptr;
}
