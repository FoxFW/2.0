#include "../predator_i.h"
#include "predator_scene.h"
#include "../helpers/predator_boards.h"
#include "../helpers/predator_esp32.h"
#include "../helpers/predator_gps.h"
#include "../helpers/predator_logging.h"
#include <gui/view.h>
#include <notification/notification_messages.h>

// ULTIMATE BOARD SELECTION - ENTERPRISE PROFESSIONAL GRADE
// The BEST Flipper Zero board selection experience for worldwide security research

typedef enum {
    BoardScreenMain,
    BoardScreenDetails,
    BoardScreenConfirm,
    BoardScreenSuccess
} BoardScreen;

typedef struct {
    uint8_t selected_index;
    BoardScreen current_screen;
    bool selection_confirmed;
    bool hardware_tested;
    uint32_t animation_tick;
    PredatorApp* app;
} BoardSelectionState;

// UNUSED: Board Selection now uses Submenu widget
// static BoardSelectionState board_state;
// static View* board_selection_view = NULL;

// UNUSED: Board Selection now uses Submenu widget
/*
// ULTIMATE BOARD SELECTION SCREENS

static void draw_main_screen(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 15, 10, "BOARD SELECTION");
    canvas_draw_line(canvas, 0, 12, 128, 12);
    
    // Animated selection indicator
    uint8_t anim_offset = (board_state.animation_tick / 10) % 3;
    canvas_set_font(canvas, FontSecondary);
    
    // Board name with animation - add bounds checking
    char board_display[32];
    uint8_t safe_index = board_state.selected_index;
    if(safe_index >= PredatorBoardTypeCount) {
        safe_index = PredatorBoardTypeOriginal;
    }
    
    // CRITICAL FIX: Use actual board config names (same as main menu)
    const char* board_name = predator_boards_get_name(safe_index);
    snprintf(board_display, sizeof(board_display), "%s%s", 
             anim_offset == 0 ? "► " : anim_offset == 1 ? "▶ " : "▷ ",
             board_name);
    canvas_draw_str(canvas, 2, 25, board_display);
    
    // Quick capabilities preview
    bool has_esp32 = (board_state.selected_index == PredatorBoardType3in1AIO || 
                     board_state.selected_index == PredatorBoardTypeScreen28 ||
                     board_state.selected_index == PredatorBoardTypeDrB0rkMultiV2 ||
                     board_state.selected_index == PredatorBoardType3in1NrfCcEsp);
    
    canvas_draw_str(canvas, 2, 35, "Capabilities:");
    canvas_draw_str(canvas, 2, 45, has_esp32 ? "📡 WiFi+BT+GPS+SubGHz+NFC" : "📻 SubGHz+NFC Only");
    
    // Navigation hints
    canvas_draw_str(canvas, 2, 58, "↑↓=Browse  OK=Details  Back=Exit");
}

static void draw_details_screen(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 20, 10, "BOARD DETAILS");
    canvas_draw_line(canvas, 0, 12, 128, 12);
    
    canvas_set_font(canvas, FontSecondary);
    
    // Board name - add bounds checking
    uint8_t safe_index = board_state.selected_index;
    if(safe_index >= PredatorBoardTypeCount) {
        safe_index = PredatorBoardTypeOriginal;
    }
    // CRITICAL FIX: Use actual board config name
    canvas_draw_str(canvas, 2, 22, predator_boards_get_name(safe_index));
    
    // Detailed capabilities
    bool has_esp32 = (board_state.selected_index == PredatorBoardType3in1AIO || 
                     board_state.selected_index == PredatorBoardTypeScreen28 ||
                     board_state.selected_index == PredatorBoardTypeDrB0rkMultiV2 ||
                     board_state.selected_index == PredatorBoardType3in1NrfCcEsp);
    
    canvas_draw_str(canvas, 2, 32, has_esp32 ? "✓ WiFi Attacks" : "✗ WiFi Attacks");
    canvas_draw_str(canvas, 2, 40, has_esp32 ? "✓ Bluetooth Attacks" : "✗ Bluetooth Attacks");
    canvas_draw_str(canvas, 2, 48, has_esp32 ? "✓ GPS Tracking" : "✗ GPS Tracking");
    canvas_draw_str(canvas, 2, 56, "✓ SubGHz/Car Attacks");
    
    canvas_draw_str(canvas, 2, 64, "OK=Select  Back=Return");
}

static void draw_confirm_screen(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 25, 10, "CONFIRM BOARD");
    canvas_draw_line(canvas, 0, 12, 128, 12);
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 25, "Selected Board:");
    
    // Add bounds checking
    uint8_t safe_index = board_state.selected_index;
    if(safe_index >= PredatorBoardTypeCount) {
        safe_index = PredatorBoardTypeOriginal;
    }
    // CRITICAL FIX: Use actual board config name
    canvas_draw_str(canvas, 2, 35, predator_boards_get_name(safe_index));
    
    if(board_state.hardware_tested) {
        canvas_draw_str(canvas, 2, 45, "✓ Hardware Test: PASSED");
    } else {
        canvas_draw_str(canvas, 2, 45, "⏳ Testing hardware...");
    }
    
    canvas_draw_str(canvas, 2, 58, "OK=Confirm  Back=Cancel");
}

static void draw_success_screen(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 30, 10, "SUCCESS!");
    canvas_draw_line(canvas, 0, 12, 128, 12);
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 25, "Board configured:");
    
    // Add bounds checking
    uint8_t safe_index = board_state.selected_index;
    if(safe_index >= PredatorBoardTypeCount) {
        safe_index = PredatorBoardTypeOriginal;
    }
    // CRITICAL FIX: Use actual board config name
    canvas_draw_str(canvas, 2, 35, predator_boards_get_name(safe_index));
    
    // Success animation
    uint8_t anim = (board_state.animation_tick / 5) % 4;
    const char* success_icons[] = {"✓", "✓✓", "✓✓✓", "✓✓✓✓"};
    canvas_draw_str(canvas, 2, 45, success_icons[anim]);
    canvas_draw_str(canvas, 20, 45, "Ready for Research!");
    
    canvas_draw_str(canvas, 2, 58, "Any key=Back  Back=Main Menu");
}
*/

// UNUSED: Board Selection now uses Submenu widget
/*
static void board_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    if(!canvas) return;
    
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    // Increment animation counter
    board_state.animation_tick++;
    
    // Draw appropriate screen
    switch(board_state.current_screen) {
        case BoardScreenMain:
            draw_main_screen(canvas);
            break;
        case BoardScreenDetails:
            draw_details_screen(canvas);
            break;
        case BoardScreenConfirm:
            draw_confirm_screen(canvas);
            break;
        case BoardScreenSuccess:
            draw_success_screen(canvas);
            break;
    }
}

// ULTIMATE INPUT HANDLING - Multi-screen navigation
static void perform_hardware_test(void) {
    // Simulate hardware testing
    furi_delay_ms(500);
    board_state.hardware_tested = true;
}

static void trigger_success_notification(PredatorApp* app) {
    if(app && app->notifications) {
        notification_message(app->notifications, &sequence_success);
    }
}

*/

// UNUSED: Board Selection now uses Submenu widget
/*
static bool board_input_callback(InputEvent* event, void* context) {
    UNUSED(context);
    if(!event || event->type != InputTypeShort) return false;
    
    bool consumed = false;
    
    switch(board_state.current_screen) {
        case BoardScreenMain:
            switch(event->key) {
                case InputKeyUp:
                    if(board_state.selected_index > 0) {
                        board_state.selected_index--;
                        consumed = true;
                    }
                    break;
                    
                case InputKeyDown:
                    if(board_state.selected_index < PredatorBoardTypeCount - 1) {
                        board_state.selected_index++;
                        consumed = true;
                    }
                    break;
                    
                case InputKeyOk:
                    board_state.current_screen = BoardScreenDetails;
                    consumed = true;
                    break;
                    
                case InputKeyBack:
                    // DON'T consume Back on main screen - let view dispatcher handle it
                    // This allows proper scene manager navigation
                    consumed = false;
                    break;
                    
                default:
                    break;
            }
            break;
            
        case BoardScreenDetails:
            switch(event->key) {
                case InputKeyOk:
                    board_state.current_screen = BoardScreenConfirm;
                    board_state.hardware_tested = false;
                    // Start hardware test
                    perform_hardware_test();
                    consumed = true;
                    break;
                    
                case InputKeyBack:
                    board_state.current_screen = BoardScreenMain;
                    consumed = true;
                    break;
                    
                default:
                    break;
            }
            break;
            
        case BoardScreenConfirm:
            switch(event->key) {
                case InputKeyOk:
                    if(board_state.hardware_tested && board_state.app) {
                        // CRITICAL: Actually set the board type
                        board_state.app->board_type = board_state.selected_index;
                        board_state.selection_confirmed = true;
                        
                        // Log successful selection
                        predator_log_append(board_state.app, "Board configured:");
                        predator_log_append(board_state.app, predator_boards_get_name(board_state.selected_index));
                        
                        // Trigger success notification
                        trigger_success_notification(board_state.app);
                        
                        // Go to success screen
                        board_state.current_screen = BoardScreenSuccess;
                        board_state.animation_tick = 0; // Reset animation
                    }
                    consumed = true;
                    break;
                    
                case InputKeyBack:
                    board_state.current_screen = BoardScreenDetails;
                    consumed = true;
                    break;
                    
                default:
                    break;
            }
            break;
            
        case BoardScreenSuccess:
            // CRITICAL FIX: Success screen - DON'T consume back button!
            // Let it propagate to on_event handler
            if(event->type == InputTypeShort && event->key == InputKeyBack) {
                FURI_LOG_I("BoardSelection", "Success screen - BACK button, NOT consuming (let on_event handle it)");
                consumed = false;  // DON'T consume - let it propagate
            } else {
                // Other keys - consume them
                FURI_LOG_I("BoardSelection", "Success screen - key %d type %d, consuming", event->key, event->type);
                consumed = true;
            }
            break;
    }
    
    return consumed;
}
*/

static void board_selection_submenu_cb(void* context, uint32_t index) {
    PredatorApp* app = context;
    if(!app || !app->view_dispatcher) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void predator_scene_board_selection_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app || !app->submenu) return;
    
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "SELECT BOARD");
    
    // Get current board name
    const char* current_board = "None";
    if(app->board_type < PredatorBoardTypeCount) {
        const PredatorBoardConfig* config = predator_boards_get_config(app->board_type);
        if(config) current_board = config->name;
    }
    
    char header[64];
    snprintf(header, sizeof(header), "Current: %s", current_board);
    submenu_add_item(app->submenu, header, 0, board_selection_submenu_cb, app);
    
    // Add all available boards
    submenu_add_item(app->submenu, "Original Predator", PredatorBoardTypeOriginal + 1, board_selection_submenu_cb, app);
    submenu_add_item(app->submenu, "3in1 AIO Board V1.4", PredatorBoardType3in1AIO + 1, board_selection_submenu_cb, app);
    submenu_add_item(app->submenu, "DrB0rk Multi-Board", PredatorBoardTypeDrB0rkMultiV2 + 1, board_selection_submenu_cb, app);
    submenu_add_item(app->submenu, "2.8\" Screen Board", PredatorBoardTypeScreen28 + 1, board_selection_submenu_cb, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
    
    FURI_LOG_E("BoardSelection", "========== BOARD SELECTION ENTERED ==========");
}

bool predator_scene_board_selection_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    if(!app) return false;
    
    // Handle back button - return to main menu
    if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_E("BoardSelection", "==== BACK EVENT ====");
        scene_manager_previous_scene(app->scene_manager);
        return true;  // Consumed - prevents framework bug
    }
    
    // Handle board selection
    if(event.type == SceneManagerEventTypeCustom && event.event > 0) {
        PredatorBoardType selected = (PredatorBoardType)(event.event - 1);
        if(selected < PredatorBoardTypeCount) {
            app->board_type = selected;
            FURI_LOG_I("BoardSelection", "Board selected: %d", selected);
            predator_log_append(app, "Board configured");
            predator_log_append(app, predator_boards_get_name(selected));
            
            // Success notification
            if(app->notifications) {
                notification_message(app->notifications, &sequence_success);
            }
            
            // Go back to main menu
            scene_manager_previous_scene(app->scene_manager);
        }
        return true;
    }
    
    return false;
}

void predator_scene_board_selection_ui_on_exit(void* context) {
    PredatorApp* app = context;
    if(!app || !app->submenu) return;
    
    submenu_reset(app->submenu);
    
    FURI_LOG_E("BoardSelection", "========== BOARD SELECTION EXITED ==========");
}
