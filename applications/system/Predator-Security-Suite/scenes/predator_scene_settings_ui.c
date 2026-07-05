#include "../predator_i.h"
#include "../helpers/predator_settings.h"
#include "../helpers/predator_compliance.h"
#include "../helpers/predator_logging.h"
#include "../helpers/predator_constants.h"
#include <gui/view.h>
#include <string.h>

// Settings - Professional UI
// Comprehensive settings with region selection and feature toggles

typedef enum {
    SettingRegion,
    SettingDeauthBurst,
    SettingLogging,
    SettingBrightness,
    SettingCount
} SettingItem;

typedef struct {
    SettingItem current_setting;
    uint8_t region_index;
    bool deauth_burst_enabled;
    bool logging_enabled;
    uint8_t brightness;
    bool settings_changed;
    PredatorApp* app;
} SettingsState;

static SettingsState settings_state;

// UNUSED: Settings now uses Submenu widget
/*
static void draw_settings_header(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "SETTINGS");
    canvas_draw_line(canvas, 0, 12, 128, 12);
}

static void draw_settings_list(Canvas* canvas, SettingsState* state) {
    if(!canvas || !state) return;
    
    canvas_set_font(canvas, FontSecondary);
    
    // Region setting
    uint8_t y = 24;
    if(state->current_setting == SettingRegion) {
        canvas_draw_str(canvas, 2, y, ">");
    }
    canvas_draw_str(canvas, 12, y, "Region:");
    // Safe array access with bounds checking
    if(state->region_index < PREDATOR_REGION_COUNT) {
        canvas_draw_str(canvas, 70, y, PREDATOR_REGION_NAMES[state->region_index]);
    } else {
        canvas_draw_str(canvas, 70, y, "ERROR");
    }
    
    // Deauth burst setting
    y += 10;
    if(state->current_setting == SettingDeauthBurst) {
        canvas_draw_str(canvas, 2, y, ">");
    }
    canvas_draw_str(canvas, 12, y, "Deauth Burst:");
    canvas_draw_str(canvas, 90, y, state->deauth_burst_enabled ? "ON" : "OFF");
    
    // Logging setting
    y += 10;
    if(state->current_setting == SettingLogging) {
        canvas_draw_str(canvas, 2, y, ">");
    }
    canvas_draw_str(canvas, 12, y, "Logging:");
    canvas_draw_str(canvas, 90, y, state->logging_enabled ? "ON" : "OFF");
    
    // Brightness setting
    y += 10;
    if(state->current_setting == SettingBrightness) {
        canvas_draw_str(canvas, 2, y, ">");
    }
    canvas_draw_str(canvas, 12, y, "Brightness:");
    char bright_str[8];
    snprintf(bright_str, sizeof(bright_str), "%u%%", (unsigned)state->brightness);
    canvas_draw_str(canvas, 90, y, bright_str);
    
    // Status
    if(state->settings_changed) {
        canvas_draw_str(canvas, 2, 58, "* Settings changed");
    }
}

static void settings_ui_draw_callback(Canvas* canvas, void* context) {
    furi_assert(canvas);
    furi_assert(context);
    
    UNUSED(context);
    
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    draw_settings_header(canvas);
    draw_settings_list(canvas, &settings_state);
    
    canvas_set_font(canvas, FontSecondary);
    if(settings_state.settings_changed) {
        canvas_draw_str(canvas, 25, 64, "OK=Save  Back=Cancel");
    } else {
        canvas_draw_str(canvas, 20, 64, "↑↓=Select  ←→=Change");
    }
}

static bool settings_ui_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    if(!app) return false;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            if(settings_state.settings_changed) {
                // Cancel changes
                settings_state.settings_changed = false;
                // Reload settings from app
                return true;
            }
            return false; // Let scene manager handle back
        } else if(event->key == InputKeyOk) {
            if(settings_state.settings_changed) {
                // Save settings with safety checks
                if(settings_state.region_index < PREDATOR_REGION_COUNT) {
                    predator_compliance_set_region(app, (PredatorRegion)settings_state.region_index);
                    settings_state.settings_changed = false;
                    
                    predator_log_append(app, "Settings saved");
                    FURI_LOG_I("SettingsUI", "Settings saved: region=%d", settings_state.region_index);
                } else {
                    FURI_LOG_E("SettingsUI", "Invalid region index: %d", settings_state.region_index);
                }
                return true;
            }
        } else if(event->key == InputKeyUp) {
            if(settings_state.current_setting > 0) {
                settings_state.current_setting--;
                return true;
            }
        } else if(event->key == InputKeyDown) {
            if(settings_state.current_setting < (SettingCount - 1)) {
                settings_state.current_setting++;
                return true;
            }
        } else if(event->key == InputKeyLeft) {
            settings_state.settings_changed = true;
            switch(settings_state.current_setting) {
                case SettingRegion:
                    if(settings_state.region_index > 0) {
                        settings_state.region_index--;
                    }
                    break;
                case SettingDeauthBurst:
                    settings_state.deauth_burst_enabled = !settings_state.deauth_burst_enabled;
                    break;
                case SettingLogging:
                    settings_state.logging_enabled = !settings_state.logging_enabled;
                    break;
                case SettingBrightness:
                    if(settings_state.brightness > 10) {
                        settings_state.brightness -= 10;
                    }
                    break;
                default:
                    break;
            }
            return true;
        } else if(event->key == InputKeyRight) {
            settings_state.settings_changed = true;
            switch(settings_state.current_setting) {
                case SettingRegion:
                    if(settings_state.region_index < (PREDATOR_REGION_COUNT - 1)) {
                        settings_state.region_index++;
                    }
                    break;
                case SettingDeauthBurst:
                    settings_state.deauth_burst_enabled = !settings_state.deauth_burst_enabled;
                    break;
                case SettingLogging:
                    settings_state.logging_enabled = !settings_state.logging_enabled;
                    break;
                case SettingBrightness:
                    if(settings_state.brightness < 100) {
                        settings_state.brightness += 10;
                    }
                    break;
                default:
                    break;
            }
            return true;
        }
    }
    
    return true;
}
*/

void predator_scene_settings_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app) {
        FURI_LOG_E("SettingsUI", "App context is NULL");
        return;
    }
    
    // Safe initialization with NULL checks
    memset(&settings_state, 0, sizeof(SettingsState));
    settings_state.current_setting = SettingRegion;
    
    // Safe region access with bounds checking
    if(app->region < PREDATOR_REGION_COUNT) {
        settings_state.region_index = (uint8_t)app->region;
    } else {
        settings_state.region_index = 0; // Default to Auto
        FURI_LOG_W("SettingsUI", "Invalid region %d, defaulting to Auto", app->region);
    }
    
    settings_state.deauth_burst_enabled = true;
    settings_state.logging_enabled = true;
    settings_state.brightness = 80;
    settings_state.settings_changed = false;
    
    FURI_LOG_I("SettingsUI", "Settings initialized: region=%d", settings_state.region_index);
    
    if(!app->submenu) return;
    
    // Convert to submenu display
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "SETTINGS");
    
    // Show current region
    const char* regions[] = {"AUTO", "EU", "US", "JP", "UNBLOCK"};
    char region_str[64];
    snprintf(region_str, sizeof(region_str), "Region: %s", regions[settings_state.region_index]);
    submenu_add_item(app->submenu, region_str, 0, NULL, app);
    
    submenu_add_item(app->submenu, "Region: Auto-detect", 1, NULL, app);
    submenu_add_item(app->submenu, "Region: EU (Europe)", 2, NULL, app);
    submenu_add_item(app->submenu, "Region: US (America)", 3, NULL, app);
    submenu_add_item(app->submenu, "Region: JP (Japan)", 4, NULL, app);
    submenu_add_item(app->submenu, "Region: UNBLOCK (All)", 5, NULL, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
    
    FURI_LOG_I("SettingsUI", "Settings UI initialized");
}

bool predator_scene_settings_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    if(!app) return false;
    
    // Handle back button - return to main menu
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;  // Consumed - prevents framework bug
    }
    
    if(event.type == SceneManagerEventTypeCustom) {
        return true;
    }
    
    return false;
}

void predator_scene_settings_ui_on_exit(void* context) {
    PredatorApp* app = context;
    if(!app || !app->submenu) return;
    
    submenu_reset(app->submenu);
    
    FURI_LOG_I("SettingsUI", "Settings UI exited");
}
