#include "desktop_view_clock_lock.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/elements.h>
#include <stdio.h>

typedef struct {
    uint8_t hour;
    uint8_t minute;
    bool ringing;
    bool blink_on;
    bool show_hint;
    bool show_brightness;
    uint8_t brightness_percent;
} ClockLockModel;

struct DesktopClockLockView {
    View* view;
    DesktopClockLockViewCallback callback;
    void* context;
    DesktopClockLockBacklightCallback backlight_callback;
    void* backlight_context;
    DesktopClockLockBrightnessCallback brightness_callback;
    void* brightness_context;
    DesktopClockLockTickCallback tick_callback;
    void* tick_context;
    FuriTimer* timer;
    FuriTimer* hint_timer;
};

#define HINT_DISPLAY_MS 2000

static void desktop_clock_lock_hint_timer_callback(void* context) {
    DesktopClockLockView* clock_lock = context;
    with_view_model(
        clock_lock->view,
        ClockLockModel * model,
        {
            model->show_hint = false;
            model->show_brightness = false;
        },
        true);
}

static void desktop_clock_lock_timer_callback(void* context) {
    DesktopClockLockView* clock_lock = context;

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    with_view_model(
        clock_lock->view,
        ClockLockModel* model,
        {
            model->hour = dt.hour;
            model->minute = dt.minute;
            model->blink_on = !model->blink_on;
        },
        true);

    if(clock_lock->tick_callback) {
        clock_lock->tick_callback(clock_lock->tick_context);
    }
}

#define SEG_W    22
#define SEG_H    40
#define SEG_T     3
#define SEG_GAP   3
#define COL_W    10

static const uint8_t seg_map[10] = {
    0b0111111,
    0b0000110,
    0b1011011,
    0b1001111,
    0b1100110,
    0b1101101,
    0b1111101,
    0b0000111,
    0b1111111,
    0b1101111,
};

static void draw_7seg_digit(Canvas* canvas, int16_t x, int16_t y, uint8_t d) {
    if(d > 9) return;
    uint8_t s = seg_map[d];
    int16_t mid = y + SEG_H / 2 - 1;

    if(s & (1 << 0)) canvas_draw_box(canvas, x + SEG_T, y,            SEG_W - 2*SEG_T, SEG_T);
    if(s & (1 << 1)) canvas_draw_box(canvas, x + SEG_W - SEG_T, y + SEG_T, SEG_T, SEG_H/2 - SEG_T - 1);
    if(s & (1 << 2)) canvas_draw_box(canvas, x + SEG_W - SEG_T, mid + SEG_T, SEG_T, SEG_H/2 - SEG_T);
    if(s & (1 << 3)) canvas_draw_box(canvas, x + SEG_T, y + SEG_H - SEG_T, SEG_W - 2*SEG_T, SEG_T);
    if(s & (1 << 4)) canvas_draw_box(canvas, x,                mid + SEG_T, SEG_T, SEG_H/2 - SEG_T);
    if(s & (1 << 5)) canvas_draw_box(canvas, x,                y + SEG_T,   SEG_T, SEG_H/2 - SEG_T - 1);
    if(s & (1 << 6)) canvas_draw_box(canvas, x + SEG_T,        mid,         SEG_W - 2*SEG_T, SEG_T);
}

static void desktop_clock_lock_draw_callback(Canvas* canvas, void* model) {
    ClockLockModel* m = model;

    canvas_clear(canvas);

    bool invert = m->ringing && m->blink_on;
    if(invert) {
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }

    const int16_t total_w = 4 * SEG_W + COL_W + 3 * SEG_GAP;
    const int16_t sx = (128 - total_w) / 2;
    const int16_t sy = 13 + (51 - SEG_H) / 2;

    if(m->ringing) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignCenter, "ALARM");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "OK or hold DOWN to stop");
    }

    int16_t x = sx;
    draw_7seg_digit(canvas, x, sy, m->hour / 10);   x += SEG_W + SEG_GAP;
    draw_7seg_digit(canvas, x, sy, m->hour % 10);   x += SEG_W;

    int16_t cx = x + COL_W / 2;
    canvas_draw_disc(canvas, cx, sy + SEG_H / 3,     3);
    canvas_draw_disc(canvas, cx, sy + SEG_H * 2 / 3, 3);
    x += COL_W + SEG_GAP;

    draw_7seg_digit(canvas, x, sy, m->minute / 10);  x += SEG_W + SEG_GAP;
    draw_7seg_digit(canvas, x, sy, m->minute % 10);

    if(!m->ringing && m->show_brightness) {
        const int16_t box_w = 90, box_h = 20;
        const int16_t box_x = (128 - box_w) / 2;
        const int16_t box_y = (64 - box_h) / 2 + 6;
        char text[16];
        snprintf(text, sizeof(text), "Brightness %u%%", m->brightness_percent);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, box_x, box_y, box_w, box_h);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, box_x, box_y, box_w, box_h);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, box_y + box_h / 2, AlignCenter, AlignCenter, text);
    } else if(!m->ringing && m->show_hint) {
        const int16_t box_w = 108, box_h = 26;
        const int16_t box_x = (128 - box_w) / 2;
        const int16_t box_y = (64 - box_h) / 2 + 6;
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, box_x, box_y, box_w, box_h);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, box_x, box_y, box_w, box_h);
        canvas_set_font(canvas, FontSecondary);
        elements_multiline_text_aligned(
            canvas, 64, box_y + box_h / 2, AlignCenter, AlignCenter, "Press and Hold DOWN\nto Exit");
    }
}

static bool desktop_clock_lock_input_callback(InputEvent* event, void* context) {
    DesktopClockLockView* clock_lock = context;

    bool ringing = false;
    with_view_model(clock_lock->view, ClockLockModel * model, { ringing = model->ringing; }, false);

    bool dismiss = (event->type == InputTypeLong &&
                     (event->key == InputKeyDown || event->key == InputKeyBack)) ||
                   (ringing && event->type == InputTypeShort && event->key == InputKeyOk);

    if(dismiss) {
        if(clock_lock->callback) {
            clock_lock->callback(clock_lock->context);
        }
        return true;
    }

    if(!ringing && event->type == InputTypeShort && clock_lock->backlight_callback) {
        if(event->key == InputKeyLeft) {
            clock_lock->backlight_callback(clock_lock->backlight_context, false);
        } else if(event->key == InputKeyRight) {
            clock_lock->backlight_callback(clock_lock->backlight_context, true);
        }
    }

    if(!ringing && event->type == InputTypeShort && clock_lock->brightness_callback) {
        if(event->key == InputKeyUp) {
            clock_lock->brightness_callback(clock_lock->brightness_context, true);
        } else if(event->key == InputKeyDown) {
            clock_lock->brightness_callback(clock_lock->brightness_context, false);
        }
    }

    if(!ringing && event->type == InputTypeShort && event->key == InputKeyBack) {
        with_view_model(
            clock_lock->view,
            ClockLockModel * model,
            {
                model->show_hint = true;
                model->show_brightness = false;
            },
            true);
        furi_timer_start(clock_lock->hint_timer, furi_ms_to_ticks(HINT_DISPLAY_MS));
    }

    return true;
}

static void desktop_clock_lock_enter_callback(void* context) {
    DesktopClockLockView* clock_lock = context;

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    with_view_model(
        clock_lock->view,
        ClockLockModel* model,
        {
            model->hour = dt.hour;
            model->minute = dt.minute;
            model->show_hint = false;
            model->show_brightness = false;
        },
        true);

    furi_timer_start(clock_lock->timer, furi_ms_to_ticks(1000));
}

static void desktop_clock_lock_exit_callback(void* context) {
    DesktopClockLockView* clock_lock = context;
    furi_timer_stop(clock_lock->timer);
    furi_timer_stop(clock_lock->hint_timer);
}

DesktopClockLockView* desktop_clock_lock_alloc(void) {
    DesktopClockLockView* clock_lock = malloc(sizeof(DesktopClockLockView));

    clock_lock->callback = NULL;
    clock_lock->context = NULL;
    clock_lock->backlight_callback = NULL;
    clock_lock->backlight_context = NULL;
    clock_lock->brightness_callback = NULL;
    clock_lock->brightness_context = NULL;
    clock_lock->tick_callback = NULL;
    clock_lock->tick_context = NULL;

    clock_lock->view = view_alloc();
    view_set_context(clock_lock->view, clock_lock);
    view_allocate_model(clock_lock->view, ViewModelTypeLocking, sizeof(ClockLockModel));
    view_set_draw_callback(clock_lock->view, desktop_clock_lock_draw_callback);
    view_set_input_callback(clock_lock->view, desktop_clock_lock_input_callback);
    view_set_enter_callback(clock_lock->view, desktop_clock_lock_enter_callback);
    view_set_exit_callback(clock_lock->view, desktop_clock_lock_exit_callback);

    clock_lock->timer = furi_timer_alloc(
        desktop_clock_lock_timer_callback, FuriTimerTypePeriodic, clock_lock);
    clock_lock->hint_timer = furi_timer_alloc(
        desktop_clock_lock_hint_timer_callback, FuriTimerTypeOnce, clock_lock);

    return clock_lock;
}

void desktop_clock_lock_free(DesktopClockLockView* clock_lock) {
    furi_assert(clock_lock);
    furi_timer_free(clock_lock->timer);
    furi_timer_free(clock_lock->hint_timer);
    view_free(clock_lock->view);
    free(clock_lock);
}

View* desktop_clock_lock_get_view(DesktopClockLockView* clock_lock) {
    return clock_lock->view;
}

void desktop_clock_lock_set_callback(
    DesktopClockLockView* clock_lock,
    DesktopClockLockViewCallback callback,
    void* context) {
    clock_lock->callback = callback;
    clock_lock->context = context;
}

void desktop_clock_lock_set_ringing(DesktopClockLockView* clock_lock, bool ringing) {
    with_view_model(
        clock_lock->view,
        ClockLockModel* model,
        {
            model->ringing = ringing;
            model->blink_on = true;
        },
        true);
}

void desktop_clock_lock_set_backlight_callback(
    DesktopClockLockView* clock_lock,
    DesktopClockLockBacklightCallback callback,
    void* context) {
    clock_lock->backlight_callback = callback;
    clock_lock->backlight_context = context;
}

void desktop_clock_lock_set_tick_callback(
    DesktopClockLockView* clock_lock,
    DesktopClockLockTickCallback callback,
    void* context) {
    clock_lock->tick_callback = callback;
    clock_lock->tick_context = context;
}

void desktop_clock_lock_set_brightness_callback(
    DesktopClockLockView* clock_lock,
    DesktopClockLockBrightnessCallback callback,
    void* context) {
    clock_lock->brightness_callback = callback;
    clock_lock->brightness_context = context;
}

void desktop_clock_lock_show_brightness(DesktopClockLockView* clock_lock, uint8_t percent) {
    with_view_model(
        clock_lock->view,
        ClockLockModel* model,
        {
            model->show_brightness = true;
            model->show_hint = false;
            model->brightness_percent = percent;
        },
        true);
    furi_timer_start(clock_lock->hint_timer, furi_ms_to_ticks(HINT_DISPLAY_MS));
}
