#include "../predator_i.h"
#include "../helpers/predator_crypto_em4305.h"
#include "../helpers/predator_em4305_hal.h"
#include <gui/elements.h>

// EM4305 Custom Write - Manual data entry
// Pattern: Simple write with confirmation

typedef enum {
    WriteStateInput,
    WriteStateWriting,
    WriteStateComplete,
    WriteStateError,
} WriteStateType;

typedef struct {
    WriteStateType state;
    EM4305Card card;
    uint8_t progress;
    char status_text[64];
} CustomWriteState;

static CustomWriteState* write_state = NULL;

static void custom_write_draw_callback(Canvas* canvas, void* context) {
    PredatorApp* app = context;
    if(!write_state) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Custom Write");

    canvas_set_font(canvas, FontSecondary);
    
    switch(write_state->state) {
    case WriteStateInput:
        canvas_draw_str(canvas, 2, 24, "ID to write:");
        
        char id_str[64];
        snprintf(id_str, sizeof(id_str), "%08lX", write_state->card.id);
        canvas_draw_str(canvas, 2, 34, id_str);
        
        canvas_draw_str(canvas, 2, 50, "Press OK to write");
        break;
        
    case WriteStateWriting:
        canvas_draw_str(canvas, 2, 24, "Writing...");
        
        // Progress bar
        canvas_draw_frame(canvas, 2, 32, 124, 10);
        if(write_state->progress > 0) {
            uint8_t fill = (120 * write_state->progress) / 100;
            canvas_draw_box(canvas, 4, 34, fill, 6);
        }
        
        char progress_str[16];
        snprintf(progress_str, sizeof(progress_str), "%u%%", write_state->progress);
        canvas_draw_str(canvas, 54, 54, progress_str);
        break;
        
    case WriteStateComplete:
        canvas_draw_icon(canvas, 40, 20, &I_Success_25x25);
        canvas_draw_str(canvas, 25, 50, "Write Complete!");
        break;
        
    case WriteStateError:
        canvas_draw_str(canvas, 20, 30, "Write Failed");
        canvas_draw_str(canvas, 10, 40, "Check card/password");
        break;
    }
    
    // Status bar
    canvas_draw_line(canvas, 0, 60, 128, 60);
    canvas_draw_str(canvas, 2, 63, write_state->status_text);
}

static bool custom_write_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        
        if(event->key == InputKeyOk && write_state->state == WriteStateInput) {
            write_state->state = WriteStateWriting;
            write_state->progress = 0;
            view_port_update(app->view_port);
            return true;
        }
    }
    
    return false;
}

static void custom_write_timer_callback(void* context) {
    PredatorApp* app = context;
    
    if(!write_state || write_state->state != WriteStateWriting) return;
    
    if(write_state->progress < 100) {
        // Write blocks progressively
        uint8_t block = (write_state->progress / 25) + 1;
        
        if(write_state->progress % 25 == 0 && block <= 4) {
            FURI_LOG_I("EM4305CustomWrite", "Writing block %u", block);
            
            if(em4305_write_block(app, block, &write_state->card)) {
                write_state->progress += 25;
            } else {
                write_state->state = WriteStateError;
                snprintf(write_state->status_text, sizeof(write_state->status_text),
                         "Write error");
                FURI_LOG_E("EM4305CustomWrite", "Failed to write block %u", block);
            }
        }
    } else {
        write_state->state = WriteStateComplete;
        snprintf(write_state->status_text, sizeof(write_state->status_text),
                 "Success!");
    }
    
    view_port_update(app->view_port);
}

void predator_scene_em4305_custom_write_on_enter(void* context) {
    PredatorApp* app = context;
    
    write_state = malloc(sizeof(CustomWriteState));
    memset(write_state, 0, sizeof(CustomWriteState));
    
    write_state->state = WriteStateInput;
    write_state->card.id = 0x12345678; // Default ID
    write_state->card.card_type = EM4305_EM4100;
    
    // Encode ID into card data
    write_state->card.data[0] = (write_state->card.id >> 24) & 0xFF;
    write_state->card.data[1] = (write_state->card.id >> 16) & 0xFF;
    write_state->card.data[2] = (write_state->card.id >> 8) & 0xFF;
    write_state->card.data[3] = write_state->card.id & 0xFF;
    
    snprintf(write_state->status_text, sizeof(write_state->status_text),
             "Ready");
    
    view_port_draw_callback_set(app->view_port, custom_write_draw_callback, app);
    view_port_input_callback_set(app->view_port, custom_write_input_callback, app);
    
    app->timer = furi_timer_alloc(custom_write_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 200);
    
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
}

bool predator_scene_em4305_custom_write_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void predator_scene_em4305_custom_write_on_exit(void* context) {
    PredatorApp* app = context;
    
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
    
    gui_remove_view_port(app->gui, app->view_port);
    
    if(write_state) {
        free(write_state);
        write_state = NULL;
    }
}
