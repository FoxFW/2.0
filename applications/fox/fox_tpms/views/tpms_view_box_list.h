#pragma once
#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Full-width double-row box list - same visual as SubGhz's own Mode Picker
 * screen (applications/main/subghz/views/subghz_view_mode_picker.c, itself
 * "same box geometry as fox_update_downloader/view_menu.c" per that file's
 * own comment) and Garage's Protocol Groups list. Unlike those two, the
 * option list here isn't a compile-time const array baked into this file -
 * tpms_box_list_set_options() below points it at whichever array the
 * current scene wants (the Start menu's 3 rows, or the Vehicle Make
 * picker's 5), so one view instance (allocated once in tpms_app_alloc(),
 * like TPMSViewSubmenu/TPMSViewWidget) can be reused across both scenes
 * instead of needing a second near-identical view type. */

typedef struct TPMSBoxList TPMSBoxList;

typedef void (*TPMSBoxListCallback)(void* context, uint32_t index);

typedef struct {
    const char* title; /* top row of the box */
    const char* subtitle; /* bottom row of the box */
} TPMSBoxListOption;

TPMSBoxList* tpms_box_list_alloc(void);
void tpms_box_list_free(TPMSBoxList* instance);
View* tpms_box_list_get_view(TPMSBoxList* instance);
void tpms_box_list_set_callback(
    TPMSBoxList* instance,
    TPMSBoxListCallback callback,
    void* context);

/* `options` must stay valid for as long as this view might redraw - both
 * call sites (tpms_scene_start.c, tpms_scene_vehicle_make.c) pass a static
 * const array, so that's always true here. Resets the cursor to 0. */
void tpms_box_list_set_options(
    TPMSBoxList* instance,
    const TPMSBoxListOption* options,
    uint8_t count);

void tpms_box_list_set_selected(TPMSBoxList* instance, uint8_t index);

/* Starts/stops the horizontal scroll timer for the cursor row's title/
 * subtitle (only the cursor row scrolls - a static row scrolling unread
 * text underneath it is just noise, same call as Garage's Protocol Groups
 * list makes). Since this one view instance is shared by two scenes
 * (tpms_scene_start.c, tpms_scene_vehicle_make.c), each scene's on_enter/
 * on_exit must call resume/pause respectively - starting the timer once at
 * alloc time would leave it ticking in the background, calling
 * view_port_update() on a view that isn't even the one on screen, for
 * every scene visited afterward. */
void tpms_box_list_resume_scroll(TPMSBoxList* instance);
void tpms_box_list_pause_scroll(TPMSBoxList* instance);

#ifdef __cplusplus
}
#endif
