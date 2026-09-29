#include "../mifare_fuzzer_i.h"
#include "../mifare_fuzzer_custom_events.h"

enum SubmenuIndex {
    SubmenuIndexTestValue,
    SubmenuIndexRandomValuesAttack,
    SubmenuIndexLoadUIDsFromFile,
};

void mifare_fuzzer_scene_attack_submenu_callback(void* context, uint32_t index) {

    MifareFuzzerApp* app = context;
    uint8_t custom_event = 255;
    switch(index) {
    case SubmenuIndexTestValue:
        custom_event = MifareFuzzerEventTestValueAttack;
        break;
    case SubmenuIndexRandomValuesAttack:
        custom_event = MifareFuzzerEventRandomValuesAttack;
        break;
    case SubmenuIndexLoadUIDsFromFile:
        custom_event = MifareFuzzerEventLoadUIDsFromFileAttack;
        break;
    default:
        return;
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, custom_event);
}

void mifare_fuzzer_scene_attack_on_enter(void* context) {

    MifareFuzzerApp* app = context;

    Submenu* submenu_attack = app->submenu_attack;
    submenu_set_header(submenu_attack, "Mifare Fuzzer (attack)");
    submenu_add_item(
        submenu_attack,
        "Test Values",
        SubmenuIndexTestValue,
        mifare_fuzzer_scene_attack_submenu_callback,
        app);
    submenu_add_item(
        submenu_attack,
        "Random Values",
        SubmenuIndexRandomValuesAttack,
        mifare_fuzzer_scene_attack_submenu_callback,
        app);
    submenu_add_item(
        submenu_attack,
        "Load UIDs from file",
        SubmenuIndexLoadUIDsFromFile,
        mifare_fuzzer_scene_attack_submenu_callback,
        app);

    submenu_set_selected_item(
        submenu_attack,
        scene_manager_get_scene_state(app->scene_manager, MifareFuzzerSceneAttack));

    view_dispatcher_switch_to_view(app->view_dispatcher, MifareFuzzerViewSelectAttack);
}

bool mifare_fuzzer_scene_attack_on_event(void* context, SceneManagerEvent event) {

    MifareFuzzerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {

        if(event.event == MifareFuzzerEventTestValueAttack) {

            scene_manager_set_scene_state(
                app->scene_manager, MifareFuzzerSceneAttack, SubmenuIndexTestValue);

            app->attack = MifareFuzzerAttackTestValues;
            mifare_fuzzer_emulator_set_attack(app->emulator_view, app->attack);

            scene_manager_next_scene(app->scene_manager, MifareFuzzerSceneEmulator);
            consumed = true;
        } else if(event.event == MifareFuzzerEventRandomValuesAttack) {

            scene_manager_set_scene_state(
                app->scene_manager, MifareFuzzerSceneAttack, SubmenuIndexRandomValuesAttack);

            app->attack = MifareFuzzerAttackRandomValues;
            mifare_fuzzer_emulator_set_attack(app->emulator_view, app->attack);

            scene_manager_next_scene(app->scene_manager, MifareFuzzerSceneEmulator);
            consumed = true;
        } else if(event.event == MifareFuzzerEventLoadUIDsFromFileAttack) {

            scene_manager_set_scene_state(
                app->scene_manager, MifareFuzzerSceneAttack, SubmenuIndexLoadUIDsFromFile);

            app->attack = MifareFuzzerAttackLoadUidsFromFile;
            mifare_fuzzer_emulator_set_attack(app->emulator_view, app->attack);

            DialogsFileBrowserOptions browser_options;
            dialog_file_browser_set_basic_options(
                &browser_options, MIFARE_FUZZER_UID_FILE_EXT, NULL);
            browser_options.hide_ext = false;
            bool res = dialog_file_browser_show(
                app->dialogs, app->uid_file_path, app->app_folder, &browser_options);
            if(res) {
                app->uids_stream = buffered_file_stream_alloc(app->storage);
                res = buffered_file_stream_open(
                    app->uids_stream,
                    furi_string_get_cstr(app->uid_file_path),
                    FSAM_READ,
                    FSOM_OPEN_EXISTING);
                if(res) {

                    scene_manager_next_scene(app->scene_manager, MifareFuzzerSceneEmulator);
                } else {
                    buffered_file_stream_close(app->uids_stream);
                }
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {

    }

    return consumed;
}

void mifare_fuzzer_scene_attack_on_exit(void* context) {

    MifareFuzzerApp* app = context;
    submenu_reset(app->submenu_attack);
}
