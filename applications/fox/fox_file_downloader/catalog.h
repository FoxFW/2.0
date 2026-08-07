#pragma once

#include "app.h"

void catalog_open(App* app);
void catalog_show_disclaimer(App* app, const char* text, bool is_mismatch);
void catalog_render_menu(App* app);
void catalog_menu_select(App* app, uint32_t index);

void catalog_show_app_list(App* app);

View* catalog_app_list_view_alloc(App* app);
void catalog_app_list_view_free(View* v);

View* catalog_disclaimer_view_alloc(App* app);
void catalog_disclaimer_view_free(View* v);

void catalog_free_buffers(App* app);
