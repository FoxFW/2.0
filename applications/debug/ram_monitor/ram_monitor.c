/**
 * @file ram_monitor.c
 * @brief Diagnostic app - shows the same numbers as the CLI "free" command,
 * refreshed live, so RAM use can be watched on-screen while
 * plugging/unplugging qFlipper (no need to disconnect qFlipper just to get
 * a CLI session). Started as a throwaway scratch tool, but kept around
 * permanently - most users will never open it, but it's useful enough to
 * leave in the Apps list.
 */

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

#define RAM_MONITOR_REFRESH_MS 250

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* input_queue;
    FuriTimer* timer;
} RamMonitorApp;

static void ram_monitor_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    char line[40];
    uint8_t y = 8;
    const uint8_t step = 9;

    snprintf(line, sizeof(line), "Free heap:    %zu", memmgr_get_free_heap());
    canvas_draw_str(canvas, 2, y, line);
    y += step;

    snprintf(line, sizeof(line), "Total heap:   %zu", memmgr_get_total_heap());
    canvas_draw_str(canvas, 2, y, line);
    y += step;

    snprintf(line, sizeof(line), "Minimum heap: %zu", memmgr_get_minimum_free_heap());
    canvas_draw_str(canvas, 2, y, line);
    y += step;

    snprintf(line, sizeof(line), "Max heap blk: %zu", memmgr_heap_get_max_free_block());
    canvas_draw_str(canvas, 2, y, line);
    y += step;

    snprintf(line, sizeof(line), "Pool free:    %zu", memmgr_pool_get_free());
    canvas_draw_str(canvas, 2, y, line);
    y += step;

    snprintf(line, sizeof(line), "Max pool blk: %zu", memmgr_pool_get_max_block());
    canvas_draw_str(canvas, 2, y, line);

    canvas_draw_str(canvas, 2, 63, "Back: exit");
}

static void ram_monitor_input_callback(InputEvent* event, void* context) {
    RamMonitorApp* app = context;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

static void ram_monitor_timer_callback(void* context) {
    RamMonitorApp* app = context;
    view_port_update(app->view_port);
}

int32_t ram_monitor_app(void* p) {
    UNUSED(p);

    RamMonitorApp* app = malloc(sizeof(RamMonitorApp));
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, ram_monitor_draw_callback, app);
    view_port_input_callback_set(app->view_port, ram_monitor_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->timer = furi_timer_alloc(ram_monitor_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, furi_ms_to_ticks(RAM_MONITOR_REFRESH_MS));

    InputEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(app->input_queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.key == InputKeyBack) {
                running = false;
            }
        }
    }

    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);

    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    free(app);

    return 0;
}
