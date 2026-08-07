#pragma once

#include "app.h"

void my_apps_open(App* app);
void my_apps_free_buffers(App* app);

View* my_apps_list_view_alloc(App* app);
void my_apps_list_view_free(View* v);
