#include "../predator_i.h"
#include "../helpers/predator_logging.h"

// ENTERPRISE PROFESSIONAL - BARRIER MANUFACTURER SELECTION
// NEW FLOW: Auto-navigates based on region, no manual selection needed

// OLD: Manual selection callback (unused in new streamlined flow)
/*
static void manufacturer_select_submenu_cb(void* context, uint32_t index) {
    PredatorApp* app = context;
    if(!app || !app->view_dispatcher) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}
*/

void predator_scene_barrier_manufacturer_select_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    
    // SMART FLOW: Auto-select manufacturers based on region and skip directly to attack
    // Region 0 = Worldwide (try all 35), Regions 1-7 = specific regions
    
    if(app->selected_barrier_region == 0) {
        // Worldwide - try all 35 manufacturers
        app->selected_barrier_manufacturer = 0xFF;
        predator_log_append(app, "AUTO: Trying All 35 Manufacturers Worldwide");
        scene_manager_next_scene(app->scene_manager, PredatorSceneBarrierAttackUI);
        return;
    }
    
    // Region-specific auto-selection
    // For now, auto-try all manufacturers in the selected region
    app->selected_barrier_manufacturer = 0xFF;  // Will be filtered by region in attack scene
    
    const char* region_names[] = {
        "Worldwide", "Europe (15)", "N.America (5)", "Japan/Asia (5)",
        "Australia (1)", "LatinAm (2)", "MEA (2)", "HighSec (5)"
    };
    
    char log_msg[64];
    snprintf(log_msg, sizeof(log_msg), "AUTO: Trying %s manufacturers",
             app->selected_barrier_region < 8 ? region_names[app->selected_barrier_region] : "Unknown");
    predator_log_append(app, log_msg);
    
    // Skip manufacturer selection - go directly to attack
    scene_manager_next_scene(app->scene_manager, PredatorSceneBarrierAttackUI);
    return;
    
    // OLD CODE: Manual manufacturer selection (commented out for streamlined flow)
    /*
    if(!app->submenu) return;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "SELECT MANUFACTURER");
    
    // Option 0: Try ALL 35 manufacturers automatically
    submenu_add_item(app->submenu, "⚡ Try All 35 (Auto)", 0, manufacturer_select_submenu_cb, app);
    
    // === EUROPE (10 manufacturers) ===
    submenu_add_item(app->submenu, "🇪🇺 CAME (Italy)", 1, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇪🇺 BFT (Italy)", 2, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇪🇺 Nice (France)", 3, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇪🇺 Somfy (France)", 4, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇪🇺 Faac (Italy)", 5, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇪🇺 Hörmann (EU)", 6, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇪🇺 Benincà (Italy)", 7, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇪🇺 DEA System (Italy)", 8, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇪🇺 Ditec (Italy)", 9, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇪🇺 Roger Tech (Italy)", 10, manufacturer_select_submenu_cb, app);
    
    // === NORTH AMERICA (5 manufacturers) ===
    submenu_add_item(app->submenu, "🇺🇸 Chamberlain (USA)", 11, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇺🇸 LiftMaster (USA)", 12, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇺🇸 Linear (USA)", 13, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇺🇸 Genie (USA)", 14, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇺🇸 Overhead Door (USA)", 15, manufacturer_select_submenu_cb, app);
    
    // === ADDITIONAL EUROPE (5 manufacturers) ===
    submenu_add_item(app->submenu, "🇩🇪 Marantec (Germany)", 16, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇩🇪 Sommer (Germany)", 17, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇪🇸 ERREKA (Spain)", 18, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇩🇪 Novoferm (Germany)", 19, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇮🇹 V2 (Italy)", 20, manufacturer_select_submenu_cb, app);
    
    // === ASIA-PACIFIC / GLOBAL (3 manufacturers) ===
    submenu_add_item(app->submenu, "🇨🇳 ET (China)", 21, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🌍 SERAI (M.East)", 22, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇿🇦 Centurion (Africa)", 23, manufacturer_select_submenu_cb, app);
    
    // === HIGH SECURITY / PARKING (2 manufacturers) ===
    submenu_add_item(app->submenu, "🔒 Auto Systems (BE)", 24, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🏛️ Parkare (Parking)", 25, manufacturer_select_submenu_cb, app);
    
    // === JAPAN / ADDITIONAL ASIA (4 manufacturers) ===
    submenu_add_item(app->submenu, "🇯🇵 TOYO (Japan)", 26, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🌍 ASSA ABLOY (Global)", 27, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇯🇵 FUJITEC (Japan)", 28, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇨🇳 NICE-Apollo (China)", 29, manufacturer_select_submenu_cb, app);
    
    // === AUSTRALIA / OCEANIA (1 manufacturer) ===
    submenu_add_item(app->submenu, "🇦🇺 B&D (Australia)", 30, manufacturer_select_submenu_cb, app);
    
    // === LATIN AMERICA (2 manufacturers) ===
    submenu_add_item(app->submenu, "🇧🇷 PPA (Brazil)", 31, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🇦🇷 SEG (Argentina)", 32, manufacturer_select_submenu_cb, app);
    
    // === GLOBAL HIGH-END (3 manufacturers) ===
    submenu_add_item(app->submenu, "🇨🇭 DORMA+KABA (Swiss)", 33, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🌟 Stanley Access (USA)", 34, manufacturer_select_submenu_cb, app);
    submenu_add_item(app->submenu, "🌐 CAME Auto (Global)", 35, manufacturer_select_submenu_cb, app);
    
    submenu_set_selected_item(app->submenu, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
    
    FURI_LOG_I("ManufacturerSelect", "Manufacturer selection menu initialized");
}

bool predator_scene_barrier_manufacturer_select_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    if(!app) return false;
    
    // Handle back button - CRITICAL: Match CarModelsUI pattern exactly
    if(event.type == SceneManagerEventTypeBack) {
        // Go back to parking barriers selection
        scene_manager_previous_scene(app->scene_manager);
        return true;  // Must return true when explicitly calling scene_manager_previous_scene()
    }
    
    // Handle manufacturer selection
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event <= 35) {
            // Store manufacturer selection
            if(event.event == 0) {
                // Try all manufacturers (now 35 total!)
                app->selected_barrier_manufacturer = 0xFF;  // Special value for "try all"
                predator_log_append(app, "MODE: Try All 35 Manufacturers Worldwide");
            } else {
                // Specific manufacturer (1-35 maps to BarrierManufacturer enum 0-34)
                app->selected_barrier_manufacturer = event.event - 1;
                
                const char* manufacturer_names[] = {
                    // Europe (10)
                    "CAME (Italy)", "BFT (Italy)", "Nice (France)",
                    "Somfy (France)", "Faac (Italy)", "Hörmann (EU)",
                    "Benincà (Italy)", "DEA System (Italy)", "Ditec (Italy)", "Roger Tech (Italy)",
                    // North America (5)
                    "Chamberlain (USA)", "LiftMaster (USA)", "Linear (USA)",
                    "Genie (USA)", "Overhead Door (USA)",
                    // Additional Europe (5)
                    "Marantec (Germany)", "Sommer (Germany)", "ERREKA (Spain)",
                    "Novoferm (Germany)", "V2 (Italy)",
                    // Asia-Pacific / Global (3)
                    "ET (China)", "SERAI (M.East)", "Centurion (Africa)",
                    // High Security (2)
                    "Auto Systems (BE)", "Parkare (Parking)",
                    // Japan / Asia (4)
                    "TOYO (Japan)", "ASSA ABLOY (Global)", "FUJITEC (Japan)", "NICE-Apollo (China)",
                    // Australia (1)
                    "B&D (Australia)",
                    // Latin America (2)
                    "PPA (Brazil)", "SEG (Argentina)",
                    // Global High-End (3)
                    "DORMA+KABA (Swiss)", "Stanley Access (USA)", "CAME Auto (Global)"
                };
                
                char log_msg[64];
                snprintf(log_msg, sizeof(log_msg), "MANUFACTURER: %s",
                        manufacturer_names[app->selected_barrier_manufacturer]);
                predator_log_append(app, log_msg);
            }
            
            // Navigate to barrier attack
            scene_manager_next_scene(app->scene_manager, PredatorSceneBarrierAttackUI);
            return true;
        }
        return true;
    }
    
    return false;
    */
}

// Event handler - not needed since scene auto-navigates
bool predator_scene_barrier_manufacturer_select_ui_on_event(void* context, SceneManagerEvent event) {
    (void)context;
    (void)event;
    return false;
}

void predator_scene_barrier_manufacturer_select_ui_on_exit(void* context) {
    (void)context;
    // Nothing to cleanup
}
