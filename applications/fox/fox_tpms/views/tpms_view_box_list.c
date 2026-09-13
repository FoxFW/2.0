#include "tpms_view_box_list.h"
#include <gui/elements.h>
#include <furi.h>
#include <stdio.h>

/* Full-width double-row boxes, 2 visible per page - same geometry as
 * SubGhz's Mode Picker (subghz_view_mode_picker.c): BOX_X=4/BOX_W=120
 * leaves the rightmost 4px for elements_scrollbar(), 28px-tall boxes, no
 * heading, Up/Down/OK is the entire input model. See that file's own
 * comment for why these particular numbers. */
#define BOX_X 4
#define BOX_W 120
#define BOX_H 28
#define BOX_R 4
#define OPTIONS_VISIBLE 2
#define TEXT_PAD 4

#define SCROLL_TIMER_PERIOD_MS 333

static const uint8_t k_slot_y[OPTIONS_VISIBLE] = {2, 34};

typedef struct {
    const TPMSBoxListOption* options;
    uint8_t count;
    uint8_t cursor;
    size_t scroll_counter;
} TPMSBoxListModel;

struct TPMSBoxList {
    View* view;
    FuriTimer* scroll_timer;
    bool scroll_running;
    TPMSBoxListCallback callback;
    void* context;
};

static uint8_t box_list_scroll_top(uint8_t cursor, uint8_t count) {
    if(count <= OPTIONS_VISIBLE) return 0;
    uint8_t max_top = count - OPTIONS_VISIBLE;
    return (cursor > max_top) ? max_top : cursor;
}

static void box_list_draw_cb(Canvas* canvas, void* _model) {
    TPMSBoxListModel* m = _model;
    canvas_clear(canvas);

    if(m->count == 0 || m->options == NULL) return;

    uint8_t top = box_list_scroll_top(m->cursor, m->count);

    for(uint8_t slot = 0; slot < OPTIONS_VISIBLE; slot++) {
        uint8_t idx = top + slot;
        if(idx >= m->count) break;
        bool at_cursor = (idx == m->cursor);
        uint8_t y = k_slot_y[slot];
        const TPMSBoxListOption* opt = &m->options[idx];

        canvas_set_color(canvas, ColorBlack);
        if(at_cursor) {
            canvas_draw_rbox(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
        }

        // Centered like the box-list style elsewhere (SubGhz's own Mode
        // Picker, subghz_view_mode_picker.c) for anything that fits, with
        // scrolling reserved for text that's actually too wide for the row
        // (e.g. Renault's long "Clio, Captur, Zoe, Dacia Sandero" subtitle,
        // which used to draw centered and simply overflow past the box's
        // own edges on both sides). elements_scrollable_text_line_str()
        // measures the string with canvas_string_width() first: when it
        // fits within `text_w` it draws centered (AlignCenter/AlignBottom)
        // at the x it's given, same as before; only when it's too wide does
        // it fall back to left-aligned + scrolling, re-deriving the left
        // edge itself from the center x (`x -= width / 2`) - so `text_x`
        // must still be the box's horizontal *center*, not its left edge,
        // for this to land in the same place either way. Only the cursor
        // row scrolls (a static row scrolling unread text underneath it
        // would just be noise).
        uint8_t text_x = BOX_X + BOX_W / 2;
        uint8_t text_w = BOX_W - (TEXT_PAD * 2);
        size_t scroll = at_cursor ? m->scroll_counter : 0;

        canvas_set_font(canvas, FontPrimary);
        elements_scrollable_text_line_str(
            canvas, text_x, y + 11, text_w, opt->title, scroll, false, true);
        canvas_set_font(canvas, FontSecondary);
        elements_scrollable_text_line_str(
            canvas, text_x, y + 23, text_w, opt->subtitle, scroll, false, true);

        canvas_set_color(canvas, ColorBlack);
    }

    if(m->count > OPTIONS_VISIBLE) {
        elements_scrollbar(canvas, m->cursor, m->count);
    }
}

static bool box_list_input_cb(InputEvent* event, void* context) {
    TPMSBoxList* instance = context;
    if(event->type != InputTypeShort) return false;

    bool consumed = false;
    bool fire = false;
    uint32_t ev_val = 0;

    with_view_model(
        instance->view,
        TPMSBoxListModel* m,
        {
            if(m->count == 0) {
                // Nothing to navigate/select yet.
            } else if(event->key == InputKeyUp) {
                m->cursor = (m->cursor == 0) ? (uint8_t)(m->count - 1) : m->cursor - 1;
                m->scroll_counter = 0;
                consumed = true;
            } else if(event->key == InputKeyDown) {
                m->cursor = (uint8_t)((m->cursor + 1) % m->count);
                m->scroll_counter = 0;
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

static void box_list_scroll_timer_cb(void* context) {
    TPMSBoxList* instance = context;
    with_view_model(instance->view, TPMSBoxListModel* m, { m->scroll_counter++; }, true);
}

TPMSBoxList* tpms_box_list_alloc(void) {
    TPMSBoxList* instance = malloc(sizeof(TPMSBoxList));
    instance->view = view_alloc();
    instance->callback = NULL;
    instance->context = NULL;
    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(TPMSBoxListModel));
    view_set_draw_callback(instance->view, box_list_draw_cb);
    view_set_input_callback(instance->view, box_list_input_cb);

    with_view_model(
        instance->view,
        TPMSBoxListModel* m,
        {
            m->options = NULL;
            m->count = 0;
            m->cursor = 0;
            m->scroll_counter = 0;
        },
        false);

    // Not started here - see tpms_box_list_resume_scroll()'s own comment
    // in the header for why (same reasoning, same pattern, as Garage's
    // Protocol Groups list).
    instance->scroll_timer =
        furi_timer_alloc(box_list_scroll_timer_cb, FuriTimerTypePeriodic, instance);
    instance->scroll_running = false;

    return instance;
}

void tpms_box_list_free(TPMSBoxList* instance) {
    furi_assert(instance);
    furi_timer_stop(instance->scroll_timer);
    furi_timer_free(instance->scroll_timer);
    view_free(instance->view);
    free(instance);
}

void tpms_box_list_resume_scroll(TPMSBoxList* instance) {
    furi_assert(instance);
    if(instance->scroll_running) return;
    instance->scroll_running = true;
    furi_timer_start(instance->scroll_timer, SCROLL_TIMER_PERIOD_MS);
}

void tpms_box_list_pause_scroll(TPMSBoxList* instance) {
    furi_assert(instance);
    if(!instance->scroll_running) return;
    instance->scroll_running = false;
    furi_timer_stop(instance->scroll_timer);
}

View* tpms_box_list_get_view(TPMSBoxList* instance) {
    furi_assert(instance);
    return instance->view;
}

void tpms_box_list_set_callback(
    TPMSBoxList* instance,
    TPMSBoxListCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->context = context;
}

void tpms_box_list_set_options(
    TPMSBoxList* instance,
    const TPMSBoxListOption* options,
    uint8_t count) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        TPMSBoxListModel* m,
        {
            m->options = options;
            m->count = count;
            m->cursor = 0;
            m->scroll_counter = 0;
        },
        true);
}

void tpms_box_list_set_selected(TPMSBoxList* instance, uint8_t index) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        TPMSBoxListModel* m,
        {
            if(m->count != 0 && index < m->count) {
                m->cursor = index;
            }
            m->scroll_counter = 0;
        },
        true);
}
