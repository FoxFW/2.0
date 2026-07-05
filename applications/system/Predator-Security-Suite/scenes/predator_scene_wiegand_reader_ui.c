#include "../predator_i.h"
#include "../helpers/predator_crypto_securakey.h"
#include <gui/elements.h>

// Wiegand Card Reader - Real-time D0/D1 monitoring
// Pattern: Similar to Transit readers but for access control

typedef struct {
    WiegandCard card;
    bool card_detected;
    char bit_string[65];
    char format_name[32];
    char manufacturer_name[32];
    uint32_t last_update;
    char status_text[64];
} WiegandReaderState;

static WiegandReaderState* reader_state = NULL;

static void wiegand_reader_draw_callback(Canvas* canvas, void* context) {
    PredatorApp* app = context;
    if(!reader_state) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(!reader_state->card_detected) {
        // Waiting for card
        canvas_draw_str(canvas, 2, 12, "Wiegand Reader");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 24, "Monitoring D0/D1 lines...");
        
        // Animated waiting
        char waiting[4] = ".  ";
        uint32_t ticks = furi_get_tick() / 500;
        for(int i = 0; i < (ticks % 3) + 1; i++) {
            waiting[i] = '.';
        }
        canvas_draw_str(canvas, 102, 24, waiting);
        
        // Supported formats
        canvas_draw_str(canvas, 2, 38, "Supported:");
        canvas_draw_str(canvas, 2, 47, "26-bit HID, 35-bit Corp");
        canvas_draw_str(canvas, 2, 55, "37-bit H10304, SECURAKEY");
        
    } else {
        // Card detected
        canvas_draw_str(canvas, 2, 10, "Card Detected!");
        
        // Format and manufacturer
        canvas_set_font(canvas, FontSecondary);
        char info_str[64];
        snprintf(info_str, sizeof(info_str), 
                 "%s (%s)",
                 reader_state->format_name,
                 reader_state->manufacturer_name);
        canvas_draw_str(canvas, 2, 20, info_str);
        
        // Facility and card number
        char fc_str[64];
        snprintf(fc_str, sizeof(fc_str),
                 "Facility: %u",
                 reader_state->card.facility_code);
        canvas_draw_str(canvas, 2, 30, fc_str);
        
        char card_str[64];
        snprintf(card_str, sizeof(card_str),
                 "Card: %lu",
                 reader_state->card.card_number);
        canvas_draw_str(canvas, 2, 38, card_str);
        
        // Parity status
        const char* parity_status = reader_state->card.parity_valid ? 
            "✓ Valid" : "✗ Invalid";
        char parity_str[64];
        snprintf(parity_str, sizeof(parity_str), "Parity: %s", parity_status);
        canvas_draw_str(canvas, 2, 46, parity_str);
        
        // Bit representation (first 26 bits)
        canvas_set_font(canvas, FontKeyboard);
        char bits_short[28];
        strncpy(bits_short, reader_state->bit_string, 26);
        bits_short[26] = '\0';
        canvas_draw_str(canvas, 2, 54, bits_short);
    }
    
    // Status bar
    canvas_draw_line(canvas, 0, 60, 128, 60);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 63, reader_state->status_text);
}

static bool wiegand_reader_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        
        if(event->key == InputKeyOk && reader_state->card_detected) {
            scene_manager_next_scene(app->scene_manager, PredatorSceneWiegandActions);
            return true;
        }
    }
    
    return false;
}

static void wiegand_reader_timer_callback(void* context) {
    PredatorApp* app = context;
    
    if(!reader_state) return;
    
    // Try to detect card
    if(!reader_state->card_detected) {
        // Real implementation would call:
        // if(wiegand_read_card(app, &reader_state->card, 1000)) {
        //     reader_state->card_detected = true;
        //     
        //     // Get format name
        //     strncpy(reader_state->format_name,
        //             wiegand_get_format_name(reader_state->card.format),
        //             sizeof(reader_state->format_name));
        //     
        //     // Get manufacturer
        //     strncpy(reader_state->manufacturer_name,
        //             wiegand_get_manufacturer_name(reader_state->card.manufacturer),
        //             sizeof(reader_state->manufacturer_name));
        //     
        //     // Convert to bit string
        //     wiegand_to_bit_string(reader_state->card.raw_data,
        //                          reader_state->card.bit_length,
        //                          reader_state->bit_string);
        // }
        
        snprintf(reader_state->status_text, sizeof(reader_state->status_text),
                 "Listening on D0/D1...");
    } else {
        snprintf(reader_state->status_text, sizeof(reader_state->status_text),
                 "Press OK for actions");
    }
    
    view_port_update(app->view_port);
}

void predator_scene_wiegand_reader_on_enter(void* context) {
    PredatorApp* app = context;
    
    // Allocate state
    reader_state = malloc(sizeof(WiegandReaderState));
    memset(reader_state, 0, sizeof(WiegandReaderState));
    
    snprintf(reader_state->status_text, sizeof(reader_state->status_text),
             "Ready");
    
    // Setup view port
    view_port_draw_callback_set(app->view_port, wiegand_reader_draw_callback, app);
    view_port_input_callback_set(app->view_port, wiegand_reader_input_callback, app);
    
    // Start timer
    app->timer = furi_timer_alloc(wiegand_reader_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 500);
    
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
}

bool predator_scene_wiegand_reader_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void predator_scene_wiegand_reader_on_exit(void* context) {
    PredatorApp* app = context;
    
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
    
    gui_remove_view_port(app->gui, app->view_port);
    
    if(reader_state) {
        free(reader_state);
        reader_state = NULL;
    }
}
