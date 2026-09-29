#include "desktop_settings_view_usb_mode.h"
#include <gui/elements.h>
#include <gui/icon.h>
#include "desktop_settings_icons.h"
#include <furi.h>

#define OPTION_COUNT 2
#define BOX_X        4
#define BOX_W        120
#define BOX_H        28
#define BOX_R        4
#define TEXT_PAD     3
#define ICON_GAP     3

static const uint8_t k_slot_y[OPTION_COUNT] = {2, 34};

static const char* const k_title[OPTION_COUNT] = {"qFlipper Mode", "Mass Storage Mode"};
static const char* const k_subtitle[OPTION_COUNT] =
    {"Select for qFlipper Access", "Select for SD Card Access"};

typedef struct {
    uint8_t cursor;
    Desktop* desktop;
} DesktopSettingsUsbModeModel;

struct DesktopSettingsViewUsbMode {
    View* view;
    DesktopSettingsViewUsbModeCallback callback;
    void* context;
};

static void usb_mode_draw_cb(Canvas* canvas, void* _model) {
    DesktopSettingsUsbModeModel* m = _model;
    canvas_clear(canvas);

    uint8_t active = (m->desktop && desktop_api_get_usb_mode(m->desktop) == DesktopUsbModeMassStorage) ? 1 : 0;

    for(uint8_t idx = 0; idx < OPTION_COUNT; idx++) {
        bool at_cursor = (idx == m->cursor);
        bool is_active = (idx == active);
        uint8_t y = k_slot_y[idx];

        canvas_set_color(canvas, ColorBlack);
        if(at_cursor) {
            canvas_draw_rbox(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
        }

        uint8_t icon_x = BOX_X + TEXT_PAD;
        uint8_t icon_y = y + 9 - icon_get_height(&I_ButtonCenter_7x7) / 2;
        uint8_t text_x = icon_x + icon_get_width(&I_ButtonCenter_7x7) + ICON_GAP;

        if(is_active) {
            canvas_draw_icon(canvas, icon_x, icon_y, &I_ButtonCenter_7x7);
        } else {
            canvas_draw_circle(
                canvas,
                icon_x + icon_get_width(&I_ButtonCenter_7x7) / 2,
                icon_y + icon_get_height(&I_ButtonCenter_7x7) / 2,
                icon_get_width(&I_ButtonCenter_7x7) / 2);
        }

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, text_x, y + 9, AlignLeft, AlignCenter, k_title[idx]);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, BOX_X + BOX_W / 2, y + 20, AlignCenter, AlignCenter, k_subtitle[idx]);

        canvas_set_color(canvas, ColorBlack);
    }
}

static bool usb_mode_input_cb(InputEvent* event, void* context) {
    DesktopSettingsViewUsbMode* instance = context;
    if(event->type != InputTypeShort) return false;

    bool consumed = false;
    bool fire = false;
    uint8_t chosen = 0;
    with_view_model(
        instance->view,
        DesktopSettingsUsbModeModel * m,
        {
            if(event->key == InputKeyUp) {
                m->cursor = (m->cursor == 0) ? (uint8_t)(OPTION_COUNT - 1) : m->cursor - 1;
                consumed = true;
            } else if(event->key == InputKeyDown) {
                m->cursor = (uint8_t)((m->cursor + 1) % OPTION_COUNT);
                consumed = true;
            } else if(event->key == InputKeyOk) {
                fire = true;
                chosen = m->cursor;
                consumed = true;
            }
        },
        true);

    if(fire && instance->callback) {
        instance->callback(
            instance->context,
            (chosen == 1) ? DesktopUsbModeMassStorage : DesktopUsbModeQflipper);
    }
    return consumed;
}

DesktopSettingsViewUsbMode* desktop_settings_view_usb_mode_alloc(void) {
    DesktopSettingsViewUsbMode* instance = malloc(sizeof(DesktopSettingsViewUsbMode));
    instance->view = view_alloc();
    instance->callback = NULL;
    instance->context = NULL;
    view_set_context(instance->view, instance);
    view_allocate_model(
        instance->view, ViewModelTypeLocking, sizeof(DesktopSettingsUsbModeModel));
    view_set_draw_callback(instance->view, usb_mode_draw_cb);
    view_set_input_callback(instance->view, usb_mode_input_cb);
    with_view_model(
        instance->view,
        DesktopSettingsUsbModeModel * m,
        {
            m->cursor = 0;
            m->desktop = NULL;
        },
        false);
    return instance;
}

void desktop_settings_view_usb_mode_free(DesktopSettingsViewUsbMode* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* desktop_settings_view_usb_mode_get_view(DesktopSettingsViewUsbMode* instance) {
    furi_assert(instance);
    return instance->view;
}

void desktop_settings_view_usb_mode_set_callback(
    DesktopSettingsViewUsbMode* instance,
    DesktopSettingsViewUsbModeCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->context = context;
}

void desktop_settings_view_usb_mode_set_desktop(
    DesktopSettingsViewUsbMode* instance,
    Desktop* desktop) {
    furi_assert(instance);
    with_view_model(
        instance->view, DesktopSettingsUsbModeModel * m, { m->desktop = desktop; }, true);
}

void desktop_settings_view_usb_mode_set_cursor(
    DesktopSettingsViewUsbMode* instance,
    DesktopUsbMode mode) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        DesktopSettingsUsbModeModel * m,
        { m->cursor = (mode == DesktopUsbModeMassStorage) ? 1 : 0; },
        true);
}
