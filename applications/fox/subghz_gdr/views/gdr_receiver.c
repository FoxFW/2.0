// views/gdr_receiver.c
#include "gdr_receiver.h"
#include "../gdr_history.h"
#include "../gdr_app_i.h"
#include <input/input.h>
#include <gui/elements.h>
#include <furi.h>

#include "gdr_icons.h"

#define FRAME_HEIGHT             12
#define MAX_LEN_PX               112
#define MENU_ITEMS               4u
#define UNLOCK_CNT               3
#define SUBGHZ_RAW_THRESHOLD_MIN -90.0f

struct GDRReceiver {
    View* view;
    GDRReceiverCallback callback;
    void* context;
};

typedef struct {
    GDRHistory* history;
    FuriMutex* history_mutex;
    uint8_t list_offset;
    uint8_t history_item;
    float rssi;
    bool auto_save;
    FuriString* frequency_str;
    FuriString* preset_str;
    FuriString* history_stat_str;
    FuriString* draw_scratch;
    bool external_radio;
    GDRLock lock;
    uint8_t lock_count;
    uint8_t animation_frame;
    bool dolphin_view;
    bool sub_decode_mode;
} GDRReceiverModel;

typedef struct {
    int8_t x;
    int8_t y;
} RadarPoint;

static const RadarPoint radar_points[] = {
    {32, 0},
    {30, 12},
    {23, 23},
    {12, 30},
    {0, 32},
    {-12, 30},
    {-23, 23},
    {-30, 12},
    {-32, 0},
    {-30, -12},
    {-23, -23},
    {-12, -30},
    {0, -32},
    {12, -30},
    {23, -23},
    {30, -12},
};

static size_t gdr_view_receiver_item_count(GDRReceiverModel* model) {
    furi_check(model);
    return model->history ? gdr_history_get_item(model->history) : 0U;
}

static void gdr_view_receiver_history_lock(GDRReceiverModel* model) {
    if(model->history_mutex) {
        furi_mutex_acquire(model->history_mutex, FuriWaitForever);
    }
}

static void gdr_view_receiver_history_unlock(GDRReceiverModel* model) {
    if(model->history_mutex) {
        furi_mutex_release(model->history_mutex);
    }
}

static void gdr_view_radar_point(
    uint8_t center_x,
    uint8_t center_y,
    uint8_t radius,
    uint8_t idx,
    int32_t* x,
    int32_t* y) {
    const RadarPoint* p = &radar_points[idx & 0x0F];
    *x = center_x + ((int32_t)radius * p->x) / 32;
    *y = center_y + ((int32_t)radius * p->y) / 32;
}

static void gdr_view_rssi_draw(Canvas* canvas, GDRReceiverModel* model) {
    furi_check(model);
    uint8_t u_rssi = 0;

    if(model->rssi >= SUBGHZ_RAW_THRESHOLD_MIN) {
        float v = model->rssi - SUBGHZ_RAW_THRESHOLD_MIN;
        if(v < 0.0f) v = 0.0f;
        if(v > 67.0f) v = 67.0f;
        u_rssi = (uint8_t)v;
    }

    //Add a 1px space between the segments
    uint8_t spacer = 0;
    for(uint8_t i = 1; i < u_rssi; i++) {
        if(i % 5) {
            uint8_t j = 46 + i + spacer;
            canvas_draw_dot(canvas, j, 52);
            canvas_draw_dot(canvas, j + 1, 53);
            canvas_draw_dot(canvas, j, 54);
        } else
            spacer++;
    }
}

void gdr_view_receiver_set_sub_decode_mode(
    GDRReceiver* receiver,
    bool sub_decode_mode) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        GDRReceiverModel * model,
        { model->sub_decode_mode = sub_decode_mode; },
        true);
}

void gdr_view_receiver_set_rssi(GDRReceiver* receiver, float rssi) {
    furi_check(receiver);
    with_view_model(
        receiver->view, GDRReceiverModel * model, { model->rssi = rssi; }, true);
}

void gdr_view_receiver_set_lock(GDRReceiver* receiver, GDRLock lock) {
    furi_check(receiver);
    with_view_model(
        receiver->view, GDRReceiverModel * model, { model->lock = lock; }, true);
}

void gdr_view_receiver_set_autosave(GDRReceiver* receiver, bool auto_save) {
    furi_check(receiver);
    with_view_model(
        receiver->view, GDRReceiverModel * model, { model->auto_save = auto_save; }, true);
}

void gdr_view_receiver_set_history_mutex(
    GDRReceiver* receiver,
    FuriMutex* history_mutex) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        GDRReceiverModel * model,
        { model->history_mutex = history_mutex; },
        false);
}

void gdr_view_receiver_set_callback(
    GDRReceiver* receiver,
    GDRReceiverCallback callback,
    void* context) {
    furi_check(receiver);
    receiver->callback = callback;
    receiver->context = context;
}

static void gdr_view_receiver_update_offset(GDRReceiver* receiver) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        GDRReceiverModel * model,
        {
            gdr_view_receiver_history_lock(model);
            size_t history_item = model->history_item;
            size_t list_offset = model->list_offset;
            size_t item_count = gdr_view_receiver_item_count(model);

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
            gdr_view_receiver_history_unlock(model);
        },
        true);
}

void gdr_view_receiver_add_data_statusbar(
    GDRReceiver* receiver,
    const char* frequency_str,
    const char* preset_str,
    const char* history_stat_str,
    bool external_radio) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        GDRReceiverModel * model,
        {
            furi_string_set_str(model->frequency_str, frequency_str);
            furi_string_set_str(model->preset_str, preset_str);
            furi_string_set_str(model->history_stat_str, history_stat_str);
            model->external_radio = external_radio;
        },
        true);
}

static void gdr_view_receiver_draw_frame(Canvas* canvas, uint16_t idx, bool scrollbar) {
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

void gdr_view_receiver_draw(Canvas* canvas, GDRReceiverModel* model) {
    gdr_view_receiver_history_lock(model);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    // Increment animation frame
    static uint8_t animation_frame = 0;
    animation_frame = (animation_frame + 1) % 96;

    size_t item_count = gdr_view_receiver_item_count(model);
    bool scrollbar = item_count > MENU_ITEMS;

    if(!model->sub_decode_mode) {
        //Config button. (Do it at the top so we dont get Inversion problems from the list view part.)
        elements_button_left(canvas, "Config");

        // Draw RSSI
        gdr_view_rssi_draw(canvas, model);
    }

    //Draw To Unlock, Locked etc...
    if(model->lock_count) {
        canvas_draw_str(canvas, 44, 63, furi_string_get_cstr(model->frequency_str));
        canvas_draw_str(canvas, 79, 63, furi_string_get_cstr(model->preset_str));
        canvas_draw_str(canvas, 96, 63, furi_string_get_cstr(model->history_stat_str));
        canvas_set_font(canvas, FontSecondary);
        elements_bold_rounded_frame(canvas, 14, 8, 99, 48);
        elements_multiline_text(canvas, 65, 26, "To unlock\npress:");
        canvas_draw_icon(canvas, 65, 42, &I_Pin_back_arrow_10x8);
        canvas_draw_icon(canvas, 80, 42, &I_Pin_back_arrow_10x8);
        canvas_draw_icon(canvas, 95, 42, &I_Pin_back_arrow_10x8);
        canvas_draw_icon(canvas, 16, 13, &I_WarningDolphin_45x42);
        canvas_draw_dot(canvas, 17, 61);
    } else {
        if(model->lock == GDRLockOn) {
            canvas_draw_icon(canvas, 64, 55, &I_Lock_7x8);
            canvas_draw_str(canvas, 74, 62, "Locked");
        } else {
            canvas_draw_str(canvas, 44, 63, furi_string_get_cstr(model->frequency_str));
            canvas_draw_str(canvas, 79, 63, furi_string_get_cstr(model->preset_str));
            canvas_draw_str(canvas, 96, 63, furi_string_get_cstr(model->history_stat_str));
        }
    }

    //Draw the List, or the Radar/Dolphin View.
    if(item_count > 0) {
        // Draw received items list
        size_t shift_position = model->list_offset;

        for(size_t i = 0; i < MIN(item_count, MENU_ITEMS); i++) {
            size_t idx = shift_position + i;
            gdr_history_get_text_item_menu(model->history, model->draw_scratch, idx);
            elements_string_fit_width(
                canvas, model->draw_scratch, scrollbar ? MAX_LEN_PX - 6 : MAX_LEN_PX);

            if(model->history_item == idx) {
                gdr_view_receiver_draw_frame(canvas, i, scrollbar);
            } else {
                canvas_set_color(canvas, ColorBlack);
            }

            canvas_draw_str(
                canvas, 4, 9 + (i * FRAME_HEIGHT), furi_string_get_cstr(model->draw_scratch));
        }

        //Draw scrollbar if needed
        if(scrollbar) {
            // Calculate maximum scroll position
            size_t max_scroll = item_count > MENU_ITEMS ? item_count - MENU_ITEMS : 0;
            size_t scroll_pos = shift_position;
            if(scroll_pos > max_scroll) {
                scroll_pos = max_scroll;
            }
            size_t scrollable_total = max_scroll > 0 ? max_scroll + 1 : 1;
            elements_scrollbar_pos(canvas, 128, 0, 49, scroll_pos, scrollable_total);
        }
    } else {
        //Are we in Radar View or FLipper View Mode?
        if(!model->dolphin_view) {
            const uint8_t center_x = 64;
            const uint8_t center_y = 22;
            for(uint8_t wave = 0; wave < 3; wave++) {
                uint8_t base_radius = ((animation_frame + wave * 32) % 96) / 3;
                if(base_radius < 28) {
                    uint8_t dot_count = base_radius < 10 ? 16 : (base_radius < 20 ? 8 : 4);
                    uint8_t step = COUNT_OF(radar_points) / dot_count;
                    for(uint8_t i = 0; i < dot_count; i++) {
                        int32_t x;
                        int32_t y;
                        gdr_view_radar_point(
                            center_x, center_y, base_radius, i * step + wave * 2, &x, &y);
                        if(x > 0 && x < 128 && y > 0 && y < 48) {
                            canvas_draw_dot(canvas, x, y);
                            if(base_radius < 5) canvas_draw_dot(canvas, x + 1, y);
                        }
                    }
                }
            }

            for(uint8_t i = 0; i < COUNT_OF(radar_points); i += 2) {
                int32_t x;
                int32_t y;
                gdr_view_radar_point(center_x, center_y, 15, i, &x, &y);
                canvas_draw_dot(canvas, x, y);
            }

            uint8_t sweep_idx = animation_frame / 6;
            for(int8_t i = -1; i <= 1; i++) {
                int32_t x;
                int32_t y;
                gdr_view_radar_point(
                    center_x, center_y, i ? 20 : 22, sweep_idx + i, &x, &y);
                canvas_draw_line(canvas, center_x, center_y, x, y);
            }
            for(uint8_t i = 1; i <= 8; i++) {
                int32_t x;
                int32_t y;
                gdr_view_radar_point(
                    center_x, center_y, 22 - i * 2, sweep_idx - i, &x, &y);
                if(i < 3 || !(i & 1)) canvas_draw_dot(canvas, x, y);
            }

            int pulse = (animation_frame % 32);
            if(pulse < 16) {
                canvas_draw_disc(canvas, center_x, center_y, 2);
            } else {
                canvas_draw_circle(canvas, center_x, center_y, 2);
            }
            if(pulse < 8 || (pulse > 16 && pulse < 24)) {
                canvas_draw_dot(canvas, center_x, center_y);
            }
        } else {
            canvas_draw_icon(
                canvas,
                0,
                0,
                model->external_radio ? &I_PP_scanning_ext_123x52 : &I_PP_scanning_123x52);
        }

        // Draw EXT/INT indicator in upper right corner
        canvas_set_font(canvas, FontSecondary);
        if(model->external_radio) {
            canvas_draw_str_aligned(canvas, 127, 0, AlignRight, AlignTop, "Ext");
        } else {
            canvas_draw_str_aligned(canvas, 127, 0, AlignRight, AlignTop, "Int");
        }

        //Draw the Auto-save Indicator
        if(model->auto_save) {
            const char* auto_save_text = "Save";
            canvas_draw_str(
                canvas, 110 - canvas_string_width(canvas, auto_save_text), 7, auto_save_text);
        }
    }
    gdr_view_receiver_history_unlock(model);
}

bool gdr_view_receiver_input(InputEvent* event, void* context) {
    furi_check(context);
    GDRReceiver* receiver = context;

    bool consumed = false;

    GDRLock lock;
    with_view_model(
        receiver->view, GDRReceiverModel * model, { lock = model->lock; }, false);

    if(lock == GDRLockOn) {
        bool do_unlock_cb = false;

        with_view_model(
            receiver->view,
            GDRReceiverModel * model,
            {
                if(event->key == InputKeyBack) {
                    if(event->type == InputTypeShort) {
                        model->lock_count++;
                        if(model->lock_count >= UNLOCK_CNT) {
                            model->lock = GDRLockOff;
                            model->lock_count = 0;
                            do_unlock_cb = true;
                        }
                    }
                } else if(
                    event->type == InputTypeShort || event->type == InputTypeLong ||
                    event->type == InputTypePress) {
                    model->lock_count = 0;
                }
            },
            true);

        if(do_unlock_cb && receiver->callback) {
            receiver->callback(GDRCustomEventViewReceiverUnlock, receiver->context);
        }

        consumed = true;
    } else if(
        event->type == InputTypeShort || event->type == InputTypeLong ||
        event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyUp:
            with_view_model(
                receiver->view,
                GDRReceiverModel * model,
                {
                    if(model->history_item > 0) {
                        model->history_item--;
                    }
                },
                true);
            gdr_view_receiver_update_offset(receiver);
            consumed = true;
            break;
        case InputKeyDown:
            with_view_model(
                receiver->view,
                GDRReceiverModel * model,
                {
                    gdr_view_receiver_history_lock(model);
                    size_t item_count = gdr_view_receiver_item_count(model);
                    if(item_count > 0 && model->history_item < item_count - 1) {
                        model->history_item++;
                    }
                    gdr_view_receiver_history_unlock(model);
                },
                true);
            gdr_view_receiver_update_offset(receiver);
            consumed = true;
            break;
        case InputKeyLeft:
            if(receiver->callback) {
                receiver->callback(GDRCustomEventViewReceiverConfig, receiver->context);
            }
            consumed = true;
            break;
        case InputKeyRight:
            if(event->type == InputTypeLong) {
                bool do_delete_cb = false;
                with_view_model(
                    receiver->view,
                    GDRReceiverModel * model,
                    {
                        gdr_view_receiver_history_lock(model);
                        do_delete_cb = gdr_view_receiver_item_count(model) > 0;
                        gdr_view_receiver_history_unlock(model);
                    },
                    false);
                if(do_delete_cb && receiver->callback) {
                    receiver->callback(
                        GDRCustomEventViewReceiverDeleteItem, receiver->context);
                }
            }
            consumed = true;
            break;
        case InputKeyOk:
            bool do_ok_cb = false;
            bool do_toggle = false;
            /* Read-only: do not redraw */
            with_view_model(
                receiver->view,
                GDRReceiverModel * model,
                {
                    gdr_view_receiver_history_lock(model);
                    size_t item_count = gdr_view_receiver_item_count(model);

                    if(item_count > 0) {
                        do_ok_cb = true;
                    } else if(event->type == InputTypeLong) {
                        do_toggle = true;
                    }
                    gdr_view_receiver_history_unlock(model);
                },
                false);
            /* Only redraw if we actually changed dolphin_view */
            if(do_toggle) {
                with_view_model(
                    receiver->view,
                    GDRReceiverModel * model,
                    { model->dolphin_view = !model->dolphin_view; },
                    true);
            }

            if(do_ok_cb && receiver->callback) {
                receiver->callback(GDRCustomEventViewReceiverOK, receiver->context);
            }

            consumed = true;
            break;
        case InputKeyBack:
            if(receiver->callback) {
                receiver->callback(GDRCustomEventViewReceiverBack, receiver->context);
            }
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void gdr_view_receiver_enter(void* context) {
    furi_check(context);
    UNUSED(context);
}

void gdr_view_receiver_exit(void* context) {
    furi_check(context);
    UNUSED(context);
}

GDRReceiver* gdr_view_receiver_alloc(bool auto_save) {
    GDRReceiver* receiver = malloc(sizeof(GDRReceiver));

    receiver->view = view_alloc();
    view_allocate_model(receiver->view, ViewModelTypeLocking, sizeof(GDRReceiverModel));
    view_set_context(receiver->view, receiver);
    view_set_draw_callback(receiver->view, (ViewDrawCallback)gdr_view_receiver_draw);
    view_set_input_callback(receiver->view, gdr_view_receiver_input);
    view_set_enter_callback(receiver->view, gdr_view_receiver_enter);
    view_set_exit_callback(receiver->view, gdr_view_receiver_exit);

    with_view_model(
        receiver->view,
        GDRReceiverModel * model,
        {
            model->history = NULL;
            model->history_mutex = NULL;
            model->frequency_str = furi_string_alloc();
            model->preset_str = furi_string_alloc();
            model->history_stat_str = furi_string_alloc();
            model->draw_scratch = furi_string_alloc();
            furi_check(model->draw_scratch);
            model->list_offset = 0;
            model->history_item = 0;
            model->rssi = -127.0f;
            model->external_radio = false;
            model->lock = GDRLockOff;
            model->lock_count = 0;
            model->auto_save = auto_save;
            model->animation_frame = 0;
            model->dolphin_view = true;
            model->sub_decode_mode = false;
        },
        true);

    return receiver;
}

void gdr_view_receiver_free(GDRReceiver* receiver) {
    furi_check(receiver);

    with_view_model(
        receiver->view,
        GDRReceiverModel * model,
        {
            furi_string_free(model->frequency_str);
            furi_string_free(model->preset_str);
            furi_string_free(model->history_stat_str);
            furi_string_free(model->draw_scratch);
        },
        false);

    view_free(receiver->view);
    free(receiver);
}

void gdr_view_receiver_reset_menu(GDRReceiver* receiver) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        GDRReceiverModel * model,
        {
            model->history = NULL;
            model->history_mutex = NULL;
            model->history_item = 0;
            model->list_offset = 0;
        },
        false);
}

void gdr_view_receiver_sync_menu_from_history(
    GDRReceiver* receiver,
    GDRHistory* history) {
    furi_check(receiver);
    furi_check(history);

    with_view_model(
        receiver->view,
        GDRReceiverModel * model,
        {
            gdr_view_receiver_history_lock(model);
            model->history = history;
            size_t item_count = gdr_view_receiver_item_count(model);
            if(item_count == 0) {
                model->history_item = 0;
                model->list_offset = 0;
            } else {
                if(model->history_item >= item_count) {
                    model->history_item = item_count - 1;
                }
                if(model->list_offset >= item_count) {
                    model->list_offset = item_count - 1;
                }
            }
            gdr_view_receiver_history_unlock(model);
        },
        true);
    gdr_view_receiver_update_offset(receiver);
}

void gdr_view_receiver_pop_first_menu_item(GDRReceiver* receiver) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        GDRReceiverModel * model,
        {
            gdr_view_receiver_history_lock(model);
            size_t item_count = gdr_view_receiver_item_count(model);
            if(item_count > 0) {
                if(model->history_item > 0) {
                    model->history_item--;
                }
                if(model->list_offset > 0 && model->list_offset >= item_count) {
                    model->list_offset = item_count > 0 ? item_count - 1 : 0;
                }
            }
            gdr_view_receiver_history_unlock(model);
        },
        true);
    gdr_view_receiver_update_offset(receiver);
}

void gdr_view_receiver_delete_item(GDRReceiver* receiver, uint16_t idx) {
    furi_check(receiver);

    with_view_model(
        receiver->view,
        GDRReceiverModel * model,
        {
            gdr_view_receiver_history_lock(model);
            size_t item_count = gdr_view_receiver_item_count(model);
            if(idx <= item_count) {
                if(item_count == 0) {
                    model->history = NULL;
                    model->history_item = 0;
                    model->list_offset = 0;
                } else {
                    if(model->history_item > idx || model->history_item >= item_count) {
                        model->history_item--;
                    }
                    if(model->history_item >= item_count) {
                        model->history_item = item_count - 1;
                    }
                    if(model->list_offset >= item_count) {
                        model->list_offset = item_count - 1;
                    }
                }
            }
            gdr_view_receiver_history_unlock(model);
        },
        true);
    gdr_view_receiver_update_offset(receiver);
}

void gdr_view_receiver_append_menu_row_from_history(
    GDRReceiver* receiver,
    GDRHistory* history,
    uint16_t idx) {
    furi_check(receiver);
    furi_check(history);
    UNUSED(idx);
    gdr_view_receiver_sync_menu_from_history(receiver, history);
}

View* gdr_view_receiver_get_view(GDRReceiver* receiver) {
    furi_check(receiver);
    return receiver->view;
}

uint16_t gdr_view_receiver_get_idx_menu(GDRReceiver* receiver) {
    furi_check(receiver);
    uint16_t idx = 0;
    with_view_model(
        receiver->view, GDRReceiverModel * model, { idx = model->history_item; }, false);
    return idx;
}

void gdr_view_receiver_set_idx_menu(GDRReceiver* receiver, uint16_t idx) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        GDRReceiverModel * model,
        {
            gdr_view_receiver_history_lock(model);
            model->history_item = idx;
            size_t item_count = gdr_view_receiver_item_count(model);
            if(model->history_item >= item_count) {
                model->history_item = item_count > 0 ? item_count - 1 : 0;
            }
            gdr_view_receiver_history_unlock(model);
        },
        true);
    gdr_view_receiver_update_offset(receiver);
}
