#include "../subghz_i.h"
#include "../views/transmitter.h"
#include <dolphin/dolphin.h>

#include <lib/subghz/blocks/custom_btn.h>
#include <string.h>

#include <lib/subghz/devices/devices.c>
#include "applications/main/subghz/helpers/subghz_txrx_i.h"
#include "lib/subghz/blocks/generic.h"

#define TAG "SubGhzSceneTransmitter"

void subghz_scene_transmitter_callback(SubGhzCustomEvent event, void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, event);
}

/* Extract label from "[Label]" after "Btn:" in a key string */
static bool txsc_btn_label(const char* s, char* d, size_t n) {
    const char* btn = strstr(s, "Btn:");
    const char* p   = btn ? strstr(btn, "[") : NULL;
    if(!p) { d[0]=0; return false; }
    p++; size_t i=0;
    while(p[i] && p[i]!=']' && i<n-1) { d[i]=p[i]; i++; }
    d[i]=0;
    return i>0;
}

bool subghz_scene_transmitter_update_data_show(void* context) {
    /* ── Button label pre-computation (no TX) ───────────────────────────
     *
     * The ENCODER writes the mapped button value for the active custom_btn
     * back into fff_data during encoder_deserialize.  We exploit the same
     * mechanism without opening the CC1101: temporarily write each possible
     * btn byte (1..4 = UP/DOWN/LEFT/RIGHT) into the "Btn" field of fff_data,
     * call the DECODER, extract the human-readable label, then restore.
     *
     * This is safe — FlipperFormat lives in RAM, nothing is transmitted.
     * It works for any protocol that stores a "Btn" hex byte in fff_data
     * and whose encoder maps custom_btn values directly to that byte
     * (KIA V5, VAG, PSA and the majority of automotive protocols).
     * Protocols that don't use a "Btn" field fall back to "^/v/</>".
     * ──────────────────────────────────────────────────────────────── */
    SubGhz* subghz = context;
    bool ret = false;
    SubGhzProtocolDecoderBase* decoder = subghz_txrx_get_decoder(subghz->txrx);

    if(!decoder) {
        subghz_view_transmitter_set_radio_device_type(
            subghz->subghz_transmitter, subghz_txrx_radio_device_get(subghz->txrx));
        return false;
    }

    FuriString* key_str        = furi_string_alloc();
    FuriString* frequency_str  = furi_string_alloc();
    FuriString* modulation_str = furi_string_alloc();
    FlipperFormat* fff         = subghz_txrx_get_fff_data(subghz->txrx);

    /* Save which button is active BEFORE we restore to OK.
     * If a direction was pressed, fff_data has been modified to that
     * direction's button value — we must NOT use that as the centre label. */
    uint8_t active_btn = subghz_custom_btn_get();

    /* Restore to OK/original before deserialising so key_str reflects the
     * captured signal, not whatever the encoder last wrote. */
    subghz_custom_btn_set(SUBGHZ_CUSTOM_BTN_OK);
    if(subghz_protocol_decoder_base_deserialize(decoder, fff) == SubGhzProtocolStatusOk) {
        subghz_protocol_decoder_base_get_string(decoder, key_str);

        subghz_txrx_get_frequency_and_modulation(
            subghz->txrx, frequency_str, modulation_str, true);

        subghz_view_transmitter_add_data_to_show(
            subghz->subghz_transmitter,
            furi_string_get_cstr(key_str),
            furi_string_get_cstr(frequency_str),
            furi_string_get_cstr(modulation_str),
            subghz_txrx_protocol_is_transmittable(subghz->txrx, false));

        if(subghz_custom_btn_is_allowed()) {
            /* Only update button labels when:
             *   a) This is the initial enter (labels not ready yet), OR
             *   b) OK was pressed (sending the original captured signal).
             * When a DIRECTION button was pressed, fff_data is modified to
             * that button's value so key_str would yield the direction's
             * label, not the original.  We keep the labels from enter so
             * the centre always shows the captured signal name and
             * duplicate-hiding never incorrectly removes a direction. */
            if(active_btn == SUBGHZ_CUSTOM_BTN_OK ||
               !subghz_view_transmitter_is_labels_ready(subghz->subghz_transmitter)) {

                char clabel[14] = {0};
                txsc_btn_label(furi_string_get_cstr(key_str), clabel, sizeof(clabel));
                if(!clabel[0]) snprintf(clabel, sizeof(clabel), "SEND");

                FuriString* proto_str = furi_string_alloc();
                flipper_format_rewind(fff);
                flipper_format_read_string(fff, "Protocol", proto_str);
                const char* pname = furi_string_get_cstr(proto_str);

                const char* ul = subghz_custom_btn_get_label_for_proto(pname, SUBGHZ_CUSTOM_BTN_UP);
                const char* dl = subghz_custom_btn_get_label_for_proto(pname, SUBGHZ_CUSTOM_BTN_DOWN);
                const char* ll = subghz_custom_btn_get_label_for_proto(pname, SUBGHZ_CUSTOM_BTN_LEFT);
                const char* rl = subghz_custom_btn_get_label_for_proto(pname, SUBGHZ_CUSTOM_BTN_RIGHT);

                furi_string_free(proto_str);

                if(!ul) ul = "^";
                if(!dl) dl = "v";
                if(!ll) ll = "<";
                if(!rl) rl = ">";

                bool u_vis = (strcmp(ul, clabel) != 0);
                bool d_vis = (strcmp(dl, clabel) != 0);
                bool l_vis = (strcmp(ll, clabel) != 0);
                bool r_vis = (strcmp(rl, clabel) != 0);

                subghz_view_transmitter_set_btn_labels(
                    subghz->subghz_transmitter,
                    clabel,
                    ul, u_vis,
                    dl, d_vis,
                    ll, l_vis,
                    rl, r_vis);
            }
            /* Direction pressed: labels stay exactly as set on enter. */
        }

        ret = true;
    }
    furi_string_free(frequency_str);
    furi_string_free(modulation_str);
    furi_string_free(key_str);

    subghz_view_transmitter_set_radio_device_type(
        subghz->subghz_transmitter, subghz_txrx_radio_device_get(subghz->txrx));
    return ret;
}

void subghz_scene_transmitter_on_enter(void* context) {
    SubGhz* subghz = context;

    subghz_custom_btns_reset();
    /* Reset button labels so progressive discovery starts fresh */
    subghz_view_transmitter_reset_labels(subghz->subghz_transmitter);

    if(!subghz_scene_transmitter_update_data_show(subghz)) {
        view_dispatcher_send_custom_event(
            subghz->view_dispatcher, SubGhzCustomEventViewTransmitterError);
    }

    subghz_view_transmitter_set_callback(
        subghz->subghz_transmitter, subghz_scene_transmitter_callback, subghz);

    subghz->state_notifications = SubGhzNotificationStateIDLE;
    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdTransmitter);
}

bool subghz_scene_transmitter_on_event(void* context, SceneManagerEvent event) {
    // key concept: we start endless TX until user release OK button, and after this we send last
    // protocols repeats - this guarantee that one press OK will
    // be guarantee send the required minimum protocol data packets
    // for all of this we use subghz_block_generic_global.endless_tx in protocols _yield function.
    SubGhz* subghz = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubGhzCustomEventViewTransmitterSendStart) {
            // user press OK - start endless TX
            subghz->state_notifications = SubGhzNotificationStateIDLE;
            subghz_block_generic_global.endless_tx = true;
            if(subghz_tx_start(subghz, subghz_txrx_get_fff_data(subghz->txrx))) {
                subghz->state_notifications = SubGhzNotificationStateTx;
                subghz_scene_transmitter_update_data_show(subghz);
                dolphin_deed(DolphinDeedSubGhzSend);
            }
            return true;
        } else if(event.event == SubGhzCustomEventViewTransmitterSendStop) {
            // user release OK
            // we switch off endless_tx - that mean protocols yield finish endless transmission,
            // send upload "repeat=xx" times, and after will be stoped by tick event down this code.
            subghz_block_generic_global.endless_tx = false;
            return true;
        } else if(event.event == SubGhzCustomEventViewTransmitterBack) {
            scene_manager_search_and_switch_to_previous_scene(
                subghz->scene_manager, SubGhzSceneStart);
            return true;
        } else if(event.event == SubGhzCustomEventViewTransmitterPageChange) {
            // Page changed via OK button, refresh display
            subghz_scene_transmitter_update_data_show(subghz);
            return true;
        } else if(event.event == SubGhzCustomEventViewTransmitterError) {
            furi_string_set(subghz->error_str, "Protocol not\nfound!");
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneShowErrorSub);
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(subghz->state_notifications == SubGhzNotificationStateTx) {
            // if hardware TX still working at this time so we just blink led and return
            if(!subghz_devices_is_async_complete_tx(subghz->txrx->radio_device)) {
                notification_message(subghz->notifications, &sequence_blink_magenta_10);
                return true;
                // if hardware TX was stoped so we stop TX correctly
            } else {
                subghz->state_notifications = SubGhzNotificationStateIDLE;
                subghz_txrx_stop(subghz->txrx);
                if(subghz_custom_btn_get() != SUBGHZ_CUSTOM_BTN_OK) {
                    subghz_custom_btn_set(SUBGHZ_CUSTOM_BTN_OK);
                    int32_t tmp_counter = furi_hal_subghz_get_rolling_counter_mult();
                    furi_hal_subghz_set_rolling_counter_mult(0);
                    // Calling restore!
                    subghz_tx_start(subghz, subghz_txrx_get_fff_data(subghz->txrx));
                    subghz_txrx_stop(subghz->txrx);
                    // Calling restore 2nd time special for FAAC SLH!
                    // TODO: Find better way to restore after custom button is used!!!
                    subghz_tx_start(subghz, subghz_txrx_get_fff_data(subghz->txrx));
                    subghz_txrx_stop(subghz->txrx);
                    furi_hal_subghz_set_rolling_counter_mult(tmp_counter);
                }
            }
        }
        return true;
    }
    return false;
}

void subghz_scene_transmitter_on_exit(void* context) {
    SubGhz* subghz = context;
    subghz->state_notifications = SubGhzNotificationStateIDLE;

    subghz_txrx_reset_dynamic_and_custom_btns(subghz->txrx);
}
