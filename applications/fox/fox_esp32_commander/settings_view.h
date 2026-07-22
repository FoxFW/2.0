#pragma once

#include "app.h"

/* Settings - two arrow-adjustable rows (Attacks ON/OFF, Expert Mode
   ON/OFF), same rounded-row/arrow visual pattern as fox_chameleon's
   settings_draw_cb/settings_input_cb, minus the "Start" action row
   fox_chameleon needed (that screen was configuring a connection that
   didn't exist yet; this one only exists after the ESP32 link is
   already up, so there's nothing to start - toggling applies
   immediately). Up/Down moves between rows, Left/Right toggles
   whichever row is currently selected. */

View* settings_view_alloc(App* app);
void settings_view_free(View* view);

/* Queries the firmware's real persisted value (SETTINGS) and updates
   app->attacks_enabled - called every time Settings is entered, so this
   never drifts from what's actually stored in the firmware's NVS. Does
   not touch app->expert_mode - that's app_expert_mode_load()'s job
   below, since it's a local file, not something the firmware knows
   about. */
void settings_view_refresh(App* app);

/* Loads app->expert_mode from
   /ext/apps_data/fox_esp32_commander/expert_mode.txt (defaults to false
   if the file doesn't exist yet - a fresh device/first run). Call once,
   from app_alloc(), before the main menu is ever rendered - unlike
   attacks_enabled, this isn't refreshed from anywhere else, so it only
   needs loading the one time. */
void app_expert_mode_load(App* app);
