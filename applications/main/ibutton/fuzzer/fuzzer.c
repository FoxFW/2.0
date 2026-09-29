#include "fuzzer_i.h"
#include "fuzzer.h"
#include "helpers/fuzzer_types.h"

static bool fuzzer_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    PacsFuzzerApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool fuzzer_app_back_event_callback(void* context) {
    furi_assert(context);
    PacsFuzzerApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void fuzzer_app_tick_event_callback(void* context) {
    furi_assert(context);
    PacsFuzzerApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

PacsFuzzerApp* fuzzer_app_alloc() {
    PacsFuzzerApp* app = malloc(sizeof(PacsFuzzerApp));

    app->fuzzer_state.menu_index = 0;
    app->fuzzer_state.proto_index = 0;

    app->worker = fuzzer_worker_alloc();
    app->payload = fuzzer_payload_alloc();

    app->file_path = furi_string_alloc();

    app->gui = furi_record_open(RECORD_GUI);

    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, FuzzerViewIDPopup, popup_get_view(app->popup));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FuzzerViewIDTextInput, text_input_get_view(app->text_input));

    app->main_view = fuzzer_view_main_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FuzzerViewIDMain, fuzzer_view_main_get_view(app->main_view));

    app->attack_view = fuzzer_view_attack_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FuzzerViewIDAttack, fuzzer_view_attack_get_view(app->attack_view));

    app->field_editor_view = fuzzer_view_field_editor_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FuzzerViewIDFieldEditor,
        fuzzer_view_field_editor_get_view(app->field_editor_view));

    app->scene_manager = scene_manager_alloc(&fuzzer_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, fuzzer_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, fuzzer_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, fuzzer_app_tick_event_callback, 100);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(app->scene_manager, FuzzerSceneMain);

    return app;
}

void fuzzer_app_free(PacsFuzzerApp* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, FuzzerViewIDMain);
    fuzzer_view_main_free(app->main_view);

    view_dispatcher_remove_view(app->view_dispatcher, FuzzerViewIDAttack);
    fuzzer_view_attack_free(app->attack_view);

    view_dispatcher_remove_view(app->view_dispatcher, FuzzerViewIDFieldEditor);
    fuzzer_view_field_editor_free(app->field_editor_view);

    view_dispatcher_remove_view(app->view_dispatcher, FuzzerViewIDPopup);
    popup_free(app->popup);

    view_dispatcher_remove_view(app->view_dispatcher, FuzzerViewIDTextInput);
    text_input_free(app->text_input);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_DIALOGS);

    furi_record_close(RECORD_GUI);

    furi_record_close(RECORD_NOTIFICATION);
    app->notifications = NULL;

    furi_string_free(app->file_path);

    fuzzer_payload_free(app->payload);
    fuzzer_worker_free(app->worker);

    free(app);
}

void fuzzer_ibtn_run(void) {
    PacsFuzzerApp* fuzzer_app = fuzzer_app_alloc();

    FuzzerConsts app_const = {
        .custom_dict_folder = "/ext/ibtnfuzzer",
        .custom_dict_extension = ".txt",
        .key_extension = ".ibtn",
        .path_key_folder = "/ext/ibutton",
        .key_icon = &I_ibutt_10px,
        .file_prefix = "iBtn",
    };
    fuzzer_app->fuzzer_const = &app_const;

    view_dispatcher_run(fuzzer_app->view_dispatcher);

    fuzzer_app_free(fuzzer_app);
}
