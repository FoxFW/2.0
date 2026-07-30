#pragma once

#include "app.h"

View* connect_settings_view_alloc(App* app);
void connect_settings_view_free(View* view);

void connect_settings_view_reset(App* app);
