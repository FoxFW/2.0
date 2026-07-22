#pragma once

#include "app.h"

/* Backs FoxCommanderViewMessage - two screens sharing one custom View
   rather than a Widget (see app.h's comment on that view for why this
   replaced the original widget_add_button_element() layout: two Widget
   buttons - Left "Settings" + Center "Retry" - rendered overlapping on
   real hardware, and Widget's per-physical-key button model can't bind
   both Right and Ok to the same "Retry" action, which is what the
   not-detected screen needed. A custom View gives full control.)

   - "Detecting..." (message_view_detecting = true) - shown the instant
     a probe is about to start, before any blocking UART call, so the
     screen has real content instead of a frozen last splash frame or a
     stale previous screen for the ~0-3s a probe can take. No buttons;
     Back exits the app, Up/Down/Left/Right/Ok are all no-ops.
   - "ESP32 not detected" (message_view_detecting = false) - shown if
     the probe fails. Bottom row: "< Settings" (Left key) / "Retry >"
     (Right key).

   Fox Portal does NOT have a "WiFi not connected" state - the captive
   portal itself provides WiFi (it's a soft-AP), so WiFi does not need
   to be pre-connected before opening the app. */

View* message_view_alloc(App* app);
void message_view_free(View* view);

/* Switches to the message view in "Detecting..." mode. Call this
   immediately before starting any blocking UART probe - see
   action_check_esp32()/app_probe_uart_selected() in main.c. */
void message_view_show_detecting(App* app);

/* Switches to the message view in "ESP32 not detected" mode. */
void message_view_show_not_detected(App* app);

