#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>

typedef enum {
    RamMonitorFooterDefault,
    RamMonitorFooterSaved,
    RamMonitorFooterSaveFailed,
} RamMonitorFooterState;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* input_queue;
    RamMonitorFooterState footer_state;
} RamMonitorApp;

static void fox_ram_monitor_debug_log(const char* what) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, "/ext/apps_data/rpc_gui");
    File* file = storage_file_alloc(storage);
    if(storage_file_open(
           file, "/ext/apps_data/rpc_gui/screen_stream_debug.log", FSAM_WRITE, FSOM_OPEN_APPEND)) {
        char line[64];
        int written = snprintf(
            line, sizeof(line), "[%lu] RamMonitor: %s\n", (unsigned long)furi_get_tick(), what);
        if(written > 0) {
            size_t write_len = (size_t)written;
            if(write_len > sizeof(line) - 1) {
                write_len = sizeof(line) - 1;
            }
            storage_file_write(file, line, write_len);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void fox_ram_monitor_heap_map_write_callback(void* context, const char* data, size_t length) {
    File* file = context;
    storage_file_write(file, data, length);
}

static bool fox_ram_monitor_save_heap_map(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, "/ext/apps_data/heap_map");

    char path[128];
    snprintf(
        path,
        sizeof(path),
        "/ext/apps_data/heap_map/heap_map_%lu_rm.txt",
        (unsigned long)furi_get_tick());

    File* file = storage_file_alloc(storage);
    bool opened = storage_file_open(file, path, FSAM_WRITE, FSOM_OPEN_APPEND);
    if(opened) {
        memmgr_heap_write_fragmentation_map(
            fox_ram_monitor_heap_map_write_callback, file, "ram_monitor_button");
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return opened;
}

static void fox_ram_monitor_draw_callback(Canvas* canvas, void* context) {
    RamMonitorApp* app = context;
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

    if(app->footer_state == RamMonitorFooterSaved) {
        canvas_draw_str(canvas, 2, 63, "Snapshot saved");
    } else if(app->footer_state == RamMonitorFooterSaveFailed) {
        canvas_draw_str(canvas, 2, 63, "Snapshot FAILED");
    } else {
        canvas_draw_str(canvas, 2, 63, "OK: Refresh Back: Exit");
    }
}

static void fox_ram_monitor_input_callback(InputEvent* event, void* context) {
    RamMonitorApp* app = context;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

int32_t fox_ram_monitor_app(void* p) {
    UNUSED(p);

    RamMonitorApp* app = malloc(sizeof(RamMonitorApp));
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->footer_state = RamMonitorFooterDefault;

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, fox_ram_monitor_draw_callback, app);
    view_port_input_callback_set(app->view_port, fox_ram_monitor_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    fox_ram_monitor_debug_log("opened");
    view_port_update(app->view_port);

    InputEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(app->input_queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.key == InputKeyBack) {
                running = false;
            } else if(event.key == InputKeyOk && event.type == InputTypeShort) {
                app->footer_state = RamMonitorFooterDefault;
                fox_ram_monitor_debug_log("refresh");
                view_port_update(app->view_port);
            } else if(event.key == InputKeyUp && event.type == InputTypeShort) {
                bool saved = fox_ram_monitor_save_heap_map();
                app->footer_state = saved ? RamMonitorFooterSaved : RamMonitorFooterSaveFailed;
                fox_ram_monitor_debug_log(saved ? "heap_map saved" : "heap_map save failed");
                view_port_update(app->view_port);
            }
        }
    }

    fox_ram_monitor_debug_log("closed");
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    free(app);

    return 0;
}
