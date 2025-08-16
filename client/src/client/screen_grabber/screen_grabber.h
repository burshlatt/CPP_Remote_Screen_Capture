#ifndef CLIENT_CLIENT_SCREEN_GRABBER_SCREEN_GRABBER_H
#define CLIENT_CLIENT_SCREEN_GRABBER_SCREEN_GRABBER_H

#include <vector>
#include <cstdint>
#include <utility>

#include "logger.h"
#include "resource_factory.h"

class ScreenGrabber {
public:
    void GrabAsPNG(std::vector<unsigned char>& out_png, int& out_w, int& out_h);

private:
    UniqueDisplay OpenDisplay();
    void GetScreenAttributes(Display* disp, XWindowAttributes& gwa);
    UniqueXImage CaptureImage(Display* disp, Window root, int width, int height);
    std::vector<unsigned char> ConvertToRGB(XImage* img, int width, int height);
    void EncodePNG(const std::vector<unsigned char>& pixels, int width, int height, std::vector<unsigned char>& out_png);
    
private:
    Logger _logger;
};

#endif // CLIENT_CLIENT_SCREEN_GRABBER_SCREEN_GRABBER_H
