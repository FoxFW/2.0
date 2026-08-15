#pragma once

#include "app.h"

#define AI_CHAT_MESSAGE_LIMIT_SEC 60

View* message_limit_view_alloc(App* app);
void message_limit_view_free(View* view);

void message_limit_view_show(App* app, uint32_t remaining_sec);
void message_limit_view_tick(App* app);
