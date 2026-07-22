#include "fox_splash.h"

#include <gui/canvas.h>
#include <stdlib.h>
#include <string.h>

#define FOX_SPLASH_TICK_MS    40
#define FOX_SPLASH_GRID_SIDE  16
#define FOX_SPLASH_GRID_TOTAL (FOX_SPLASH_GRID_SIDE * FOX_SPLASH_GRID_SIDE) /* 256 */

struct FoxSplash {
    const Icon* icon;
    uint32_t hold_ms;
    uint32_t fade_ms;
    FoxSplashDoneCallback done_cb;
    void* done_context;

    View* view;
    FuriTimer* timer;
    uint32_t elapsed_ms;
    bool finished;

    /* Shuffled once per fox_splash_start() - the order blocks get
       erased in during the dissolve, so it looks like scattered noise
       rather than a boring left-to-right wipe. */
    uint8_t block_order[FOX_SPLASH_GRID_TOTAL];
};

static FoxSplash* s_active_splash = NULL;

static void fox_splash_shuffle_blocks(FoxSplash* splash) {
    for(uint32_t i = 0; i < FOX_SPLASH_GRID_TOTAL; i++) {
        splash->block_order[i] = (uint8_t)i;
    }

    uint32_t seed = (uint32_t)furi_get_tick() | 1;
    for(uint32_t i = FOX_SPLASH_GRID_TOTAL - 1; i > 0; i--) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        uint32_t j = seed % (i + 1);
        uint8_t tmp = splash->block_order[i];
        splash->block_order[i] = splash->block_order[j];
        splash->block_order[j] = tmp;
    }
}

static void fox_splash_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    FoxSplash* splash = s_active_splash;
    if(splash == NULL) return;

    canvas_clear(canvas);

    uint8_t icon_w = icon_get_width(splash->icon);
    uint8_t icon_h = icon_get_height(splash->icon);
    int32_t x = (128 - (int32_t)icon_w) / 2;
    int32_t y = (64 - (int32_t)icon_h) / 2;
    canvas_draw_icon(canvas, x, y, splash->icon);

    if(splash->fade_ms == 0 || splash->elapsed_ms <= splash->hold_ms) return;

    uint32_t fade_elapsed = splash->elapsed_ms - splash->hold_ms;
    uint32_t revealed =
        (uint32_t)(((uint64_t)fade_elapsed * FOX_SPLASH_GRID_TOTAL) / splash->fade_ms);
    if(revealed > FOX_SPLASH_GRID_TOTAL) revealed = FOX_SPLASH_GRID_TOTAL;

    uint8_t block_px = (uint8_t)(icon_w / FOX_SPLASH_GRID_SIDE);
    if(block_px == 0) block_px = 1;

    canvas_set_color(canvas, ColorWhite);
    for(uint32_t i = 0; i < revealed; i++) {
        uint8_t block = splash->block_order[i];
        uint8_t bx = block % FOX_SPLASH_GRID_SIDE;
        uint8_t by = block / FOX_SPLASH_GRID_SIDE;
        canvas_draw_box(canvas, x + bx * block_px, y + by * block_px, block_px, block_px);
    }
    canvas_set_color(canvas, ColorBlack);
}

static void fox_splash_timer_cb(void* context) {
    FoxSplash* splash = context;
    if(splash->finished) return;

    splash->elapsed_ms += FOX_SPLASH_TICK_MS;
    uint32_t total_ms = splash->hold_ms + splash->fade_ms;

    if(splash->elapsed_ms >= total_ms) {
        splash->finished = true;
        furi_timer_stop(splash->timer);
        with_view_model(splash->view, uint8_t * _m, { UNUSED(_m); }, true);
        if(splash->done_cb != NULL) splash->done_cb(splash->done_context);
        return;
    }

    with_view_model(splash->view, uint8_t * _m, { UNUSED(_m); }, true);
}

FoxSplash* fox_splash_alloc(
    const Icon* icon,
    uint32_t hold_ms,
    uint32_t fade_ms,
    FoxSplashDoneCallback done_cb,
    void* done_context) {
    FoxSplash* splash = malloc(sizeof(FoxSplash));
    memset(splash, 0, sizeof(FoxSplash));

    splash->icon = icon;
    splash->hold_ms = hold_ms;
    splash->fade_ms = fade_ms;
    splash->done_cb = done_cb;
    splash->done_context = done_context;

    splash->view = view_alloc();
    view_set_draw_callback(splash->view, fox_splash_draw_cb);
    view_allocate_model(splash->view, ViewModelTypeLocking, sizeof(uint8_t));

    splash->timer = furi_timer_alloc(fox_splash_timer_cb, FuriTimerTypePeriodic, splash);

    return splash;
}

void fox_splash_free(FoxSplash* splash) {
    furi_timer_free(splash->timer);
    view_free(splash->view);
    if(s_active_splash == splash) s_active_splash = NULL;
    free(splash);
}

View* fox_splash_get_view(FoxSplash* splash) {
    return splash->view;
}

void fox_splash_start(FoxSplash* splash) {
    s_active_splash = splash;
    splash->elapsed_ms = 0;
    splash->finished = false;
    if(splash->fade_ms > 0) fox_splash_shuffle_blocks(splash);

    with_view_model(splash->view, uint8_t * _m, { UNUSED(_m); }, true);
    furi_timer_start(splash->timer, furi_ms_to_ticks(FOX_SPLASH_TICK_MS));
}
