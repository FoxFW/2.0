#include "../predator_i.h"

// ISO 15693 Actions Menu
// Pattern: Same as other action menus

typedef enum {
    ISO15693ActionsReadBlocks,
    ISO15693ActionsWriteBlocks,
    ISO15693ActionsPasswordAttack,
    ISO15693ActionsDump,
    ISO15693ActionsEAS,
    ISO15693ActionsBack,
} ISO15693ActionsItem;

void predator_scene_iso15693_actions_submenu_callback(void* context, uint32_t index) {
    PredatorApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void predator_scene_iso15693_actions_on_enter(void* context) {
    PredatorApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "ISO 15693 Actions");

    submenu_add_item(
        submenu,
        "📖 Read All Blocks",
        ISO15693ActionsReadBlocks,
        predator_scene_iso15693_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "✏️ Write Blocks",
        ISO15693ActionsWriteBlocks,
        predator_scene_iso15693_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "🔓 Password Attack",
        ISO15693ActionsPasswordAttack,
        predator_scene_iso15693_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "💾 Dump to File",
        ISO15693ActionsDump,
        predator_scene_iso15693_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "🏷️ EAS Control",
        ISO15693ActionsEAS,
        predator_scene_iso15693_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Back",
        ISO15693ActionsBack,
        predator_scene_iso15693_actions_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
}

bool predator_scene_iso15693_actions_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case ISO15693ActionsReadBlocks:
            scene_manager_next_scene(app->scene_manager, PredatorSceneISO15693BlockViewer);
            consumed = true;
            break;
        case ISO15693ActionsWriteBlocks:
            // TODO: Write blocks scene
            consumed = true;
            break;
        case ISO15693ActionsPasswordAttack:
            scene_manager_next_scene(app->scene_manager, PredatorSceneISO15693PasswordAttack);
            consumed = true;
            break;
        case ISO15693ActionsDump:
            // TODO: Dump scene
            consumed = true;
            break;
        case ISO15693ActionsEAS:
            scene_manager_next_scene(app->scene_manager, PredatorSceneISO15693EAS);
            consumed = true;
            break;
        case ISO15693ActionsBack:
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void predator_scene_iso15693_actions_on_exit(void* context) {
    PredatorApp* app = context;
    submenu_reset(app->submenu);
}
