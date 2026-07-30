#pragma once

#include "app.h"

View* terminal_view_alloc(App* app);
void terminal_view_free(View* view);

void terminal_unpause(App* app);

void terminal_poll_tick(App* app);
