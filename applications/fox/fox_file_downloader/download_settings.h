#pragma once

#include "app.h"

View* download_settings_view_alloc(App* app);
void download_settings_view_free(View* view);
void download_settings_view_reset(App* app);

void download_settings_load(App* app);
void download_settings_save(App* app);
