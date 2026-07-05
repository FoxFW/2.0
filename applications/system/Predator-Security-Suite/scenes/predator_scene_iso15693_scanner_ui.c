#include "../predator_i.h"
#include "../helpers/predator_crypto_iso15693.h"
#include <gui/elements.h>

// ISO 15693 Scanner - Tag detection and info display
// Pattern: Similar to FeliCa/Calypso readers

typedef struct {
    ISO15693Tag tag;
    bool tag_detected;
    char manufacturer[32];
    char status_text[64];
} ScannerState;

static ScannerState* scanner_state = NULL;

static void scanner_draw_callback(Canvas* canvas, void* context) {
    PredatorApp* app = context;
    if(!scanner_state) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "ISO 15693 Scanner");

    canvas_set_font(canvas, FontSecondary);
    
    if(!scanner_state->tag_detected) {
        canvas_draw_str(canvas, 2, 24, "Place tag on reader...");
        
        // Animated waiting
        char waiting[4] = ".  ";
        uint32_t ticks = furi_get_tick() / 500;
        for(int i = 0; i < (ticks % 3) + 1; i++) {
            waiting[i] = '.';
        }
        canvas_draw_str(canvas, 102, 24, waiting);
        
        canvas_draw_str(canvas, 2, 38, "Supported:");
        canvas_draw_str(canvas, 2, 47, "ICODE SLI/SLIX");
        canvas_draw_str(canvas, 2, 55, "TI Tag-it, Infineon");
        
    } else {
        // Tag detected
        canvas_draw_str(canvas, 2, 22, scanner_state->manufacturer);
        
        // UID
        char uid_str[64];
        snprintf(uid_str, sizeof(uid_str), "UID: %02X%02X%02X%02X",
                 scanner_state->tag.uid[0], scanner_state->tag.uid[1],
                 scanner_state->tag.uid[2], scanner_state->tag.uid[3]);
        canvas_draw_str(canvas, 2, 32, uid_str);
        
        // Memory info
        char mem_str[64];
        snprintf(mem_str, sizeof(mem_str), "Memory: %u blocks",
                 scanner_state->tag.block_count);
        canvas_draw_str(canvas, 2, 42, mem_str);
        
        // Security status
        const char* security = scanner_state->tag.has_password ? 
            "🔒 Protected" : "🔓 Open";
        canvas_draw_str(canvas, 2, 52, security);
    }
    
    // Status bar
    canvas_draw_line(canvas, 0, 60, 128, 60);
    canvas_draw_str(canvas, 2, 63, scanner_state->status_text);
}

static bool scanner_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        
        if(event->key == InputKeyOk && scanner_state->tag_detected) {
            scene_manager_next_scene(app->scene_manager, PredatorSceneISO15693Actions);
            return true;
        }
    }
    
    return false;
}

static void scanner_timer_callback(void* context) {
    PredatorApp* app = context;
    
    if(!scanner_state) return;
    
    if(!scanner_state->tag_detected) {
        // Try to detect tag
        // Real implementation: iso15693_detect_tag(app, &scanner_state->tag)
        
        snprintf(scanner_state->status_text, sizeof(scanner_state->status_text),
                 "Scanning...");
    } else {
        snprintf(scanner_state->status_text, sizeof(scanner_state->status_text),
                 "Press OK for actions");
    }
    
    view_port_update(app->view_port);
}

void predator_scene_iso15693_scanner_on_enter(void* context) {
    PredatorApp* app = context;
    
    scanner_state = malloc(sizeof(ScannerState));
    memset(scanner_state, 0, sizeof(ScannerState));
    
    snprintf(scanner_state->status_text, sizeof(scanner_state->status_text),
             "Ready");
    
    view_port_draw_callback_set(app->view_port, scanner_draw_callback, app);
    view_port_input_callback_set(app->view_port, scanner_input_callback, app);
    
    app->timer = furi_timer_alloc(scanner_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 500);
    
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
}

bool predator_scene_iso15693_scanner_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void predator_scene_iso15693_scanner_on_exit(void* context) {
    PredatorApp* app = context;
    
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
    
    gui_remove_view_port(app->gui, app->view_port);
    
    if(scanner_state) {
        free(scanner_state);
        scanner_state = NULL;
    }
}
