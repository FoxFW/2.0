#include "../predator_i.h"
#include "../helpers/predator_crypto_securakey.h"
#include "../helpers/predator_wiegand_gpio.h"
#include <gui/elements.h>

// Wiegand Brute Force - Systematic card number enumeration
// Pattern: Progress bar with stats (like car brute force)

typedef enum {
    BruteStateIdle,
    BruteStateRunning,
    BruteStateComplete,
    BruteStateStopped,
} BruteStateType;

typedef struct {
    BruteStateType state;
    uint32_t fixed_facility;
    uint32_t start_card;
    uint32_t end_card;
    uint32_t current_card;
    uint32_t tries_per_second;
    uint32_t total_tried;
    uint32_t successes;
    uint32_t start_tick;
    char status_text[64];
} BruteForceState;

static BruteForceState* bf_state = NULL;

static void bruteforce_draw_callback(Canvas* canvas, void* context) {
    PredatorApp* app = context;
    if(!bf_state) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Brute Force");

    canvas_set_font(canvas, FontSecondary);
    
    if(bf_state->state == BruteStateRunning) {
        // Current card
        char current_str[64];
        snprintf(current_str, sizeof(current_str),
                 "FC:%lu Card:%lu",
                 bf_state->fixed_facility,
                 bf_state->current_card);
        canvas_draw_str(canvas, 2, 22, current_str);
        
        // Progress bar
        uint32_t total = bf_state->end_card - bf_state->start_card;
        uint32_t done = bf_state->current_card - bf_state->start_card;
        uint8_t progress = total > 0 ? (done * 100) / total : 0;
        
        canvas_draw_frame(canvas, 2, 28, 124, 10);
        if(progress > 0) {
            uint8_t fill = (120 * progress) / 100;
            canvas_draw_box(canvas, 4, 30, fill, 6);
        }
        
        // Stats
        char progress_str[32];
        snprintf(progress_str, sizeof(progress_str), "%u%%", progress);
        canvas_draw_str(canvas, 54, 50, progress_str);
        
        char tries_str[64];
        snprintf(tries_str, sizeof(tries_str),
                 "Speed: %lu/sec",
                 bf_state->tries_per_second);
        canvas_draw_str(canvas, 2, 58, tries_str);
        
    } else if(bf_state->state == BruteStateComplete) {
        canvas_draw_icon(canvas, 40, 20, &I_Success_25x25);
        canvas_draw_str(canvas, 20, 50, "Brute Force Complete");
        
        char result_str[64];
        snprintf(result_str, sizeof(result_str),
                 "Tried: %lu",
                 bf_state->total_tried);
        canvas_draw_str(canvas, 25, 58, result_str);
    } else {
        canvas_draw_str(canvas, 2, 30, "Configure and press OK");
    }
    
    // Status bar
    canvas_draw_line(canvas, 0, 60, 128, 60);
    canvas_draw_str(canvas, 2, 63, bf_state->status_text);
}

static bool bruteforce_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            bf_state->state = BruteStateStopped;
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        
        if(event->key == InputKeyOk && bf_state->state == BruteStateIdle) {
            bf_state->state = BruteStateRunning;
            bf_state->start_tick = furi_get_tick();
            
            FURI_LOG_I("WiegandBrute", "Starting brute force FC:%lu range:%lu-%lu",
                      bf_state->fixed_facility, bf_state->start_card, bf_state->end_card);
            
            return true;
        }
    }
    
    return false;
}

static void bruteforce_timer_callback(void* context) {
    PredatorApp* app = context;
    
    if(!bf_state || bf_state->state != BruteStateRunning) return;
    
    // REAL PRODUCTION CODE: Try actual card numbers
    WiegandCard test_card;
    test_card.facility_code = bf_state->fixed_facility;
    test_card.card_number = bf_state->current_card;
    test_card.format = Wiegand_26bit;
    
    // Encode and try to emulate/transmit
    if(wiegand_create_card(
        test_card.facility_code,
        test_card.card_number,
        test_card.format,
        &test_card)) {
        
        // Try transmission via GPIO
        bool success = wiegand_try_card(app, &test_card);
        
        if(success) {
            bf_state->successes++;
            FURI_LOG_I("WiegandBrute", "SUCCESS: FC:%u Card:%lu",
                      test_card.facility_code, test_card.card_number);
        }
    }
    
    bf_state->current_card++;
    bf_state->total_tried++;
    
    // Calculate real speed
    uint32_t elapsed_ms = furi_get_tick() - bf_state->start_tick;
    if(elapsed_ms > 0) {
        bf_state->tries_per_second = (bf_state->total_tried * 1000) / elapsed_ms;
    }
    
    if(bf_state->current_card >= bf_state->end_card) {
        bf_state->state = BruteStateComplete;
        snprintf(bf_state->status_text, sizeof(bf_state->status_text),
                 "Complete! %lu successes", bf_state->successes);
    }
    
    view_port_update(app->view_port);
}

void predator_scene_wiegand_bruteforce_on_enter(void* context) {
    PredatorApp* app = context;
    
    bf_state = malloc(sizeof(BruteForceState));
    memset(bf_state, 0, sizeof(BruteForceState));
    
    bf_state->state = BruteStateIdle;
    bf_state->fixed_facility = 100;
    bf_state->start_card = 1;
    bf_state->end_card = 65535;
    bf_state->current_card = bf_state->start_card;
    
    snprintf(bf_state->status_text, sizeof(bf_state->status_text),
             "Ready");
    
    view_port_draw_callback_set(app->view_port, bruteforce_draw_callback, app);
    view_port_input_callback_set(app->view_port, bruteforce_input_callback, app);
    
    app->timer = furi_timer_alloc(bruteforce_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 100);
    
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
}

bool predator_scene_wiegand_bruteforce_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void predator_scene_wiegand_bruteforce_on_exit(void* context) {
    PredatorApp* app = context;
    
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
    
    gui_remove_view_port(app->gui, app->view_port);
    
    if(bf_state) {
        free(bf_state);
        bf_state = NULL;
    }
}
