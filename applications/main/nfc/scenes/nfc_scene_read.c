#include "../helpers/protocol_support/nfc_protocol_support.h"
#include "../nfc_app_i.h"

void nfc_scene_read_on_enter(void* context) {
    NfcApp* instance = context;
    /* FoxFW: belt-and-suspenders lock for any path that reaches Read directly
     * (the Read button normally locks earlier, in nfc_scene_detect). Idempotent;
     * restored at the Start menu (nfc_scene_start) / nfc_app_free, NOT on_exit, so
     * qFlipper stays detached across the whole detect->read flow. */
    nfc_cli_soft_lock(instance);
    nfc_show_loading_popup(instance, true);
    nfc_supported_cards_load_cache(instance->nfc_supported_cards);
    nfc_show_loading_popup(instance, false);

    nfc_protocol_support_on_enter(NfcProtocolSupportSceneRead, context);
}

bool nfc_scene_read_on_event(void* context, SceneManagerEvent event) {
    return nfc_protocol_support_on_event(NfcProtocolSupportSceneRead, context, event);
}

void nfc_scene_read_on_exit(void* context) {
    nfc_protocol_support_on_exit(NfcProtocolSupportSceneRead, context);
}
