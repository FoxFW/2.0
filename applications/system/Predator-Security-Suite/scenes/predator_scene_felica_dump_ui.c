#include "../predator_i.h"
#include "../helpers/predator_crypto_felica.h"
#include <gui/elements.h>

// FeliCa Card Dump - Progress bar operation
// Pattern: Similar to car attack progress

typedef enum {
    DumpStateInitializing,
    DumpStateReading,
    DumpStateSaving,
    DumpStateComplete,
    DumpStateError,
} DumpStateType;

typedef struct {
    FeliCaCard card;
    DumpStateType dump_state;
    uint8_t progress;  // 0-100
    uint32_t blocks_read;
    uint32_t total_blocks;
    char status_text[64];
    char error_text[64];
} DumpState;

static DumpState* state = NULL;
static View* dump_view = NULL;

static void dump_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    if(!state) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Card Dump");
    
    canvas_set_font(canvas, FontSecondary);
    
    if(state->dump_state == DumpStateError) {
        canvas_draw_str(canvas, 2, 25, "Error:");
        canvas_draw_str(canvas, 2, 35, state->error_text);
        canvas_draw_str(canvas, 2, 50, "Press Back to exit");
        return;
    }
    
    if(state->dump_state == DumpStateComplete) {
        // Success indication (icon not available in SDK)
        canvas_draw_str(canvas, 35, 30, "[SUCCESS]");
        canvas_draw_str(canvas, 25, 50, "Dump Complete!");
        canvas_draw_str(canvas, 10, 58, "Saved to SD card");
        return;
    }
    
    // Progress bar
    canvas_draw_str(canvas, 2, 22, state->status_text);
    
    // Progress bar frame
    canvas_draw_frame(canvas, 2, 28, 124, 10);
    
    // Progress bar fill
    if(state->progress > 0) {
        uint8_t fill_width = (120 * state->progress) / 100;
        canvas_draw_box(canvas, 4, 30, fill_width, 6);
    }
    
    // Percentage
    char percent_str[16];
    snprintf(percent_str, sizeof(percent_str), "%u%%", state->progress);
    canvas_draw_str(canvas, 54, 50, percent_str);
    
    // Block counter
    char blocks_str[64];
    snprintf(blocks_str, sizeof(blocks_str),
             "Blocks: %lu/%lu",
             state->blocks_read,
             state->total_blocks);
    canvas_draw_str(canvas, 25, 58, blocks_str);
}

static bool dump_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    
    return false;
}

static void dump_timer_callback(void* context) {
    PredatorApp* app = context;
    
    if(!state) return;
    
    switch(state->dump_state) {
    case DumpStateInitializing:
        snprintf(state->status_text, sizeof(state->status_text),
                 "Initializing...");
        state->dump_state = DumpStateReading;
        state->total_blocks = 16; // Typical FeliCa
        break;
        
    case DumpStateReading:
        // Simulate reading blocks
        if(state->blocks_read < state->total_blocks) {
            state->blocks_read++;
            state->progress = (state->blocks_read * 80) / state->total_blocks;
            snprintf(state->status_text, sizeof(state->status_text),
                     "Reading card data...");
        } else {
            state->dump_state = DumpStateSaving;
        }
        break;
        
    case DumpStateSaving:
        state->progress = 90;
        snprintf(state->status_text, sizeof(state->status_text),
                 "Saving to SD card...");
        
        // Real implementation would call:
        // bool saved = felica_save_dump(app, &state->card, "/ext/felica_dump.txt");
        // if(saved) {
        //     state->dump_state = DumpStateComplete;
        // } else {
        //     state->dump_state = DumpStateError;
        //     snprintf(state->error_text, sizeof(state->error_text),
        //              "Failed to save file");
        // }
        
        state->progress = 100;
        state->dump_state = DumpStateComplete;
        break;
        
    case DumpStateComplete:
    case DumpStateError:
        // Stop timer
        if(app->timer) {
            furi_timer_stop(app->timer);
        }
        break;
    }
    
    // ViewDispatcher handles redraws automatically
}

void predator_scene_felica_dump_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app || !app->view_dispatcher) return;
    
    // Allocate state
    state = malloc(sizeof(DumpState));
    memset(state, 0, sizeof(DumpState));
    state->dump_state = DumpStateInitializing;
    
    // Create view if needed
    if(!dump_view) {
        dump_view = view_alloc();
        view_set_context(dump_view, app);
        view_set_draw_callback(dump_view, dump_draw_callback);
        view_set_input_callback(dump_view, dump_input_callback);
        view_dispatcher_add_view(app->view_dispatcher, PredatorViewFelicaDump, dump_view);
    }
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewFelicaDump);
    
    // Start timer for progress
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
    }
    app->timer = furi_timer_alloc(dump_timer_callback, FuriTimerTypePeriodic, app);
    if(app->timer) furi_timer_start(app->timer, 200);
}

bool predator_scene_felica_dump_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void predator_scene_felica_dump_on_exit(void* context) {
    PredatorApp* app = context;
    
    if(app && app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
    
    if(state) {
        free(state);
        state = NULL;
    }
}
