#pragma once

#include "app.h"

/* Bluetooth menu = BLE spam (BLESPAM:IOS/WINDOWS/SAMSUNG/ANDROID/ALL) -
   the BLE bridge to a Chameleon Ultra (BLEINIT/BLESCAN/BLECONN/etc) is
   fox_chameleon's job, not this app's; see app.h's top comment. Not
   available when the connected board is an ESP32-S2 build of the
   firmware (no Bluetooth radio) - this whole menu is hidden from the
   main menu on a confirmed no-BLE board (see app->has_ble / the CAPS
   query in main.c's app_probe_uart()), so this module never actually
   runs there in practice. The firmware's own per-command
   "ERROR:Incompatible ESP32-S2 Module has no BLE" reply (same as every
   other BLE* command on that target) is still the fallback if this
   module somehow got reached anyway - e.g. an older Commander build
   talking to a newer firmware, or vice versa. */

void ble_render_menu(App* app);
void ble_menu_select(App* app, uint32_t index);
