#include "../predator_i.h"

// Wiegand Actions Menu - After card detected
// Pattern: Same as FeliCa Actions

typedef enum {
    WiegandActionsEmulate,
    WiegandActionsGenerate,
    WiegandActionsBruteForce,
    WiegandActionsSave,
    WiegandActionsBack,
} WiegandActionsItem;

void predator_scene_wiegand_actions_submenu_callback(void* context, uint32_t index) {
    PredatorApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void predator_scene_wiegand_actions_on_enter(void* context) {
    PredatorApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Wiegand Actions");

    submenu_add_item(
        submenu,
        "📡 Emulate Card",
        WiegandActionsEmulate,
        predator_scene_wiegand_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "🔢 Generate Custom",
        WiegandActionsGenerate,
        predator_scene_wiegand_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "💥 Brute Force",
        WiegandActionsBruteForce,
        predator_scene_wiegand_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "💾 Save Card",
        WiegandActionsSave,
        predator_scene_wiegand_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Back",
        WiegandActionsBack,
        predator_scene_wiegand_actions_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
}

bool predator_scene_wiegand_actions_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case WiegandActionsEmulate:
            scene_manager_next_scene(app->scene_manager, PredatorSceneWiegandEmulate);
            consumed = true;
            break;
        case WiegandActionsGenerate:
            scene_manager_next_scene(app->scene_manager, PredatorSceneWiegandGenerate);
            consumed = true;
            break;
        case WiegandActionsBruteForce:
            scene_manager_next_scene(app->scene_manager, PredatorSceneWiegandBruteForce);
            consumed = true;
            break;
        case WiegandActionsSave:
            // TODO: Save to SD card
            consumed = true;
            break;
        case WiegandActionsBack:
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void predator_scene_wiegand_actions_on_exit(void* context) {
    PredatorApp* app = context;
    submenu_reset(app->submenu);
}
