#pragma once

#include "app.h"

View* message_view_alloc(App* app);
void message_view_free(View* view);

void message_view_show_detecting(App* app);

void message_view_show_not_detected(App* app);

void message_view_show_wifi_not_connected(App* app);

void message_view_show_serial_busy(App* app);

void message_view_show_serial_retry_failed(App* app);
