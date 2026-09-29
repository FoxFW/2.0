#include <furi_hal.h>
#include <furi/core/memmgr.h>
#include <gui/gui_i.h>
#include <stdio.h>

#include "../desktop_i.h"

#define DesktopLowRamEventDone 0x00FF00F1
#define LOW_RAM_POPUP_TIMEOUT_MS 5000

static char s_low_ram_text[64];

static void desktop_scene_low_ram_callback(void* context) {
    Desktop* desktop = (Desktop*)context;
    view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopLowRamEventDone);
}

void desktop_scene_low_ram_on_enter(void* context) {
    Desktop* desktop = (Desktop*)context;

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_set_lockdown(gui, true);
    furi_record_close(RECORD_GUI);

    snprintf(
        s_low_ram_text,
        sizeof(s_low_ram_text),
        "(%zu KB free)\nCrash avoided by\nrestarting services",
        memmgr_get_free_heap() / 1024);

    Popup* popup = desktop->popup;
    popup_set_context(popup, desktop);
    popup_set_header(
        popup,
        "RAM usage exceeds\nsafe limits",
        64,
        14 + STATUS_BAR_Y_SHIFT,
        AlignCenter,
        AlignCenter);
    popup_set_text(
        popup, s_low_ram_text, 64, 37 + STATUS_BAR_Y_SHIFT, AlignCenter, AlignCenter);
    popup_set_callback(popup, desktop_scene_low_ram_callback);
    popup_set_timeout(popup, LOW_RAM_POPUP_TIMEOUT_MS);
    popup_enable_timeout(popup);

    view_dispatcher_switch_to_view(desktop->view_dispatcher, DesktopViewIdPopup);
}

bool desktop_scene_low_ram_on_event(void* context, SceneManagerEvent event) {
    Desktop* desktop = (Desktop*)context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DesktopLowRamEventDone) {

            if(!desktop->locked) {
                Gui* gui = furi_record_open(RECORD_GUI);
                gui_set_lockdown(gui, false);
                furi_record_close(RECORD_GUI);
            }
            scene_manager_previous_scene(desktop->scene_manager);
            consumed = true;
        }
    }

    return consumed;
}

void desktop_scene_low_ram_on_exit(void* context) {
    Desktop* desktop = (Desktop*)context;
    popup_reset(desktop->popup);
}
