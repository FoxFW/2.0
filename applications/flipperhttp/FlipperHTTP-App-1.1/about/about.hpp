#pragma once
#include "easy_flipper/easy_flipper.h"

class FlipperHTTPAbout
{
private:
    Widget *widget;
    ViewDispatcher **viewDispatcherRef;

    static constexpr const uint32_t FlipperHTTPViewSubmenu = 1; // View ID for submenu
    static constexpr const uint32_t FlipperHTTPViewAbout = 2;   // View ID for about

    static uint32_t callbackToSubmenu(void *context);

public:
    FlipperHTTPAbout(ViewDispatcher **viewDispatcher);
    ~FlipperHTTPAbout();
};
