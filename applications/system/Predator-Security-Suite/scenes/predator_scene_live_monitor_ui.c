#include "../predator_i.h"
#include "../helpers/predator_logging.h"
#include <gui/view.h>
#include <storage/storage.h>
#include <string.h>

// Live Monitor - Professional UI
// Shows real-time log entries with scrolling and statistics

#define LOG_PATH "/ext/apps_data/predator/predator_logs.txt"
#define MAX_LOG_LINES 5
#define MAX_LINE_LENGTH 26

typedef struct {
    char log_lines[MAX_LOG_LINES][MAX_LINE_LENGTH];
    uint32_t total_logs;
    uint32_t monitor_time_ms;
    uint8_t scroll_offset;
    bool auto_scroll;
} LiveMonitorState;

static LiveMonitorState monitor_state;
static uint32_t monitor_start_tick = 0;

static void load_recent_logs(LiveMonitorState* state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    
    // Clear existing lines
    for(size_t i = 0; i < MAX_LOG_LINES; i++) {
        state->log_lines[i][0] = '\0';
    }
    
    if(storage_file_open(file, LOG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        int32_t size = storage_file_size(file);
        int32_t pos = size;
        int collected = 0;
        char buf[128];
        
        // Read from end of file backwards
        while(pos > 0 && collected < MAX_LOG_LINES) {
            int32_t chunk = (pos >= (int32_t)sizeof(buf)) ? (int32_t)sizeof(buf) : pos;
            pos -= chunk;
            storage_file_seek(file, pos, false);
            uint16_t br = storage_file_read(file, buf, chunk);
            
            for(int i = (int)br - 1; i >= 0 && collected < MAX_LOG_LINES; --i) {
                if(buf[i] == '\n') {
                    int32_t start = pos + i + 1;
                    storage_file_seek(file, start, false);
                    size_t idx = 0;
                    char ch = 0;
                    uint16_t r = 0;
                    
                    while(idx + 1 < MAX_LINE_LENGTH && (r = storage_file_read(file, &ch, 1)) == 1) {
                        if(ch == '\n' || ch == '\r') break;
                        state->log_lines[collected][idx++] = ch;
                    }
                    state->log_lines[collected][idx] = '\0';
                    collected++;
                    storage_file_seek(file, pos, false);
                }
            }
        }
        
        // Count total lines
        storage_file_seek(file, 0, false);
        state->total_logs = 0;
        char line_buf[256];
        while(storage_file_read(file, line_buf, sizeof(line_buf)) > 0) {
            for(size_t i = 0; i < sizeof(line_buf); i++) {
                if(line_buf[i] == '\n') state->total_logs++;
            }
        }
        
        storage_file_close(file);
    }
    
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void draw_live_monitor_header(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "LIVE MONITOR");
    canvas_draw_line(canvas, 0, 12, 128, 12);
}

static void draw_live_monitor_logs(Canvas* canvas, LiveMonitorState* state) {
    canvas_set_font(canvas, FontSecondary);
    
    // Stats line
    char stats_str[32];
    uint32_t seconds = state->monitor_time_ms / 1000;
    snprintf(stats_str, sizeof(stats_str), "Logs:%lu  Time:%lus", 
            state->total_logs, seconds);
    canvas_draw_str(canvas, 2, 22, stats_str);
    
    // Log lines (newest at bottom)
    if(state->total_logs > 0) {
        uint8_t y = 32;
        for(int i = MAX_LOG_LINES - 1; i >= 0; i--) {
            if(state->log_lines[i][0] != '\0') {
                canvas_draw_str(canvas, 2, y, state->log_lines[i]);
                y += 8;
                if(y > 60) break;
            }
        }
    } else {
        canvas_draw_str(canvas, 2, 40, "No logs yet");
        canvas_draw_str(canvas, 2, 50, "Actions will appear here");
    }
    
    // Auto-scroll indicator
    if(state->auto_scroll) {
        canvas_draw_str(canvas, 100, 22, "AUTO");
    }
}

static void live_monitor_ui_draw_callback(Canvas* canvas, void* context) {
    furi_assert(canvas);
    furi_assert(context);
    
    UNUSED(context);
    
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    draw_live_monitor_header(canvas);
    draw_live_monitor_logs(canvas, &monitor_state);
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 20, 64, "↑↓=Scroll  Back=Exit");
}

static bool live_monitor_ui_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    if(!app) return false;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            return false; // Let scene manager handle back
        } else if(event->key == InputKeyUp) {
            if(monitor_state.scroll_offset > 0) {
                monitor_state.scroll_offset--;
                monitor_state.auto_scroll = false;
            }
            return true;
        } else if(event->key == InputKeyDown) {
            if(monitor_state.scroll_offset < 10) {
                monitor_state.scroll_offset++;
            }
            return true;
        } else if(event->key == InputKeyOk) {
            // Toggle auto-scroll
            monitor_state.auto_scroll = !monitor_state.auto_scroll;
            return true;
        }
    }
    
    return true;
}

static void live_monitor_ui_timer_callback(void* context) {
    furi_assert(context);
    PredatorApp* app = context;
    
    // Update monitor time
    monitor_state.monitor_time_ms = furi_get_tick() - monitor_start_tick;
    
    // Reload logs every second
    if(monitor_state.monitor_time_ms % 1000 < 100) {
        load_recent_logs(&monitor_state);
    }
    
    // Trigger view update
    if(app->view_dispatcher) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 0);
    }
}

void predator_scene_live_monitor_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    
    memset(&monitor_state, 0, sizeof(LiveMonitorState));
    monitor_state.auto_scroll = true;
    monitor_start_tick = furi_get_tick();
    
    // Load initial logs
    load_recent_logs(&monitor_state);
    
    if(!app->view_dispatcher) {
        FURI_LOG_E("LiveMonitorUI", "View dispatcher is NULL");
        return;
    }
    
    View* view = view_alloc();
    if(!view) {
        FURI_LOG_E("LiveMonitorUI", "Failed to allocate view");
        return;
    }
    
    view_set_context(view, app);
    view_set_draw_callback(view, live_monitor_ui_draw_callback);
    view_set_input_callback(view, live_monitor_ui_input_callback);
    
    view_dispatcher_add_view(app->view_dispatcher, PredatorViewLiveMonitorUI, view);
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewLiveMonitorUI);
    
    FURI_LOG_I("LiveMonitorUI", "Live Monitor UI initialized");
    
    // Start timer for updates
    app->timer = furi_timer_alloc(live_monitor_ui_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 100); // Update every 100ms
}

bool predator_scene_live_monitor_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    if(!app) return false;
    
    // Handle back button - return to previous scene
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;  // Consumed - prevents framework bug
    }
    
    if(event.type == SceneManagerEventTypeCustom) {
        return true;
    }
    
    return false;
}

void predator_scene_live_monitor_ui_on_exit(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    
    // Stop timer
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
    
    if(app->view_dispatcher) {
        view_dispatcher_remove_view(app->view_dispatcher, PredatorViewLiveMonitorUI);
    }
    
    FURI_LOG_I("LiveMonitorUI", "Live Monitor UI exited");
}
