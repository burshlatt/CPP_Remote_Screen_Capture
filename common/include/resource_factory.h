#ifndef COMMON_INCLUDE_RESOURCE_FACTORY_H
#define COMMON_INCLUDE_RESOURCE_FACTORY_H

#include <functional>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "unique_resource.h"

using UniqueFD      = UniqueResource<int, std::function<void(int)>, -1>;
using UniqueXImage  = UniqueResource<XImage*, std::function<void(XImage*)>, nullptr>;
using UniqueDisplay = UniqueResource<Display*, std::function<void(Display*)>, nullptr>;

class ResourceFactory {
public:
    static UniqueFD MakeUniqueFD(int fd);
    static UniqueXImage MakeUniqueXImage(XImage* img);
    static UniqueDisplay MakeUniqueDisplay(Display* disp);
};

#endif // COMMON_INCLUDE_RESOURCE_FACTORY_H
