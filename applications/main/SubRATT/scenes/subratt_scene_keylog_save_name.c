#include "../subratt_i.h"
#include "subratt_scene.h"
#include <storage/storage.h>
#include <toolbox/name_generator.h>

#define TAG "SubRattSceneKeylogSaveName"

void subratt_scene_keylog_save_name_on_enter(void* context) {
    SubRattState* instance = (SubRattState*)context;

    TextInput* text_input = instance->text_input;

    if(instance->device->attack == SubRattAttackLoadFile) {
        name_generator_make_auto(
            instance->text_store,
            sizeof(instance->text_store),
            subratt_protocol_file(instance->device->file_protocol_info->file));
    } else {
        name_generator_make_auto(
            instance->text_store,
            sizeof(instance->text_store),
            subratt_protocol_file(instance->device->protocol_info->file));
    }
    text_input_set_header_text(text_input, "Name of keys file");
    text_input_set_result_callback(
        text_input,
        subratt_text_input_callback,
        instance,
        instance->text_store,
        SUBRATT_MAX_LEN_NAME,
        true);

    furi_string_reset(instance->file_path);
    furi_string_set_str(instance->file_path, SUBRATT_PATH);

    ValidatorIsFile* validator_is_file = validator_is_file_alloc_init(
        furi_string_get_cstr(instance->file_path), SUBRATT_KEYLOG_EXT, "");
    text_input_set_validator(text_input, validator_is_file_callback, validator_is_file);

    view_dispatcher_switch_to_view(instance->view_dispatcher, SubRattViewTextInput);
}

bool subratt_scene_keylog_save_name_on_event(void* context, SceneManagerEvent event) {
    SubRattState* instance = (SubRattState*)context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(instance->scene_manager);
        return true;
    } else if(
        event.type == SceneManagerEventTypeCustom &&
        event.event == SubRattCustomEventTypeTextEditDone) {
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Saving: %s", instance->text_store);
#endif
        bool success = false;
        if(strcmp(instance->text_store, "") != 0) {
            furi_string_reset(instance->file_path);
            furi_string_cat_printf(
                instance->file_path,
                "%s/%s%s",
                SUBRATT_PATH,
                instance->text_store,
                SUBRATT_KEYLOG_EXT);

            Storage* storage = furi_record_open(RECORD_STORAGE);
            FS_Error rename_result = storage_common_rename(
                storage,
                SUBRATT_KEYLOG_SESSION_PATH,
                furi_string_get_cstr(instance->file_path));
            furi_record_close(RECORD_STORAGE);

            if(rename_result == FSE_OK) {
                scene_manager_next_scene(instance->scene_manager, SubRattSceneKeylogSaveSuccess);
                success = true;
                consumed = true;
            }
        }

        if(!success) {
            dialog_message_show_storage_error(instance->dialogs, "Error saving keys file!");
            consumed = scene_manager_search_and_switch_to_previous_scene(
                instance->scene_manager, SubRattSceneSetupAttack);
        }
    }

    return consumed;
}

void subratt_scene_keylog_save_name_on_exit(void* context) {
    SubRattState* instance = (SubRattState*)context;

    void* validator_context = text_input_get_validator_callback_context(instance->text_input);
    text_input_set_validator(instance->text_input, NULL, NULL);
    validator_is_file_free(validator_context);

    text_input_reset(instance->text_input);

    furi_string_reset(instance->file_path);
}
