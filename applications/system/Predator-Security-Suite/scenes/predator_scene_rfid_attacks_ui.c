#include "../predator_i.h"
#include "predator_scene.h"
#include "predator_submenu_index.h"

// RFID Attacks Submenu - Professional UI
static void rfid_attacks_submenu_callback(void* context, uint32_t index) {
    PredatorApp* app = context;
    if(!app || !app->view_dispatcher) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void predator_scene_rfid_attacks_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app || !app->submenu) return;
    
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "💳 RFID Attacks");
    
    submenu_add_item(app->submenu, "💳 RFID Clone", SubmenuIndexRfidClone, rfid_attacks_submenu_callback, app);
    submenu_add_item(app->submenu, "🔓 RFID Bruteforce", SubmenuIndexRfidBruteforce, rfid_attacks_submenu_callback, app);
    submenu_add_item(app->submenu, "🎯 RFID Fuzzing", SubmenuIndexRfidFuzzing, rfid_attacks_submenu_callback, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
}

bool predator_scene_rfid_attacks_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    bool consumed = false;
    
    // Handle back button - return to main menu
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;  // Consumed - prevents framework bug
    }
    
    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case SubmenuIndexRfidClone:
            scene_manager_next_scene(app->scene_manager, PredatorSceneRfidCloneUI);
            break;
        case SubmenuIndexRfidBruteforce: // RE-ADDED: Testing stability
            scene_manager_next_scene(app->scene_manager, PredatorSceneRfidBruteforceUI);
            break;
        // case SubmenuIndexRfidFuzzing: // REMOVED: Memory optimization
        //     scene_manager_next_scene(app->scene_manager, PredatorSceneRfidFuzzingUI);
        //     break;
        default:
            consumed = false;
            break;
        }
    }
    
    return consumed;
}

void predator_scene_rfid_attacks_ui_on_exit(void* context) {
    PredatorApp* app = context;
    if(app && app->submenu) {
        submenu_reset(app->submenu);
    }
}
