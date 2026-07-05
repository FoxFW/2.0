#include "desktop_settings_view_numeric_pin.h"
#include <gui/view.h>
#include <gui/canvas.h>
#include <stdint.h>
#include <string.h>
#include <core/check.h>

#define KEY_ROWS 3
#define KEY_COLS 4

#define KEY_WIDTH 25     
#define KEY_HEIGHT 13    
#define GRID_START_X 5
#define GRID_START_Y 16
#define X_GAP 2
#define Y_GAP 2          
#define SIDEBAR_GAP_X 12 

static const char* const main_text_map[9] = {
    "1", "2", "3",
    "4", "5", "6",
    "7", "8", "9"
};

typedef struct {
    uint8_t selected_row;
    uint8_t selected_col;
    uint8_t pin_length;
    uint8_t pin_buffer[8]; 
    bool is_confirm_mode;
    bool show_error;  
} NumericPinModel;

struct DesktopSettingsViewNumericPin {
    View* view;
    DesktopSettingsViewNumericPinCallback callback;
    void* context;
};

static void desktop_settings_view_numeric_pin_draw_callback(Canvas* canvas, void* _model) {
    furi_assert(canvas);
    furi_assert(_model);
    NumericPinModel* model = _model;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    if(model->is_confirm_mode) {
        canvas_draw_str_aligned(canvas, 59 / 2, 7, AlignCenter, AlignCenter,
        model->show_error ? "INCORRECT" : "Confirm:");
    } else {
        canvas_draw_str(canvas, 5, 11, "Enter PIN:");
    }

    canvas_draw_rframe(canvas, 59, 1, 65, 13, 2);

    uint8_t total_dots = model->pin_length;
    if (total_dots > 0) {
        int16_t box_center_x = 59 + (65 / 2);
        int16_t dot_start_x = box_center_x - (((total_dots * 7) - 1) / 2);

        for(uint8_t i = 0; i < total_dots; i++) {
            int16_t dot_x = dot_start_x + (i * 7);
            canvas_draw_disc(canvas, dot_x, 7, 2); 
        }
    }

    for(uint8_t row = 0; row < KEY_ROWS; row++) {
        for(uint8_t col = 0; col < KEY_COLS; col++) {
            int16_t box_x = GRID_START_X + (col * (KEY_WIDTH + X_GAP));
            if(col == 3) {
                box_x += SIDEBAR_GAP_X;
            }
            int16_t box_y = GRID_START_Y + (row * (KEY_HEIGHT + Y_GAP));

            bool is_selected = (row == model->selected_row && col == model->selected_col);
            if(is_selected) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_rbox(canvas, box_x, box_y, KEY_WIDTH, KEY_HEIGHT, 2);
                canvas_set_color(canvas, ColorWhite);
            } else {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_rframe(canvas, box_x, box_y, KEY_WIDTH, KEY_HEIGHT, 2);
            }

            if(col < 3) {
                uint8_t idx = (row * 3) + col;
                canvas_set_font(canvas, FontPrimary);
                canvas_draw_str_aligned(
                    canvas, 
                    box_x + (KEY_WIDTH / 2), 
                    box_y + (KEY_HEIGHT / 2) + 1, 
                    AlignCenter, 
                    AlignCenter, 
                    main_text_map[idx]
                );
            } else {
                int16_t text_x = box_x + (KEY_WIDTH / 2);
                int16_t text_y = box_y + (KEY_HEIGHT / 2) + 1;

                if(row == 0) {
                    int16_t x = box_x + 6;
                    int16_t y = box_y + 2;

                    canvas_draw_line(canvas, x, y + 4, x + 4, y);
                    canvas_draw_line(canvas, x, y + 4, x + 4, y + 8);
                    
                    canvas_draw_line(canvas, x + 4, y, x + 12, y);
                    canvas_draw_line(canvas, x + 4, y + 8, x + 12, y + 8);
                    canvas_draw_line(canvas, x + 12, y, x + 12, y + 8);

                    canvas_draw_line(canvas, x + 6, y + 2, x + 10, y + 6);
                    canvas_draw_line(canvas, x + 10, y + 2, x + 6, y + 6);
                } else if(row == 1) {
                    canvas_set_font(canvas, FontPrimary); 
                    canvas_draw_str_aligned(canvas, text_x, text_y, AlignCenter, AlignCenter, "0");
                } else if(row == 2) {
                    canvas_set_font(canvas, FontKeyboard);
                    canvas_draw_str_aligned(canvas, text_x, text_y, AlignCenter, AlignCenter, "Set");
                    canvas_draw_str_aligned(canvas, text_x + 1, text_y, AlignCenter, AlignCenter, "Set");
                }
            }
            canvas_set_color(canvas, ColorBlack);
        }
    }
}

static bool desktop_settings_view_numeric_pin_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    DesktopSettingsViewNumericPin* instance = context;
    bool consumed = false;
    bool trigger_callback = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        // Safe atomic lock: with_view_model automatically commits and releases locks safely
        with_view_model(

            instance->view,
            NumericPinModel* model,
            {
                if(model->show_error) {
                    model->show_error = false;
                    model->pin_length = 0;
                }
                if(event->key == InputKeyUp) {
                    model->selected_row = (model->selected_row + KEY_ROWS - 1) % KEY_ROWS;
                    consumed = true;
                } else if(event->key == InputKeyDown) {
                    model->selected_row = (model->selected_row + 1) % KEY_ROWS;
                    consumed = true;
                } else if(event->key == InputKeyLeft) {
                    model->selected_col = (model->selected_col + KEY_COLS - 1) % KEY_COLS;
                    consumed = true;
                } else if(event->key == InputKeyRight) {
                    model->selected_col = (model->selected_col + 1) % KEY_COLS;
                    consumed = true;
                } else if(event->key == InputKeyOk) {
                    if(model->selected_col < 3) {
                        if(model->pin_length < 8) { 
                            uint8_t idx = (model->selected_row * 3) + model->selected_col;
                            model->pin_buffer[model->pin_length] = idx + 1;
                            model->pin_length++;
                        }
                    } else {
                        if(model->selected_row == 0) { 
                            if(model->pin_length > 0) {
                                model->pin_length--;
                            }
                        } else if(model->selected_row == 1) { 
                            if(model->pin_length < 8) { 
                                model->pin_buffer[model->pin_length] = 0;
                                model->pin_length++;
                            }
                        } else if(model->selected_row == 2) { 
                            if(model->pin_length > 0 && instance->callback) {
                                trigger_callback = true; 
                            }
                        }
                    }
                    consumed = true;
                }
            },
            consumed
        );

        if(trigger_callback && instance->callback) {
            instance->callback(true, instance->context);
        }
    } else if(event->type == InputTypeLong && event->key == InputKeyBack) {
        if(instance->callback) {
            instance->callback(false, instance->context);
            consumed = true;
        }
    }
    return consumed;
}

DesktopSettingsViewNumericPin* desktop_settings_view_numeric_pin_alloc(void) {
    DesktopSettingsViewNumericPin* instance = malloc(sizeof(DesktopSettingsViewNumericPin));
    instance->view = view_alloc();
    instance->callback = NULL;
    instance->context = NULL;

    // Correctly restore ViewModelTypeLocking to map with SDK requirements
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(NumericPinModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, desktop_settings_view_numeric_pin_draw_callback);
    view_set_input_callback(instance->view, desktop_settings_view_numeric_pin_input_callback);

    desktop_settings_view_numeric_pin_reset(instance);
    return instance;
}

void desktop_settings_view_numeric_pin_free(DesktopSettingsViewNumericPin* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* desktop_settings_view_numeric_pin_get_view(DesktopSettingsViewNumericPin* instance) {
    furi_assert(instance);
    return instance->view;
}

void desktop_settings_view_numeric_pin_set_callback(
    DesktopSettingsViewNumericPin* instance,
    DesktopSettingsViewNumericPinCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->context = context;
}

void desktop_settings_view_numeric_pin_reset(DesktopSettingsViewNumericPin* instance) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        NumericPinModel* model,
        {
            model->selected_row = 0;
            model->selected_col = 0;
            model->pin_length = 0;
            model->show_error = false;
            memset(model->pin_buffer, 0, sizeof(model->pin_buffer));
        },
        true
    );
}

void desktop_settings_view_numeric_pin_set_mode(DesktopSettingsViewNumericPin* instance, bool is_confirm) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        NumericPinModel* model,
        {
            model->is_confirm_mode = is_confirm;
        },
        true
    );
}

void desktop_settings_view_numeric_pin_set_error(DesktopSettingsViewNumericPin* instance, bool error) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        NumericPinModel* model,
        {
            model->show_error = error;
            model->pin_length = 0;
        },
        true
    );
}

void desktop_settings_view_numeric_pin_get_pin(DesktopSettingsViewNumericPin* instance, uint8_t* pin_buffer, uint8_t* pin_length) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        NumericPinModel* model,
        {
            if(pin_buffer) {
                memcpy(pin_buffer, model->pin_buffer, model->pin_length);
            }
            if(pin_length) {
                *pin_length = model->pin_length;
            }
        },
        false
    );
}