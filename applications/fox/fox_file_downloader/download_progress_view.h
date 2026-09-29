#pragma once

#include "app.h"

#define FOX_DOWNLOADER_EVENT_SKIP_WAIT_TIMEOUT 4

View* download_progress_view_alloc(App* app);
void download_progress_view_free(View* view);
void download_progress_view_refresh(View* view);
void download_progress_view_reset(View* view);
void download_progress_view_clear_wait_popups(void);
