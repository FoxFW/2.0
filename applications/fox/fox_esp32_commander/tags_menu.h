#pragma once

#include "app.h"

/* Tag Detection menu - snapshot BLE scans for nearby item trackers by
   advertisement signature (BLETAGSCAN:FINDMY/SMARTTAG/TILE on the
   firmware side, see ble_tags.cpp there for exactly what's matched and
   the honesty caveats around it). Each item runs one bounded scan and
   reports whatever TAG: lines came back to the Terminal, same
   log-and-done shape as Signal Monitor/Packet Count in the WiFi Recon
   menu - there's nothing to select afterward, so no dedicated results
   screen is needed. Not available on an ESP32-S2 board (no BLE radio) -
   this whole menu is hidden from the main menu on a confirmed no-BLE
   board (see app->has_ble / the CAPS query in main.c's
   app_probe_uart()), so this module never actually runs there in
   practice. The firmware's own per-command
   "ERROR:Incompatible ESP32-S2 Module has no BLE" reply (same as every
   other BLE* command on that target) is still the fallback if this
   module somehow got reached anyway - e.g. an older Commander build
   talking to a newer firmware, or vice versa. */

void tags_render_menu(App* app);
void tags_menu_select(App* app, uint32_t index);
