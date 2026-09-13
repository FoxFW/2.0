#include "subghz_view_mode_picker.h"
#include <gui/elements.h>
#include <furi.h>
#include <stdio.h>

typedef struct {
    const char* title;    /* top row of the box */
    const char* subtitle; /* bottom row of the box */
} SubGhzModePickerOption;

static const SubGhzModePickerOption k_options[] = {
    [SUBGHZ_MODE_PICKER_AUTOMOTIVE]     = {"Automotive", "Ford, Kia, Hyundai, etc"},
    [SUBGHZ_MODE_PICKER_GARAGE]         = {"Garage/Gate/Other", "Non-Automotive Only"},
    [SUBGHZ_MODE_PICKER_JAMMER]         = {"RF Jammer", "Radio Frequency Jammer"},
    [SUBGHZ_MODE_PICKER_TPMS]           = {"TPMS", "Tire Pressure Sensors"},
    [SUBGHZ_MODE_PICKER_RADIO_SETTINGS] = {"Radio Settings", "Sub-GHz Settings/Options"},
};
#define OPTION_COUNT (sizeof(k_options) / sizeof(k_options[0]))

/* Full-width double-row boxes, 2 visible per page - same geometry/pattern
 * as subghz_garage's Protocol Group list
 * (applications/fox/subghz_garage/views/subghz_view_protocol_groups.c):
 * BOX_X=4/BOX_W=120 leaves the rightmost 4px for elements_scrollbar(), and
 * the 2-of-N-visible/clamped-top scrolling is the same mechanic. No more
 * heading (frees up the vertical space that let these boxes grow from
 * 24px to 28px tall) and no more Left/Right mode carousel - Automotive and
 * Garage are now their own rows like Radio Settings always was, so
 * Up/Down/OK is the entire input model. */
#define BOX_X          4
#define BOX_W          120
#define BOX_H          28
#define BOX_R          4
#define OPTIONS_VISIBLE 2

static const uint8_t k_slot_y[OPTIONS_VISIBLE] = {2, 34};

typedef struct {
    uint8_t cursor;
} SubGhzModePickerModel;

struct SubGhzModePicker {
    View*                     view;
    SubGhzModePickerCallback  callback;
    void*                     context;
};

static uint8_t mode_picker_scroll_top(uint8_t cursor) {
    uint8_t max_top = OPTION_COUNT - OPTIONS_VISIBLE;
    return (cursor > max_top) ? max_top : cursor;
}

static void mode_picker_draw_cb(Canvas* canvas, void* _model) {
    SubGhzModePickerModel* m = _model;
    canvas_clear(canvas);

    uint8_t top = mode_picker_scroll_top(m->cursor);

    for(uint8_t slot = 0; slot < OPTIONS_VISIBLE; slot++) {
        uint8_t idx = top + slot;
        if(idx >= OPTION_COUNT) break;
        bool at_cursor = (idx == m->cursor);
        uint8_t y = k_slot_y[slot];
        const SubGhzModePickerOption* opt = &k_options[idx];

        canvas_set_color(canvas, ColorBlack);
        if(at_cursor) {
            canvas_draw_rbox(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
        }

        uint8_t box_center_x = BOX_X + BOX_W / 2;
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, box_center_x, y + 9, AlignCenter, AlignCenter, opt->title);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, box_center_x, y + 20, AlignCenter, AlignCenter, opt->subtitle);

        canvas_set_color(canvas, ColorBlack);
    }

    elements_scrollbar(canvas, m->cursor, OPTION_COUNT);
}

static bool mode_picker_input_cb(InputEvent* event, void* context) {
    SubGhzModePicker* instance = context;
    if(event->type != InputTypeShort) return false;

    bool consumed = false;
    bool fire     = false;
    uint32_t ev_val = 0;

    with_view_model(
        instance->view,
        SubGhzModePickerModel* m,
        {
            if(event->key == InputKeyUp) {
                m->cursor = (m->cursor == 0) ? (uint8_t)(OPTION_COUNT - 1) : m->cursor - 1;
                consumed = true;
            } else if(event->key == InputKeyDown) {
                m->cursor = (uint8_t)((m->cursor + 1) % OPTION_COUNT);
                consumed = true;
            } else if(event->key == InputKeyOk) {
                fire = true;
                ev_val = m->cursor;
                consumed = true;
            }
        },
        true);

    if(fire && instance->callback) instance->callback(instance->context, ev_val);
    return consumed;
}

SubGhzModePicker* subghz_mode_picker_alloc(void) {
    SubGhzModePicker* instance = malloc(sizeof(SubGhzModePicker));
    instance->view     = view_alloc();
    instance->callback = NULL;
    instance->context  = NULL;
    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(SubGhzModePickerModel));
    view_set_draw_callback(instance->view, mode_picker_draw_cb);
    view_set_input_callback(instance->view, mode_picker_input_cb);

    with_view_model(
        instance->view, SubGhzModePickerModel* m, { m->cursor = SUBGHZ_MODE_PICKER_AUTOMOTIVE; },
        false);

    return instance;
}

void subghz_mode_picker_free(SubGhzModePicker* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* subghz_mode_picker_get_view(SubGhzModePicker* instance) {
    furi_assert(instance);
    return instance->view;
}

void subghz_mode_picker_set_callback(
    SubGhzModePicker* instance,
    SubGhzModePickerCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->context  = context;
}

void subghz_mode_picker_set_selected(SubGhzModePicker* instance, uint8_t index) {
    furi_assert(instance);
    if(index >= OPTION_COUNT) return;
    with_view_model(
        instance->view, SubGhzModePickerModel* m, { m->cursor = index; }, true);
}
