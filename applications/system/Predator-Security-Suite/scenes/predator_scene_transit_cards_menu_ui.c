#include "../predator_i.h"

// Transit Cards Menu - Main entry point for FeliCa & Calypso
// Following the proven Car Attacks menu pattern

typedef enum {
    TransitCardsMenuFeliCa,
    TransitCardsMenuCalypso,
    TransitCardsMenuBack,
} TransitCardsMenuItem;

void predator_scene_transit_cards_menu_submenu_callback(void* context, uint32_t index) {
    PredatorApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void predator_scene_transit_cards_menu_on_enter(void* context) {
    PredatorApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Transit Cards");

    // FeliCa - Japan/Asia-Pacific
    submenu_add_item(
        submenu,
        "🇯🇵 FeliCa (Japan/Asia)",
        TransitCardsMenuFeliCa,
        predator_scene_transit_cards_menu_submenu_callback,
        app);

    // Calypso - Europe
    submenu_add_item(
        submenu,
        "🇪🇺 Calypso (Europe)",
        TransitCardsMenuCalypso,
        predator_scene_transit_cards_menu_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Back",
        TransitCardsMenuBack,
        predator_scene_transit_cards_menu_submenu_callback,
        app);

    submenu_set_selected_item(
        submenu,
        scene_manager_get_scene_state(app->scene_manager, PredatorSceneTransitCardsMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
}

bool predator_scene_transit_cards_menu_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, PredatorSceneTransitCardsMenu, event.event);
        
        switch(event.event) {
        case TransitCardsMenuFeliCa:
            scene_manager_next_scene(app->scene_manager, PredatorSceneFelicaReader);
            consumed = true;
            break;
        case TransitCardsMenuCalypso:
            scene_manager_next_scene(app->scene_manager, PredatorSceneCalypsoBuyTicketUI);
            consumed = true;
            break;
        case TransitCardsMenuBack:
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void predator_scene_transit_cards_menu_on_exit(void* context) {
    PredatorApp* app = context;
    submenu_reset(app->submenu);
}
