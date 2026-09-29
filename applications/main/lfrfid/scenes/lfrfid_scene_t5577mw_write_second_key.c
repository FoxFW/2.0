#include "../lfrfid_i.h"
#include "../protocols/t5577_multiwriter_em41xx.h"

void lfrfid_scene_t5577mw_write_second_key_on_enter(void* context) {
    LfRfid* app = context;
    Popup* popup = app->popup;

    popup_set_header(popup, "Writing", 89, 30, AlignCenter, AlignTop);
    if(!furi_string_empty(app->file_name)) {
        popup_set_text(popup, furi_string_get_cstr(app->file_name), 89, 43, AlignCenter, AlignTop);
    } else {
        popup_set_text(
            popup,
            protocol_dict_get_name(app->dict, app->protocol_id),
            89,
            43,
            AlignCenter,
            AlignTop);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, LfRfidViewPopup);
    notification_message(app->notifications, &sequence_blink_start_magenta);

    size_t size = protocol_dict_get_data_size(app->dict, app->protocol_id);

    uint8_t* data = (uint8_t*)malloc(size);
    protocol_dict_get_data(app->dict, app->protocol_id, data, size);

    uint64_t key = t5577mw_bytes2num(data, size);
    free(data);
    LFRFIDT5577 data_to_write = {0};

    t5577mw_add_em41xx_data(&data_to_write, key, 3);
    t5577mw_set_em41xx_config(&data_to_write, 2);

    t5577_write_with_mask(&data_to_write, 0, 0, 0);

    view_dispatcher_send_custom_event(app->view_dispatcher, LfRfidEventWriteOK);
}

bool lfrfid_scene_t5577mw_write_second_key_on_event(void* context, SceneManagerEvent event) {
    LfRfid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == LfRfidEventWriteOK) {
            notification_message(app->notifications, &sequence_success);
            scene_manager_next_scene(app->scene_manager, LfRfidSceneT5577MwWriteSuccess);
            consumed = true;
        }
    }

    return consumed;
}

void lfrfid_scene_t5577mw_write_second_key_on_exit(void* context) {
    LfRfid* app = context;
    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);

    size_t size = protocol_dict_get_data_size(app->dict, app->protocol_id);
    protocol_dict_set_data(app->dict, app->protocol_id, app->old_key_data, size);
}
