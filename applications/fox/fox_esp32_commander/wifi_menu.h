#pragma once

#include "app.h"

void wifi_render_menu(App* app, MenuContext ctx);
void wifi_menu_select(App* app, MenuContext ctx, uint32_t index);

View* wifi_network_list_view_alloc(App* app);
void wifi_network_list_view_free(View* view);

View* wifi_station_list_view_alloc(App* app);
void wifi_station_list_view_free(View* view);

void wifi_password_submitted(App* app);

void wifi_beacon_custom_submitted(App* app);
