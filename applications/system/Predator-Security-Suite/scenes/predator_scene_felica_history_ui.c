#include "../predator_i.h"
#include "../helpers/predator_crypto_felica.h"
#include <gui/elements.h>

// FeliCa Transaction History Viewer - Scrollable list
// Pattern: Scrollable list like car model selection

typedef struct {
    FeliCaCard card;
    FeliCaTransaction transactions[20];
    uint32_t transaction_count;
    uint32_t scroll_offset;
    char status_text[64];
} HistoryState;

static HistoryState* state = NULL;
static View* history_view = NULL;

static void history_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    if(!state) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Transaction History");

    if(state->transaction_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 30, "No transactions found");
        return;
    }

    // Draw transactions (3 visible at a time)
    canvas_set_font(canvas, FontSecondary);
    int y = 20;
    
    for(uint32_t i = state->scroll_offset; 
        i < state->transaction_count && i < state->scroll_offset + 3; 
        i++) {
        
        // Decode station
        char station[64];
        felica_decode_station_id(
            state->transactions[i].terminal_id,
            station,
            sizeof(station));
        
        // Transaction type
        const char* type = state->transactions[i].transaction_type == 0x01 ? "Entry" : "Exit";
        
        // Draw transaction
        char line1[128];
        snprintf(line1, sizeof(line1), 
                 "%s: %s",
                 type,
                 station);
        canvas_draw_str(canvas, 2, y, line1);
        
        // Amount and balance
        char line2[128];
        snprintf(line2, sizeof(line2),
                 "  -¥%u (Bal: ¥%u)",
                 state->transactions[i].amount,
                 state->transactions[i].balance_after);
        canvas_draw_str(canvas, 2, y + 9, line2);
        
        y += 18;
    }
    
    // Scroll indicator
    if(state->transaction_count > 3) {
        char scroll_info[32];
        snprintf(scroll_info, sizeof(scroll_info),
                 "(%lu/%lu)",
                 state->scroll_offset + 1,
                 state->transaction_count);
        canvas_draw_str(canvas, 90, 10, scroll_info);
        
        // Arrows
        if(state->scroll_offset > 0) {
            canvas_draw_str(canvas, 120, 30, "↑");
        }
        if(state->scroll_offset + 3 < state->transaction_count) {
            canvas_draw_str(canvas, 120, 50, "↓");
        }
    }
    
    // Status bar
    canvas_draw_line(canvas, 0, 60, 128, 60);
    canvas_draw_str(canvas, 2, 63, state->status_text);
}

static bool history_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        
        if(event->key == InputKeyUp && state->scroll_offset > 0) {
            state->scroll_offset--;
            // ViewDispatcher handles redraws automatically
            return true;
        }
        
        if(event->key == InputKeyDown && 
           state->scroll_offset + 3 < state->transaction_count) {
            state->scroll_offset++;
            // ViewDispatcher handles redraws automatically
            return true;
        }
    }
    
    return false;
}

void predator_scene_felica_history_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app || !app->view_dispatcher) return;
    
    // Allocate state
    state = malloc(sizeof(HistoryState));
    memset(state, 0, sizeof(HistoryState));
    
    // Read transaction history
    // Real implementation would call:
    // state->transaction_count = felica_read_history(
    //     app, &state->card, state->transactions, 20);
    
    snprintf(state->status_text, sizeof(state->status_text),
             "↑/↓ Scroll, Back to exit");
    
    // Create view if needed
    if(!history_view) {
        history_view = view_alloc();
        view_set_context(history_view, app);
        view_set_draw_callback(history_view, history_draw_callback);
        view_set_input_callback(history_view, history_input_callback);
        view_dispatcher_add_view(app->view_dispatcher, PredatorViewFelicaHistory, history_view);
    }
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewFelicaHistory);
}

bool predator_scene_felica_history_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void predator_scene_felica_history_on_exit(void* context) {
    UNUSED(context);
    
    if(state) {
        free(state);
        state = NULL;
    }
}
