#include "../predator_i.h"

// EM4305 Actions Menu
// Pattern: Same as other action menus

typedef enum {
    EM4305ActionsCloneEM4100,
    EM4305ActionsCloneHID,
    EM4305ActionsCustomWrite,
    EM4305ActionsPasswordAttack,
    EM4305ActionsRead,
    EM4305ActionsBack,
} EM4305ActionsItem;

void predator_scene_em4305_actions_submenu_callback(void* context, uint32_t index) {
    PredatorApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void predator_scene_em4305_actions_on_enter(void* context) {
    PredatorApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "EM4305 Actions");

    submenu_add_item(
        submenu,
        "🔄 Clone EM4100",
        EM4305ActionsCloneEM4100,
        predator_scene_em4305_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "🔑 Clone HID Prox",
        EM4305ActionsCloneHID,
        predator_scene_em4305_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "📝 Custom Write",
        EM4305ActionsCustomWrite,
        predator_scene_em4305_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "🔓 Password Attack",
        EM4305ActionsPasswordAttack,
        predator_scene_em4305_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "📖 Read/Dump",
        EM4305ActionsRead,
        predator_scene_em4305_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Back",
        EM4305ActionsBack,
        predator_scene_em4305_actions_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
}

bool predator_scene_em4305_actions_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case EM4305ActionsCloneEM4100:
        case EM4305ActionsCloneHID:
            scene_manager_next_scene(app->scene_manager, PredatorSceneEM4305Clone);
            consumed = true;
            break;
        case EM4305ActionsCustomWrite:
            scene_manager_next_scene(app->scene_manager, PredatorSceneEM4305CustomWrite);
            consumed = true;
            break;
        case EM4305ActionsPasswordAttack:
            scene_manager_next_scene(app->scene_manager, PredatorSceneEM4305PasswordAttack);
            consumed = true;
            break;
        case EM4305ActionsRead:
            // TODO: Read/dump scene
            consumed = true;
            break;
        case EM4305ActionsBack:
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void predator_scene_em4305_actions_on_exit(void* context) {
    PredatorApp* app = context;
    submenu_reset(app->submenu);
}
