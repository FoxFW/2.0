#include "../predator_i.h"
#include "predator_scene.h"
#include "predator_submenu_index.h"

/**
 * Building Access Testing - Main Menu
 * 
 * v2.1 "Building Access Edition" - NEW FEATURE
 * 
 * Authorized physical access control security research for:
 * - Hotels (key cards)
 * - Corporate offices (badge systems)
 * - Universities (campus access)
 * - Hospitals (facility access)
 * - Parking garages (integrated systems)
 * 
 * CRITICAL: Requires explicit written authorization
 */

// Building Access submenu callback
static void building_access_submenu_callback(void* context, uint32_t index) {
    PredatorApp* app = context;
    if(!app || !app->view_dispatcher) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void predator_scene_building_access_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app || !app->submenu) return;
    
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "BUILDING ACCESS TESTING");
    
    // MIFARE Classic (90%+ of hotels/buildings)
    submenu_add_item(app->submenu, 
                     "🏨 MIFARE Classic (Hotels)", 
                     1, 
                     building_access_submenu_callback, 
                     app);
    
    // HID iClass (Corporate/Enterprise)
    submenu_add_item(app->submenu, 
                     "🏢 HID iClass (Corporate)", 
                     2, 
                     building_access_submenu_callback, 
                     app);
    
    // Combined parking + building
    submenu_add_item(app->submenu, 
                     "🅿️ Integrated Systems", 
                     3, 
                     building_access_submenu_callback, 
                     app);
    
    // BLE Smart Locks
    submenu_add_item(app->submenu, 
                     "🔐 BLE Smart Locks", 
                     4, 
                     building_access_submenu_callback, 
                     app);
    
    // Security audit mode
    submenu_add_item(app->submenu, 
                     "🔍 Security Audit", 
                     5, 
                     building_access_submenu_callback, 
                     app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
    
    FURI_LOG_I("BuildingAccess", "[v2.1] Building Access Testing initialized");
    FURI_LOG_W("BuildingAccess", "⚠️ AUTHORIZATION REQUIRED for all testing");
}

bool predator_scene_building_access_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    bool consumed = false;
    
    // Handle back button
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    
    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case 1: // MIFARE Classic
            FURI_LOG_I("BuildingAccess", "User selected: MIFARE Classic (Hotels)");
            // TODO: Navigate to MIFARE Classic scene
            // scene_manager_next_scene(app->scene_manager, PredatorSceneMifareClassicUI);
            
            // For now, show coming soon popup
            {
                if(app && app->popup) {
                    popup_reset(app->popup);
                    popup_set_header(app->popup, "MIFARE Classic", 64, 10, AlignCenter, AlignTop);
                    popup_set_text(app->popup, 
                                  "v2.1 Feature\n\n"
                                  "Hotels: 90%+\n"
                                  "Buildings: 85%+\n"
                                  "Parking: 80%+\n\n"
                                  "Full implementation\n"
                                  "coming in v2.1!", 
                                  64, 20, AlignCenter, AlignTop);
                    popup_set_timeout(app->popup, 3000);
                    popup_set_context(app->popup, app);
                    popup_enable_timeout(app->popup);
                    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewPopup);
                }
            }
            return true;
            
        case 2: // HID iClass
            FURI_LOG_I("BuildingAccess", "User selected: HID iClass (Corporate)");
            // TODO: Navigate to HID iClass scene
            {
                if(app && app->popup) {
                    popup_reset(app->popup);
                    popup_set_header(app->popup, "HID iClass", 64, 10, AlignCenter, AlignTop);
                    popup_set_text(app->popup, 
                                  "v2.1 Feature\n\n"
                                  "Corporate: 95%+\n"
                                  "Hospitals: 90%+\n"
                                  "Universities: 85%+\n\n"
                                  "Full implementation\n"
                                  "coming in v2.1!", 
                                  64, 20, AlignCenter, AlignTop);
                    popup_set_timeout(app->popup, 3000);
                    popup_set_context(app->popup, app);
                    popup_enable_timeout(app->popup);
                    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewPopup);
                }
            }
            return true;
            
        case 3: // Integrated Systems
            FURI_LOG_I("BuildingAccess", "User selected: Integrated Systems");
            {
                if(app && app->popup) {
                    popup_reset(app->popup);
                    popup_set_header(app->popup, "Integrated Systems", 64, 10, AlignCenter, AlignTop);
                    popup_set_text(app->popup, 
                                  "v2.1 Feature\n\n"
                                  "Parking + Building\n"
                                  "Multi-technology\n"
                                  "Seamless testing\n\n"
                                  "Full implementation\n"
                                  "coming in v2.1!", 
                                  64, 20, AlignCenter, AlignTop);
                    popup_set_timeout(app->popup, 3000);
                    popup_set_context(app->popup, app);
                    popup_enable_timeout(app->popup);
                    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewPopup);
                }
            }
            return true;
            
        case 4: // BLE Smart Locks
            FURI_LOG_I("BuildingAccess", "User selected: BLE Smart Locks");
            {
                if(app && app->popup) {
                    popup_reset(app->popup);
                    popup_set_header(app->popup, "BLE Smart Locks", 64, 10, AlignCenter, AlignTop);
                    popup_set_text(app->popup, 
                                  "v2.1 Feature\n\n"
                                  "Smart home locks\n"
                                  "Hotel smart locks\n"
                                  "IoT door systems\n\n"
                                  "Full implementation\n"
                                  "coming in v2.1!", 
                                  64, 20, AlignCenter, AlignTop);
                    popup_set_timeout(app->popup, 3000);
                    popup_set_context(app->popup, app);
                    popup_enable_timeout(app->popup);
                    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewPopup);
                }
            }
            return true;
            
        case 5: // Security Audit
            FURI_LOG_I("BuildingAccess", "User selected: Security Audit");
            {
                if(app && app->popup) {
                    popup_reset(app->popup);
                    popup_set_header(app->popup, "Security Audit Mode", 64, 10, AlignCenter, AlignTop);
                    popup_set_text(app->popup, 
                                  "v2.1 Feature\n\n"
                                  "Comprehensive scan\n"
                                  "Vulnerability report\n"
                                  "Risk assessment\n\n"
                                  "Full implementation\n"
                                  "coming in v2.1!", 
                                  64, 20, AlignCenter, AlignTop);
                    popup_set_timeout(app->popup, 3000);
                    popup_set_context(app->popup, app);
                    popup_enable_timeout(app->popup);
                    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewPopup);
                }
            }
            return true;
            
        default:
            consumed = false;
            break;
        }
    }
    
    return consumed;
}

void predator_scene_building_access_ui_on_exit(void* context) {
    PredatorApp* app = context;
    if(app && app->submenu) {
        submenu_reset(app->submenu);
    }
    FURI_LOG_I("BuildingAccess", "Building Access Testing exited");
}
