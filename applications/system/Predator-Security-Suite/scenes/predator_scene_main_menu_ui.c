#include "../predator_i.h"
#include "predator_scene.h"
#include "../helpers/predator_boards.h"
#include "predator_submenu_index.h"

// Main Menu - Professional UI (Memory Optimized, No Guards)
// Clean submenu implementation for Tesla demo
// ANTI-ACCIDENTAL-EXIT: Double-press Back to exit app
// 🎮 KONAMI CODE: Up Up Down Down Left Right Left Right OK (Easter Egg!)

static uint32_t last_back_press_time = 0;
static uint32_t menu_enter_time = 0;
static bool allow_exit = false;  // CRITICAL: Flag to control exit
#define DOUBLE_PRESS_TIMEOUT_MS 2000 // 2 seconds to press Back again
#define GRACE_PERIOD_MS 1000 // Ignore back presses for first 1000ms after entering menu (was 500ms)

// 🎮 KONAMI CODE DISABLED - Breaks submenu navigation when enabled
// TODO: Re-implement using view_dispatcher input callback instead of view-level override
/*
// Konami Code State Tracker
typedef enum {
    KonamiStep0_Start = 0,
    KonamiStep1_Up,
    KonamiStep2_Up2,
    KonamiStep3_Down,
    KonamiStep4_Down2,
    KonamiStep5_Left,
    KonamiStep6_Right,
    KonamiStep7_Left2,
    KonamiStep8_Right2,
    KonamiStep9_Complete
} KonamiCodeStep;

static KonamiCodeStep konami_state = KonamiStep0_Start;
static uint32_t last_konami_input_time = 0;
#define KONAMI_TIMEOUT_MS 5000
*/

// Safe exit macro
#define PREDATOR_SAFE_EXIT_APP(app) \
    do { \
        if(app && app->view_dispatcher) { \
            view_dispatcher_stop(app->view_dispatcher); \
        } \
    } while(0)

static void main_menu_submenu_callback(void* context, uint32_t index) {
    PredatorApp* app = context;
    if(!app || !app->view_dispatcher) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void predator_scene_main_menu_ui_on_enter(void* context) {
    PredatorApp* app = context;
    FURI_LOG_E("MainMenu", "========== MAIN MENU ENTERED ==========");
    FURI_LOG_E("MainMenu", "🔥🔥🔥 VERSION 2.0 - NUCLEAR EXIT PROTECTION ACTIVE 🔥🔥🔥");
    
    if(!app || !app->submenu) {
        FURI_LOG_E("MainMenu", "ERROR: app or submenu is NULL!");
        return;
    }
    
    // CRITICAL FIX: Reset ALL exit protection flags
    menu_enter_time = furi_get_tick();
    last_back_press_time = 0;  // Reset back press timer
    allow_exit = false;  // BLOCK all exits until double-press
    
    FURI_LOG_I("MainMenu", "Menu enter time: %lu, grace period: %d ms, exit BLOCKED", 
               menu_enter_time, GRACE_PERIOD_MS);
    
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "🔥 PREDATOR v2.0 NUCLEAR");
    
    // PROFESSIONAL - OPTIMIZED FOR MEMORY & FUNCTIONALITY
    submenu_add_item(app->submenu, "🚗 Tesla Security", 1, main_menu_submenu_callback, app);
    submenu_add_item(app->submenu, "🚗 Car Models", 2, main_menu_submenu_callback, app);
    // 🎮 Walking Open removed - replaced by Konami code easter egg!
    submenu_add_item(app->submenu, "🔥 Dictionary Attack", 16, main_menu_submenu_callback, app);
    submenu_add_item(app->submenu, "🚇 Transit Cards", 17, main_menu_submenu_callback, app);
    // Access Control disabled to save 12KB for Transit Cards
    // submenu_add_item(app->submenu, "🔑 Access Control", 18, main_menu_submenu_callback, app);
    submenu_add_item(app->submenu, "🚧 Parking Barriers", 15, main_menu_submenu_callback, app);
    submenu_add_item(app->submenu, "📡 WiFi Attacks", 4, main_menu_submenu_callback, app);
    submenu_add_item(app->submenu, "📱 Bluetooth Attacks", 5, main_menu_submenu_callback, app);
    submenu_add_item(app->submenu, "💳 RFID Attacks", 6, main_menu_submenu_callback, app);
    submenu_add_item(app->submenu, "📻 SubGHz Attacks", 7, main_menu_submenu_callback, app);
    submenu_add_item(app->submenu, "📊 Module Status", 8, main_menu_submenu_callback, app);
    
    // Simple card/board selection menu item
    submenu_add_item(app->submenu, "🔧 Card Selection", 12, main_menu_submenu_callback, app);
    
    submenu_add_item(app->submenu, "⚙️ Settings", 13, main_menu_submenu_callback, app);
    submenu_add_item(app->submenu, "ℹ️ About", 14, main_menu_submenu_callback, app);
    
    // 🎮 KONAMI CODE NOTE: Custom input callback breaks submenu navigation
    // TODO: Implement via view_dispatcher input callback instead of view-level override
    // For now, Konami code logic is disabled to maintain menu functionality
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
}

bool predator_scene_main_menu_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    
    FURI_LOG_D("MainMenu", "Event received - type: %d", event.type);
    
    // CRITICAL: Handle back button FIRST, before anything else
    // ALWAYS return true to prevent app from exiting
    if(event.type == SceneManagerEventTypeBack) {
        uint32_t current_time = furi_get_tick();
        uint32_t time_since_enter = current_time - menu_enter_time;
        uint32_t time_since_last_back = (last_back_press_time == 0) ? 99999 : (current_time - last_back_press_time);
        
        FURI_LOG_E("MainMenu", "BACK EVENT - enter:%lu ms ago, last_back:%lu ms ago, allow_exit:%d", 
                  time_since_enter, time_since_last_back, allow_exit);
        
        // ABSOLUTE PROTECTION: Grace period - ignore ALL back presses for first 1000ms
        if(time_since_enter < GRACE_PERIOD_MS) {
            FURI_LOG_W("MainMenu", "Back BLOCKED - grace period (%lu ms < %d ms)", 
                      time_since_enter, GRACE_PERIOD_MS);
            return true;  // CONSUME - NEVER exit
        }
        
        // Check if this is a double press (second press within timeout)
        if(last_back_press_time != 0 && time_since_last_back < DOUBLE_PRESS_TIMEOUT_MS) {
            // DOUBLE PRESS DETECTED - Allow exit
            FURI_LOG_E("MainMenu", "!!! DOUBLE BACK PRESS DETECTED - EXITING APP !!!");
            allow_exit = true;  // Allow the exit
            PREDATOR_SAFE_EXIT_APP(app);
            return true;
        } else {
            // FIRST PRESS - Show warning and start timer
            FURI_LOG_W("MainMenu", "FIRST back press - press again within 2s to exit (allow_exit=false)");
            last_back_press_time = current_time;
            allow_exit = false;  // Ensure exit is still blocked
            
            // Vibrate to confirm first press
            if(app->notifications) {
                notification_message(app->notifications, &sequence_single_vibro);
            }
            
            return true; // CRITICAL: ALWAYS return true - IMPOSSIBLE to return false
        }
    }
    
    // Handle menu item selections
    if(event.type == SceneManagerEventTypeCustom) {
        FURI_LOG_D("MainMenu", "Menu item selected: %lu", event.event);
        switch(event.event) {
        case 1: // Tesla Security
            scene_manager_next_scene(app->scene_manager, PredatorSceneCarTeslaUI);
            break;
        case 2: // Car Models
            scene_manager_next_scene(app->scene_manager, PredatorSceneCarContinentUI);
            break;
        case 16: // 🔥 Dictionary Attack - 980+ keys
            scene_manager_next_scene(app->scene_manager, PredatorSceneDictionaryAttackUI);
            break;
        case 17: // 🚇 Transit Cards - FeliCa & Calypso
            scene_manager_next_scene(app->scene_manager, PredatorSceneTransitCardsMenu);
            break;
        // case 18: Access Control disabled (saves 12KB for Transit Cards)
        case 15: // Parking Barriers - Enterprise Professional (Region → Facility → Attack)
            scene_manager_next_scene(app->scene_manager, PredatorSceneBarrierRegionSelectUI);
            break;
        case 4: // WiFi Attacks
            scene_manager_next_scene(app->scene_manager, PredatorSceneWifiAttacksUI);
            break;
        case 5: // Bluetooth Attacks
            scene_manager_next_scene(app->scene_manager, PredatorSceneBluetoothAttacksUI);
            break;
        case 6: // RFID Attacks
            scene_manager_next_scene(app->scene_manager, PredatorSceneRfidAttacksUI);
            break;
        case 7: // SubGHz Attacks
            scene_manager_next_scene(app->scene_manager, PredatorSceneSubGhzAttacksUI);
            break;
        case 8: // Module Status
            scene_manager_next_scene(app->scene_manager, PredatorSceneModuleStatusUI);
            break;
        case 12: // Board Selection
            scene_manager_next_scene(app->scene_manager, PredatorSceneBoardSelectionUI);
            break;
        case 13: // Settings
            scene_manager_next_scene(app->scene_manager, PredatorSceneSettingsUI);
            break;
        case 14: // About
            scene_manager_next_scene(app->scene_manager, PredatorSceneAboutUI);
            break;
        default:
            FURI_LOG_W("MainMenu", "Unknown menu item: %lu", event.event);
            break;
        }
        return true;  // Always consume custom events
    }
    
    // CRITICAL: Default return - consume ALL events to prevent app exit
    FURI_LOG_D("MainMenu", "Event type %d - consuming to prevent exit", event.type);
    return true;
}

void predator_scene_main_menu_ui_on_exit(void* context) {
    PredatorApp* app = context;
    FURI_LOG_E("MainMenu", "!!! EXITING MAIN MENU !!! allow_exit=%d", allow_exit);
    if(!allow_exit) {
        FURI_LOG_E("MainMenu", "WARNING: Main menu exiting WITHOUT double-press! This is a BUG!");
    }
    if(app && app->submenu) {
        submenu_reset(app->submenu);
    }
}
