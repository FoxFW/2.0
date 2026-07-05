#include "../lfrfid_i.h"
#include <lfrfid_icons.h>  /* FAP-local icons — generated from images/ by fbt, not in lib path */

void lfrfid_scene_emulate_on_enter(void* context) {
    LfRfid* app = context;
    Widget* widget = app->widget;

    FuriString* display_text = furi_string_alloc_set("\e#Emulating\e#\n");

    furi_string_cat_printf(
        display_text,
        "[%s]\n%s",
        protocol_dict_get_name(app->dict, app->protocol_id),
        furi_string_empty(app->file_name) ? "Unsaved Tag" : furi_string_get_cstr(app->file_name));

    // Fox logo on the left (64px wide), text on the right
    widget_add_icon_element(widget, 0, 0, &I_fox_64x64);
    widget_add_text_box_element(
        widget, 66, 4, 60, 56, AlignCenter, AlignCenter, furi_string_get_cstr(display_text), false);

    furi_string_free(display_text);

    lfrfid_worker_start_thread(app->lfworker);
    lfrfid_worker_emulate_start(app->lfworker, (LFRFIDProtocol)app->protocol_id);
    notification_message(app->notifications, &sequence_blink_start_magenta);

    view_dispatcher_switch_to_view(app->view_dispatcher, LfRfidViewWidget);
}

bool lfrfid_scene_emulate_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void lfrfid_scene_emulate_on_exit(void* context) {
    LfRfid* app = context;
    notification_message(app->notifications, &sequence_blink_stop);
    widget_reset(app->widget);
    lfrfid_worker_stop(app->lfworker);
    lfrfid_worker_stop_thread(app->lfworker);
}
