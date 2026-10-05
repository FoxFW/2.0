#include "../nfc_app_i.h"
#include "nfc_icons.h"

void nfc_scene_detect_scan_callback(NfcScannerEvent event, void* context) {
    furi_assert(context);

    NfcApp* instance = context;

    if(event.type == NfcScannerEventTypeDetected) {
        nfc_detected_protocols_set(
            instance->detected_protocols, event.data.protocols, event.data.protocol_num);
        view_dispatcher_send_custom_event(instance->view_dispatcher, NfcCustomEventWorkerExit);
    }
}

void nfc_scene_detect_on_enter(void* context) {
    NfcApp* instance = context;

    /* FoxFW: this IS the Read-button scan scene — it allocs NfcScanner and sweeps
     * every protocol, loading each external .fal group (the OOM point for a bare
     * ISO14443-3A card). Detach USB/CLI + suppress the screen stream FIRST so the
     * sweep has qFlipper's RPC/CDC RAM free. Stays locked through the whole read
     * read flow; restored when leaving to the Start menu (nfc_scene_start) and in
     * nfc_app_free. INVESTIGATING: repeated lock/unlock cycles accumulate heap
     * fragmentation that fails ~3rd consecutive read with qFlipper connected. */
    nfc_cli_soft_lock(instance);

    nfc_show_loading_popup(instance, true);
    nfc_supported_cards_load_cache(instance->nfc_supported_cards);
    nfc_show_loading_popup(instance, false);

    popup_reset(instance->popup);
    popup_set_header(instance->popup, "Reading", 97, 15, AlignCenter, AlignTop);
    popup_set_text(
        instance->popup, "Hold card next\nto Flipper's back", 94, 27, AlignCenter, AlignTop);
    popup_set_icon(instance->popup, 0, 8, &I_NFC_manual_60x50);
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcViewPopup);

    nfc_detected_protocols_reset(instance->detected_protocols);

    /* FoxFW RAM: free the view modules this scan doesn't use, giving the on-demand
     * .fal group loads ~8-10KB more headroom. Reloaded in on_exit. */
    nfc_app_idle_views_unload(instance);

    instance->scanner = nfc_scanner_alloc(instance->nfc);
    nfc_scanner_start(instance->scanner, nfc_scene_detect_scan_callback, instance);

    nfc_blink_detect_start(instance);
}

bool nfc_scene_detect_on_event(void* context, SceneManagerEvent event) {
    NfcApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcCustomEventWorkerExit) {
            if(nfc_detected_protocols_get_num(instance->detected_protocols) > 1) {
                notification_message(instance->notifications, &sequence_single_vibro);
                scene_manager_next_scene(instance->scene_manager, NfcSceneSelectProtocol);
            } else {
                scene_manager_next_scene(instance->scene_manager, NfcSceneRead);
            }
            consumed = true;
        }
    }

    return consumed;
}

void nfc_scene_detect_on_exit(void* context) {
    NfcApp* instance = context;

    nfc_scanner_stop(instance->scanner);
    nfc_scanner_free(instance->scanner);

    /* FoxFW RAM: scan done -> restore the view modules for the next scene. */
    nfc_app_idle_views_reload(instance);
    popup_reset(instance->popup);

    nfc_blink_stop(instance);
}
