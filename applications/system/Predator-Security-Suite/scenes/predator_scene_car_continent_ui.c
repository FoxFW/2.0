#include "../predator_i.h"
#include "predator_scene.h"
#include "../helpers/predator_models_hardcoded.h"

// Continent selection scene - Europe/Asia/America
// User selects continent, then sees car models from that continent only

static void continent_submenu_callback(void* context, uint32_t index) {
    PredatorApp* app = context;
    if(!app || !app->view_dispatcher) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void predator_scene_car_continent_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app || !app->submenu) return;
    
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "🌍 Select Continent");
    
    // Use static labels to save stack memory (counts pre-calculated)
    // Europe: 124, Asia: 154, America: 37 (from database)
    submenu_add_item(app->submenu, "🇪🇺 Europe (124 models)", CarContinentEurope, continent_submenu_callback, app);
    submenu_add_item(app->submenu, "🇯🇵 Asia (154 models)", CarContinentAsia, continent_submenu_callback, app);
    submenu_add_item(app->submenu, "🇺🇸 America (37 models)", CarContinentAmerica, continent_submenu_callback, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
}

bool predator_scene_car_continent_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    if(!app) return false;
    
    if(event.type == SceneManagerEventTypeBack) {
        // Go back to main menu
        return false;
    }
    
    if(event.type == SceneManagerEventTypeCustom) {
        // User selected a continent
        if(event.event < CarContinentCount) {
            // Store selected continent in app state
            app->selected_continent = (CarContinent)event.event;
            
            // Navigate to models scene (which will filter by continent)
            scene_manager_next_scene(app->scene_manager, PredatorSceneCarModelsUI);
            return true;
        }
    }
    
    return false;
}

void predator_scene_car_continent_ui_on_exit(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    // Nothing to cleanup - using shared submenu
}
