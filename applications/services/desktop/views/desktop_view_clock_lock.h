#pragma once
#include <gui/view.h>

typedef struct DesktopClockLockView DesktopClockLockView;
typedef void (*DesktopClockLockViewCallback)(void* context);

DesktopClockLockView* desktop_clock_lock_alloc(void);
void desktop_clock_lock_free(DesktopClockLockView* clock_lock);
View* desktop_clock_lock_get_view(DesktopClockLockView* clock_lock);
void desktop_clock_lock_set_callback(DesktopClockLockView* clock_lock, DesktopClockLockViewCallback callback, void* context);

void desktop_clock_lock_set_ringing(DesktopClockLockView* clock_lock, bool ringing);

typedef void (*DesktopClockLockBacklightCallback)(void* context, bool turn_on);
void desktop_clock_lock_set_backlight_callback(
    DesktopClockLockView* clock_lock,
    DesktopClockLockBacklightCallback callback,
    void* context);

typedef void (*DesktopClockLockBrightnessCallback)(void* context, bool increase);
void desktop_clock_lock_set_brightness_callback(
    DesktopClockLockView* clock_lock,
    DesktopClockLockBrightnessCallback callback,
    void* context);

void desktop_clock_lock_show_brightness(DesktopClockLockView* clock_lock, uint8_t percent);

typedef void (*DesktopClockLockTickCallback)(void* context);
void desktop_clock_lock_set_tick_callback(
    DesktopClockLockView* clock_lock,
    DesktopClockLockTickCallback callback,
    void* context);
