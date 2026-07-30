#pragma once

#include "app.h"

#define CHAT_WRAP_LINE_MAX 48

size_t chat_wrap_lines(
    Canvas* canvas,
    const char* text,
    int max_w,
    char out_lines[][CHAT_WRAP_LINE_MAX],
    size_t out_capacity);

#define FOX_DEVICE_NAME_MAX 32
bool chat_find_username_split(const char* text, size_t* out_name_len);

View* chat_list_view_alloc(App* app);
void chat_list_view_free(View* view);

void chat_list_view_show(App* app);
