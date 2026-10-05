#include "../mfkey_collector_i.h"

/* Forward declarations of the scene handlers (defined in the copied scene files,
 * whose function names are kept identical to the NFC app). */
void nfc_scene_mf_classic_detect_reader_on_enter(void* context);
bool nfc_scene_mf_classic_detect_reader_on_event(void* context, SceneManagerEvent event);
void nfc_scene_mf_classic_detect_reader_on_exit(void* context);

void nfc_scene_mf_classic_mfkey_nonces_info_on_enter(void* context);
bool nfc_scene_mf_classic_mfkey_nonces_info_on_event(void* context, SceneManagerEvent event);
void nfc_scene_mf_classic_mfkey_nonces_info_on_exit(void* context);

void nfc_scene_mf_classic_mfkey_complete_on_enter(void* context);
bool nfc_scene_mf_classic_mfkey_complete_on_event(void* context, SceneManagerEvent event);
void nfc_scene_mf_classic_mfkey_complete_on_exit(void* context);

static void (*const mfkey_collector_on_enter_handlers[])(void*) = {
    nfc_scene_mf_classic_detect_reader_on_enter,
    nfc_scene_mf_classic_mfkey_nonces_info_on_enter,
    nfc_scene_mf_classic_mfkey_complete_on_enter,
};

static bool (*const mfkey_collector_on_event_handlers[])(void*, SceneManagerEvent) = {
    nfc_scene_mf_classic_detect_reader_on_event,
    nfc_scene_mf_classic_mfkey_nonces_info_on_event,
    nfc_scene_mf_classic_mfkey_complete_on_event,
};

static void (*const mfkey_collector_on_exit_handlers[])(void*) = {
    nfc_scene_mf_classic_detect_reader_on_exit,
    nfc_scene_mf_classic_mfkey_nonces_info_on_exit,
    nfc_scene_mf_classic_mfkey_complete_on_exit,
};

const SceneManagerHandlers mfkey_collector_scene_handlers = {
    .on_enter_handlers = mfkey_collector_on_enter_handlers,
    .on_event_handlers = mfkey_collector_on_event_handlers,
    .on_exit_handlers = mfkey_collector_on_exit_handlers,
    .scene_num = NfcSceneNum,
};
