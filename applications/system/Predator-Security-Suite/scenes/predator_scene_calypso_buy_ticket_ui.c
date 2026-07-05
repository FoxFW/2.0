#include "../predator_i.h"

// MEMORY SAFE - Buy TL Ticket Scene
// Uses minimal stack, safe for 3KB limit

typedef enum {
    TicketStateSelect,
    TicketStateBuy,
    TicketStateReady
} TicketState;

static TicketState state = TicketStateSelect;
static uint8_t city_idx = 0;  // 0=Lausanne, 1=Paris, 2=Brussels
static uint32_t tick = 0;

static const char* cities[] = {"TL Lausanne", "VMCV Montreux", "Navigo Paris", "MOBIB Brussels"};
static const char* prices[] = {"100 CHF", "80 CHF", "75 EUR", "49 EUR"};

static void draw_cb(Canvas* c, void* ctx) {
    UNUSED(ctx);
    canvas_clear(c);
    canvas_set_font(c, FontPrimary);
    
    if(state == TicketStateSelect) {
        canvas_draw_str(c, 2, 10, "Buy Ticket");
        canvas_set_font(c, FontSecondary);
        for(int i = 0; i < 4; i++) {
            char buf[24];  // Reduced from 32
            snprintf(buf, 24, "%s%s - %s", i==city_idx?"> ":"  ", cities[i], prices[i]);
            canvas_draw_str(c, 2, 25+i*10, buf);
        }
        canvas_draw_str(c, 2, 60, "[OK] Buy [↕] Select");
    } else if(state == TicketStateBuy) {
        canvas_draw_str(c, 2, 10, "Purchasing...");
        canvas_set_font(c, FontSecondary);
        char dots[5] = "...";
        dots[(tick/5)%4] = '\0';
        canvas_draw_str(c, 2, 30, "Creating ticket");
        canvas_draw_str(c, 90, 30, dots);
    } else {
        canvas_draw_str(c, 2, 10, cities[city_idx]);
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, 2, 25, "Purchased!");
        canvas_set_font(c, FontPrimary);
        canvas_draw_str(c, 20, 40, prices[city_idx]);
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, 2, 52, "50 trips");
        canvas_draw_str(c, 2, 60, "[OK] Use ticket");
    }
}

static bool input_cb(InputEvent* e, void* ctx) {
    PredatorApp* app = ctx;
    if(e->type != InputTypeShort) return false;
    
    if(e->key == InputKeyBack) {
        if(state == TicketStateSelect) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(state == TicketStateSelect) {
        if(e->key == InputKeyUp && city_idx > 0) city_idx--;
        if(e->key == InputKeyDown && city_idx < 3) city_idx++;
        if(e->key == InputKeyOk) { state = TicketStateBuy; tick = 0; }
        return true;
    }
    if(state == TicketStateReady && e->key == InputKeyOk) {
        scene_manager_next_scene(app->scene_manager, PredatorSceneCalypsoEmulateUI);
        return true;
    }
    return false;
}

static void timer_cb(void* ctx) {
    PredatorApp* app = ctx;
    tick++;
    if(state == TicketStateBuy && tick > 30) state = TicketStateReady;
    if(app->view_dispatcher) view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void predator_scene_calypso_buy_ticket_ui_on_enter(void* ctx) {
    PredatorApp* app = ctx;
    state = TicketStateSelect;
    city_idx = 0;
    tick = 0;
    
    View* v = view_alloc();
    view_set_context(v, app);
    view_set_draw_callback(v, draw_cb);
    view_set_input_callback(v, input_cb);
    view_dispatcher_add_view(app->view_dispatcher, 100, v);
    view_dispatcher_switch_to_view(app->view_dispatcher, 100);
    
    if(app->timer) { furi_timer_stop(app->timer); furi_timer_free(app->timer); }
    app->timer = furi_timer_alloc(timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 100);
}

bool predator_scene_calypso_buy_ticket_ui_on_event(void* ctx, SceneManagerEvent e) {
    UNUSED(ctx); UNUSED(e);
    return false;
}

void predator_scene_calypso_buy_ticket_ui_on_exit(void* ctx) {
    PredatorApp* app = ctx;
    if(app->timer) { furi_timer_stop(app->timer); furi_timer_free(app->timer); app->timer = NULL; }
}
