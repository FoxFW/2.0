#include "mindful_view.h"
#include "fox_chill_icons.h"

#include <furi.h>

#define MINDFUL_TICKS_PER_FLASH 2
#define MINDFUL_FLASH_COUNT 20
#define MINDFUL_TOTAL_TICKS (MINDFUL_FLASH_COUNT * MINDFUL_TICKS_PER_FLASH) // 40 = 10s
#define MINDFUL_VIBE_EVERY_TICKS 8

typedef enum {
    MindfulShowBe,
    MindfulShowMindful,
    MindfulShowBoth,
} MindfulFlashKind;

static const MindfulFlashKind k_sequence[MINDFUL_FLASH_COUNT] = {
    MindfulShowBe,
    MindfulShowMindful,
    MindfulShowBe,
    MindfulShowMindful,
    MindfulShowBoth,
    MindfulShowBoth,
    MindfulShowBoth,
    MindfulShowBe,
    MindfulShowMindful,
    MindfulShowBoth,
    MindfulShowBe,
    MindfulShowMindful,
    MindfulShowBe,
    MindfulShowMindful,
    MindfulShowBoth,
    MindfulShowBoth,
    MindfulShowBoth,
    MindfulShowBe,
    MindfulShowMindful,
    MindfulShowBoth,
};

static const NotificationSequence* const k_led_cycle[] = {
    &sequence_set_only_red_255,
    &sequence_set_only_green_255,
    &sequence_set_only_blue_255,
    &sequence_solid_yellow,
};
#define LED_CYCLE_COUNT (sizeof(k_led_cycle) / sizeof(k_led_cycle[0]))

static App* s_mindful_app = NULL;

static void mindful_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_mindful_app;
    if(app == NULL) return;

    uint32_t flash_index = app->mindful_tick / MINDFUL_TICKS_PER_FLASH;
    if(flash_index >= MINDFUL_FLASH_COUNT) flash_index = MINDFUL_FLASH_COUNT - 1;

    bool invert = (flash_index % 2) == 1;
    Color bg = invert ? ColorBlack : ColorWhite;
    Color fg = invert ? ColorWhite : ColorBlack;

    canvas_set_color(canvas, bg);
    canvas_draw_box(canvas, 0, 0, 128, 64);
    canvas_set_color(canvas, fg);

    switch(k_sequence[flash_index]) {
    case MindfulShowBe:
        fox_chill_draw_big_words(canvas, &I_BE, NULL, 0, 64);
        break;
    case MindfulShowMindful:
        fox_chill_draw_big_words(canvas, NULL, &I_MINDFUL, 0, 64);
        break;
    case MindfulShowBoth:
        fox_chill_draw_big_words(canvas, &I_BE, &I_MINDFUL, 0, 64);
        break;
    }

    canvas_set_color(canvas, ColorBlack);
}

static bool mindful_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeLong) return false;

    switch(event->key) {
    case InputKeyOk:
        fox_chill_goto_menu(app);
        return true;
    case InputKeyBack:
        return false;
    case InputKeyUp:
    case InputKeyDown:
    case InputKeyLeft:
    case InputKeyRight:
        return true; // swallow - arrow keys do nothing on this screen
    default:
        return false;
    }
}

View* mindful_view_alloc(App* app) {
    s_mindful_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, mindful_draw_cb);
    view_set_input_callback(v, mindful_input_cb);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(uint8_t));
    return v;
}

void mindful_view_free(View* view) {
    s_mindful_app = NULL;
    view_free(view);
}

void mindful_view_show(App* app) {
    app->mindful_tick = 0;
    fox_chill_save_note_mindful(app);
    notification_message(app->notifications, k_led_cycle[0]);
    app->current_view = FoxChillViewBeMindful;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxChillViewBeMindful);
}

bool mindful_view_tick(App* app) {
    app->mindful_tick++;

    notification_message(
        app->notifications, k_led_cycle[app->mindful_tick % LED_CYCLE_COUNT]);

    if(app->mindful_tick % MINDFUL_VIBE_EVERY_TICKS == 0) {
        notification_message(app->notifications, &sequence_single_vibro);
    }

    with_view_model(app->be_mindful_view, uint8_t * _m, { UNUSED(_m); }, true);

    return app->mindful_tick >= MINDFUL_TOTAL_TICKS;
}
