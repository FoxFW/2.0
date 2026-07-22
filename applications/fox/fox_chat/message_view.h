#pragma once

#include "app.h"

/* Backs FoxCommanderViewMessage - two related screens sharing one
   custom View rather than a Widget (see app.h's comment on that view
   for why this replaced the original widget_add_button_element()
   layout: two Widget buttons - Left "Settings" + Center "Retry" -
   rendered overlapping on real hardware, and Widget's per-physical-key
   button model can't bind both Right and Ok to the same "Retry"
   action, which is what the not-detected screen needed. A custom View
   gives full control over both).

   - "Detecting..." (message_view_detecting = true) - shown the instant
     a probe is about to start, before any blocking UART call, so the
     screen has real content instead of a frozen last splash frame or a
     stale previous screen for the ~0-3s a probe can take. No buttons;
     Back exits the app (same as everywhere else this view is used),
     Up/Down/Left/Right/Ok are all no-ops.
   - "ESP32 not detected" (message_view_detecting = false) - shown if
     that probe fails. Bottom row: "< Settings" (Left key) / "Retry >"
     (Right key) - Ok is also wired to Retry, so either Right or Ok
     re-probes, matching how most of this app's other OK-to-confirm
     screens behave. Up/Down are no-ops; Back exits the app entirely
     (there's nothing useful to do in this app without a working ESP32
     link). */

View* message_view_alloc(App* app);
void message_view_free(View* view);

/* Switches to the message view in "Detecting..." mode. Call this
   immediately before starting any blocking UART probe - see
   action_check_esp32()/app_probe_uart_selected() in main.c. */
void message_view_show_detecting(App* app);

/* Switches to the message view in "ESP32 not detected" mode. */
void message_view_show_not_detected(App* app);

/* Switches to the message view in "WiFi not connected" mode. Shown
   after a successful ESP32 detection when [WIFI/STATUS] comes back
   false - the user needs to connect WiFi (e.g. via Fox Commander)
   before the chat can work. Retry re-checks WiFi without re-probing
   the ESP32. */
void message_view_show_wifi_not_connected(App* app);
