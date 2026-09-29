#pragma once

#include "icon_animation.h"

#include <furi.h>

struct IconAnimation {
    const Icon* icon;
    uint8_t frame;
    bool animating;
    FuriTimer* timer;
    IconAnimationCallback callback;
    void* callback_context;
};

const uint8_t* icon_animation_get_data(const IconAnimation* instance);

void icon_animation_next_frame(IconAnimation* instance);

void icon_animation_timer_callback(void* context);
