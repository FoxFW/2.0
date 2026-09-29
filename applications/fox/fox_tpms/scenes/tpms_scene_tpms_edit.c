#include "../tpms_app_i.h"
#include "../protocols/tpms_pack.h"

#define TAG "TPMSSceneTpmsEdit"

static void tpms_scene_tpms_edit_number_callback(void* context, int32_t number) {
    furi_assert(context);
    TPMSApp* app = context;
    app->tpms_edit_number_value = number;
    view_dispatcher_send_custom_event(app->view_dispatcher, TPMSCustomEventNumberInputDone);
}

static void tpms_scene_tpms_edit_byte_callback(void* context) {
    furi_assert(context);
    TPMSApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, TPMSCustomEventByteInputDone);
}

static bool tpms_scene_tpms_edit_load_generic(TPMSApp* app, TPMSBlockGeneric* generic) {
    FlipperFormat* fff = tpms_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen);
    if(!fff) {
        FURI_LOG_E(TAG, "No history fff");
        return false;
    }
    return tpms_block_generic_deserialize(generic, fff) == SubGhzProtocolStatusOk;
}

void tpms_scene_tpms_edit_on_enter(void* context) {
    TPMSApp* app = context;
    TPMSBlockGeneric generic = {0};
    generic.protocol_name =
        tpms_history_get_protocol_name(app->txrx->history, app->txrx->idx_menu_chosen);
    if(!tpms_scene_tpms_edit_load_generic(app, &generic)) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    switch(app->tpms_edit_field) {
    case TPMSFieldPressure: {

        int32_t init = (int32_t)(generic.pressure * 10.0f + 0.5f);
        number_input_set_header_text(app->number_input, "Pressure x10 (bar)");
        number_input_set_result_callback(
            app->number_input, tpms_scene_tpms_edit_number_callback, app, init, 0, 440);
        view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewNumberInput);
        break;
    }
    case TPMSFieldTemperature: {
        int32_t init = (int32_t)(generic.temperature + (generic.temperature >= 0 ? 0.5f : -0.5f));
        number_input_set_header_text(app->number_input, "Temperature (C)");
        number_input_set_result_callback(
            app->number_input, tpms_scene_tpms_edit_number_callback, app, init, -50, 205);
        view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewNumberInput);
        break;
    }
    case TPMSFieldId: {
        uint32_t id = generic.id;
        app->tpms_edit_id_bytes[0] = (id >> 24) & 0xFF;
        app->tpms_edit_id_bytes[1] = (id >> 16) & 0xFF;
        app->tpms_edit_id_bytes[2] = (id >> 8) & 0xFF;
        app->tpms_edit_id_bytes[3] = (id >> 0) & 0xFF;
        byte_input_set_header_text(app->byte_input, "Edit ID (hex)");
        byte_input_set_result_callback(
            app->byte_input,
            tpms_scene_tpms_edit_byte_callback,
            NULL,
            app,
            app->tpms_edit_id_bytes,
            sizeof(app->tpms_edit_id_bytes));
        view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewByteInput);
        break;
    }
    default:
        FURI_LOG_E(TAG, "Unsupported field for editor: %d", (int)app->tpms_edit_field);
        scene_manager_previous_scene(app->scene_manager);
        break;
    }
}

bool tpms_scene_tpms_edit_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event != TPMSCustomEventNumberInputDone &&
       event.event != TPMSCustomEventByteInputDone) {
        return false;
    }

    TPMSBlockGeneric generic = {0};
    const char* protocol_name =
        tpms_history_get_protocol_name(app->txrx->history, app->txrx->idx_menu_chosen);
    generic.protocol_name = protocol_name;
    if(!tpms_scene_tpms_edit_load_generic(app, &generic)) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    generic.protocol_name = protocol_name;

    switch(app->tpms_edit_field) {
    case TPMSFieldPressure:

        generic.pressure = (float)app->tpms_edit_number_value / 10.0f;
        tpms_pack(protocol_name, &generic);
        break;
    case TPMSFieldTemperature:
        generic.temperature = (float)app->tpms_edit_number_value;
        tpms_pack(protocol_name, &generic);
        break;
    case TPMSFieldId: {
        uint32_t id = ((uint32_t)app->tpms_edit_id_bytes[0] << 24) |
                      ((uint32_t)app->tpms_edit_id_bytes[1] << 16) |
                      ((uint32_t)app->tpms_edit_id_bytes[2] << 8) |
                      ((uint32_t)app->tpms_edit_id_bytes[3]);
        generic.id = id;
        tpms_pack(protocol_name, &generic);
        break;
    }
    default:
        break;
    }

    if(!tpms_history_replace_payload(app->txrx->history, app->txrx->idx_menu_chosen, &generic)) {
        FURI_LOG_E(TAG, "history replace failed");
    } else {
        tpms_view_receiver_info_update(
            app->tpms_receiver_info,
            tpms_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen));
    }

    scene_manager_previous_scene(app->scene_manager);
    return true;
}

void tpms_scene_tpms_edit_on_exit(void* context) {
    TPMSApp* app = context;
    number_input_set_result_callback(app->number_input, NULL, NULL, 0, 0, 0);
    number_input_set_header_text(app->number_input, "");
    byte_input_set_result_callback(app->byte_input, NULL, NULL, NULL, NULL, 0);
    byte_input_set_header_text(app->byte_input, "");
}
