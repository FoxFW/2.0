#include "../nfc_app_i.h"

enum SubmenuIndex {
    SubmenuIndexRead,
    SubmenuIndexDetectReader,
    SubmenuIndexSaved,
    SubmenuIndexFuzzer,
    SubmenuIndexExtraAction,
    SubmenuIndexAddManually,
    SubmenuIndexDebug,
};

void nfc_scene_start_submenu_callback(void* context, uint32_t index) {
    NfcApp* nfc = context;

    view_dispatcher_send_custom_event(nfc->view_dispatcher, index);
}

void nfc_scene_start_on_enter(void* context) {
    NfcApp* nfc = context;
    Submenu* submenu = nfc->submenu;

    /* FoxFW: back at the Start menu (not in Read/Emulate) -> reconnect qFlipper. */
    nfc_cli_soft_unlock(nfc);

    if(nfc->startup_holder) {
        view_holder_set_view(nfc->startup_holder, NULL);
        view_holder_free(nfc->startup_holder);
        nfc->startup_holder = NULL;
    }
    if(nfc->startup_loading) {
        loading_free(nfc->startup_loading);
        nfc->startup_loading = NULL;
    }

    furi_string_reset(nfc->file_name);
    nfc_device_clear(nfc->nfc_device);
    iso14443_3a_reset(nfc->iso14443_3a_edit_data);

    nfc_detected_protocols_reset(nfc->detected_protocols);

    submenu_add_item(submenu, "Read", SubmenuIndexRead, nfc_scene_start_submenu_callback, nfc);
    submenu_add_item(
        submenu,
        "Extract MFC Keys",
        SubmenuIndexDetectReader,
        nfc_scene_start_submenu_callback,
        nfc);
    submenu_add_item(submenu, "Saved", SubmenuIndexSaved, nfc_scene_start_submenu_callback, nfc);
    submenu_add_item(submenu, "Fuzzer", SubmenuIndexFuzzer, nfc_scene_start_submenu_callback, nfc);
    submenu_add_item(
        submenu, "Extra Actions", SubmenuIndexExtraAction, nfc_scene_start_submenu_callback, nfc);
    submenu_add_item(
        submenu, "Add Manually", SubmenuIndexAddManually, nfc_scene_start_submenu_callback, nfc);

    if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug)) {
        submenu_add_item(
            submenu, "Debug", SubmenuIndexDebug, nfc_scene_start_submenu_callback, nfc);
    }

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(nfc->scene_manager, NfcSceneStart));

    /* FoxFW: returning from the externalized MFKey nonce collector -> re-select
     * the "Extract MFC Keys" item (the loader relaunched nfc.fap fresh, so the
     * in-memory selection was lost; the marker carries it across). */
    if(storage_common_exists(nfc->storage, NFC_FOCUS_MENU_MARKER_PATH)) {
        submenu_set_selected_item(submenu, SubmenuIndexDetectReader);
        storage_simply_remove(nfc->storage, NFC_FOCUS_MENU_MARKER_PATH);
    }

    /* FoxFW: returning from the externalized MIFARE Fuzzer -> re-select "Fuzzer". */
    if(storage_common_exists(nfc->storage, NFC_FOCUS_FUZZER_MARKER_PATH)) {
        submenu_set_selected_item(submenu, SubmenuIndexFuzzer);
        storage_simply_remove(nfc->storage, NFC_FOCUS_FUZZER_MARKER_PATH);
    }

    view_dispatcher_switch_to_view(nfc->view_dispatcher, NfcViewMenu);
}

bool nfc_scene_start_on_event(void* context, SceneManagerEvent event) {
    NfcApp* nfc = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == SubmenuIndexRead) {
            scene_manager_next_scene(nfc->scene_manager, NfcSceneDetect);
        } else if(event.event == SubmenuIndexDetectReader) {
            /* FoxFW: externalized — launch the standalone MFKey nonce collector
             * (fresh random 4K card) instead of the in-app detect_reader scene. */
            nfc_run_mfkey_collector(nfc, NULL);
        } else if(event.event == SubmenuIndexSaved) {
            scene_manager_next_scene(nfc->scene_manager, NfcSceneFileSelect);
        } else if(event.event == SubmenuIndexFuzzer) {
            /* FoxFW: externalized — launch the standalone MIFARE Fuzzer instead
             * of the in-app fuzzer scene (frees its RAM from nfc.fap). */
            nfc_run_mifare_fuzzer(nfc);
        } else if(event.event == SubmenuIndexExtraAction) {
            scene_manager_next_scene(nfc->scene_manager, NfcSceneExtraActions);
        } else if(event.event == SubmenuIndexAddManually) {
            scene_manager_next_scene(nfc->scene_manager, NfcSceneSetType);
        } else if(event.event == SubmenuIndexDebug) {
            scene_manager_next_scene(nfc->scene_manager, NfcSceneDebug);
        } else {
            consumed = false;
        }
        if(consumed) {
            scene_manager_set_scene_state(nfc->scene_manager, NfcSceneStart, event.event);
        }
    }
    return consumed;
}

void nfc_scene_start_on_exit(void* context) {
    NfcApp* nfc = context;

    submenu_reset(nfc->submenu);
}
