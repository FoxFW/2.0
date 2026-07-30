#pragma once

#include "app.h"

void foxportal_render_menu(App* app);
void foxportal_menu_select(App* app, uint32_t index);

void foxportal_ssid_submitted(App* app);

void foxportal_sync_saved_results(App* app, bool background);
void foxportal_show_saved_results(App* app);

View* foxportal_qr_view_alloc(App* app);
