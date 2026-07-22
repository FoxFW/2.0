#pragma once

#include "app.h"

/* FoxPortal - Start / Stop / Edit Input Names / Show QR, talking to the
   firmware's fox_portal.cpp module (WIFIFOXPORTAL:*) over the same line
   protocol as every other command in this app.

   apps_data/fox_portal/ on the Flipper's own SD card
   (start.html, finish.html, inputnames.txt - see FOXPORTAL_DIR in
   foxportal_menu.c) is the actual source of truth for a deployment's
   field list and page HTML, not anything stored on the ESP32. Those
   three files can be edited two ways: in-app via "Edit Input Names"
   (fields only - the two HTML files are edit-by-hand only, there's no
   in-app HTML editor), or directly on a PC by pulling the SD card,
   editing the files, and putting the card back - both end up in the
   same place, since Start always re-reads all three files fresh and
   pushes them to the firmware (FIELDS, then SETPAGE:START/THANKS)
   right before sending WIFIFOXPORTAL:START. */

void foxportal_render_menu(App* app);
void foxportal_menu_select(App* app, uint32_t index);

/* Called from main.c's text_input_result_callback() for
   TextInputPurposeFoxPortalSsid. */
void foxportal_ssid_submitted(App* app);

/* Edit Input Names - "+ Add Input Name" plus one item per configured
   field, loaded fresh from inputnames.txt on every render. Selecting
   an existing field routes to the delete-confirm screen below instead
   of deleting immediately, so a stray OK press can't silently drop a
   field. */
void foxportal_fields_render_menu(App* app);
void foxportal_fields_menu_select(App* app, uint32_t index);

/* "+ Add Input Name" text entry result - validates (letters/digits/
   underscore only, matching the firmware's own FIELDS key validation),
   rejects duplicates, then appends and saves. */
void foxportal_new_field_submitted(App* app);

/* Delete-confirm - "Delete '<name>'?" with Yes/Cancel. Refuses to drop
   the last remaining field (inputnames.txt must never end up with 0
   lines - see foxportal_menu.c's foxportal_fields_load()). */
void foxportal_field_delete_render_menu(App* app);
void foxportal_field_delete_menu_select(App* app, uint32_t index);

/* Allocates the QR View - called once from app_alloc() in main.c, same
   lifecycle as every other view in this app. */
View* foxportal_qr_view_alloc(App* app);

