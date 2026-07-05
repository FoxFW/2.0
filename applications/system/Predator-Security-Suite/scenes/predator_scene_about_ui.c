#include "../predator_i.h"
#include <gui/view.h>
#include <string.h>

// About Screen - Professional UI
// Shows app information with page navigation

typedef struct {
    uint8_t current_page;
    uint8_t total_pages;
} AboutState;

// UNUSED: About now uses Submenu widget
/*
static AboutState about_state;

static void draw_predator_logo(Canvas* canvas) {
    // Professional PREDATOR logo
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 25, 10, "PREDATOR");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 10, 20, "🔧 Security Testing Platform 🔧");
    canvas_draw_line(canvas, 0, 22, 128, 22);
}

static void draw_about_header(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "ABOUT");
    canvas_draw_line(canvas, 0, 12, 128, 12);
}

static void draw_about_page(Canvas* canvas, AboutState* state) {
    canvas_set_font(canvas, FontSecondary);
    
    switch(state->current_page) {
        case 0: // Main info with logo
            draw_predator_logo(canvas);
            canvas_draw_str(canvas, 2, 32, "🌍 Worldwide Enterprise v2.0");
            canvas_draw_str(canvas, 2, 42, "💼 Professional Security Research");
            canvas_draw_str(canvas, 2, 52, "⚡ Version: 2.0 - API: 86.0");
            canvas_draw_str(canvas, 2, 62, "🌐 98%+ Global Coverage");
            break;
            
        case 1: // Professional Features
            canvas_draw_str(canvas, 2, 24, "🔧 TESTING CAPABILITIES:");
            canvas_draw_str(canvas, 2, 34, "🚗 Tesla Security Analysis");
            canvas_draw_str(canvas, 2, 44, "🚙 90+ Car Models Database");
            canvas_draw_str(canvas, 2, 54, "🚶 Mobile Testing Workflows");
            canvas_draw_str(canvas, 2, 64, "🏛️ Government Compliance");
            break;
            
        case 2: // Hardware
            canvas_draw_str(canvas, 2, 24, "Hardware Support:");
            canvas_draw_str(canvas, 2, 34, "- ESP32 (WiFi/BLE)");
            canvas_draw_str(canvas, 2, 44, "- GPS Module");
            canvas_draw_str(canvas, 2, 54, "- SubGHz Radio");
            break;
            
        case 3: // Professional Partnerships
            canvas_draw_str(canvas, 2, 24, "🏛️ WORLDWIDE PARTNERS:");
            canvas_draw_str(canvas, 2, 34, "🌍 Enterprise Professional");
            canvas_draw_str(canvas, 2, 44, "🇺🇸 North America Contracts");
            canvas_draw_str(canvas, 2, 54, "🇪🇺 European Research Teams");
            canvas_draw_str(canvas, 2, 64, "🌏 Global Security Platform");
            break;
            
        case 4: // Legal
            canvas_draw_str(canvas, 2, 24, "Legal Notice:");
            canvas_draw_str(canvas, 2, 34, "For ethical use only");
            canvas_draw_str(canvas, 2, 44, "Educational purposes");
            canvas_draw_str(canvas, 2, 54, "Obey local laws");
            break;
    }
    
    // Page indicator
    char page_str[16];
    snprintf(page_str, sizeof(page_str), "Page %u/%u", 
            state->current_page + 1, state->total_pages);
    canvas_draw_str(canvas, 45, 64, page_str);
}

static void about_ui_draw_callback(Canvas* canvas, void* context) {
    furi_assert(canvas);
    furi_assert(context);
    
    UNUSED(context);
    
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    draw_about_header(canvas);
    draw_about_page(canvas, &about_state);
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 64, "←→");
}

static bool about_ui_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    if(!app) return false;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            return false; // Let scene manager handle back
        } else if(event->key == InputKeyLeft) {
            if(about_state.current_page > 0) {
                about_state.current_page--;
                return true;
            }
        } else if(event->key == InputKeyRight) {
            if(about_state.current_page < (about_state.total_pages - 1)) {
                about_state.current_page++;
                return true;
            }
        }
    }
    
    return true;
}
*/

void predator_scene_about_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    
    if(!app->submenu) return;
    
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "ABOUT");
    
    submenu_add_item(app->submenu, "Predator Professional", 0, NULL, app);
    submenu_add_item(app->submenu, "Version: 2.0 Enterprise", 0, NULL, app);
    submenu_add_item(app->submenu, "Worldwide Professional", 0, NULL, app);
    submenu_add_item(app->submenu, "35 Barrier Manufacturers", 0, NULL, app);
    submenu_add_item(app->submenu, "400+ Car Models Global", 0, NULL, app);
    submenu_add_item(app->submenu, "5 Expansion Boards", 0, NULL, app);
    submenu_add_item(app->submenu, "WiFi+BT+GPS+SubGHz+NFC", 0, NULL, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
    
    FURI_LOG_I("AboutUI", "About UI initialized");
}

bool predator_scene_about_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    if(!app) return false;
    
    // Handle back button - return to main menu
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;  // Consumed - prevents framework bug
    }
    
    return false;
}

void predator_scene_about_ui_on_exit(void* context) {
    PredatorApp* app = context;
    if(!app || !app->submenu) return;
    
    submenu_reset(app->submenu);
    
    FURI_LOG_I("AboutUI", "About UI exited");
}
