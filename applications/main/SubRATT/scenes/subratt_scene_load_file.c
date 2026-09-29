#include "../subratt_i.h"
#include "subratt_scene.h"

#define TAG "SubRattSceneLoadFile"

void subratt_scene_load_file_on_enter(void* context) {
    furi_assert(context);
    SubRattState* instance = (SubRattState*)context;

    FuriString* app_folder;
    FuriString* load_path;
    load_path = furi_string_alloc();
    app_folder = furi_string_alloc_set(SUBRATT_PATH);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, SUBRATT_FILE_EXT, &I_sub1_10px);

    SubRattFileResult load_result;

#ifdef SUBRATT_FAST_TRACK
    bool res = true;
    furi_string_printf(load_path, "%s", "/ext/subghz/princeton.sub");
#else
    bool res =
        dialog_file_browser_show(instance->dialogs, load_path, app_folder, &browser_options);
#endif
#ifdef FURI_DEBUG
    FURI_LOG_D(
        TAG,
        "load_path: %s, app_folder: %s",
        furi_string_get_cstr(load_path),
        furi_string_get_cstr(app_folder));
#endif
    if(res) {
        load_result =
            subratt_device_load_from_file(instance->device, furi_string_get_cstr(load_path));
        if(load_result == SubRattFileResultOk) {
            instance->settings->last_index = SubRattAttackLoadFile;
            subratt_settings_set_repeats(
                instance->settings, subratt_main_view_get_repeats(instance->view_main));
            uint8_t extra_repeats = subratt_settings_get_current_repeats(instance->settings);

            load_result = subratt_device_attack_set(
                instance->device, instance->settings->last_index, extra_repeats);
            if(load_result == SubRattFileResultOk) {
                if(!subratt_worker_init_file_attack(
                       instance->worker,
                       instance->device->current_step,
                       instance->device->bit_index,
                       instance->device->key_from_file,
                       instance->device->file_protocol_info,
                       extra_repeats,
                       instance->device->two_bytes)) {
                    furi_crash("Invalid attack set!");
                }

                FURI_LOG_I(TAG, "Ready to run");
                load_result = true;
            }
        }

        if(load_result == SubRattFileResultOk) {
            subratt_settings_save(instance->settings);
            scene_manager_next_scene(instance->scene_manager, SubRattSceneLoadSelect);
        } else {
            FURI_LOG_E(TAG, "Returned error: %d", load_result);

            FuriString* dialog_msg;
            dialog_msg = furi_string_alloc();
            furi_string_cat_printf(
                dialog_msg, "Cannot parse\nfile: %s", subratt_device_error_get_desc(load_result));
            dialog_message_show_storage_error(instance->dialogs, furi_string_get_cstr(dialog_msg));
            furi_string_free(dialog_msg);
            scene_manager_search_and_switch_to_previous_scene(
                instance->scene_manager, SubRattSceneStart);
        }
    } else {
        scene_manager_search_and_switch_to_previous_scene(
            instance->scene_manager, SubRattSceneStart);
    }

    furi_string_free(app_folder);
    furi_string_free(load_path);
}

void subratt_scene_load_file_on_exit(void* context) {
    UNUSED(context);
}

bool subratt_scene_load_file_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}
