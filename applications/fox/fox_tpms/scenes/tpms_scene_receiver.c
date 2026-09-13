#include "../tpms_app_i.h"
#include "../views/tpms_receiver.h"

/* Auto-retry interval for the LF wake pulse while this screen is "hunting"
 * (see lf_auto_retrigger_armed's comment in tpms_app_i.h). 50 ticks at the
 * app's 100ms tick period (tpms_app.c) = 5 seconds between the start of one
 * pulse and the next: the LF coil itself only drives for the first 3 of
 * those (LF_RELEARN_DURATION_MS, tpms_app_i.c), leaving a ~2s listen
 * window for the sensor's reply before trying again. Chosen from how real
 * TPMS trigger tools are actually used - a sensor answers an LF trigger
 * with a single RF reply, not a held stream (confirmed against an NXP TPMS
 * sensor reference design's own RF-datagram send routine), and because the
 * LF field is short-range near-field coupling, professional tool guidance
 * (e.g. Continental's TPMS-D manual) is explicitly "wait a few seconds,
 * and if nothing comes back, reposition and try again" - not "wait
 * indefinitely". 5s keeps that same reposition-and-retry rhythm without
 * hammering the LF coil or interrupting the RX hopper too often. */
#define LF_AUTO_RETRIGGER_INTERVAL_TICKS 50

static const NotificationSequence subghz_sequence_rx = {
    &message_green_255,

    &message_vibro_on,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,

    &message_delay_50,
    NULL,
};

static const NotificationSequence subghz_sequence_rx_locked = {
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

// Long vibrate fired once per automatic retry - tells the user, eyes-off-
// screen, that the position they're holding the Flipper in right now just
// missed the sensor and it's trying again. Deliberately longer (500ms,
// vs. the 50ms confirmation buzzes elsewhere on this screen) so it reads as
// a distinct "no luck" cue rather than blending in with the normal
// found-a-tag blink/buzz.
static const NotificationSequence subghz_sequence_reposition = {
    &message_vibro_on,
    &message_delay_500,
    &message_vibro_off,
    NULL,
};

static void tpms_scene_receiver_update_statusbar(void* context) {
    TPMSApp* app = context;
    FuriString* history_stat_str;
    history_stat_str = furi_string_alloc();
    if(!tpms_history_get_text_space_left(app->txrx->history, history_stat_str)) {
        FuriString* frequency_str;
        FuriString* modulation_str;

        frequency_str = furi_string_alloc();
        modulation_str = furi_string_alloc();

        tpms_get_frequency_modulation(app, frequency_str, modulation_str);

        tpms_view_receiver_add_data_statusbar(
            app->tpms_receiver,
            furi_string_get_cstr(frequency_str),
            furi_string_get_cstr(modulation_str),
            furi_string_get_cstr(history_stat_str),
            radio_device_loader_is_external(app->txrx->radio_device));

        furi_string_free(frequency_str);
        furi_string_free(modulation_str);
    } else {
        tpms_view_receiver_add_data_statusbar(
            app->tpms_receiver,
            furi_string_get_cstr(history_stat_str),
            "",
            "",
            radio_device_loader_is_external(app->txrx->radio_device));
    }
    furi_string_free(history_stat_str);
}

// Pauses RX (if active), fires the shared LF wake pulse, and resumes RX -
// same pause-then-tpms_begin()+tpms_rx()-again sequence tpms_hopper_update()
// (tpms_app_i.c) already uses to safely retune, needed here because
// furi_hal_rfid_tim_read_start() (fired via tpms_relearn_lf_start() below)
// must not run concurrently with an active RX session. Shared by the manual
// Right-key Re-Trigger event and the tick-driven auto-retry below so both
// paths fire the pulse identically. Callers set app->lf_relearn_is_retry
// beforehand to say whether this particular pulse counts as a "miss retry"
// (drives the "*Reposition*" screen cue - see tpms_receiver.c's draw code)
// or not (the guided flow's own first pulse, or a manual Re-Trigger).
static void tpms_scene_receiver_fire_lf_retrigger(TPMSApp* app) {
    if(app->txrx->txrx_state == TPMSTxRxStateRx) {
        tpms_rx_end(app);
    }
    tpms_relearn_lf_start(app);
    notification_message(app->notifications, &sequence_blink_green_10);
    if(app->txrx->txrx_state == TPMSTxRxStateIDLE) {
        tpms_begin(
            app,
            subghz_setting_get_preset_data_by_name(
                app->setting, furi_string_get_cstr(app->txrx->preset->name)));
        tpms_rx(app, app->txrx->preset->frequency);
    }
}

void tpms_scene_receiver_callback(TPMSCustomEvent event, void* context) {
    furi_assert(context);
    TPMSApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

static void tpms_scene_receiver_add_to_history_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    furi_assert(context);
    TPMSApp* app = context;
    FuriString* str_buff;
    str_buff = furi_string_alloc();

    if(tpms_history_add_to_history(app->txrx->history, decoder_base, app->txrx->preset) ==
       TPMSHistoryStateAddKeyNewDada) {
        furi_string_reset(str_buff);

        tpms_history_get_text_item_menu(
            app->txrx->history, str_buff, tpms_history_get_item(app->txrx->history) - 1);
        tpms_view_receiver_add_item_to_menu(
            app->tpms_receiver,
            furi_string_get_cstr(str_buff),
            tpms_history_get_type_protocol(
                app->txrx->history, tpms_history_get_item(app->txrx->history) - 1));

        tpms_scene_receiver_update_statusbar(app);
        notification_message(app->notifications, &sequence_blink_green_10);
        if(app->lock != TPMSLockOn) {
            notification_message(app->notifications, &subghz_sequence_rx);
        } else {
            notification_message(app->notifications, &subghz_sequence_rx_locked);
        }
    }
    subghz_receiver_reset(receiver);
    furi_string_free(str_buff);
    app->txrx->rx_key_state = TPMSRxKeyStateAddKey;
}

void tpms_scene_receiver_on_enter(void* context) {
    TPMSApp* app = context;

    FuriString* str_buff;
    str_buff = furi_string_alloc();

    if(app->txrx->rx_key_state == TPMSRxKeyStateIDLE) {
        bool group_active = app->active_vehicle_group >= 0 &&
                             app->active_vehicle_group < TPMS_VEHICLE_GROUP_COUNT;
        if(group_active) {
            // Guided "Select Model" flow: seed on the vehicle group's first
            // RF candidate and force hopping on, so the receiver
            // automatically cycles through that group's short
            // {frequency, modulation} list (tpms_hopper_update(),
            // tpms_app_i.c) instead of waiting on the user to also visit
            // Receiver Config and switch Hopping on by hand.
            app->txrx->hopper_idx_frequency = 0;
            tpms_vehicle_group_apply_candidate(app, 0);
            app->txrx->hopper_state = TPMSHopperStateRunnig;
            // The guided flow's last step (tpms_scene_vehicle_steps.c)
            // already fired one LF pulse before switching here - arm the
            // auto-retry countdown so this screen keeps re-firing it every
            // few seconds on its own instead of waiting forever on that
            // single pulse. See LF_AUTO_RETRIGGER_INTERVAL_TICKS's comment.
            // That first pulse was not a "miss retry" (nothing has failed
            // yet), so lf_relearn_is_retry stays false here.
            app->lf_auto_retrigger_armed = true;
            app->lf_auto_retrigger_countdown = LF_AUTO_RETRIGGER_INTERVAL_TICKS;
            app->lf_relearn_is_retry = false;
        } else {
            // Untouched original behaviour: Manual Scan / no vehicle picked.
            tpms_preset_init(
                app, "AM650", subghz_setting_get_default_frequency(app->setting), NULL, 0);
        }
        tpms_history_reset(app->txrx->history);
        app->txrx->rx_key_state = TPMSRxKeyStateStart;
    }

    tpms_view_receiver_set_lock(app->tpms_receiver, app->lock);

    //Load history to receiver
    tpms_view_receiver_exit(app->tpms_receiver);
    for(uint8_t i = 0; i < tpms_history_get_item(app->txrx->history); i++) {
        furi_string_reset(str_buff);
        tpms_history_get_text_item_menu(app->txrx->history, str_buff, i);
        tpms_view_receiver_add_item_to_menu(
            app->tpms_receiver,
            furi_string_get_cstr(str_buff),
            tpms_history_get_type_protocol(app->txrx->history, i));
        app->txrx->rx_key_state = TPMSRxKeyStateAddKey;
    }
    furi_string_free(str_buff);
    tpms_scene_receiver_update_statusbar(app);

    tpms_view_receiver_set_callback(app->tpms_receiver, tpms_scene_receiver_callback, app);
    subghz_receiver_set_rx_callback(
        app->txrx->receiver, tpms_scene_receiver_add_to_history_callback, app);

    if(app->txrx->txrx_state == TPMSTxRxStateRx) {
        tpms_rx_end(app);
    };
    if((app->txrx->txrx_state == TPMSTxRxStateIDLE) ||
       (app->txrx->txrx_state == TPMSTxRxStateSleep)) {
        tpms_begin(
            app,
            subghz_setting_get_preset_data_by_name(
                app->setting, furi_string_get_cstr(app->txrx->preset->name)));

        tpms_rx(app, app->txrx->preset->frequency);
    }

    tpms_view_receiver_set_idx_menu(app->tpms_receiver, app->txrx->idx_menu_chosen);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewReceiver);
}

bool tpms_scene_receiver_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case TPMSCustomEventViewReceiverBack:
            // Stop CC1101 Rx
            if(app->txrx->txrx_state == TPMSTxRxStateRx) {
                tpms_rx_end(app);
                tpms_sleep(app);
            };
            app->txrx->hopper_state = TPMSHopperStateOFF;
            app->txrx->idx_menu_chosen = 0;
            app->lf_auto_retrigger_armed = false;
            subghz_receiver_set_rx_callback(app->txrx->receiver, NULL, app);

            // A later Manual Scan must never inherit a stale guided-flow
            // group - see active_vehicle_group's comment in tpms_app_i.h.
            app->active_vehicle_group = -1;

            app->txrx->rx_key_state = TPMSRxKeyStateIDLE;
            tpms_preset_init(
                app, "AM650", subghz_setting_get_default_frequency(app->setting), NULL, 0);
            if(scene_manager_has_previous_scene(app->scene_manager, TPMSSceneStart)) {
                consumed = scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, TPMSSceneStart);
            } else {
                scene_manager_next_scene(app->scene_manager, TPMSSceneStart);
            }
            break;
        case TPMSCustomEventViewReceiverOK:
            app->txrx->idx_menu_chosen = tpms_view_receiver_get_idx_menu(app->tpms_receiver);
            scene_manager_next_scene(app->scene_manager, TPMSSceneReceiverInfo);
            consumed = true;
            break;
        case TPMSCustomEventViewReceiverConfig:
            app->txrx->idx_menu_chosen = tpms_view_receiver_get_idx_menu(app->tpms_receiver);
            scene_manager_next_scene(app->scene_manager, TPMSSceneReceiverConfig);
            consumed = true;
            break;
        case TPMSCustomEventViewReceiverOffDisplay:
            notification_message(app->notifications, &sequence_display_backlight_off);
            consumed = true;
            break;
        case TPMSCustomEventViewReceiverUnlock:
            app->lock = TPMSLockOff;
            consumed = true;
            break;
        case TPMSCustomEventViewReceiverRetrigger:
            // Inline "Re-Trigger" shortcut (Right key) on the Scanning
            // screen - unlike the dedicated Trigger scene (tpms_scene_
            // relearn.c, only ever reached from the Start menu, where
            // txrx_state is always IDLE), this screen always has an active
            // SubGHz RX session (tpms_rx(), started by on_enter above and
            // kept running by tpms_hopper_update()). See
            // tpms_scene_receiver_fire_lf_retrigger()'s comment above for
            // why RX has to be paused/resumed around the pulse.
            app->lf_relearn_is_retry = false;
            tpms_scene_receiver_fire_lf_retrigger(app);
            // A manual Re-Trigger also (re)arms the auto-retry countdown -
            // see LF_AUTO_RETRIGGER_INTERVAL_TICKS's comment - so pressing
            // Right always starts/continues the same "keep trying every
            // few seconds" hunt the guided flow arrives with already,
            // rather than firing once and going quiet again.
            app->lf_auto_retrigger_armed = true;
            app->lf_auto_retrigger_countdown = LF_AUTO_RETRIGGER_INTERVAL_TICKS;
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(app->txrx->hopper_state != TPMSHopperStateOFF) {
            tpms_hopper_update(app);
            tpms_scene_receiver_update_statusbar(app);
        }
        // Get current RSSI
        float rssi = furi_hal_subghz_get_rssi();
        tpms_view_receiver_set_rssi(app->tpms_receiver, rssi);

        if(app->txrx->txrx_state == TPMSTxRxStateRx) {
            notification_message(app->notifications, &sequence_blink_cyan_10);
        }

        // Keep re-firing the LF wake pulse every
        // LF_AUTO_RETRIGGER_INTERVAL_TICKS while armed, instead of the
        // screen going quiet forever after the one pulse that got it here
        // - see that macro's comment and lf_auto_retrigger_armed's comment
        // in tpms_app_i.h. Runs continuously for as long as this screen is
        // armed (i.e. until Back disarms it), on purpose - a guided scan
        // covers one wheel at a time, and the user may keep moving the
        // Flipper to the next wheel/sensor after an earlier one already
        // answered, same as a real trigger tool being re-used wheel to
        // wheel rather than stopping after the first hit.
        if(app->lf_auto_retrigger_armed) {
            if(app->lf_auto_retrigger_countdown > 0) {
                app->lf_auto_retrigger_countdown--;
            } else {
                // The previous pulse's listen window just ran out with no
                // reply - a miss. Long-vibrate to say so without the user
                // needing to be looking at the screen, flag this pulse as
                // a retry (drives the "*Reposition*" cue below), and fire.
                notification_message(app->notifications, &subghz_sequence_reposition);
                app->lf_relearn_is_retry = true;
                tpms_scene_receiver_fire_lf_retrigger(app);
                app->lf_auto_retrigger_countdown = LF_AUTO_RETRIGGER_INTERVAL_TICKS;
            }
        }

        // Mirror lf_relearn_is_retry onto the view for exactly as long as
        // this particular pulse is actually driving the LF coil
        // (lf_relearn_active) - the view then reverts to its normal
        // animated "TPMS ..." line on its own once the pulse ends, with no
        // separate timer needed on either side. See show_reposition's
        // comment in tpms_receiver.c.
        tpms_view_receiver_set_show_reposition(
            app->tpms_receiver, app->lf_relearn_active && app->lf_relearn_is_retry);
    }
    return consumed;
}

void tpms_scene_receiver_on_exit(void* context) {
    TPMSApp* app = context;
    // Mirrors tpms_scene_relearn_config_on_exit()'s own teardown: leaving
    // the Scanning screen mid-pulse (Retrigger fired, then Back pressed
    // before the 3s window elapses) should stop the LF coil rather than
    // leaving it to time out on its own after the view holding it has gone.
    if(app->lf_relearn_active) {
        tpms_relearn_lf_stop(app);
    }
}
