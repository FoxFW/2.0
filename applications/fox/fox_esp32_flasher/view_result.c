#include "fox_esp32_flasher.h"
#include <string.h>

static FlasherApp* s_app = NULL;

#define RESULT_LINE_LEN 27

typedef struct {
    bool    success;
    uint8_t board_index;
    char    chip[FLASHER_CHIP_NAME_LEN];
    char    note[FLASHER_STATUS_LEN];
} ResultModel;

#define BTN_X 4
#define BTN_W 120
#define BTN_H 16
#define BTN_R 4

static void result_draw(Canvas* canvas, void* model_ptr) {
    ResultModel* m = model_ptr;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(m->success) {
        canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "Flash Complete!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Fox ESP32 Firmware");
        if(m->chip[0]) {
            char line[40];
            snprintf(line, sizeof(line), "installed on %s.", m->chip);
            canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, line);
        } else {
            canvas_draw_str_aligned(
                canvas, 64, 30, AlignCenter, AlignTop, "installed successfully.");
        }
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "ESP32 is restarting...");
    } else {
        canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "Flash Failed");
        canvas_set_font(canvas, FontSecondary);
        if(m->note[0] && strcmp(m->note, "Flash Failed") != 0) {
            char note_short[RESULT_LINE_LEN];
            strncpy(note_short, m->note, sizeof(note_short) - 1);
            note_short[sizeof(note_short) - 1] = '\0';
            canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, note_short);
            if(m->chip[0] && !strstr(m->note, m->chip)) {
                char line[40];
                snprintf(line, sizeof(line), "This board is %s.", m->chip);
                canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, line);
            }
            canvas_draw_str_aligned(
                canvas, 64, 40, AlignCenter, AlignTop, "See Terminal for details.");
        } else {
            canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Check wiring, put the");
            canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "ESP32 in boot mode,");
            canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "then try again.");
        }
    }

    flasher_draw_ok_button(canvas, BTN_X, 50, BTN_W, BTN_H, BTN_R, "Continue");
}

static bool result_input(InputEvent* event, void* context) {
    FlasherApp* app = context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyOk || event->key == InputKeyBack) {
        furi_timer_stop(app->result_dwell_timer);
        view_dispatcher_send_custom_event(app->view_dispatcher, FlasherEventResultDwellDone);
        return true;
    }
    return false;
}

View* view_result_alloc(FlasherApp* app) {
    s_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, result_draw);
    view_set_input_callback(v, result_input);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(ResultModel));
    with_view_model(v, ResultModel* m, {
        m->success     = false;
        m->board_index = 0;
        m->chip[0]     = '\0';
        m->note[0]     = '\0';
    }, false);
    return v;
}

void view_result_free(View* v) {
    s_app = NULL;
    view_free(v);
}

void view_result_set(View* v, bool success, uint8_t board_index) {
    char chip[FLASHER_CHIP_NAME_LEN];
    char note[FLASHER_STATUS_LEN];
    chip[0] = '\0';
    note[0] = '\0';
    if(s_app) {
        snprintf(chip, sizeof(chip), "%s", s_app->detected_chip);
        furi_mutex_acquire(s_app->worker_state.mutex, FuriWaitForever);
        snprintf(note, sizeof(note), "%s", s_app->worker_state.status);
        furi_mutex_release(s_app->worker_state.mutex);
    }
    with_view_model(v, ResultModel* m, {
        m->success     = success;
        m->board_index = board_index;
        snprintf(m->chip, sizeof(m->chip), "%s", chip);
        snprintf(m->note, sizeof(m->note), "%s", note);
    }, true);
}
