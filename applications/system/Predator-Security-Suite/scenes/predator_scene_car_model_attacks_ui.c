#include "../predator_i.h"
#include "predator_scene.h"
#include "../helpers/predator_subghz.h"
#include "../helpers/predator_logging.h"

// Attack methods for the model selected in app->selected_model_*
// Navigates to existing attack scenes with proper UI and live status

static void model_attacks_cb(void* context, uint32_t index) {
    PredatorApp* app = context;
    if(!app || !app->view_dispatcher) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void predator_scene_car_model_attacks_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app || !app->submenu) return;

    submenu_reset(app->submenu);

    char header[48];
    // Show Make Model and MHz (integer formatting, trimmed to fit safely)
    unsigned long mhz_i = (unsigned long)(app->selected_model_freq / 1000000U);
    unsigned long mhz_d = (unsigned long)((app->selected_model_freq % 1000000U) / 10000U); // 2 decimals
    snprintf(header, sizeof(header), "🚗 %.12s %.16s (%lu.%02luMHz)",
             app->selected_model_make,
             app->selected_model_name,
             mhz_i, mhz_d);
    submenu_set_header(app->submenu, header);

    // Offer all methods - navigates to existing professional attack scenes
    submenu_add_item(app->submenu, "🔐 Protocol Test", 1, model_attacks_cb, app);  // NEW: Test crypto protocols
    submenu_add_item(app->submenu, "🔑 Key Bruteforce", 2, model_attacks_cb, app);
    submenu_add_item(app->submenu, "🔄 Rolling Code Attack", 3, model_attacks_cb, app);
    submenu_add_item(app->submenu, "🔐 Smart Key (AES-128)", 4, model_attacks_cb, app);  // NEW: Smart key attack
    submenu_add_item(app->submenu, "📡 RF Jamming", 5, model_attacks_cb, app);
    submenu_add_item(app->submenu, "🚗 Passive Opener", 6, model_attacks_cb, app);
    submenu_add_item(app->submenu, "📈 Live Monitor", 7, model_attacks_cb, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
}

bool predator_scene_car_model_attacks_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    if(!app) return false;

    if(event.type == SceneManagerEventTypeBack) {
        // Ensure any TX is stopped, then let scene manager navigate back
        predator_subghz_stop_attack(app);
        // Return false to let scene manager do normal back navigation
        return false;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
            case 1: // Protocol Test - NEW: Test crypto algorithms
                scene_manager_next_scene(app->scene_manager, PredatorSceneProtocolTestUI);
                return true;
            case 2: // Key Bruteforce - Auto-detect protocol
                scene_manager_set_scene_state(app->scene_manager, PredatorSceneCarKeyBruteforceUI, 0); // 0 = auto-detect
                scene_manager_next_scene(app->scene_manager, PredatorSceneCarKeyBruteforceUI);
                return true;
            case 3: // Rolling Code Attack - Force Keeloq/Hitag2
                predator_log_append(app, "Rolling Code: Advanced cryptographic attack");
                scene_manager_set_scene_state(app->scene_manager, PredatorSceneCarKeyBruteforceUI, 1); // 1 = force rolling
                scene_manager_next_scene(app->scene_manager, PredatorSceneCarKeyBruteforceUI);
                return true;
            case 4: // Smart Key Attack - Force AES-128
                predator_log_append(app, "Smart Key: AES-128 challenge-response attack");
                scene_manager_set_scene_state(app->scene_manager, PredatorSceneCarKeyBruteforceUI, 2); // 2 = force smart key
                scene_manager_next_scene(app->scene_manager, PredatorSceneCarKeyBruteforceUI);
                return true;
            case 5: // RF Jamming - Navigate to existing scene
                scene_manager_next_scene(app->scene_manager, PredatorSceneCarJammingUI);
                return true;
            case 6: // Passive Opener - Navigate to existing scene
                scene_manager_next_scene(app->scene_manager, PredatorSceneCarPassiveOpenerUI);
                return true;
            case 7: // Live Monitor - Navigate to existing scene
                scene_manager_next_scene(app->scene_manager, PredatorSceneLiveMonitorUI);
                return true;
            default:
                break;
        }
    }

    return false;
}

void predator_scene_car_model_attacks_ui_on_exit(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    predator_subghz_stop_attack(app);
}
