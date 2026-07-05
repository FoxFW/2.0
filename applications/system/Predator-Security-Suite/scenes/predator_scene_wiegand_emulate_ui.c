#include "../predator_i.h"
#include "../helpers/predator_crypto_securakey.h"
#include "../helpers/predator_wiegand_gpio.h"
#include <gui/elements.h>

// Wiegand Emulator - Play back captured card
// Pattern: Timer-based with real-time display

typedef struct {
    WiegandCard card;
    bool emulating;
    uint32_t duration_seconds;
    uint32_t start_tick;
    char status_text[64];
} EmulateState;

static EmulateState* state = NULL;

static void emulate_draw_callback(Canvas* canvas, void* context) {
    PredatorApp* app = context;
    if(!state) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Wiegand Emulator");

    canvas_set_font(canvas, FontSecondary);
    
    if(state->emulating) {
        // Show card being emulated
        char fc_str[64];
        snprintf(fc_str, sizeof(fc_str), "FC: %u", state->card.facility_code);
        canvas_draw_str(canvas, 2, 24, fc_str);
        
        char card_str[64];
        snprintf(card_str, sizeof(card_str), "Card: %lu", state->card.card_number);
        canvas_draw_str(canvas, 2, 34, card_str);
        
        // Duration
        uint32_t elapsed = (furi_get_tick() - state->start_tick) / 1000;
        char duration_str[64];
        snprintf(duration_str, sizeof(duration_str), "Duration: %lu sec", elapsed);
        canvas_draw_str(canvas, 2, 44, duration_str);
        
        // Emulating indicator
        canvas_draw_icon(canvas, 54, 18, &I_Success_25x25);
        canvas_draw_str(canvas, 32, 54, "EMULATING");
        
    } else {
        canvas_draw_str(canvas, 2, 30, "Press OK to start");
        canvas_draw_str(canvas, 2, 40, "emulation");
    }
    
    // Status bar
    canvas_draw_line(canvas, 0, 60, 128, 60);
    canvas_draw_str(canvas, 2, 63, state->status_text);
}

static bool emulate_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            state->emulating = false;
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        
        if(event->key == InputKeyOk) {
            state->emulating = !state->emulating;
            if(state->emulating) {
                state->start_tick = furi_get_tick();
                
                // REAL PRODUCTION CODE: Start Wiegand emulation via GPIO
                wiegand_start_emulation(app, &state->card);
                
                FURI_LOG_I("WiegandEmulate", "Started emulation FC:%u Card:%lu",
                           state->card.facility_code, state->card.card_number);
                
                snprintf(state->status_text, sizeof(state->status_text),
                         "Emulating...");
            } else {
                // Stop emulation
                wiegand_stop_emulation(app);
                
                FURI_LOG_I("WiegandEmulate", "Stopped emulation");
                
                snprintf(state->status_text, sizeof(state->status_text),
                         "Stopped");
            }
            view_port_update(app->view_port);
            return true;
        }
    }
    
    return false;
}

void predator_scene_wiegand_emulate_on_enter(void* context) {
    PredatorApp* app = context;
    
    state = malloc(sizeof(EmulateState));
    memset(state, 0, sizeof(EmulateState));
    
    snprintf(state->status_text, sizeof(state->status_text),
             "OK to toggle");
    
    view_port_draw_callback_set(app->view_port, emulate_draw_callback, app);
    view_port_input_callback_set(app->view_port, emulate_input_callback, app);
    
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
}

bool predator_scene_wiegand_emulate_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void predator_scene_wiegand_emulate_on_exit(void* context) {
    PredatorApp* app = context;
    
    if(state && state->emulating) {
        // Stop emulation
        state->emulating = false;
    }
    
    gui_remove_view_port(app->gui, app->view_port);
    
    if(state) {
        free(state);
        state = NULL;
    }
}
