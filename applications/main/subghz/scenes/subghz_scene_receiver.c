#include "../subghz_i.h"
#include <lib/subghz/protocols/bin_raw.h>
#include <toolbox/name_generator.h>

#define TAG "SubGhzSceneReceiver"

const NotificationSequence subghz_sequence_rx = {
    &message_green_255,

    &message_display_backlight_on,

    &message_vibro_on,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,

    &message_delay_50,
    NULL,
};

const NotificationSequence subghz_sequence_rx_locked = {
    &message_green_255,

    &message_display_backlight_on,

    &message_vibro_on,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,

    &message_delay_500,

    &message_display_backlight_off,
    NULL,
};

static void subghz_scene_receiver_update_statusbar(void* context) {
    SubGhz* subghz = context;
    FuriString* history_stat_str = furi_string_alloc();
    if(!subghz_history_get_text_space_left(subghz->history, history_stat_str)) {
        FuriString* frequency_str = furi_string_alloc();
        FuriString* modulation_str = furi_string_alloc();

#ifdef SUBGHZ_EXT_PRESET_NAME
        if(subghz_history_get_last_index(subghz->history) > 0) {
            subghz_txrx_get_frequency_and_modulation(
                subghz->txrx, frequency_str, modulation_str, false);
        } else {
            FuriString* temp_str = furi_string_alloc();

            subghz_txrx_get_frequency_and_modulation(subghz->txrx, frequency_str, temp_str, true);
            furi_string_printf(
                modulation_str,
                "%s        Mod: %s",
                (subghz_txrx_radio_device_get(subghz->txrx) == SubGhzRadioDeviceTypeInternal) ?
                    "Int" :
                    "Ext",
                furi_string_get_cstr(temp_str));
            furi_string_free(temp_str);
        }
#else
        subghz_txrx_get_frequency_and_modulation(
            subghz->txrx, frequency_str, modulation_str, false);
#endif

        subghz_view_receiver_add_data_statusbar(
            subghz->subghz_receiver,
            furi_string_get_cstr(frequency_str),
            furi_string_get_cstr(modulation_str),
            furi_string_get_cstr(history_stat_str),
            subghz_txrx_hopper_get_state(subghz->txrx) != SubGhzHopperStateOFF,
            READ_BIT(subghz->filter, SubGhzProtocolFlag_BinRAW) > 0);

        furi_string_free(frequency_str);
        furi_string_free(modulation_str);
    } else {
        subghz_view_receiver_add_data_statusbar(
            subghz->subghz_receiver,
            furi_string_get_cstr(history_stat_str),
            "",
            "",
            subghz_txrx_hopper_get_state(subghz->txrx) != SubGhzHopperStateOFF,
            READ_BIT(subghz->filter, SubGhzProtocolFlag_BinRAW) > 0);
        subghz->state_notifications = SubGhzNotificationStateIDLE;
    }
    furi_string_free(history_stat_str);

    subghz_view_receiver_set_radio_device_type(
        subghz->subghz_receiver, subghz_txrx_radio_device_get(subghz->txrx));
}

void subghz_scene_receiver_callback(SubGhzCustomEvent event, void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, event);
}

static void subghz_scene_receiver_process_auto_save(SubGhz* subghz) {
    uint16_t idx = 0;
    while(subghz_history_find_auto_save_pending(subghz->history, &idx)) {
        FlipperFormat* ff = subghz_history_get_raw_data(subghz->history, idx);
        bool success = false;
        if(ff) {
            FuriString* protocol_name = furi_string_alloc();
            flipper_format_rewind(ff);
            if(!flipper_format_read_string(ff, "Protocol", protocol_name)) {
                furi_string_set(protocol_name, "Unknown");
            }

            char file_name_buf[SUBGHZ_MAX_LEN_NAME] = {0};
            name_generator_make_auto_datetime(
                file_name_buf, SUBGHZ_MAX_LEN_NAME, furi_string_get_cstr(protocol_name), NULL);
            furi_string_free(protocol_name);

            if(subghz->last_settings->file_prefix[0] != '\0') {

                char tmp[SUBGHZ_MAX_LEN_NAME + sizeof(subghz->last_settings->file_prefix)];
                snprintf(
                    tmp, sizeof(tmp), "%s%s", subghz->last_settings->file_prefix, file_name_buf);
                strncpy(file_name_buf, tmp, SUBGHZ_MAX_LEN_NAME - 1);
                file_name_buf[SUBGHZ_MAX_LEN_NAME - 1] = '\0';
            }

            FuriString* file_path = furi_string_alloc();
            furi_string_set(file_path, SUBGHZ_APP_FOLDER);
            furi_string_cat_printf(
                file_path, "/%s%s", file_name_buf, SUBGHZ_APP_FILENAME_EXTENSION);

            success = subghz_save_protocol_to_file(subghz, ff, furi_string_get_cstr(file_path));
            furi_string_free(file_path);
        }

        if(success) {
            FURI_LOG_I(TAG, "Auto-saved history item %u", idx);
            notification_message(subghz->notifications, &sequence_double_vibro);
        } else {
            FURI_LOG_E(TAG, "Auto-save failed for history item %u", idx);
            notification_message(subghz->notifications, &sequence_error);
        }
        subghz_history_set_auto_save_pending(subghz->history, idx, false);
    }
}

static void subghz_scene_add_to_history_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    furi_assert(context);
    SubGhz* subghz = context;

    if((decoder_base->protocol->flag & subghz->ignore_filter) == 0) {
        SubGhzHistory* history = subghz->history;
        FuriString* item_name = furi_string_alloc();
        FuriString* item_time = furi_string_alloc();
        uint16_t idx = subghz_history_get_item(history);

        SubGhzRadioPreset preset = subghz_txrx_get_preset(subghz->txrx);
        if(subghz->last_settings->delete_old_signals) {
            if(subghz_history_get_last_index(subghz->history) >= 54) {
                subghz->state_notifications = SubGhzNotificationStateRx;

                subghz_view_receiver_disable_draw_callback(subghz->subghz_receiver);

                subghz_history_delete_item(subghz->history, 0);
                subghz_view_receiver_delete_item(subghz->subghz_receiver, 0);
                subghz_view_receiver_enable_draw_callback(subghz->subghz_receiver);

                subghz_scene_receiver_update_statusbar(subghz);
                subghz->idx_menu_chosen =
                    subghz_view_receiver_get_idx_menu(subghz->subghz_receiver);
                idx--;
            }
        }
        if(subghz_history_add_to_history(history, decoder_base, &preset)) {
            furi_string_reset(item_name);
            furi_string_reset(item_time);

            subghz->state_notifications = SubGhzNotificationStateRxDone;

            subghz_history_get_text_item_menu(history, item_name, idx);
            subghz_history_get_time_item_menu(history, item_time, idx);
            subghz_view_receiver_add_item_to_menu(
                subghz->subghz_receiver,
                furi_string_get_cstr(item_name),
                furi_string_get_cstr(item_time),
                subghz_history_get_type_protocol(history, idx));

            subghz_scene_receiver_update_statusbar(subghz);
            if(subghz_history_get_text_space_left(subghz->history, NULL)) {
                notification_message(subghz->notifications, &sequence_error);
            }
            subghz_rx_key_state_set(subghz, SubGhzRxKeyStateAddKey);
            if(subghz->last_settings->auto_save) {

                subghz_history_set_auto_save_pending(history, idx, true);
            }
        }
        subghz_receiver_reset(receiver);
        furi_string_free(item_name);
        furi_string_free(item_time);
    } else {
        FURI_LOG_D(TAG, "%s protocol ignored", decoder_base->protocol->name);
    }
}

void subghz_scene_receiver_on_enter(void* context) {
    SubGhz* subghz = context;
    SubGhzHistory* history = subghz->history;

    FuriString* item_name = furi_string_alloc();
    FuriString* item_time = furi_string_alloc();

    if(subghz_rx_key_state_get(subghz) == SubGhzRxKeyStateIDLE) {
        subghz_txrx_set_preset_internal(
            subghz->txrx,
            subghz->last_settings->frequency,
            subghz->last_settings->preset_index,
            subghz->last_settings->tx_power);

        subghz->filter = subghz->last_settings->filter;
        subghz_txrx_receiver_set_filter(subghz->txrx, subghz->filter);
        subghz->ignore_filter = subghz->last_settings->ignore_filter;
        subghz->tx_power = subghz->last_settings->tx_power;

        subghz_history_reset(history);
        subghz_rx_key_state_set(subghz, SubGhzRxKeyStateStart);
        subghz->idx_menu_chosen = 0;
    }

    subghz_view_receiver_set_mode(subghz->subghz_receiver, SubGhzViewReceiverModeLive);

    subghz_view_receiver_exit(subghz->subghz_receiver);
    for(uint16_t i = 0; i < subghz_history_get_item(history); i++) {
        furi_string_reset(item_name);
        furi_string_reset(item_time);
        subghz_history_get_text_item_menu(history, item_name, i);
        subghz_history_get_time_item_menu(history, item_time, i);
        subghz_view_receiver_add_item_to_menu(
            subghz->subghz_receiver,
            furi_string_get_cstr(item_name),
            furi_string_get_cstr(item_time),
            subghz_history_get_type_protocol(history, i));
        subghz_rx_key_state_set(subghz, SubGhzRxKeyStateAddKey);
    }
    furi_string_free(item_name);
    furi_string_free(item_time);

    subghz_view_receiver_set_callback(
        subghz->subghz_receiver, subghz_scene_receiver_callback, subghz);
    subghz_txrx_set_rx_callback(subghz->txrx, subghz_scene_add_to_history_callback, subghz);

    if(!subghz_history_get_text_space_left(subghz->history, NULL)) {
        subghz->state_notifications = SubGhzNotificationStateRx;
    }

    if(subghz->last_settings->enable_hopping) {
        subghz_txrx_hopper_set_state(subghz->txrx, SubGhzHopperStateRunning);
    } else {
        subghz_txrx_hopper_set_state(subghz->txrx, SubGhzHopperStateOFF);
    }

    if(subghz->last_settings->enable_preset_hopping) {
        subghz_txrx_preset_hopper_set_state(subghz->txrx, SubGhzPresetHopperStateRunning);
    } else {
        subghz_txrx_preset_hopper_set_state(subghz->txrx, SubGhzPresetHopperStateOFF);
    }

    subghz_txrx_rx_start(subghz->txrx);
    subghz_view_receiver_set_idx_menu(subghz->subghz_receiver, subghz->idx_menu_chosen);

    furi_check(
        subghz_txrx_load_decoder_by_name_protocol(subghz->txrx, SUBGHZ_PROTOCOL_BIN_RAW_NAME));

    subghz_scene_receiver_update_statusbar(subghz);

    subghz_view_receiver_set_lock(subghz->subghz_receiver, subghz_is_locked(subghz));

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdReceiver);
}

bool subghz_scene_receiver_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {

        subghz->idx_menu_chosen = subghz_view_receiver_get_idx_menu(subghz->subghz_receiver);

        switch(event.event) {
        case SubGhzCustomEventViewReceiverBack:

            subghz->state_notifications = SubGhzNotificationStateIDLE;
            subghz_txrx_stop(subghz->txrx);
            subghz_txrx_hopper_set_state(subghz->txrx, SubGhzHopperStateOFF);
            subghz_txrx_preset_hopper_set_state(subghz->txrx, SubGhzPresetHopperStateOFF);
            subghz_txrx_set_rx_callback(subghz->txrx, NULL, subghz);

            if(subghz_rx_key_state_get(subghz) == SubGhzRxKeyStateAddKey) {
                subghz_rx_key_state_set(subghz, SubGhzRxKeyStateExit);
                scene_manager_next_scene(subghz->scene_manager, SubGhzSceneNeedSaving);
            } else {
                subghz_rx_key_state_set(subghz, SubGhzRxKeyStateIDLE);
                subghz_txrx_set_default_preset(subghz->txrx, subghz->last_settings->frequency);
                scene_manager_search_and_switch_to_previous_scene(
                    subghz->scene_manager, SubGhzSceneStart);
            }
            consumed = true;
            break;
        case SubGhzCustomEventViewReceiverOK:

            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneReceiverInfo);
            consumed = true;
            break;
        case SubGhzCustomEventViewReceiverDeleteItem:
            subghz->state_notifications = SubGhzNotificationStateRx;

            subghz_view_receiver_disable_draw_callback(subghz->subghz_receiver);

            subghz_history_delete_item(subghz->history, subghz->idx_menu_chosen);
            subghz_view_receiver_delete_item(
                subghz->subghz_receiver,
                subghz_view_receiver_get_idx_menu(subghz->subghz_receiver));
            subghz_view_receiver_enable_draw_callback(subghz->subghz_receiver);

            subghz_scene_receiver_update_statusbar(subghz);
            subghz->idx_menu_chosen = subghz_view_receiver_get_idx_menu(subghz->subghz_receiver);
            if(subghz_history_get_last_index(subghz->history) == 0) {
                subghz_rx_key_state_set(subghz, SubGhzRxKeyStateStart);
            }
            consumed = true;
            break;
        case SubGhzCustomEventViewReceiverConfig:

            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzViewIdReceiver, SubGhzCustomEventManagerSet);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneReceiverConfig);
            consumed = true;
            break;
        case SubGhzCustomEventViewReceiverOffDisplay:
            notification_message(subghz->notifications, &sequence_display_backlight_off);
            consumed = true;
            break;
        case SubGhzCustomEventViewReceiverUnlock:
            subghz_unlock(subghz);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(subghz->last_settings->auto_save) {
            subghz_scene_receiver_process_auto_save(subghz);
        }
        if(subghz_txrx_hopper_get_state(subghz->txrx) != SubGhzHopperStateOFF) {
            subghz_txrx_hopper_update(subghz->txrx, subghz->last_settings->hopping_threshold);
            subghz_scene_receiver_update_statusbar(subghz);
        }
        if(subghz_txrx_preset_hopper_get_state(subghz->txrx) != SubGhzPresetHopperStateOFF) {
            subghz_txrx_preset_hopper_update(subghz->txrx, subghz->last_settings->preset_hopping_threshold);
            subghz_scene_receiver_update_statusbar(subghz);
        }

        SubGhzThresholdRssiData ret_rssi = subghz_threshold_get_rssi_data(
            subghz->threshold_rssi, subghz_txrx_radio_device_get_rssi(subghz->txrx));

        subghz_receiver_rssi(subghz->subghz_receiver, ret_rssi.rssi);
        subghz_protocol_decoder_bin_raw_data_input_rssi(
            (SubGhzProtocolDecoderBinRAW*)subghz_txrx_get_decoder(subghz->txrx), ret_rssi.rssi);

        switch(subghz->state_notifications) {
        case SubGhzNotificationStateRx:
            notification_message(subghz->notifications, &sequence_blink_cyan_10);
            break;
        case SubGhzNotificationStateRxDone:
            if(!subghz_is_locked(subghz)) {
                notification_message(subghz->notifications, &subghz_sequence_rx);
            } else {
                notification_message(subghz->notifications, &subghz_sequence_rx_locked);
            }
            subghz->state_notifications = SubGhzNotificationStateRx;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void subghz_scene_receiver_on_exit(void* context) {
    UNUSED(context);
}
