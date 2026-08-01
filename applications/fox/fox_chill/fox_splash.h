#pragma once

#include <gui/view.h>
#include <gui/icon.h>
#include <furi.h>

typedef struct FoxSplash FoxSplash;

typedef void (*FoxSplashDoneCallback)(void* context);

FoxSplash* fox_splash_alloc(
    const Icon* icon,
    uint32_t hold_ms,
    uint32_t fade_ms,
    FoxSplashDoneCallback done_cb,
    void* done_context);
void fox_splash_free(FoxSplash* splash);

View* fox_splash_get_view(FoxSplash* splash);

void fox_splash_start(FoxSplash* splash);
