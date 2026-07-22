#include "desktop_view_clock_lock.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/elements.h>
 
typedef struct {
    uint8_t hour;
    uint8_t minute;
} ClockLockModel;
 
struct DesktopClockLockView {
    View* view;
    DesktopClockLockViewCallback callback;
    void* context;
    FuriTimer* timer;
};
 
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
        },
        true);
}
 
#define SEG_W    22   // digit width
#define SEG_H    40   // digit height
#define SEG_T     3   // segment thickness
#define SEG_GAP   3   // gap between digits
#define COL_W    10   // colon area width

// Segment bitmask: bit0=A(top) bit1=B(top-right) bit2=C(bot-right)
//                  bit3=D(bottom) bit4=E(bot-left) bit5=F(top-left) bit6=G(middle)
static const uint8_t seg_map[10] = {
    0b0111111, // 0: ABCDEF
    0b0000110, // 1: BC
    0b1011011, // 2: ABDEG
    0b1001111, // 3: ABCDG
    0b1100110, // 4: BCFG
    0b1101101, // 5: ACDFG
    0b1111101, // 6: ACDEFG
    0b0000111, // 7: ABC
    0b1111111, // 8: all
    0b1101111, // 9: ABCDFG
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
    canvas_set_color(canvas, ColorBlack);

    // Layout: HH:MM centred in the usable area (below 13px status bar)
    // Total width = 4 digits + colon + 3 gaps
    const int16_t total_w = 4 * SEG_W + COL_W + 3 * SEG_GAP;
    const int16_t sx = (128 - total_w) / 2;
    const int16_t sy = 13 + (51 - SEG_H) / 2;  // 51 = 64-13 usable px

    int16_t x = sx;
    draw_7seg_digit(canvas, x, sy, m->hour / 10);   x += SEG_W + SEG_GAP;
    draw_7seg_digit(canvas, x, sy, m->hour % 10);   x += SEG_W;

    // Colon — two dots centred in COL_W
    int16_t cx = x + COL_W / 2;
    canvas_draw_disc(canvas, cx, sy + SEG_H / 3,     3);
    canvas_draw_disc(canvas, cx, sy + SEG_H * 2 / 3, 3);
    x += COL_W + SEG_GAP;

    draw_7seg_digit(canvas, x, sy, m->minute / 10);  x += SEG_W + SEG_GAP;
    draw_7seg_digit(canvas, x, sy, m->minute % 10);
}
 
static bool desktop_clock_lock_input_callback(InputEvent* event, void* context) {
    DesktopClockLockView* clock_lock = context;
 
    if(event->type == InputTypeLong && event->key == InputKeyDown) {
        if(clock_lock->callback) {
            clock_lock->callback(clock_lock->context);
        }
        return true;
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
        },
        true);
 
    furi_timer_start(clock_lock->timer, furi_ms_to_ticks(1000));
}
 
static void desktop_clock_lock_exit_callback(void* context) {
    DesktopClockLockView* clock_lock = context;
    furi_timer_stop(clock_lock->timer);
}
 
DesktopClockLockView* desktop_clock_lock_alloc(void) {
    DesktopClockLockView* clock_lock = malloc(sizeof(DesktopClockLockView));
 
    clock_lock->callback = NULL;
    clock_lock->context = NULL;
 
    clock_lock->view = view_alloc();
    view_set_context(clock_lock->view, clock_lock);
    view_allocate_model(clock_lock->view, ViewModelTypeLocking, sizeof(ClockLockModel));
    view_set_draw_callback(clock_lock->view, desktop_clock_lock_draw_callback);
    view_set_input_callback(clock_lock->view, desktop_clock_lock_input_callback);
    view_set_enter_callback(clock_lock->view, desktop_clock_lock_enter_callback);
    view_set_exit_callback(clock_lock->view, desktop_clock_lock_exit_callback);
 
    clock_lock->timer = furi_timer_alloc(
        desktop_clock_lock_timer_callback, FuriTimerTypePeriodic, clock_lock);
 
    return clock_lock;
}
 
void desktop_clock_lock_free(DesktopClockLockView* clock_lock) {
    furi_assert(clock_lock);
    furi_timer_free(clock_lock->timer);
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