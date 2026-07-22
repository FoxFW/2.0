#pragma once

#include "app.h"

/* WiFi menu tree: Wifi (Connect / My IP / Recon / Attacks) ->
   WifiRecon (Scan APs / Scan Stations / Select AP / Signal Monitor /
   Packet Count / Wardrive / Packet Capture) and WifiAttacks (Select AP / Select Station /
   Deauth (Broadcast) / Deauth (Targeted) / Beacon Spam (Random) /
   Beacon Spam (Custom) / Probe Flood), plus the two rounded-list
   screens shared across those: the network list (Connect and
   "Select AP") and the station list ("Select Station", feeding
   targeted deauth).

   A general arbitrary-URL HTTP client is deliberately kept in its own
   module (http_menu.c) rather than folded in here - this file was
   already the largest in the app before that feature existed. */

void wifi_render_menu(App* app, MenuContext ctx);
void wifi_menu_select(App* app, MenuContext ctx, uint32_t index);

/* Allocates and wires the custom rounded-list network view (draw/input
   callbacks + context) - called once from app_alloc(). Ownership of the
   returned View stays with this module (freed via
   wifi_network_list_view_free()); main.c just adds/removes it from the
   ViewDispatcher by ID. */
View* wifi_network_list_view_alloc(App* app);
void wifi_network_list_view_free(View* view);

/* Same pattern as the network list above, but for WIFISCANSTA results -
   backs the WifiAttacks "Select Station" / targeted-deauth flow. */
View* wifi_station_list_view_alloc(App* app);
void wifi_station_list_view_free(View* view);

/* Called from main.c's text_input_result_callback() when
   app->text_input_purpose == TextInputPurposePassword. */
void wifi_password_submitted(App* app);

/* Called from main.c's text_input_result_callback() when
   app->text_input_purpose == TextInputPurposeBeaconSsids - sends
   WIFIATTACK:BEACON:<comma-separated ssids from app->text_input_buffer>. */
void wifi_beacon_custom_submitted(App* app);
