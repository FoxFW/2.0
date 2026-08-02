#include "app.h"
#include "fox_update_downloader_icons.h"
#include <gui/icon_i.h>
#include <string.h>

static UpdaterApp* s_app = NULL;

#define STATUS_MAX_LINES 3
#define STATUS_LINE_BUF  48
#define STATUS_TEXT_MAX_WIDTH 120
#define STATUS_LINE_HEIGHT 9
#define STATUS_REGION_TOP 16
#define STATUS_REGION_BOTTOM 46

static size_t status_wrap_text(
    Canvas* canvas,
    const char* text,
    char lines[][STATUS_LINE_BUF],
    size_t max_lines) {
    size_t count = 0;
    size_t len = strlen(text);
    size_t pos = 0;
    while(pos < len && count < max_lines) {
        size_t i = pos;
        size_t last_space = 0;
        bool have_space = false;
        while(i < len) {
            size_t candidate_len = i - pos + 1;
            if(candidate_len >= STATUS_LINE_BUF) break;
            char tmp[STATUS_LINE_BUF];
            memcpy(tmp, text + pos, candidate_len);
            tmp[candidate_len] = '\0';
            if(canvas_string_width(canvas, tmp) > STATUS_TEXT_MAX_WIDTH) break;
            if(text[i] == ' ') {
                last_space = i - pos;
                have_space = true;
            }
            i++;
        }
        size_t take = i - pos;
        if(take == 0) take = 1;
        if(i < len && have_space) take = last_space;
        size_t copy_len = take;
        if(copy_len >= STATUS_LINE_BUF) copy_len = STATUS_LINE_BUF - 1;
        memcpy(lines[count], text + pos, copy_len);
        lines[count][copy_len] = '\0';
        count++;
        pos += take;
        while(pos < len && text[pos] == ' ') pos++;
    }
    return count;
}

static void status_draw(Canvas* canvas, void* model_ptr) {
    UNUSED(model_ptr);
    UpdaterApp* app = s_app;
    if(!app) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, app->status_title);

    canvas_set_font(canvas, FontSecondary);

    char all_lines[STATUS_MAX_LINES][STATUS_LINE_BUF];
    size_t total = 0;
    if(app->status_line1[0]) {
        total += status_wrap_text(canvas, app->status_line1, all_lines + total, STATUS_MAX_LINES - total);
    }
    if(app->status_line2[0] && total < STATUS_MAX_LINES) {
        total += status_wrap_text(canvas, app->status_line2, all_lines + total, STATUS_MAX_LINES - total);
    }

    size_t region_center = (STATUS_REGION_TOP + STATUS_REGION_BOTTOM) / 2;
    size_t total_height = total * STATUS_LINE_HEIGHT;
    size_t first_center = region_center - (total_height / 2) + (STATUS_LINE_HEIGHT / 2);
    for(size_t i = 0; i < total; i++) {
        canvas_draw_str_aligned(
            canvas, 64, first_center + i * STATUS_LINE_HEIGHT, AlignCenter, AlignCenter, all_lines[i]);
    }

    if(app->status_cycle_mode) {
        canvas_set_font(canvas, FontSecondary);
        const Icon* icon = &I_ButtonCenter_7x7;
        int32_t icon_gap = 3;
        int32_t pad = 8;

        int32_t max_label_w = 0;
        for(uint8_t i = 0; i < app->status_cycle_count; i++) {
            int32_t w = (int32_t)canvas_string_width(canvas, app->status_cycle_options[i]);
            if(w > max_label_w) max_label_w = w;
        }
        int32_t btn_w = icon->width + icon_gap + max_label_w + pad * 2;
        if(btn_w > 100) btn_w = 100;
        int32_t btn_x = (128 - btn_w) / 2;

        const char* label = app->status_cycle_options[app->status_cycle_selected];
        updater_draw_ok_button(canvas, (uint8_t)btn_x, 48, (uint8_t)btn_w, 14, 3, label);

        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str_aligned(canvas, 6, 55, AlignLeft, AlignCenter, "<");
        canvas_draw_str_aligned(canvas, 122, 55, AlignRight, AlignCenter, ">");
        return;
    }

    bool both = app->status_has_left && app->status_has_right;
    if(app->status_has_left) {
        if(both && app->status_selected == 0) {
            updater_draw_ok_button(canvas, 4, 48, 58, 14, 3, app->status_btn_left);
        } else {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_rframe(canvas, 4, 48, 58, 14, 3);
            canvas_draw_str_aligned(canvas, 33, 55, AlignCenter, AlignCenter, app->status_btn_left);
        }
    }
    if(app->status_has_right) {
        if(both && app->status_selected != 1) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_rframe(canvas, 66, 48, 58, 14, 3);
            canvas_draw_str_aligned(canvas, 95, 55, AlignCenter, AlignCenter, app->status_btn_right);
        } else if(both) {
            updater_draw_ok_button(canvas, 66, 48, 58, 14, 3, app->status_btn_right);
        } else {
            updater_draw_ok_button_centered(canvas, 48, 14, 3, app->status_btn_right);
        }
    }
}

static bool status_input(InputEvent* event, void* context) {
    UpdaterApp* app = context;
    if(event->type != InputTypeShort) return false;

    if(app->status_cycle_mode) {
        if(event->key == InputKeyLeft) {
            app->status_cycle_selected = (app->status_cycle_selected == 0) ?
                                              (uint8_t)(app->status_cycle_count - 1) :
                                              (uint8_t)(app->status_cycle_selected - 1);
            view_status_refresh(app->status_view);
            return true;
        }
        if(event->key == InputKeyRight) {
            app->status_cycle_selected =
                (uint8_t)((app->status_cycle_selected + 1) % app->status_cycle_count);
            view_status_refresh(app->status_view);
            return true;
        }
        if(event->key == InputKeyOk) {
            view_dispatcher_send_custom_event(app->view_dispatcher, UpdaterEventCachedOptionConfirm);
            return true;
        }
        if(event->key == InputKeyBack) {

            app->status_cycle_selected = 1;
            view_dispatcher_send_custom_event(app->view_dispatcher, UpdaterEventCachedOptionConfirm);
            return true;
        }
        return false;
    }

    bool both = app->status_has_left && app->status_has_right;

    if((event->key == InputKeyLeft || event->key == InputKeyRight) && both) {
        app->status_selected ^= 1;
        view_status_refresh(app->status_view);
        return true;
    }
    if(event->key == InputKeyOk) {
        if(both) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher,
                app->status_selected == 1 ? UpdaterEventStatusConfirm : UpdaterEventStatusBack);
            return true;
        }
        if(app->status_has_right) {
            view_dispatcher_send_custom_event(app->view_dispatcher, UpdaterEventStatusConfirm);
            return true;
        }
    }
    if(event->key == InputKeyBack && app->status_has_left) {
        view_dispatcher_send_custom_event(app->view_dispatcher, UpdaterEventStatusBack);
        return true;
    }
    return false;
}

View* view_status_alloc(UpdaterApp* app) {
    s_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, status_draw);
    view_set_input_callback(v, status_input);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(uint8_t));
    return v;
}

void view_status_free(View* v) {
    s_app = NULL;
    view_free(v);
}

void view_status_refresh(View* v) {
    with_view_model(v, uint8_t * m, { UNUSED(m); }, true);
}
