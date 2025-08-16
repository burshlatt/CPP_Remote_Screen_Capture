#include <unistd.h>

#include "resource_factory.h"

UniqueFD ResourceFactory::MakeUniqueFD(int fd) {
    return UniqueFD(fd, [](int f) { close(f); });
}

UniqueXImage ResourceFactory::MakeUniqueXImage(XImage* img) {
    return UniqueXImage(img, [](XImage* i) { XDestroyImage(i); });
}

UniqueDisplay ResourceFactory::MakeUniqueDisplay(Display* disp) {
    return UniqueDisplay(disp, [](Display* d) { XCloseDisplay(d); });
}
