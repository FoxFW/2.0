#include "../predator_i.h"
#include "../helpers/predator_crypto_em4305.h"
#include "../helpers/predator_em4305_hal.h"
#include <gui/elements.h>

// EM4305 Clone Wizard - Multi-step cloning process
// Pattern: State machine wizard like car clone

typedef enum {
    CloneStepReadSource,
    CloneStepShowData,
    CloneStepPlaceTarget,
    CloneStepWrite,
    CloneStepVerify,
    CloneStepComplete,
} CloneStepType;

typedef struct {
    CloneStepType current_step;
    EM4305Card source_card;
    uint8_t progress;
    char status_text[64];
} CloneWizardState;

static CloneWizardState* wiz_state = NULL;

static void clone_wizard_draw_callback(Canvas* canvas, void* context) {
    PredatorApp* app = context;
    if(!wiz_state) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "EM4305 Clone Wizard");

    canvas_set_font(canvas, FontSecondary);
    
    switch(wiz_state->current_step) {
    case CloneStepReadSource:
        canvas_draw_str(canvas, 2, 24, "Step 1: Read source card");
        canvas_draw_str(canvas, 2, 34, "Place EM4100/HID card");
        canvas_draw_str(canvas, 2, 44, "on reader...");
        break;
        
    case CloneStepShowData:
        canvas_draw_str(canvas, 2, 24, "Step 2: Card detected");
        char id_str[64];
        snprintf(id_str, sizeof(id_str),
                 "ID: %08lX",
                 wiz_state->source_card.id);
        canvas_draw_str(canvas, 2, 34, id_str);
        canvas_draw_str(canvas, 2, 50, "Press OK to continue");
        break;
        
    case CloneStepPlaceTarget:
        canvas_draw_str(canvas, 2, 24, "Step 3: Place target");
        canvas_draw_str(canvas, 2, 34, "Place blank EM4305");
        canvas_draw_str(canvas, 2, 44, "on reader...");
        break;
        
    case CloneStepWrite:
        canvas_draw_str(canvas, 2, 24, "Step 4: Writing...");
        
        // Progress bar
        canvas_draw_frame(canvas, 2, 32, 124, 10);
        if(wiz_state->progress > 0) {
            uint8_t fill = (120 * wiz_state->progress) / 100;
            canvas_draw_box(canvas, 4, 34, fill, 6);
        }
        
        char progress_str[16];
        snprintf(progress_str, sizeof(progress_str), "%u%%", wiz_state->progress);
        canvas_draw_str(canvas, 54, 54, progress_str);
        break;
        
    case CloneStepVerify:
        canvas_draw_str(canvas, 2, 24, "Step 5: Verifying...");
        canvas_draw_str(canvas, 2, 34, "Reading back data...");
        break;
        
    case CloneStepComplete:
        canvas_draw_icon(canvas, 40, 20, &I_Success_25x25);
        canvas_draw_str(canvas, 22, 50, "Clone Complete!");
        canvas_draw_str(canvas, 12, 58, "Press Back to exit");
        break;
    }
    
    // Status bar
    canvas_draw_line(canvas, 0, 60, 128, 60);
    canvas_draw_str(canvas, 2, 63, wiz_state->status_text);
}

static bool clone_wizard_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        
        if(event->key == InputKeyOk && wiz_state->current_step == CloneStepShowData) {
            wiz_state->current_step = CloneStepPlaceTarget;
            view_port_update(app->view_port);
            return true;
        }
    }
    
    return false;
}

static void clone_wizard_timer_callback(void* context) {
    PredatorApp* app = context;
    
    if(!wiz_state) return;
    
    switch(wiz_state->current_step) {
    case CloneStepReadSource:
        // REAL PRODUCTION CODE: Detect and read source card
        if(em4305_detect_source(app, &wiz_state->source_card)) {
            wiz_state->current_step = CloneStepShowData;
            
            FURI_LOG_I("EM4305Clone", "Source card detected ID:%08lX",
                      wiz_state->source_card.id);
            
            snprintf(wiz_state->status_text, sizeof(wiz_state->status_text),
                     "Card detected!");
        }
        break;
        
    case CloneStepPlaceTarget:
        // REAL PRODUCTION CODE: Detect blank EM4305
        if(em4305_detect_blank(app)) {
            wiz_state->current_step = CloneStepWrite;
            wiz_state->progress = 0;
            
            FURI_LOG_I("EM4305Clone", "Blank EM4305 detected, starting write");
            
            snprintf(wiz_state->status_text, sizeof(wiz_state->status_text),
                     "Writing...");
        }
        break;
        
    case CloneStepWrite:
        // REAL PRODUCTION CODE: Write blocks to EM4305
        if(wiz_state->progress < 100) {
            uint8_t block = wiz_state->progress / 20; // 5 blocks total
            
            if(em4305_write_block(app, block, &wiz_state->source_card)) {
                wiz_state->progress += 20;
                
                FURI_LOG_I("EM4305Clone", "Wrote block %u (%u%%)",
                          block, wiz_state->progress);
            } else {
                // Write error
                FURI_LOG_E("EM4305Clone", "Write block %u failed", block);
            }
        }
        
        if(wiz_state->progress >= 100) {
            wiz_state->current_step = CloneStepVerify;
        }
        break;
        
    case CloneStepVerify:
        // REAL PRODUCTION CODE: Verify clone by reading back
        EM4305Card verify_card;
        if(em4305_read_card(app, &verify_card)) {
            if(verify_card.id == wiz_state->source_card.id) {
                wiz_state->current_step = CloneStepComplete;
                
                FURI_LOG_I("EM4305Clone", "Verification SUCCESS");
                
                snprintf(wiz_state->status_text, sizeof(wiz_state->status_text),
                         "Clone successful!");
            } else {
                FURI_LOG_E("EM4305Clone", "Verification FAILED: ID mismatch");
            }
        }
        break;
        
    default:
        break;
    }
    
    view_port_update(app->view_port);
}

void predator_scene_em4305_clone_on_enter(void* context) {
    PredatorApp* app = context;
    
    wiz_state = malloc(sizeof(CloneWizardState));
    memset(wiz_state, 0, sizeof(CloneWizardState));
    
    wiz_state->current_step = CloneStepReadSource;
    snprintf(wiz_state->status_text, sizeof(wiz_state->status_text),
             "Wizard started");
    
    view_port_draw_callback_set(app->view_port, clone_wizard_draw_callback, app);
    view_port_input_callback_set(app->view_port, clone_wizard_input_callback, app);
    
    app->timer = furi_timer_alloc(clone_wizard_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 500);
    
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
}

bool predator_scene_em4305_clone_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void predator_scene_em4305_clone_on_exit(void* context) {
    PredatorApp* app = context;
    
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
    
    gui_remove_view_port(app->gui, app->view_port);
    
    if(wiz_state) {
        free(wiz_state);
        wiz_state = NULL;
    }
}
