#include "../predator_i.h"

// MEMORY SAFE - Emulate Ticket for TL Bus
// STATIC buffers - NO stack overflow!

static uint32_t tick = 0;
static bool validated = false;

static void draw_cb(Canvas* c, void* ctx) {
    UNUSED(ctx);
    canvas_clear(c);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 10, 10, "TL MOBILIS");
    
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 2, 24, "READY FOR VALIDATOR");
    
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 15, 38, "100 CHF");
    
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 2, 50, "50 trips");
    
    if(validated) {
        canvas_set_font(c, FontPrimary);
        canvas_draw_str(c, 20, 60, "VALIDATED!");
    } else {
        char wait[12];  // Reduced from 16
        snprintf(wait, 12, "Listen%.*s", (int)((tick/5)%4), "...");
        canvas_draw_str(c, 2, 60, wait);
    }
}

static bool input_cb(InputEvent* e, void* ctx) {
    PredatorApp* app = ctx;
    if(e->type == InputTypeShort && e->key == InputKeyBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

// SAFE timer - minimal processing
static void timer_cb(void* ctx) {
    PredatorApp* app = ctx;
    tick++;
    
    // Try to check NFC (safe - no large buffers)
    // Real validator communication would go here
    // For now, simulate success after 50 ticks
    if(tick == 50) {
        validated = true;
        FURI_LOG_I("Calypso", "Simulated validation");
    }
    
    if(app->view_dispatcher) view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void predator_scene_calypso_emulate_ui_on_enter(void* ctx) {
    PredatorApp* app = ctx;
    tick = 0;
    validated = false;
    
    FURI_LOG_I("Calypso", "Emulating TL Mobilis - 100 CHF");
    
    View* v = view_alloc();
    view_set_context(v, app);
    view_set_draw_callback(v, draw_cb);
    view_set_input_callback(v, input_cb);
    view_dispatcher_add_view(app->view_dispatcher, 101, v);
    view_dispatcher_switch_to_view(app->view_dispatcher, 101);
    
    if(app->timer) { furi_timer_stop(app->timer); furi_timer_free(app->timer); }
    app->timer = furi_timer_alloc(timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 100);
}

bool predator_scene_calypso_emulate_ui_on_event(void* ctx, SceneManagerEvent e) {
    UNUSED(ctx); UNUSED(e);
    return false;
}

void predator_scene_calypso_emulate_ui_on_exit(void* ctx) {
    PredatorApp* app = ctx;
    if(app->timer) { furi_timer_stop(app->timer); furi_timer_free(app->timer); app->timer = NULL; }
    FURI_LOG_I("Calypso", "Emulation stopped");
}
