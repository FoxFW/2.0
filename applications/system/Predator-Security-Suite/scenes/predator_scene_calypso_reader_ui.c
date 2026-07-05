#include "../predator_i.h"
#include "../helpers/predator_crypto_calypso.h"
#include <gui/elements.h>

// Calypso Card Reader - Real-time card detection (Paris Navigo, Brussels MOBIB, etc.)
// Pattern: Similar to FeliCa but for European transit

typedef struct {
    CalypsoCard card;
    bool card_detected;
    CalypsoContract contracts[2];
    uint32_t contract_count;
    CalypsoEvent events[5];
    uint32_t event_count;
    uint32_t last_update;
    char status_text[64];
} CalypsoReaderState;

static CalypsoReaderState* reader_state = NULL;
static View* reader_view = NULL;

static void calypso_reader_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    if(!reader_state) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(!reader_state->card_detected) {
        // Waiting for card
        canvas_draw_str(canvas, 2, 12, "Calypso Reader");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 24, "Place card on reader...");
        
        // Animated waiting
        char waiting[4] = ".  ";
        uint32_t ticks = furi_get_tick() / 500;
        for(uint32_t i = 0; i < (ticks % 3) + 1; i++) {
            waiting[i] = '.';
        }
        canvas_draw_str(canvas, 90, 24, waiting);
        
        // Supported cards
        canvas_draw_str(canvas, 2, 38, "Supported:");
        canvas_draw_str(canvas, 2, 47, "Navigo (Paris)");
        canvas_draw_str(canvas, 2, 55, "MOBIB (Brussels)");
        
    } else {
        // Card detected
        canvas_draw_str(canvas, 2, 10, calypso_get_card_name(reader_state->card.card_type));
        
        // Active contracts
        canvas_set_font(canvas, FontSecondary);
        if(reader_state->contract_count > 0) {
            char contract_str[64];
            snprintf(contract_str, sizeof(contract_str), 
                     "Contract #%u (Active)",
                     reader_state->contracts[0].contract_number);
            canvas_draw_str(canvas, 2, 22, contract_str);
            
            // Validity
            char valid_str[64];
            snprintf(valid_str, sizeof(valid_str),
                     "Valid until: %02X/%02X/%02X",
                     reader_state->contracts[0].validity_end[2],
                     reader_state->contracts[0].validity_end[1],
                     reader_state->contracts[0].validity_end[0]);
            canvas_draw_str(canvas, 2, 32, valid_str);
        } else {
            canvas_draw_str(canvas, 2, 22, "No active contracts");
        }
        
        // Recent events
        if(reader_state->event_count > 0) {
            canvas_draw_str(canvas, 2, 42, "Last Trip:");
            
            char station[64];
            calypso_decode_navigo_station(
                reader_state->events[0].location_id,
                station,
                sizeof(station));
            
            char event_str[128];
            const char* event_type = reader_state->events[0].event_type == 0x01 ? 
                "Entry" : "Exit";
            snprintf(event_str, sizeof(event_str),
                     "%s at %s",
                     event_type,
                     station);
            
            canvas_draw_str(canvas, 4, 52, event_str);
        }
    }
    
    // Status bar
    canvas_draw_line(canvas, 0, 60, 128, 60);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 63, reader_state->status_text);
}

static bool calypso_reader_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        
        if(event->key == InputKeyOk && reader_state->card_detected) {
            scene_manager_next_scene(app->scene_manager, PredatorSceneCalypsoActionsUI);
            return true;
        }
    }
    
    return false;
}

static void calypso_reader_timer_callback(void* context) {
    UNUSED(context);
    
    if(!reader_state) return;
    
    // Try to detect card
    if(!reader_state->card_detected) {
        // Real implementation would call:
        // if(calypso_detect_card(app, &reader_state->card)) {
        //     reader_state->card_detected = true;
        //     // Read contracts
        //     reader_state->contract_count = calypso_read_all_contracts(
        //         app, &reader_state->card,
        //         reader_state->contracts, 2);
        //     // Read events
        //     reader_state->event_count = calypso_read_event_log(
        //         app, &reader_state->card,
        //         reader_state->events, 5);
        // }
        
        snprintf(reader_state->status_text, sizeof(reader_state->status_text),
                 "Scanning...");
    } else {
        snprintf(reader_state->status_text, sizeof(reader_state->status_text),
                 "Press OK for actions");
    }
    
    // ViewDispatcher handles redraws automatically
}

void predator_scene_calypso_reader_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app || !app->view_dispatcher) return;
    
    // Allocate state
    reader_state = malloc(sizeof(CalypsoReaderState));
    memset(reader_state, 0, sizeof(CalypsoReaderState));
    
    snprintf(reader_state->status_text, sizeof(reader_state->status_text),
             "Waiting for card...");
    
    // Create view if needed
    if(!reader_view) {
        reader_view = view_alloc();
        view_set_context(reader_view, app);
        view_set_draw_callback(reader_view, calypso_reader_draw_callback);
        view_set_input_callback(reader_view, calypso_reader_input_callback);
        view_dispatcher_add_view(app->view_dispatcher, PredatorViewCalypsoReader, reader_view);
    }
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewCalypsoReader);
    
    // Start timer
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
    }
    app->timer = furi_timer_alloc(calypso_reader_timer_callback, FuriTimerTypePeriodic, app);
    if(app->timer) furi_timer_start(app->timer, 500);
}

bool predator_scene_calypso_reader_ui_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void predator_scene_calypso_reader_ui_on_exit(void* context) {
    PredatorApp* app = context;
    
    if(app && app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
    
    if(reader_state) {
        free(reader_state);
        reader_state = NULL;
    }
}
