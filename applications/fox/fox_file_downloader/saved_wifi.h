#pragma once

#include "app.h"

void wifi_saved_sync(App* app);
void wifi_saved_remove_from_files(const char* ssid);

void wifi_saved_list_show(App* app);
View* wifi_saved_list_view_alloc(App* app);
void wifi_saved_list_view_free(View* view);

void wifi_saved_action_render_menu(App* app);
void wifi_saved_action_select(App* app, uint32_t index);

void wifi_saved_edit_password_submitted(App* app);
