#include "mfkey_collector_i.h"

#include <string.h>

/* FoxFW MFKey Nonce Collector — externalized "Extract MFC Keys" reader-attack.
 * See mfkey_collector_i.h for the rationale. The collection/save logic is a
 * byte-for-byte copy of the NFC app's original implementation. */

void nfc_blink_stop(NfcApp* nfc) {
    notification_message(nfc->notifications, &sequence_blink_stop);
}

void nfc_app_run_external(NfcApp* nfc, const char* app_path) {
    furi_assert(nfc);
    furi_assert(app_path);

    /* Launch the MFKey32 cracker, then return to the NFC app. The pending queue
     * currently holds the NFC-app relaunch that was enqueued when this collector
     * was started; clear it and rebuild the order so the cracker runs first and
     * the NFC app follows. */
    Loader* loader = furi_record_open(RECORD_LOADER);
    loader_clear_launch_queue(loader);
    loader_enqueue_launch(loader, app_path, NULL, LoaderDeferredLaunchFlagGui);
    loader_enqueue_launch(loader, "nfc", NULL, LoaderDeferredLaunchFlagGui);
    furi_record_close(RECORD_LOADER);

    view_dispatcher_stop(nfc->view_dispatcher);
}

static bool mfkey_collector_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    NfcApp* instance = context;
    return scene_manager_handle_custom_event(instance->scene_manager, event);
}

static bool mfkey_collector_back_event_callback(void* context) {
    furi_assert(context);
    NfcApp* instance = context;
    return scene_manager_handle_back_event(instance->scene_manager);
}

static NfcApp* mfkey_collector_app_alloc(void) {
    NfcApp* instance = malloc(sizeof(NfcApp));
    memset(instance, 0, sizeof(NfcApp));

    instance->gui = furi_record_open(RECORD_GUI);
    instance->notifications = furi_record_open(RECORD_NOTIFICATION);
    instance->storage = furi_record_open(RECORD_STORAGE);

    instance->view_dispatcher = view_dispatcher_alloc();
    instance->scene_manager = scene_manager_alloc(&mfkey_collector_scene_handlers, instance);
    view_dispatcher_set_event_callback_context(instance->view_dispatcher, instance);
    view_dispatcher_set_custom_event_callback(
        instance->view_dispatcher, mfkey_collector_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        instance->view_dispatcher, mfkey_collector_back_event_callback);

    instance->nfc = nfc_alloc();
    instance->nfc_device = nfc_device_alloc();

    instance->widget = widget_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, NfcViewWidget, widget_get_view(instance->widget));

    instance->detect_reader = detect_reader_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher,
        NfcViewDetectReader,
        detect_reader_get_view(instance->detect_reader));

    instance->listener = NULL;
    instance->mfkey32_logger = NULL;
    instance->timer = NULL;

    return instance;
}

static void mfkey_collector_app_free(NfcApp* instance) {
    furi_assert(instance);

    /* Defensive cleanup: if the app exits straight from the root (emulation)
     * scene, that scene's on_exit is not invoked, so release anything still held. */
    if(instance->listener) {
        nfc_listener_stop(instance->listener);
        nfc_listener_free(instance->listener);
        instance->listener = NULL;
    }
    if(instance->timer) {
        furi_timer_stop(instance->timer);
        furi_timer_free(instance->timer);
        instance->timer = NULL;
    }
    if(instance->mfkey32_logger) {
        mfkey32_logger_free(instance->mfkey32_logger);
        instance->mfkey32_logger = NULL;
    }
    notification_message(instance->notifications, &sequence_blink_stop);

    view_dispatcher_remove_view(instance->view_dispatcher, NfcViewWidget);
    view_dispatcher_remove_view(instance->view_dispatcher, NfcViewDetectReader);
    widget_free(instance->widget);
    detect_reader_free(instance->detect_reader);

    view_dispatcher_free(instance->view_dispatcher);
    scene_manager_free(instance->scene_manager);

    nfc_device_free(instance->nfc_device);
    nfc_free(instance->nfc);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(instance);
}

int32_t mfkey_collector_app(void* p) {
    NfcApp* instance = mfkey_collector_app_alloc();

    view_dispatcher_attach_to_gui(
        instance->view_dispatcher, instance->gui, ViewDispatcherTypeFullscreen);

    /* Optional launch arg: a .nfc card path handed over by the NFC app for the
     * per-card reader attack. With no arg the emulation scene makes a fresh
     * random MIFARE Classic 4K card (the main-menu "Extract MFC Keys" behaviour).
     * If the load fails, nfc_device stays empty and the random-card path runs. */
    const char* args = p;
    if(args && strlen(args)) {
        nfc_device_load(instance->nfc_device, args);
    }

    scene_manager_next_scene(instance->scene_manager, NfcSceneMfClassicDetectReader);
    view_dispatcher_run(instance->view_dispatcher);

    mfkey_collector_app_free(instance);

    return 0;
}
