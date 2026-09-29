#include "../tpms_app_i.h"
#include "../views/tpms_receiver.h"
#include "../protocols/tpms_pack.h"

void tpms_scene_receiver_info_callback(TPMSCustomEvent event, void* context) {
    furi_assert(context);
    TPMSApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

static void tpms_scene_receiver_info_tpms_action(TPMSReceiverInfoAction action, void* context) {
    furi_assert(context);
    TPMSApp* app = context;
    switch(action) {
    case TPMSReceiverInfoActionEdit: {
        TPMSField field = tpms_view_receiver_info_get_selected_field(app->tpms_receiver_info);
        app->tpms_edit_field = field;
        if(field == TPMSFieldPressure || field == TPMSFieldTemperature ||
           field == TPMSFieldId) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher,
                (uint32_t)(field == TPMSFieldId ?
                               TPMSCustomEventTpmsEditId :
                               (field == TPMSFieldTemperature ?
                                    TPMSCustomEventTpmsEditTemperature :
                                    TPMSCustomEventTpmsEditPressure)));
        }
        break;
    }
    case TPMSReceiverInfoActionToggleBattery:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, TPMSCustomEventTpmsToggleBattery);
        break;
    }
}

static void tpms_scene_receiver_info_toggle_battery(TPMSApp* app) {
    TPMSBlockGeneric generic = {0};
    const char* protocol_name =
        tpms_history_get_protocol_name(app->txrx->history, app->txrx->idx_menu_chosen);
    generic.protocol_name = protocol_name;
    FlipperFormat* fff = tpms_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen);
    if(!fff || tpms_block_generic_deserialize(&generic, fff) != SubGhzProtocolStatusOk) {
        return;
    }
    generic.protocol_name = protocol_name;

    if(generic.battery_low == TPMS_NO_BATT) {
        generic.battery_low = 0;
    } else if(generic.battery_low == 0) {
        generic.battery_low = 1;
    } else {
        generic.battery_low = TPMS_NO_BATT;
    }

    tpms_pack(protocol_name, &generic);
    if(tpms_history_replace_payload(app->txrx->history, app->txrx->idx_menu_chosen, &generic)) {
        tpms_view_receiver_info_update(
            app->tpms_receiver_info,
            tpms_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen));
    }
}

static void tpms_scene_receiver_info_add_to_history_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    furi_assert(context);
    TPMSApp* app = context;

    if(tpms_history_add_to_history(app->txrx->history, decoder_base, app->txrx->preset) ==
       TPMSHistoryStateAddKeyUpdateData) {
        tpms_view_receiver_info_update(
            app->tpms_receiver_info,
            tpms_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen));
        subghz_receiver_reset(receiver);

        notification_message(app->notifications, &sequence_blink_green_10);
        app->txrx->rx_key_state = TPMSRxKeyStateAddKey;
    }
}

void tpms_scene_receiver_info_on_enter(void* context) {
    TPMSApp* app = context;

    subghz_receiver_set_rx_callback(
        app->txrx->receiver, tpms_scene_receiver_info_add_to_history_callback, app);
    tpms_view_receiver_info_set_callback(
        app->tpms_receiver_info, tpms_scene_receiver_info_tpms_action, app);
    tpms_view_receiver_info_update(
        app->tpms_receiver_info,
        tpms_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen));
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewReceiverInfo);
}

bool tpms_scene_receiver_info_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == TPMSCustomEventTpmsEditPressure ||
           event.event == TPMSCustomEventTpmsEditTemperature ||
           event.event == TPMSCustomEventTpmsEditId) {
            scene_manager_next_scene(app->scene_manager, TPMSSceneTpmsEdit);
            consumed = true;
        } else if(event.event == TPMSCustomEventTpmsToggleBattery) {
            tpms_scene_receiver_info_toggle_battery(app);
            consumed = true;
        }
    }
    return consumed;
}

void tpms_scene_receiver_info_on_exit(void* context) {
    UNUSED(context);
}
