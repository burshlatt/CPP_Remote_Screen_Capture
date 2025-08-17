#include <iostream>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "screen_grabber.h"

UniqueDisplay ScreenGrabber::OpenDisplay() {
    UniqueDisplay u_disp(ResourceFactory::MakeUniqueDisplay(XOpenDisplay(nullptr)));

    if (!u_disp.Valid()) {
        throw grabber_error("XOpenDisplay() failed. Check DISPLAY.");
    }

    return u_disp;
}

void ScreenGrabber::GetScreenAttributes(Display* disp, XWindowAttributes& gwa) {
    int screen{DefaultScreen(disp)};
    Window root{RootWindow(disp, screen)};

    if (!XGetWindowAttributes(disp, root, &gwa)) {
        throw grabber_error("XGetWindowAttributes() failed.");
    }
}

UniqueXImage ScreenGrabber::CaptureImage(Display* disp, Window root, int width, int height) {
    XImage* x_img{XGetImage(disp, root, 0, 0, width, height, AllPlanes, ZPixmap)}; 
    UniqueXImage u_ximg(ResourceFactory::MakeUniqueXImage(x_img));

    if (!u_ximg.Valid()) {
        throw grabber_error("XGetImage() failed.");
    }

    return u_ximg;
}

std::vector<unsigned char> ScreenGrabber::ConvertToRGB(XImage* img, int width, int height) {
    const int channels{3};
    std::vector<unsigned char> pixels(width * height * channels);

    int bpp{img->bits_per_pixel};

    if (bpp != 24 && bpp != 32) {
        throw grabber_error("Unsupported bits_per_pixel: " + std::to_string(bpp));
    }

    const bool is_lsb_first{(img->byte_order == LSBFirst)};

    for (int y{0}; y < height; ++y) {
        unsigned char* row{reinterpret_cast<unsigned char*>(img->data + y * img->bytes_per_line)};

        for (int x{0}; x < width; ++x) {
            unsigned char red;
            unsigned char green;
            unsigned char blue;

            if (bpp == 32) {
                if (is_lsb_first) {
                    // 32 bpp LSBFirst: BGRA
                    blue  = row[x * 4 + 0];
                    green = row[x * 4 + 1];
                    red   = row[x * 4 + 2];
                } else {
                    // 32 bpp MSBFirst: ARGB
                    red   = row[x * 4 + 1];
                    green = row[x * 4 + 2];
                    blue  = row[x * 4 + 3];
                }
            } else {
                if (is_lsb_first) {
                    // 24 bpp LSBFirst: BGR
                    blue  = row[x * 3 + 0];
                    green = row[x * 3 + 1];
                    red   = row[x * 3 + 2];
                } else {
                    // 24 bpp MSBFirst: RGB
                    red   = row[x * 3 + 0];
                    green = row[x * 3 + 1];
                    blue  = row[x * 3 + 2];
                }
            }

            size_t idx{static_cast<size_t>((y * width + x) * channels)};

            pixels[idx + 0] = red;
            pixels[idx + 1] = green;
            pixels[idx + 2] = blue;
        }
    }

    return pixels;
}

void ScreenGrabber::EncodePNG(const std::vector<unsigned char>& pixels, int width, int height, std::vector<unsigned char>& out_png) {
    struct MemWriter {
        static void write(void* context, void* data, int size) {
            auto* vec{reinterpret_cast<std::vector<unsigned char>*>(context)};
            auto char_data{reinterpret_cast<unsigned char*>(data)};

            vec->insert(vec->end(), char_data, char_data + size);
        }
    };

    out_png.clear();

    stbi_write_png_to_func(MemWriter::write, &out_png, width, height, 3, pixels.data(), width * 3);
}

void ScreenGrabber::GrabAsPNG(std::vector<unsigned char>& out_png, int& out_w, int& out_h) {
    UniqueDisplay disp(OpenDisplay());
    XWindowAttributes gwa;

    GetScreenAttributes(disp.Get(), gwa);

    int width{gwa.width};
    int height{gwa.height};

    if (width <= 0 || height <= 0) {
        throw grabber_error("Invalid screen size.");
    }

    Window root{gwa.root};
    UniqueXImage img(CaptureImage(disp.Get(), root, width, height));
    std::vector<unsigned char> pixels(ConvertToRGB(img.Get(), width, height));

    EncodePNG(pixels, width, height, out_png);

    out_w = width;
    out_h = height;
}
