#include "../predator_i.h"

// Calypso Actions Menu - What to do with detected card
// Pattern: Same as FeliCa Actions

typedef enum {
    CalypsoActionsViewJourney,
    CalypsoActionsViewContracts,
    CalypsoActionsDumpCard,
    CalypsoActionsAnalyze,
    CalypsoActionsBack,
} CalypsoActionsItem;

void predator_scene_calypso_actions_submenu_callback(void* context, uint32_t index) {
    PredatorApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void predator_scene_calypso_actions_ui_on_enter(void* context) {
    PredatorApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Calypso Actions");

    submenu_add_item(
        submenu,
        "🚇 View Journey Log",
        CalypsoActionsViewJourney,
        predator_scene_calypso_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "🎫 View Contracts",
        CalypsoActionsViewContracts,
        predator_scene_calypso_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "💾 Dump Card Data",
        CalypsoActionsDumpCard,
        predator_scene_calypso_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "🔍 Security Analysis",
        CalypsoActionsAnalyze,
        predator_scene_calypso_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Back",
        CalypsoActionsBack,
        predator_scene_calypso_actions_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
}

bool predator_scene_calypso_actions_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case CalypsoActionsViewJourney:
            scene_manager_next_scene(app->scene_manager, PredatorSceneCalypsoJourneyUI);
            consumed = true;
            break;
        case CalypsoActionsViewContracts:
            scene_manager_next_scene(app->scene_manager, PredatorSceneCalypsoContractsUI);
            consumed = true;
            break;
        case CalypsoActionsDumpCard:
            // TODO: Dump feature - disabled for now
            // scene_manager_next_scene(app->scene_manager, PredatorSceneCalypsoJourney);
            consumed = true;
            break;
        case CalypsoActionsAnalyze:
            // TODO: Analyze feature - disabled for now
            // scene_manager_next_scene(app->scene_manager, PredatorSceneCalypsoContracts);
            consumed = true;
            break;
        case CalypsoActionsBack:
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void predator_scene_calypso_actions_ui_on_exit(void* context) {
    PredatorApp* app = context;
    submenu_reset(app->submenu);
}
