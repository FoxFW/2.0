#pragma once

#include "app.h"

void http_render_menu(App* app);
void http_menu_select(App* app, uint32_t index);

void http_get_url_submitted(App* app);
void http_post_url_submitted(App* app);
void http_post_body_submitted(App* app);
