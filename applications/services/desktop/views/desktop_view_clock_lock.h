#pragma once
#include <gui/view.h>

typedef struct DesktopClockLockView DesktopClockLockView;
typedef void (*DesktopClockLockViewCallback)(void* context);

DesktopClockLockView* desktop_clock_lock_alloc(void);
void desktop_clock_lock_free(DesktopClockLockView* clock_lock);
View* desktop_clock_lock_get_view(DesktopClockLockView* clock_lock);
void desktop_clock_lock_set_callback(DesktopClockLockView* clock_lock, DesktopClockLockViewCallback callback, void* context);

// Fox Alarm Clock ringing state - shows an "ALARM" banner and accepts a
// short OK press as an extra dismiss shortcut on top of the usual long-press
// Down exit gesture.
void desktop_clock_lock_set_ringing(DesktopClockLockView* clock_lock, bool ringing);

// Left = force the backlight to stay on while this screen is showing;
// Right = go back to the normal display timeout. Mirrors the "Keep
// Backlight On" toggle in Fox Settings > Alarm Clock - this is just a quick
// in-screen shortcut for the same setting.
typedef void (*DesktopClockLockBacklightCallback)(void* context, bool keep_on);
void desktop_clock_lock_set_backlight_callback(
    DesktopClockLockView* clock_lock,
    DesktopClockLockBacklightCallback callback,
    void* context);

// Fires once a second while this screen is showing (reuses the view's own
// digit-refresh timer) - used to keep re-asserting "Keep Backlight On"
// without needing a second always-running timer elsewhere.
typedef void (*DesktopClockLockTickCallback)(void* context);
void desktop_clock_lock_set_tick_callback(
    DesktopClockLockView* clock_lock,
    DesktopClockLockTickCallback callback,
    void* context);