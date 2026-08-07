#pragma once

#include "app.h"

// Sent when the "Not waiting for ESP32..." popup's 2.5s auto-dismiss
// timer expires - handled in main.c, which does the actual
// app_switch_to_menu (FuriTimer callbacks run on the timer service
// thread, not the ViewDispatcher's own thread, so the switch can't happen
// directly inside progress_timer_cb).
#define FOX_DOWNLOADER_EVENT_SKIP_WAIT_TIMEOUT 4

View* download_progress_view_alloc(App* app);
void download_progress_view_free(View* view);
void download_progress_view_refresh(View* view);
void download_progress_view_reset(View* view);
void download_progress_view_clear_wait_popups(void);
