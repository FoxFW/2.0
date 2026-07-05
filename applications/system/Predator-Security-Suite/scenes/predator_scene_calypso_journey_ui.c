#include "../predator_i.h"
#include "../helpers/predator_crypto_calypso.h"
#include <gui/elements.h>

// Calypso Journey Log Viewer - Scrollable event list
// Pattern: Same as FeliCa History

typedef struct {
    CalypsoCard card;
    CalypsoEvent events[20];
    uint32_t event_count;
    uint32_t scroll_offset;
    char status_text[64];
} JourneyState;

static JourneyState* state = NULL;
static View* journey_view = NULL;

static void journey_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    if(!state) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Journey Log");

    if(state->event_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 30, "No journey data found");
        return;
    }

    // Draw events (3 visible at a time)
    canvas_set_font(canvas, FontSecondary);
    int y = 20;
    
    for(uint32_t i = state->scroll_offset; 
        i < state->event_count && i < state->scroll_offset + 3; 
        i++) {
        
        // Decode station
        char station[64];
        calypso_decode_navigo_station(
            state->events[i].location_id,
            station,
            sizeof(station));
        
        // Event type icon and name
        const char* event_icon;
        const char* event_name;
        
        switch(state->events[i].event_type) {
        case 0x01:
            event_icon = "→";
            event_name = "Entry";
            break;
        case 0x02:
            event_icon = "←";
            event_name = "Exit";
            break;
        case 0x07:
            event_icon = "✓";
            event_name = "Check";
            break;
        default:
            event_icon = "•";
            event_name = "Event";
            break;
        }
        
        // Draw event
        char line1[128];
        snprintf(line1, sizeof(line1), 
                 "%s %s: %s",
                 event_icon,
                 event_name,
                 station);
        canvas_draw_str(canvas, 2, y, line1);
        
        // Date and contract
        char line2[128];
        snprintf(line2, sizeof(line2),
                 "  %02X/%02X/%02X Contract #%u",
                 state->events[i].date[2],
                 state->events[i].date[1],
                 state->events[i].date[0],
                 state->events[i].contract_used);
        canvas_draw_str(canvas, 2, y + 9, line2);
        
        y += 18;
    }
    
    // Scroll indicator
    if(state->event_count > 3) {
        char scroll_info[32];
        snprintf(scroll_info, sizeof(scroll_info),
                 "(%lu/%lu)",
                 state->scroll_offset + 1,
                 state->event_count);
        canvas_draw_str(canvas, 90, 10, scroll_info);
        
        // Arrows
        if(state->scroll_offset > 0) {
            canvas_draw_str(canvas, 120, 30, "↑");
        }
        if(state->scroll_offset + 3 < state->event_count) {
            canvas_draw_str(canvas, 120, 50, "↓");
        }
    }
    
    // Status bar
    canvas_draw_line(canvas, 0, 60, 128, 60);
    canvas_draw_str(canvas, 2, 63, state->status_text);
}

static bool journey_input_callback(InputEvent* event, void* context) {
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
           state->scroll_offset + 3 < state->event_count) {
            state->scroll_offset++;
            // ViewDispatcher handles redraws automatically
            return true;
        }
    }
    
    return false;
}

void predator_scene_calypso_journey_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app || !app->view_dispatcher) return;
    
    // Allocate state
    state = malloc(sizeof(JourneyState));
    memset(state, 0, sizeof(JourneyState));
    
    // Read journey history
    // Real implementation would call:
    // state->event_count = calypso_read_event_log(
    //     app, &state->card, state->events, 20);
    
    snprintf(state->status_text, sizeof(state->status_text),
             "↑/↓ Scroll, Back to exit");
    
    // Create view if needed
    if(!journey_view) {
        journey_view = view_alloc();
        view_set_context(journey_view, app);
        view_set_draw_callback(journey_view, journey_draw_callback);
        view_set_input_callback(journey_view, journey_input_callback);
        view_dispatcher_add_view(app->view_dispatcher, PredatorViewCalypsoJourney, journey_view);
    }
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewCalypsoJourney);
}

bool predator_scene_calypso_journey_ui_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void predator_scene_calypso_journey_ui_on_exit(void* context) {
    UNUSED(context);
    
    if(state) {
        free(state);
        state = NULL;
    }
}
