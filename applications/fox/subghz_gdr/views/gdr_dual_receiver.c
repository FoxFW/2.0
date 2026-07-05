// views/gdr_dual_receiver.c
#include "gdr_dual_receiver.h"

#ifdef ENABLE_DUAL_RX_SCENE

#include "../gdr_history.h"
#include <input/input.h>
#include <gui/elements.h>
#include <furi.h>

#define FRAME_HEIGHT             12
#define MENU_ITEMS               3u
#define MAX_LEN_PX               118
#define LIST_HEIGHT              (MENU_ITEMS * FRAME_HEIGHT) // 36
#define STATUS_TOP_Y             (LIST_HEIGHT) // 36
#define SUBGHZ_RAW_THRESHOLD_MIN -90.0f

struct GDRDualReceiver {
    View* view;
    GDRDualReceiverCallback callback;
    void* context;
};

typedef struct {
    char tag[4];
    FuriString* frequency_str;
    FuriString* modulation_str;
    float rssi;
    bool external;
} DualChainStatus;

typedef struct {
    GDRHistory* history;
    FuriMutex* history_mutex;
    uint8_t list_offset;
    uint8_t history_item;
    DualChainStatus chain[2];
    FuriString* history_stat_str;
    FuriString* draw_scratch;
} GDRDualReceiverModel;

static size_t gdr_view_dual_receiver_item_count(GDRDualReceiverModel* model) {
    return model->history ? gdr_history_get_item(model->history) : 0U;
}

void gdr_view_dual_receiver_set_callback(
    GDRDualReceiver* receiver,
    GDRDualReceiverCallback callback,
    void* context) {
    furi_check(receiver);
    receiver->callback = callback;
    receiver->context = context;
}

void gdr_view_dual_receiver_set_history(
    GDRDualReceiver* receiver,
    GDRHistory* history) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        GDRDualReceiverModel * model,
        { model->history = history; },
        true);
}

void gdr_view_dual_receiver_set_history_mutex(
    GDRDualReceiver* receiver,
    FuriMutex* mutex) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        GDRDualReceiverModel * model,
        { model->history_mutex = mutex; },
        false);
}

void gdr_view_dual_receiver_set_chain_status(
    GDRDualReceiver* receiver,
    uint8_t slot,
    const char* tag,
    const char* frequency_str,
    const char* modulation_str,
    bool external) {
    furi_check(receiver);
    if(slot > 1) return;
    with_view_model(
        receiver->view,
        GDRDualReceiverModel * model,
        {
            snprintf(model->chain[slot].tag, sizeof(model->chain[slot].tag), "%s", tag);
            furi_string_set_str(model->chain[slot].frequency_str, frequency_str);
            furi_string_set_str(model->chain[slot].modulation_str, modulation_str);
            model->chain[slot].external = external;
        },
        true);
}

void gdr_view_dual_receiver_set_rssi(
    GDRDualReceiver* receiver,
    uint8_t slot,
    float rssi) {
    furi_check(receiver);
    if(slot > 1) return;
    with_view_model(
        receiver->view,
        GDRDualReceiverModel * model,
        { model->chain[slot].rssi = rssi; },
        true);
}

void gdr_view_dual_receiver_set_history_stat(
    GDRDualReceiver* receiver,
    const char* history_stat_str) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        GDRDualReceiverModel * model,
        { furi_string_set_str(model->history_stat_str, history_stat_str); },
        true);
}

static void gdr_view_dual_receiver_update_offset(GDRDualReceiver* receiver) {
    with_view_model(
        receiver->view,
        GDRDualReceiverModel * model,
        {
            size_t history_item = model->history_item;
            size_t list_offset = model->list_offset;
            size_t item_count = gdr_view_dual_receiver_item_count(model);

            if(history_item < list_offset) {
                model->list_offset = history_item;
            } else if(history_item >= (list_offset + MENU_ITEMS)) {
                model->list_offset = history_item - (MENU_ITEMS - 1);
            }

            if(item_count < MENU_ITEMS) {
                model->list_offset = 0;
            } else {
                size_t max_offset = item_count - MENU_ITEMS;
                if(model->list_offset > max_offset) {
                    model->list_offset = max_offset;
                }
            }
        },
        true);
}

static void gdr_view_dual_receiver_draw_frame(Canvas* canvas, uint16_t idx, bool scrollbar) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0 + idx * FRAME_HEIGHT, scrollbar ? 122 : 127, FRAME_HEIGHT);

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_dot(canvas, 0, 0 + idx * FRAME_HEIGHT);
    canvas_draw_dot(canvas, 1, 0 + idx * FRAME_HEIGHT);
    canvas_draw_dot(canvas, 0, (0 + idx * FRAME_HEIGHT) + 1);

    canvas_draw_dot(canvas, 0, (0 + idx * FRAME_HEIGHT) + 11);
    canvas_draw_dot(canvas, scrollbar ? 121 : 126, 0 + idx * FRAME_HEIGHT);
    canvas_draw_dot(canvas, scrollbar ? 121 : 126, (0 + idx * FRAME_HEIGHT) + 11);
}

static void
    gdr_view_dual_receiver_draw_rssi(Canvas* canvas, float rssi, uint8_t x, uint8_t y) {
    uint8_t segments = 0;
    if(rssi > SUBGHZ_RAW_THRESHOLD_MIN) {
        float v = rssi - SUBGHZ_RAW_THRESHOLD_MIN;
        segments = (uint8_t)(v / 13.0f);
        if(segments > 5) segments = 5;
    }
    for(uint8_t i = 0; i < 5; i++) {
        uint8_t bx = x + i * 3;
        if(i < segments) {
            canvas_draw_box(canvas, bx, y - (i + 1), 2, i + 2);
        } else {
            canvas_draw_dot(canvas, bx, y);
        }
    }
}

static void gdr_view_dual_receiver_draw_status_row(
    Canvas* canvas,
    DualChainStatus* chain,
    uint8_t baseline_y) {
    char row[28];
    snprintf(
        row,
        sizeof(row),
        "%s %s %s",
        chain->tag,
        furi_string_get_cstr(chain->frequency_str),
        furi_string_get_cstr(chain->modulation_str));
    canvas_draw_str(canvas, 0, baseline_y, row);
    gdr_view_dual_receiver_draw_rssi(canvas, chain->rssi, 108, baseline_y);
}

static void
    gdr_view_dual_receiver_draw(Canvas* canvas, GDRDualReceiverModel* model) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    if(model->history_mutex) {
        furi_mutex_acquire(model->history_mutex, FuriWaitForever);
    }

    size_t item_count = gdr_view_dual_receiver_item_count(model);
    bool scrollbar = item_count > MENU_ITEMS;

    if(item_count > 0) {
        size_t shift_position = model->list_offset;
        for(size_t i = 0; i < MIN(item_count, (size_t)MENU_ITEMS); i++) {
            size_t idx = shift_position + i;
            gdr_history_get_text_item_menu(model->history, model->draw_scratch, idx);
            elements_string_fit_width(
                canvas, model->draw_scratch, scrollbar ? MAX_LEN_PX - 6 : MAX_LEN_PX);

            if(model->history_item == idx) {
                gdr_view_dual_receiver_draw_frame(canvas, i, scrollbar);
            } else {
                canvas_set_color(canvas, ColorBlack);
            }
            canvas_draw_str(
                canvas, 4, 9 + (i * FRAME_HEIGHT), furi_string_get_cstr(model->draw_scratch));
        }

        if(scrollbar) {
            size_t max_scroll = item_count - MENU_ITEMS;
            size_t scroll_pos = shift_position;
            if(scroll_pos > max_scroll) scroll_pos = max_scroll;
            elements_scrollbar_pos(canvas, 128, 0, LIST_HEIGHT, scroll_pos, max_scroll + 1);
        }

        if(model->history_mutex) {
            furi_mutex_release(model->history_mutex);
        }
    } else {
        if(model->history_mutex) {
            furi_mutex_release(model->history_mutex);
        }

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop, "Dual RX");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignTop, "Listening on both radios");
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    canvas_draw_line(canvas, 0, STATUS_TOP_Y, 127, STATUS_TOP_Y);
    canvas_draw_str_aligned(
        canvas,
        127,
        STATUS_TOP_Y + 2,
        AlignRight,
        AlignTop,
        furi_string_get_cstr(model->history_stat_str));

    gdr_view_dual_receiver_draw_status_row(canvas, &model->chain[0], STATUS_TOP_Y + 10);
    gdr_view_dual_receiver_draw_status_row(canvas, &model->chain[1], STATUS_TOP_Y + 20);
}

static bool gdr_view_dual_receiver_input(InputEvent* event, void* context) {
    furi_check(context);
    GDRDualReceiver* receiver = context;
    bool consumed = false;

    if(event->type == InputTypeShort || event->type == InputTypeLong ||
       event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyUp:
            with_view_model(
                receiver->view,
                GDRDualReceiverModel * model,
                {
                    if(model->history_item > 0) model->history_item--;
                },
                true);
            gdr_view_dual_receiver_update_offset(receiver);
            consumed = true;
            break;
        case InputKeyDown:
            with_view_model(
                receiver->view,
                GDRDualReceiverModel * model,
                {
                    size_t item_count = gdr_view_dual_receiver_item_count(model);
                    if(item_count > 0 && model->history_item < item_count - 1) {
                        model->history_item++;
                    }
                },
                true);
            gdr_view_dual_receiver_update_offset(receiver);
            consumed = true;
            break;
        case InputKeyLeft:
            if(receiver->callback) {
                receiver->callback(
                    GDRCustomEventViewDualReceiverConfig, receiver->context);
            }
            consumed = true;
            break;
        case InputKeyRight:
            if(event->type == InputTypeLong) {
                bool do_delete = false;
                with_view_model(
                    receiver->view,
                    GDRDualReceiverModel * model,
                    { do_delete = gdr_view_dual_receiver_item_count(model) > 0; },
                    false);
                if(do_delete && receiver->callback) {
                    receiver->callback(
                        GDRCustomEventViewDualReceiverDeleteItem, receiver->context);
                }
            }
            consumed = true;
            break;
        case InputKeyOk: {
            bool do_ok = false;
            with_view_model(
                receiver->view,
                GDRDualReceiverModel * model,
                { do_ok = gdr_view_dual_receiver_item_count(model) > 0; },
                false);
            if(do_ok && receiver->callback) {
                receiver->callback(GDRCustomEventViewDualReceiverOK, receiver->context);
            }
            consumed = true;
            break;
        }
        case InputKeyBack:
            if(receiver->callback) {
                receiver->callback(GDRCustomEventViewDualReceiverBack, receiver->context);
            }
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

GDRDualReceiver* gdr_view_dual_receiver_alloc(void) {
    GDRDualReceiver* receiver = malloc(sizeof(GDRDualReceiver));
    furi_check(receiver);
    receiver->callback = NULL;
    receiver->context = NULL;

    receiver->view = view_alloc();
    view_allocate_model(
        receiver->view, ViewModelTypeLocking, sizeof(GDRDualReceiverModel));
    view_set_context(receiver->view, receiver);
    view_set_draw_callback(receiver->view, (ViewDrawCallback)gdr_view_dual_receiver_draw);
    view_set_input_callback(receiver->view, gdr_view_dual_receiver_input);

    with_view_model(
        receiver->view,
        GDRDualReceiverModel * model,
        {
            model->history = NULL;
            model->history_mutex = NULL;
            model->list_offset = 0;
            model->history_item = 0;
            model->history_stat_str = furi_string_alloc();
            model->draw_scratch = furi_string_alloc();
            for(uint8_t i = 0; i < 2; i++) {
                model->chain[i].frequency_str = furi_string_alloc();
                model->chain[i].modulation_str = furi_string_alloc();
                model->chain[i].rssi = -127.0f;
                model->chain[i].external = false;
                model->chain[i].tag[0] = '\0';
            }
        },
        true);

    return receiver;
}

void gdr_view_dual_receiver_free(GDRDualReceiver* receiver) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        GDRDualReceiverModel * model,
        {
            furi_string_free(model->history_stat_str);
            furi_string_free(model->draw_scratch);
            for(uint8_t i = 0; i < 2; i++) {
                furi_string_free(model->chain[i].frequency_str);
                furi_string_free(model->chain[i].modulation_str);
            }
        },
        false);
    view_free(receiver->view);
    free(receiver);
}

View* gdr_view_dual_receiver_get_view(GDRDualReceiver* receiver) {
    furi_check(receiver);
    return receiver->view;
}

void gdr_view_dual_receiver_reset_menu(GDRDualReceiver* receiver) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        GDRDualReceiverModel * model,
        {
            model->history = NULL;
            model->history_item = 0;
            model->list_offset = 0;
        },
        false);
}

void gdr_view_dual_receiver_sync_menu_from_history(
    GDRDualReceiver* receiver,
    GDRHistory* history) {
    furi_check(receiver);
    furi_check(history);
    with_view_model(
        receiver->view,
        GDRDualReceiverModel * model,
        {
            model->history = history;
            size_t item_count = gdr_view_dual_receiver_item_count(model);
            if(item_count == 0) {
                model->history_item = 0;
                model->list_offset = 0;
            } else {
                if(model->history_item >= item_count) model->history_item = item_count - 1;
                if(model->list_offset >= item_count) model->list_offset = item_count - 1;
            }
        },
        true);
    gdr_view_dual_receiver_update_offset(receiver);
}

uint16_t gdr_view_dual_receiver_get_idx_menu(GDRDualReceiver* receiver) {
    furi_check(receiver);
    uint16_t idx = 0;
    with_view_model(
        receiver->view,
        GDRDualReceiverModel * model,
        { idx = model->history_item; },
        false);
    return idx;
}

void gdr_view_dual_receiver_set_idx_menu(GDRDualReceiver* receiver, uint16_t idx) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        GDRDualReceiverModel * model,
        {
            model->history_item = idx;
            size_t item_count = gdr_view_dual_receiver_item_count(model);
            if(model->history_item >= item_count) {
                model->history_item = item_count > 0 ? item_count - 1 : 0;
            }
        },
        true);
    gdr_view_dual_receiver_update_offset(receiver);
}

void gdr_view_dual_receiver_delete_item(GDRDualReceiver* receiver, uint16_t idx) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        GDRDualReceiverModel * model,
        {
            size_t item_count = gdr_view_dual_receiver_item_count(model);
            if(idx <= item_count) {
                if(item_count == 0) {
                    model->history = NULL;
                    model->history_item = 0;
                    model->list_offset = 0;
                } else {
                    if(model->history_item > idx || model->history_item >= item_count) {
                        if(model->history_item > 0) model->history_item--;
                    }
                    if(model->history_item >= item_count) model->history_item = item_count - 1;
                    if(model->list_offset >= item_count) model->list_offset = item_count - 1;
                }
            }
        },
        true);
    gdr_view_dual_receiver_update_offset(receiver);
}

#endif // ENABLE_DUAL_RX_SCENE
