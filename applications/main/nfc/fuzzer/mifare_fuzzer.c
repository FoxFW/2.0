#include "mifare_fuzzer_i.h"
#include "fuzzer.h"

static bool mifare_fuzzer_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    MifareFuzzerApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool mifare_fuzzer_back_event_callback(void* context) {
    furi_assert(context);
    MifareFuzzerApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void mifare_fuzzer_tick_event_callback(void* context) {
    furi_assert(context);
    MifareFuzzerApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

MifareFuzzerApp* mifare_fuzzer_alloc(Nfc* nfc) {
    MifareFuzzerApp* app = malloc(sizeof(MifareFuzzerApp));

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&mifare_fuzzer_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, mifare_fuzzer_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, mifare_fuzzer_back_event_callback);

    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, mifare_fuzzer_tick_event_callback, MIFARE_FUZZER_TICK_PERIOD);

    app->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->submenu_card = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MifareFuzzerViewSelectCard, submenu_get_view(app->submenu_card));

    app->submenu_attack = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MifareFuzzerViewSelectAttack, submenu_get_view(app->submenu_attack));

    app->emulator_view = mifare_fuzzer_emulator_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        MifareFuzzerViewEmulator,
        mifare_fuzzer_emulator_get_view(app->emulator_view));

    app->worker = mifare_fuzzer_worker_alloc(nfc);

    app->storage = furi_record_open(RECORD_STORAGE);
    if(!storage_simply_mkdir(app->storage, MIFARE_FUZZER_APP_FOLDER)) {
        FURI_LOG_E(TAG, "Could not create folder: %s", MIFARE_FUZZER_APP_FOLDER);
    }

    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->uid_str = furi_string_alloc();
    app->uid_file_path = furi_string_alloc();
    app->card_file_path = furi_string_alloc();
    app->app_folder = furi_string_alloc_set(MIFARE_FUZZER_APP_FOLDER);

    return app;
}

void mifare_fuzzer_free(MifareFuzzerApp* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, MifareFuzzerViewSelectCard);
    view_dispatcher_remove_view(app->view_dispatcher, MifareFuzzerViewSelectAttack);
    view_dispatcher_remove_view(app->view_dispatcher, MifareFuzzerViewEmulator);

    submenu_free(app->submenu_card);
    submenu_free(app->submenu_attack);

    view_dispatcher_free(app->view_dispatcher);

    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    app->gui = NULL;

    furi_record_close(RECORD_NOTIFICATION);
    app->notifications = NULL;

    mifare_fuzzer_worker_free(app->worker);

    furi_record_close(RECORD_STORAGE);
    app->storage = NULL;

    furi_record_close(RECORD_DIALOGS);
    app->dialogs = NULL;

    furi_string_free(app->uid_str);
    furi_string_free(app->uid_file_path);
    furi_string_free(app->app_folder);
    if(app->card_file_path != NULL) {
        furi_string_free(app->card_file_path);
    }

    free(app);
}

void fuzzer_mifare_run(Nfc* nfc) {
    MifareFuzzerApp* app = mifare_fuzzer_alloc(nfc);

    scene_manager_set_scene_state(app->scene_manager, MifareFuzzerSceneStart, 0);
    scene_manager_set_scene_state(app->scene_manager, MifareFuzzerSceneAttack, 0);

    scene_manager_next_scene(app->scene_manager, MifareFuzzerSceneStart);
    view_dispatcher_run(app->view_dispatcher);

    mifare_fuzzer_free(app);
}
