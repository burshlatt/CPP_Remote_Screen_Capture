#ifndef CLIENT_CLIENT_SCREEN_GRABBER_SCREEN_GRABBER_H
#define CLIENT_CLIENT_SCREEN_GRABBER_SCREEN_GRABBER_H

#include <vector>
#include <cstdint>
#include <utility>

#include "unique_display.h"
#include "unique_x_image.h"

class ScreenGrabber {
public:
    bool GrabAsPNG(std::vector<unsigned char>& out_png, int& out_w, int& out_h);

private:
    UniqueDisplay OpenDisplay();
    bool GetScreenAttributes(Display* disp, XWindowAttributes& gwa);
    UniqueXImage CaptureImage(Display* disp, Window root, int width, int height);
    std::vector<unsigned char> ConvertToRGB(XImage* img, int width, int height);
    void EncodePNG(const std::vector<unsigned char>& pixels, int width, int height, std::vector<unsigned char>& out_png);
};

#endif // CLIENT_CLIENT_SCREEN_GRABBER_SCREEN_GRABBER_H
