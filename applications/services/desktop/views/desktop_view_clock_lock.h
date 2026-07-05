#pragma once
#include <gui/view.h>

typedef struct DesktopClockLockView DesktopClockLockView;
typedef void (*DesktopClockLockViewCallback)(void* context);

DesktopClockLockView* desktop_clock_lock_alloc(void);
void desktop_clock_lock_free(DesktopClockLockView* clock_lock);
View* desktop_clock_lock_get_view(DesktopClockLockView* clock_lock);
void desktop_clock_lock_set_callback(DesktopClockLockView* clock_lock, DesktopClockLockViewCallback callback, void* context);