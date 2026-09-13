#pragma once

#include "app.h"

View* chat_save_result_view_alloc(App* app);
void chat_save_result_view_free(View* view);

void chat_save_result_view_show(App* app, bool success, const char* line1, const char* line2);
void chat_save_result_view_dismiss(App* app);
