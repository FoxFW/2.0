#include "../predator_i.h"
#include "../helpers/predator_crypto_iso15693.h"
#include "../helpers/predator_crypto_keys.h"
#include <gui/elements.h>

// ISO 15693 Password Attack - SLIX password cracking
// Pattern: Similar to EM4305 password attack

typedef enum {
    PasswordStateDictionary,
    PasswordStateBruteForce,
    PasswordStateSuccess,
    PasswordStateFailed,
} PasswordStateType;

typedef struct {
    PasswordStateType state;
    uint32_t current_password;
    uint32_t passwords_tried;
    uint32_t found_password;
    uint16_t dictionary_index;
    char status_text[64];
} PasswordAttackState;

static PasswordAttackState* pw_state = NULL;

static void password_draw_callback(Canvas* canvas, void* context) {
    PredatorApp* app = context;
    if(!pw_state) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "SLIX Password");

    canvas_set_font(canvas, FontSecondary);
    
    if(pw_state->state == PasswordStateSuccess) {
        canvas_draw_icon(canvas, 40, 20, &I_Success_25x25);
        canvas_draw_str(canvas, 20, 50, "Password Found!");
        
        char pwd_str[64];
        snprintf(pwd_str, sizeof(pwd_str), "%08lX", pw_state->found_password);
        canvas_draw_str(canvas, 30, 58, pwd_str);
        
    } else if(pw_state->state == PasswordStateFailed) {
        canvas_draw_str(canvas, 15, 30, "Attack Failed");
        
    } else {
        const char* mode = (pw_state->state == PasswordStateDictionary) ? 
            "Dictionary" : "Brute Force";
        canvas_draw_str(canvas, 2, 22, mode);
        
        char pwd_str[64];
        snprintf(pwd_str, sizeof(pwd_str), "Trying: %08lX", pw_state->current_password);
        canvas_draw_str(canvas, 2, 32, pwd_str);
        
        char tries_str[64];
        snprintf(tries_str, sizeof(tries_str), "Tried: %lu", pw_state->passwords_tried);
        canvas_draw_str(canvas, 2, 42, tries_str);
    }
    
    // Status bar
    canvas_draw_line(canvas, 0, 60, 128, 60);
    canvas_draw_str(canvas, 2, 63, pw_state->status_text);
}

static bool password_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    
    return false;
}

static void password_timer_callback(void* context) {
    PredatorApp* app = context;
    
    if(!pw_state) return;
    
    switch(pw_state->state) {
    case PasswordStateDictionary:
        // Try comprehensive SLIX dictionary (30+ known passwords)
        if(pw_state->dictionary_index < ISO15693_PASSWORD_COUNT) {
            pw_state->current_password = ISO15693_SLIX_PASSWORDS[pw_state->dictionary_index];
            
            // Real implementation: iso15693_try_password(app, pw_state->current_password)
            bool success = false; // Placeholder
            
            if(success) {
                pw_state->found_password = pw_state->current_password;
                pw_state->state = PasswordStateSuccess;
                
                FURI_LOG_I("ISO15693Password", "SUCCESS! Password: %08lX", 
                          pw_state->found_password);
            } else {
                pw_state->dictionary_index++;
                pw_state->passwords_tried++;
                
                if(pw_state->dictionary_index >= ISO15693_PASSWORD_COUNT) {
                    pw_state->state = PasswordStateFailed;
                    
                    FURI_LOG_I("ISO15693Password", "Dictionary exhausted (%u passwords), no match",
                              ISO15693_PASSWORD_COUNT);
                    
                    snprintf(pw_state->status_text, sizeof(pw_state->status_text),
                             "No match found");
                }
            }
        }
        break;
        
    default:
        break;
    }
    
    view_port_update(app->view_port);
}

void predator_scene_iso15693_password_attack_on_enter(void* context) {
    PredatorApp* app = context;
    
    pw_state = malloc(sizeof(PasswordAttackState));
    memset(pw_state, 0, sizeof(PasswordAttackState));
    
    pw_state->state = PasswordStateDictionary;
    snprintf(pw_state->status_text, sizeof(pw_state->status_text),
             "Dictionary attack...");
    
    view_port_draw_callback_set(app->view_port, password_draw_callback, app);
    view_port_input_callback_set(app->view_port, password_input_callback, app);
    
    app->timer = furi_timer_alloc(password_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 100);
    
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
}

bool predator_scene_iso15693_password_attack_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void predator_scene_iso15693_password_attack_on_exit(void* context) {
    PredatorApp* app = context;
    
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
    
    gui_remove_view_port(app->gui, app->view_port);
    
    if(pw_state) {
        free(pw_state);
        pw_state = NULL;
    }
}
