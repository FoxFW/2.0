#pragma once

#include "app.h"

/* Manual pin/baud override + Retry - reached via the "Settings" button
   on the not-detected Message screen (see main.c's
   show_not_detected_message()). Mirrors fox_chameleon's own
   Pins/Baud/Start settings_draw_cb/settings_input_cb pattern: Up/Down
   moves the selected row, Left/Right adjusts Pins/Baud, OK on the
   Start row re-probes with exactly the selected pair (see app.h's
   app_probe_uart_selected()) rather than looping over both pairs the
   way the automatic Retry button does. */

View* connect_settings_view_alloc(App* app);
void connect_settings_view_free(View* view);

/* Resets the row selection to Pins each time the screen is entered -
   same "always start from a known state" approach as
   settings_view_refresh() takes for the main Settings screen. */
void connect_settings_view_reset(App* app);
