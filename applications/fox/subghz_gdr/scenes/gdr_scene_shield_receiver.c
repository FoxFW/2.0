// scenes/gdr_scene_shield_receiver.c
#include "../gdr_app_i.h"

#ifdef ENABLE_SHIELD_RX_SCENE

#include "../helpers/gdr_storage.h"
#include "views/gdr_receiver.h"
#include <notification/notification_messages.h>
#include <stdio.h>

#define TAG "GDRSceneShieldRx"

#define SHIELD_SCENE_STATE_NONE        0u
#define SHIELD_SCENE_STATE_TO_SUBSCENE 1u

#define SHIELD_TX_OFFSET_COUNT 12U
static const int32_t shield_tx_offset_hz[SHIELD_TX_OFFSET_COUNT] = {
    75000L,
    100000L,
    150000L,
    200000L,
    250000L,
    300000L,
    -75000L,
    -100000L,
    -150000L,
    -200000L,
    -250000L,
    -300000L,
};

static bool s_shield_devices_inited = false;

void gdr_scene_shield_receiver_view_callback(GDRCustomEvent event, void* context);

static bool gdr_scene_shield_receiver_auto_save_locked(GDRApp* app) {
    uint16_t item_count = gdr_history_get_item(app->shield_history);
    if(item_count == 0) {
        return false;
    }

    FlipperFormat* ff = gdr_history_get_raw_data(app->shield_history, item_count - 1U);
    if(!ff) {
        return false;
    }

    FuriString* protocol = furi_string_alloc();
    FuriString* saved_path = furi_string_alloc();
    if(!protocol || !saved_path) {
        if(protocol) {
            furi_string_free(protocol);
        }
        if(saved_path) {
            furi_string_free(saved_path);
        }
        return false;
    }

    flipper_format_rewind(ff);
    if(!flipper_format_read_string(ff, FF_PROTOCOL, protocol)) {
        furi_string_set_str(protocol, "Unknown");
    }
    furi_string_replace_all(protocol, "/", "_");
    furi_string_replace_all(protocol, " ", "_");

    bool saved = gdr_storage_save_capture(ff, furi_string_get_cstr(protocol), saved_path);
    furi_string_free(protocol);
    furi_string_free(saved_path);
    return saved;
}

static int32_t gdr_scene_shield_receiver_tx_offset_hz(const GDRApp* app) {
    furi_check(app);
    uint8_t index = app->shield_tx_offset_index;
    if(index >= SHIELD_TX_OFFSET_COUNT) {
        index = 3;
    }
    return shield_tx_offset_hz[index];
}

static int32_t gdr_scene_shield_receiver_resolve_tx_offset(GDRApp* app) {

    return gdr_scene_shield_receiver_tx_offset_hz(app);
}

static void gdr_scene_shield_receiver_decode_cb(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    UNUSED(receiver);
    furi_check(decoder_base);
    furi_check(context);
    GDRApp* app = context;

    if(!app->shield_history || !app->shield_history_mutex || !app->shield_rx_chain) {
        return;
    }

    furi_mutex_acquire(app->shield_history_mutex, FuriWaitForever);
    bool added = gdr_history_add_to_history(
        app->shield_history,
        decoder_base,
        &app->shield_rx_chain->preset,
        GDRHistorySourceExternal);
    bool auto_save = app->auto_save;
    bool auto_saved = added && auto_save &&
                      gdr_scene_shield_receiver_auto_save_locked(app);
    if(added && auto_save && !auto_saved) {
        app->shield_auto_save_failed = true;
    }
    furi_mutex_release(app->shield_history_mutex);

    if(added) {
        notification_message(app->notifications, &sequence_semi_success);
        if(auto_saved) {
            notification_message(app->notifications, &sequence_double_vibro);
        } else if(auto_save) {
            notification_message(app->notifications, &sequence_error);
        }
        view_dispatcher_send_custom_event(
            app->view_dispatcher, GDRCustomEventShieldReceiverUpdate);
    }
}

static void gdr_scene_shield_receiver_update_statusbar(GDRApp* app) {
    furi_check(app);

    char frequency_str[16] = {0};
    char modulation_str[24] = {0};
    char history_stat_str[16] = {0};

    if(app->shield_rx_chain) {
        snprintf(
            frequency_str,
            sizeof(frequency_str),
            "%03lu.%02lu",
            (unsigned long)((app->shield_rx_chain->frequency / 1000000UL) % 1000UL),
            (unsigned long)((app->shield_rx_chain->frequency / 10000UL) % 100UL));
        snprintf(
            modulation_str,
            sizeof(modulation_str),
            "%s %luk/%+ldk",
            furi_string_get_cstr(app->shield_rx_chain->preset.name),
            (unsigned long)((app->shield_rx_chain->rx_bandwidth_hz + 500UL) / 1000UL),
            (long)(gdr_scene_shield_receiver_tx_offset_hz(app) / 1000L));
    }

    if(app->shield_history) {
        furi_mutex_acquire(app->shield_history_mutex, FuriWaitForever);
        gdr_history_format_status_text(
            app->shield_history, history_stat_str, sizeof(history_stat_str));
        furi_mutex_release(app->shield_history_mutex);
    } else {
        snprintf(history_stat_str, sizeof(history_stat_str), "0/%u", GDR_HISTORY_MAX);
    }

    gdr_view_receiver_add_data_statusbar(
        app->gdr_receiver, frequency_str, modulation_str, history_stat_str, true);
}

static void gdr_scene_shield_receiver_teardown(GDRApp* app) {
    furi_check(app);

    if(app->shield_rx_chain) {
        gdr_rx_chain_free(app->shield_rx_chain);
        app->shield_rx_chain = NULL;
    }
    if(app->shield_tx_chain) {
        gdr_tx_chain_free(app->shield_tx_chain);
        app->shield_tx_chain = NULL;
    }

    if(s_shield_devices_inited) {
        subghz_devices_deinit();
        s_shield_devices_inited = false;
    }
}

static bool gdr_scene_shield_receiver_build(GDRApp* app) {
    furi_check(app);

    gdr_radio_deinit(app);

    if(!app->shield_history) {
        app->shield_history = gdr_history_alloc();
        if(!app->shield_history) {
            FURI_LOG_E(TAG, "Failed to allocate shield history");
            return false;
        }
    }
    if(!app->shield_history_mutex) {
        app->shield_history_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
        if(!app->shield_history_mutex) {
            FURI_LOG_E(TAG, "Failed to allocate shield history mutex");
            return false;
        }
    }
    furi_mutex_acquire(app->shield_history_mutex, FuriWaitForever);
    if(gdr_history_get_item(app->shield_history) == 0) {
        app->shield_auto_save_failed = false;
    }
    furi_mutex_release(app->shield_history_mutex);

    subghz_devices_init();
    s_shield_devices_inited = true;

    app->shield_rx_chain = gdr_rx_chain_alloc('E');
    app->shield_tx_chain = gdr_tx_chain_alloc();
    if(!app->shield_rx_chain || !app->shield_tx_chain) {
        FURI_LOG_E(TAG, "Failed to allocate shield chains");
        gdr_scene_shield_receiver_teardown(app);
        return false;
    }

    if(!gdr_rx_chain_acquire_device(
           app->shield_rx_chain, SubGhzRadioDeviceTypeExternalCC1101)) {
        FURI_LOG_E(TAG, "External CC1101 unavailable - Shield RX requires it");
        gdr_scene_shield_receiver_teardown(app);
        return false;
    }
    if(!gdr_tx_chain_acquire_device(app->shield_tx_chain)) {
        FURI_LOG_E(TAG, "Internal CC1101 unavailable");
        gdr_scene_shield_receiver_teardown(app);
        return false;
    }

    size_t preset_count = subghz_setting_get_preset_count(app->setting);
    if(app->shield_preset_index >= preset_count) {
        FURI_LOG_E(TAG, "Invalid shield preset index");
        gdr_scene_shield_receiver_teardown(app);
        return false;
    }

    const char* preset_name = subghz_setting_get_preset_name(app->setting, app->shield_preset_index);
    if(!gdr_rx_chain_set_preset(
           app->shield_rx_chain, app->setting, preset_name, app->shield_freq) ||
       !gdr_rx_chain_apply_shield_profile(app->shield_rx_chain)) {
        FURI_LOG_E(TAG, "Failed to configure Shield RX profile");
        gdr_scene_shield_receiver_teardown(app);
        return false;
    }

    if(!gdr_tx_chain_configure(
           app->shield_tx_chain,
           app->setting,
           app->shield_freq,
           gdr_scene_shield_receiver_resolve_tx_offset(app),
           app->shield_tx_power)) {
        FURI_LOG_E(TAG, "Failed to configure Shield TX");
        gdr_scene_shield_receiver_teardown(app);
        return false;
    }

    if(!gdr_rx_chain_init_receiver(app->shield_rx_chain)) {
        FURI_LOG_E(TAG, "Failed to init external RX chain");
        gdr_scene_shield_receiver_teardown(app);
        return false;
    }

    gdr_rx_chain_set_decode_callback(
        app->shield_rx_chain, gdr_scene_shield_receiver_decode_cb, app);

    return true;
}

void gdr_scene_shield_receiver_on_enter(void* context) {
    furi_check(context);
    GDRApp* app = context;

    if(!gdr_ensure_receiver_view(app)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, GDRSceneStart);
        return;
    }

    if(!gdr_scene_shield_receiver_build(app)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, GDRSceneStart);
        return;
    }

    gdr_view_receiver_set_history_mutex(
        app->gdr_receiver, app->shield_history_mutex);
    gdr_view_receiver_sync_menu_from_history(
        app->gdr_receiver, app->shield_history);
    gdr_view_receiver_set_callback(
        app->gdr_receiver, gdr_scene_shield_receiver_view_callback, app);
    gdr_view_receiver_set_lock(app->gdr_receiver, app->lock);
    gdr_view_receiver_set_autosave(app->gdr_receiver, app->auto_save);
    gdr_view_receiver_set_sub_decode_mode(app->gdr_receiver, false);

    if(app->selected_capture.owner == GDRCaptureOwnerShieldReceiver &&
       app->selected_capture.history == app->shield_history &&
       gdr_selected_capture_is_valid(app)) {
        gdr_view_receiver_set_idx_menu(
            app->gdr_receiver, app->selected_capture.index);
    }

    gdr_scene_shield_receiver_update_statusbar(app);
    scene_manager_set_scene_state(
        app->scene_manager, GDRSceneShieldReceiver, SHIELD_SCENE_STATE_NONE);

    view_dispatcher_switch_to_view(app->view_dispatcher, GDRViewReceiver);
    view_dispatcher_send_custom_event(
        app->view_dispatcher, GDRCustomEventShieldReceiverDeferredStart);
}

static void gdr_scene_shield_receiver_handle_back(GDRApp* app) {
    bool has_history = false;
    bool auto_save_failed = false;
    if(app->shield_history && app->shield_history_mutex) {
        furi_mutex_acquire(app->shield_history_mutex, FuriWaitForever);
        has_history = gdr_history_get_item(app->shield_history) > 0;
        auto_save_failed = app->shield_auto_save_failed;
        furi_mutex_release(app->shield_history_mutex);
    }

    if(has_history && (!app->auto_save || auto_save_failed)) {
        app->unsaved_history_owner = GDRCaptureOwnerShieldReceiver;
        scene_manager_set_scene_state(
            app->scene_manager, GDRSceneShieldReceiver, SHIELD_SCENE_STATE_TO_SUBSCENE);
        scene_manager_next_scene(app->scene_manager, GDRSceneNeedSaving);
    } else {
        app->unsaved_history_owner = GDRCaptureOwnerNone;
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, GDRSceneStart);
    }
}

bool gdr_scene_shield_receiver_on_event(void* context, SceneManagerEvent event) {
    furi_check(context);
    GDRApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case GDRCustomEventShieldReceiverDeferredStart:
            if(!gdr_rx_chain_start(app->shield_rx_chain) ||
               !gdr_tx_chain_start_carrier(app->shield_tx_chain)) {
                FURI_LOG_E(TAG, "Failed to start shield TX/RX");
                gdr_scene_shield_receiver_teardown(app);
                notification_message(app->notifications, &sequence_error);
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, GDRSceneStart);
            } else {
                notification_message(app->notifications, &sequence_tx);
                gdr_scene_shield_receiver_update_statusbar(app);
            }
            consumed = true;
            break;

        case GDRCustomEventShieldReceiverUpdate:
            gdr_view_receiver_sync_menu_from_history(
                app->gdr_receiver, app->shield_history);
            gdr_scene_shield_receiver_update_statusbar(app);
            consumed = true;
            break;

        case GDRCustomEventViewReceiverOK: {
            uint16_t idx = gdr_view_receiver_get_idx_menu(app->gdr_receiver);
            furi_mutex_acquire(app->shield_history_mutex, FuriWaitForever);
            bool valid = idx < gdr_history_get_item(app->shield_history);
            furi_mutex_release(app->shield_history_mutex);
            if(valid) {
                gdr_selected_capture_set(
                    app,
                    app->shield_history,
                    app->shield_history_mutex,
                    idx,
                    GDRCaptureOwnerShieldReceiver);
                scene_manager_set_scene_state(
                    app->scene_manager,
                    GDRSceneShieldReceiver,
                    SHIELD_SCENE_STATE_TO_SUBSCENE);
                scene_manager_next_scene(app->scene_manager, GDRSceneReceiverInfo);
            }
            consumed = true;
            break;
        }

        case GDRCustomEventViewReceiverDeleteItem: {
            uint16_t idx = gdr_view_receiver_get_idx_menu(app->gdr_receiver);
            furi_mutex_acquire(app->shield_history_mutex, FuriWaitForever);
            bool valid = idx < gdr_history_get_item(app->shield_history);
            if(valid) {
                gdr_history_delete_item(app->shield_history, idx);
            }
            uint16_t count_after = gdr_history_get_item(app->shield_history);
            if(count_after == 0) {
                app->shield_auto_save_failed = false;
            }
            furi_mutex_release(app->shield_history_mutex);

            if(valid) {
                gdr_view_receiver_delete_item(app->gdr_receiver, idx);

                if(count_after == 0) {
                    gdr_view_receiver_sync_menu_from_history(
                        app->gdr_receiver, app->shield_history);
                    gdr_view_receiver_set_idx_menu(app->gdr_receiver, 0);
                }
                gdr_scene_shield_receiver_update_statusbar(app);
            }
            consumed = true;
            break;
        }

        case GDRCustomEventViewReceiverConfig:
            scene_manager_set_scene_state(
                app->scene_manager,
                GDRSceneShieldReceiver,
                SHIELD_SCENE_STATE_TO_SUBSCENE);
            scene_manager_next_scene(app->scene_manager, GDRSceneShieldReceiverConfig);
            consumed = true;
            break;

        case GDRCustomEventViewReceiverBack:
            gdr_scene_shield_receiver_handle_back(app);
            consumed = true;
            break;

        case GDRCustomEventViewReceiverUnlock:
            app->lock = GDRLockOff;
            gdr_view_receiver_set_lock(app->gdr_receiver, app->lock);
            consumed = true;
            break;

        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(app->shield_rx_chain) {
            gdr_view_receiver_set_rssi(
                app->gdr_receiver, gdr_rx_chain_get_rssi(app->shield_rx_chain));
            notification_message(app->notifications, &sequence_blink_cyan_10);
        }
        consumed = true;
    }

    return consumed;
}

void gdr_scene_shield_receiver_on_exit(void* context) {
    furi_check(context);
    GDRApp* app = context;

    const bool leaving_for_subscene =
        (scene_manager_get_scene_state(app->scene_manager, GDRSceneShieldReceiver) ==
         SHIELD_SCENE_STATE_TO_SUBSCENE);

    gdr_scene_shield_receiver_teardown(app);

    if(leaving_for_subscene) {
        return;
    }

    gdr_view_receiver_reset_menu(app->gdr_receiver);
    if(app->selected_capture.owner == GDRCaptureOwnerShieldReceiver) {
        gdr_selected_capture_clear(app);
    }
}

void gdr_scene_shield_receiver_view_callback(GDRCustomEvent event, void* context) {
    furi_check(context);
    GDRApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

#endif // ENABLE_SHIELD_RX_SCENE
