#include "../predator_i.h"
#include "../helpers/predator_logging.h"
#include <furi.h>

// WORLDWIDE ENTERPRISE - REGION SELECTION FOR SMART BARRIER ATTACKS
// Automatically selects region-appropriate manufacturers

typedef enum {
    BarrierRegionWorldwide,     // Try all 35 manufacturers
    BarrierRegionEurope,        // 15 European manufacturers
    BarrierRegionNorthAmerica,  // 5 North American manufacturers
    BarrierRegionJapanAsia,     // 5 Japan/Asian manufacturers
    BarrierRegionAustralia,     // 1 Australian manufacturer
    BarrierRegionLatinAmerica,  // 2 Latin American manufacturers
    BarrierRegionMEA,           // 2 Middle East/Africa manufacturers
    BarrierRegionHighSecurity,  // 5 High security/specialized
    BarrierRegionCount
} BarrierRegion;

static void barrier_region_submenu_cb(void* context, uint32_t index) {
    PredatorApp* app = context;
    if(!app) return;
    if(app->view_dispatcher) {
        view_dispatcher_send_custom_event(app->view_dispatcher, index);
    }
}

void predator_scene_barrier_region_select_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app || !app->submenu) return;
    
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "SELECT REGION");
    
    // Worldwide option - tries all 35 manufacturers
    submenu_add_item(app->submenu, "🌍 Worldwide (All 35)", BarrierRegionWorldwide, barrier_region_submenu_cb, app);
    
    // Regional options - auto-selects region-specific manufacturers
    submenu_add_item(app->submenu, "🇪🇺 Europe (15 brands)", BarrierRegionEurope, barrier_region_submenu_cb, app);
    submenu_add_item(app->submenu, "🇺🇸 North America (5)", BarrierRegionNorthAmerica, barrier_region_submenu_cb, app);
    submenu_add_item(app->submenu, "🇯🇵 Japan/Asia (5)", BarrierRegionJapanAsia, barrier_region_submenu_cb, app);
    submenu_add_item(app->submenu, "🇦🇺 Australia (1)", BarrierRegionAustralia, barrier_region_submenu_cb, app);
    submenu_add_item(app->submenu, "🇧🇷 Latin America (2)", BarrierRegionLatinAmerica, barrier_region_submenu_cb, app);
    submenu_add_item(app->submenu, "🌍 M.East/Africa (2)", BarrierRegionMEA, barrier_region_submenu_cb, app);
    submenu_add_item(app->submenu, "🔒 High Security (5)", BarrierRegionHighSecurity, barrier_region_submenu_cb, app);
    
    submenu_set_selected_item(app->submenu, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
    
    predator_log_append(app, "BARRIER REGION: Select your location");
    FURI_LOG_I("BarrierRegion", "Region selection initialized");
}

bool predator_scene_barrier_region_select_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    if(!app) return false;
    
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event < BarrierRegionCount) {
            // Store selected region in app context
            app->selected_barrier_region = event.event;
            
            const char* region_names[] = {
                "Worldwide (All 35)",
                "Europe (15 manufacturers)",
                "North America (5 manufacturers)",
                "Japan/Asia (5 manufacturers)",
                "Australia (B&D only)",
                "Latin America (2 manufacturers)",
                "Middle East/Africa (2 manufacturers)",
                "High Security (5 specialized)"
            };
            
            char log_msg[64];
            snprintf(log_msg, sizeof(log_msg), "REGION: %s", region_names[event.event]);
            predator_log_append(app, log_msg);
            
            FURI_LOG_I("BarrierRegion", "Region selected: %lu (%s)", event.event, region_names[event.event]);
            
            // Navigate to facility type selection
            scene_manager_next_scene(app->scene_manager, PredatorSceneParkingBarriersUI);
            return true;
        }
    }
    
    return false;
}

void predator_scene_barrier_region_select_ui_on_exit(void* context) {
    (void)context;
    // No cleanup needed
}
