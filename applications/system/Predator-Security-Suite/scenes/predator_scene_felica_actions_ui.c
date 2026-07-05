#include "../predator_i.h"

// FeliCa Actions Menu - What to do with detected card
// Pattern: Similar to Car Key Actions

typedef enum {
    FelicaActionsViewHistory,
    FelicaActionsViewBalance,
    FelicaActionsDumpCard,
    FelicaActionsAnalyze,
    FelicaActionsBack,
} FelicaActionsItem;

void predator_scene_felica_actions_submenu_callback(void* context, uint32_t index) {
    PredatorApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void predator_scene_felica_actions_on_enter(void* context) {
    PredatorApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "FeliCa Actions");

    submenu_add_item(
        submenu,
        "📊 View Full History",
        FelicaActionsViewHistory,
        predator_scene_felica_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "💰 Check Balance",
        FelicaActionsViewBalance,
        predator_scene_felica_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "💾 Dump Card Data",
        FelicaActionsDumpCard,
        predator_scene_felica_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "🔍 Security Analysis",
        FelicaActionsAnalyze,
        predator_scene_felica_actions_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Back",
        FelicaActionsBack,
        predator_scene_felica_actions_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
}

bool predator_scene_felica_actions_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case FelicaActionsViewHistory:
            scene_manager_next_scene(app->scene_manager, PredatorSceneFelicaHistory);
            consumed = true;
            break;
        case FelicaActionsViewBalance:
            scene_manager_next_scene(app->scene_manager, PredatorSceneFelicaBalance);
            consumed = true;
            break;
        case FelicaActionsDumpCard:
            scene_manager_next_scene(app->scene_manager, PredatorSceneFelicaDump);
            consumed = true;
            break;
        case FelicaActionsAnalyze:
            // TODO: Analyze feature - disabled for now
            // scene_manager_next_scene(app->scene_manager, PredatorSceneFelicaHistory);
            consumed = true;
            break;
        case FelicaActionsBack:
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void predator_scene_felica_actions_on_exit(void* context) {
    PredatorApp* app = context;
    submenu_reset(app->submenu);
}
