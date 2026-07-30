#pragma once

#include "app.h"

View* settings_view_alloc(App* app);
void settings_view_free(View* view);

void settings_view_refresh(App* app);

void app_expert_mode_load(App* app);
