#include "subratt_i.h"
#include "scenes/subratt_scene.h"

#include <string.h>
#include <storage/storage.h>
#include <loader/loader.h>

#define TAG "SubRattApp"

static bool subratt_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    SubRattState* instance = context;
    return scene_manager_handle_custom_event(instance->scene_manager, event);
}

static bool subratt_back_event_callback(void* context) {
    furi_assert(context);
    SubRattState* instance = context;
    return scene_manager_handle_back_event(instance->scene_manager);
}

static void subratt_tick_event_callback(void* context) {
    furi_assert(context);
    SubRattState* instance = context;
    scene_manager_handle_tick_event(instance->scene_manager);
}

SubRattState* subratt_alloc() {

    SubRattState* instance = calloc(1, sizeof(SubRattState));

    memset(instance->text_store, 0, sizeof(instance->text_store));
    instance->file_path = furi_string_alloc();

    instance->scene_manager = scene_manager_alloc(&subratt_scene_handlers, instance);
    instance->view_dispatcher = view_dispatcher_alloc();

    instance->gui = furi_record_open(RECORD_GUI);

    view_dispatcher_set_event_callback_context(instance->view_dispatcher, instance);
    view_dispatcher_set_custom_event_callback(
        instance->view_dispatcher, subratt_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        instance->view_dispatcher, subratt_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        instance->view_dispatcher, subratt_tick_event_callback, 100);

    instance->dialogs = furi_record_open(RECORD_DIALOGS);

    instance->notifications = furi_record_open(RECORD_NOTIFICATION);

    instance->loading = loading_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, SubRattViewLoading, loading_get_view(instance->loading));
    view_dispatcher_attach_to_gui(
        instance->view_dispatcher, instance->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(instance->view_dispatcher, SubRattViewLoading);
    notification_message(instance->notifications, &sequence_display_backlight_on);

    subghz_devices_init();

    FURI_LOG_I(TAG, "Acquiring radio device");
    instance->radio_device =
        subratt_radio_device_loader_set(NULL, SubGhzRadioDeviceTypeExternalCC1101);

    FURI_LOG_I(TAG, "Radio device acquired, calling reset");
    subghz_devices_reset(instance->radio_device);
    FURI_LOG_I(TAG, "Reset done, calling idle");
    subghz_devices_idle(instance->radio_device);
    FURI_LOG_I(TAG, "Idle done");

    FURI_LOG_I(TAG, "Calling subratt_device_alloc");
    instance->device = subratt_device_alloc(instance->radio_device);
    FURI_LOG_I(TAG, "subratt_device_alloc done");

    FURI_LOG_I(TAG, "Calling subratt_worker_alloc");
    instance->worker = subratt_worker_alloc(instance->radio_device);
    FURI_LOG_I(TAG, "subratt_worker_alloc done");

    instance->text_input = text_input_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher,
        SubRattViewTextInput,
        text_input_get_view(instance->text_input));
    FURI_LOG_I(TAG, "TextInput view added");

    instance->widget = widget_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, SubRattViewWidget, widget_get_view(instance->widget));
    FURI_LOG_I(TAG, "Widget view added");

    instance->var_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher,
        SubRattViewVarList,
        variable_item_list_get_view(instance->var_list));
    FURI_LOG_I(TAG, "VarList view added");

    instance->popup = popup_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, SubRattViewPopup, popup_get_view(instance->popup));
    FURI_LOG_I(TAG, "Popup view added");

    instance->view_stack = view_stack_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, SubRattViewStack, view_stack_get_view(instance->view_stack));
    FURI_LOG_I(TAG, "ViewStack view added");

    FURI_LOG_I(TAG, "Calling subratt_main_view_alloc");
    instance->view_main = subratt_main_view_alloc();
    FURI_LOG_I(TAG, "subratt_main_view_alloc done, adding view");
    view_dispatcher_add_view(
        instance->view_dispatcher,
        SubRattViewMain,
        subratt_main_view_get_view(instance->view_main));
    FURI_LOG_I(TAG, "SubRattMainView added");

    FURI_LOG_I(TAG, "Calling subratt_attack_view_alloc");
    instance->view_attack = subratt_attack_view_alloc();
    FURI_LOG_I(TAG, "subratt_attack_view_alloc done, adding view");
    view_dispatcher_add_view(
        instance->view_dispatcher,
        SubRattViewAttack,
        subratt_attack_view_get_view(instance->view_attack));
    FURI_LOG_I(TAG, "SubRattAttackView added");

    instance->view_attack_mode = subratt_attack_mode_view_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher,
        SubRattViewAttackMode,
        subratt_attack_mode_view_get_view(instance->view_attack_mode));
    FURI_LOG_I(TAG, "SubRattAttackModeView added");

    FURI_LOG_I(TAG, "Calling subratt_settings_alloc/load");
    instance->settings = subratt_settings_alloc();
    subratt_settings_load(instance->settings);
    FURI_LOG_I(TAG, "subratt_alloc finished, returning instance");

    return instance;
}

void subratt_free(SubRattState* instance) {
    furi_assert(instance);

    subratt_worker_stop(instance->worker);
    subratt_worker_free(instance->worker);

    subratt_device_free(instance->device);
    subghz_devices_deinit();

    subratt_settings_free(instance->settings);

    notification_message(instance->notifications, &sequence_blink_stop);
    furi_record_close(RECORD_NOTIFICATION);
    instance->notifications = NULL;

    view_dispatcher_remove_view(instance->view_dispatcher, SubRattViewMain);
    subratt_main_view_free(instance->view_main);

    view_dispatcher_remove_view(instance->view_dispatcher, SubRattViewAttack);
    subratt_attack_view_free(instance->view_attack);

    view_dispatcher_remove_view(instance->view_dispatcher, SubRattViewAttackMode);
    subratt_attack_mode_view_free(instance->view_attack_mode);

    view_dispatcher_remove_view(instance->view_dispatcher, SubRattViewLoading);
    loading_free(instance->loading);

    view_dispatcher_remove_view(instance->view_dispatcher, SubRattViewTextInput);
    text_input_free(instance->text_input);

    view_dispatcher_remove_view(instance->view_dispatcher, SubRattViewWidget);
    widget_free(instance->widget);

    view_dispatcher_remove_view(instance->view_dispatcher, SubRattViewVarList);
    variable_item_list_free(instance->var_list);

    view_dispatcher_remove_view(instance->view_dispatcher, SubRattViewPopup);
    popup_free(instance->popup);

    view_dispatcher_remove_view(instance->view_dispatcher, SubRattViewStack);
    view_stack_free(instance->view_stack);

    furi_record_close(RECORD_DIALOGS);
    instance->dialogs = NULL;

    scene_manager_free(instance->scene_manager);

    view_dispatcher_free(instance->view_dispatcher);

    furi_record_close(RECORD_GUI);
    instance->gui = NULL;

    furi_string_free(instance->file_path);

    free(instance);
}

void subratt_text_input_callback(void* context) {
    furi_assert(context);
    SubRattState* instance = context;
    view_dispatcher_send_custom_event(
        instance->view_dispatcher, SubRattCustomEventTypeTextEditDone);
}

void subratt_popup_closed_callback(void* context) {
    furi_assert(context);
    SubRattState* instance = context;
    view_dispatcher_send_custom_event(
        instance->view_dispatcher, SubRattCustomEventTypePopupClosed);
}

int32_t subratt_app(void* p) {
    char return_marker[24] = {0};
    if(p && ((const char*)p)[0]) {
        const char* arg = (const char*)p;
        if(strncmp(arg, "core:", 5) == 0) {
            arg += 5;
        }
        strncpy(return_marker, arg, sizeof(return_marker) - 1);
    }

    furi_hal_power_suppress_charge_enter();

    SubRattState* instance = subratt_alloc();

    FURI_LOG_I(TAG, "subratt_alloc returned, entering SubRattSceneStart");
    scene_manager_next_scene(instance->scene_manager, SubRattSceneStart);
    FURI_LOG_I(TAG, "SubRattSceneStart entered, starting view_dispatcher_run");

    view_dispatcher_run(instance->view_dispatcher);
    FURI_LOG_I(TAG, "view_dispatcher_run returned (app exiting)");

    subratt_free(instance);

    furi_hal_power_suppress_charge_exit();

    if(return_marker[0]) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        File* file = storage_file_alloc(storage);
        if(storage_file_open(file, "/ext/subghz/.focus_menu", FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_write(file, return_marker, strlen(return_marker));
        }
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);

        Loader* loader = furi_record_open(RECORD_LOADER);
        loader_enqueue_launch(loader, "subghz", NULL, LoaderDeferredLaunchFlagNone);
        furi_record_close(RECORD_LOADER);
    }

    return 0;
}
