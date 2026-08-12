#include "desktop_settings_view_alarm_edit.h"
#include <gui/view.h>
#include <gui/canvas.h>
#include <locale/locale.h>
#include <stdio.h>
#include <stdlib.h>

// Field order for Left/Right cursor navigation. Day cells are skipped during
// navigation whenever the alarm isn't recurring (nothing to edit there).
typedef enum {
    FieldHour = 0,
    FieldMinute,
    FieldDaySun,
    FieldDayMon,
    FieldDayTue,
    FieldDayWed,
    FieldDayThu,
    FieldDayFri,
    FieldDaySat,
    FieldRecurring,
    FieldActive,
    FieldDelete,
    FieldCount,
} AlarmEditField;

static const uint8_t day_bit[7] = {
    FOX_ALARM_DAY_SUN,
    FOX_ALARM_DAY_MON,
    FOX_ALARM_DAY_TUE,
    FOX_ALARM_DAY_WED,
    FOX_ALARM_DAY_THU,
    FOX_ALARM_DAY_FRI,
    FOX_ALARM_DAY_SAT};
static const char* const day_letters[7] = {"S", "M", "T", "W", "T", "F", "S"};

typedef struct {
    FoxAlarm alarm;
    uint8_t field;
} AlarmEditModel;

struct DesktopSettingsViewAlarmEdit {
    View* view;
    DesktopSettingsAlarmEditDeleteCallback delete_callback;
    void* delete_context;
};

static bool field_is_editable(const AlarmEditModel* model, uint8_t field) {
    if(field >= FieldDaySun && field <= FieldDaySat) {
        return model->alarm.recurring != 0;
    }
    return true;
}

static void move_field(AlarmEditModel* model, int8_t dir) {
    uint8_t start = model->field;
    do {
        model->field = (uint8_t)((model->field + dir + FieldCount) % FieldCount);
    } while(!field_is_editable(model, model->field) && model->field != start);
}

static void change_field(AlarmEditModel* model, int8_t dir) {
    FoxAlarm* a = &model->alarm;
    if(model->field == FieldHour) {
        a->hour = (uint8_t)((a->hour + dir + 24) % 24);
    } else if(model->field == FieldMinute) {
        a->minute = (uint8_t)((a->minute + dir + 60) % 60);
    } else if(model->field == FieldRecurring) {
        a->recurring = !a->recurring;
    } else if(model->field == FieldActive) {
        a->active = !a->active;
    } else if(model->field >= FieldDaySun && model->field <= FieldDaySat) {
        a->days_mask ^= day_bit[model->field - FieldDaySun];
    }
    // FieldDelete: value change is a no-op, deletion is a dedicated long-OK gesture.
}

static void draw_box_field(
    Canvas* canvas, int16_t x, int16_t y, uint8_t w, uint8_t h, bool selected, bool filled,
    const char* text) {
    canvas_set_color(canvas, ColorBlack);
    if(filled) {
        canvas_draw_box(canvas, x, y, w, h);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_frame(canvas, x, y, w, h);
    }
    canvas_draw_str_aligned(canvas, x + w / 2, y + h / 2, AlignCenter, AlignCenter, text);
    canvas_set_color(canvas, ColorBlack);
    if(selected) {
        canvas_draw_frame(canvas, x - 1, y - 1, w + 2, h + 2);
    }
}

static void draw_text_field(
    Canvas* canvas, int16_t x, int16_t y, uint8_t w, uint8_t h, bool selected, const char* text) {
    canvas_set_color(canvas, ColorBlack);
    if(selected) {
        canvas_draw_box(canvas, x, y, w, h);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_frame(canvas, x, y, w, h);
    }
    canvas_draw_str_aligned(canvas, x + w / 2, y + h / 2, AlignCenter, AlignCenter, text);
    canvas_set_color(canvas, ColorBlack);
}

static void desktop_settings_view_alarm_edit_draw_callback(Canvas* canvas, void* _model) {
    AlarmEditModel* model = _model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 7, AlignCenter, AlignCenter, "Edit Alarm");

    char buf[8];

    bool is12h = locale_get_time_format() == LocaleTimeFormat12h;
    uint8_t display_hour = model->alarm.hour;
    if(is12h) {
        display_hour = model->alarm.hour % 12;
        if(display_hour == 0) display_hour = 12;
    }

    // Layout budget (canvas is 128x64, rows 0-63): every selectable
    // draw_box_field() cell grows by 1px on each side when selected (the
    // cursor outline), so rows are spaced to leave room for that growth as
    // well as the cell itself - otherwise adjacent rows bleed into each
    // other when a field on either side is selected.
    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buf, sizeof(buf), "%02u", display_hour);
    draw_box_field(canvas, 22, 11, 30, 17, model->field == FieldHour, true, buf);
    canvas_draw_str_aligned(canvas, 57, 20, AlignCenter, AlignCenter, ":");
    snprintf(buf, sizeof(buf), "%02u", model->alarm.minute);
    draw_box_field(canvas, 64, 11, 30, 17, model->field == FieldMinute, true, buf);

    canvas_set_font(canvas, FontSecondary);
    if(is12h) {
        canvas_draw_str_aligned(
            canvas, 108, 20, AlignCenter, AlignCenter, model->alarm.hour < 12 ? "AM" : "PM");
    }

    uint8_t cellw = 16;
    uint8_t gap = 2;
    int16_t dx = (128 - (7 * cellw + 6 * gap)) / 2;
    for(uint8_t i = 0; i < 7; i++) {
        bool on = (model->alarm.days_mask & day_bit[i]) != 0;
        bool selected = model->field == FieldDaySun + i;
        int16_t x = dx + i * (cellw + gap);
        draw_box_field(
            canvas, x, 31, cellw, 10, selected, on && model->alarm.recurring, day_letters[i]);
    }

    char rec_buf[20];
    snprintf(rec_buf, sizeof(rec_buf), "Repeat: %s", model->alarm.recurring ? "ON" : "Once");
    draw_text_field(canvas, 2, 44, 62, 8, model->field == FieldRecurring, rec_buf);

    char act_buf[16];
    snprintf(act_buf, sizeof(act_buf), "Active: %s", model->alarm.active ? "ON" : "OFF");
    draw_text_field(canvas, 66, 44, 60, 8, model->field == FieldActive, act_buf);

    const char* delete_label = model->field == FieldDelete ? "Hold OK: Delete" : "Delete Alarm";
    draw_text_field(canvas, 2, 54, 124, 9, model->field == FieldDelete, delete_label);
}

static bool desktop_settings_view_alarm_edit_input_callback(InputEvent* event, void* context) {
    DesktopSettingsViewAlarmEdit* instance = context;
    bool consumed = false;
    bool delete_requested = false;

    with_view_model(
        instance->view,
        AlarmEditModel* model,
        {
            if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
                if(event->key == InputKeyLeft) {
                    move_field(model, -1);
                    consumed = true;
                } else if(event->key == InputKeyRight) {
                    move_field(model, 1);
                    consumed = true;
                } else if(event->key == InputKeyUp) {
                    change_field(model, 1);
                    consumed = true;
                } else if(event->key == InputKeyDown) {
                    change_field(model, -1);
                    consumed = true;
                }
            } else if(
                event->key == InputKeyOk && event->type == InputTypeLong &&
                model->field == FieldDelete) {
                delete_requested = true;
                consumed = true;
            }
        },
        true);

    if(delete_requested && instance->delete_callback) {
        instance->delete_callback(instance->delete_context);
    }

    return consumed;
}

DesktopSettingsViewAlarmEdit* desktop_settings_view_alarm_edit_alloc(void) {
    DesktopSettingsViewAlarmEdit* instance = malloc(sizeof(DesktopSettingsViewAlarmEdit));
    instance->view = view_alloc();
    instance->delete_callback = NULL;
    instance->delete_context = NULL;
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(AlarmEditModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, desktop_settings_view_alarm_edit_draw_callback);
    view_set_input_callback(instance->view, desktop_settings_view_alarm_edit_input_callback);
    return instance;
}

void desktop_settings_view_alarm_edit_free(DesktopSettingsViewAlarmEdit* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* desktop_settings_view_alarm_edit_get_view(DesktopSettingsViewAlarmEdit* instance) {
    furi_assert(instance);
    return instance->view;
}

void desktop_settings_view_alarm_edit_set_alarm(
    DesktopSettingsViewAlarmEdit* instance, const FoxAlarm* alarm) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        AlarmEditModel* model,
        {
            model->alarm = *alarm;
            model->field = FieldHour;
        },
        true);
}

void desktop_settings_view_alarm_edit_get_alarm(
    DesktopSettingsViewAlarmEdit* instance, FoxAlarm* out) {
    furi_assert(instance);
    with_view_model(
        instance->view, AlarmEditModel* model, { *out = model->alarm; }, false);
}

void desktop_settings_view_alarm_edit_set_delete_callback(
    DesktopSettingsViewAlarmEdit* instance,
    DesktopSettingsAlarmEditDeleteCallback callback,
    void* context) {
    furi_assert(instance);
    instance->delete_callback = callback;
    instance->delete_context = context;
}
