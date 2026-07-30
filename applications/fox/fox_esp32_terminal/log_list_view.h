#pragma once

#include "app.h"

View* log_list_view_alloc(App* app);
void log_list_view_free(View* view);
void log_list_scroll_stop(App* app);
