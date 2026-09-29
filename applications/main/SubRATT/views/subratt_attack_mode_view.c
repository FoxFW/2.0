#include "subratt_attack_mode_view.h"

#include <gui/elements.h>
#include <furi.h>
#include <stdio.h>
#include <string.h>

enum {
    SubRattAttackModeOptionBruteForce,
    SubRattAttackModeOptionRandomize,
    SubRattAttackModeOptionCount,
};

static const char* const k_option_names[SubRattAttackModeOptionCount] = {
    [SubRattAttackModeOptionBruteForce] = "Brute Force",
    [SubRattAttackModeOptionRandomize] = "Randomize",
};

#define HEADER_H 14
#define ROW_X    8
#define ROW_W    112
#define ROW_H    22
#define ROW_R    3
#define ROW_VIS  2

struct SubRattAttackModeView {
    View* view;
    SubRattAttackModeViewCallback callback;
    void* context;
};

typedef struct {
    uint8_t cursor;
    char title[40];
} SubRattAttackModeViewModel;

static void subratt_attack_mode_view_draw_callback(Canvas* canvas, void* _model) {
    SubRattAttackModeViewModel* model = _model;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_box(canvas, 0, 0, canvas_width(canvas), HEADER_H);
    canvas_invert_color(canvas);
    canvas_draw_str_aligned(canvas, 64, 3, AlignCenter, AlignTop, model->title);
    canvas_invert_color(canvas);

    for(uint8_t i = 0; i < SubRattAttackModeOptionCount; i++) {
        int32_t by = HEADER_H + i * ROW_H + 1;
        int32_t bh = ROW_H - 2;
        bool selected = (i == model->cursor);

        canvas_set_color(canvas, ColorBlack);
        if(selected) {
            canvas_draw_rbox(canvas, ROW_X, by, ROW_W, bh, ROW_R);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, ROW_X, by, ROW_W, bh, ROW_R);
        }

        canvas_draw_str_aligned(
            canvas, ROW_X + ROW_W / 2, by + bh / 2, AlignCenter, AlignCenter, k_option_names[i]);
        canvas_set_color(canvas, ColorBlack);
    }

    if(SubRattAttackModeOptionCount > ROW_VIS) {
        elements_scrollbar_pos(
            canvas,
            canvas_width(canvas),
            HEADER_H,
            canvas_height(canvas) - HEADER_H,
            model->cursor,
            SubRattAttackModeOptionCount);
    }
}

static bool subratt_attack_mode_view_input_callback(InputEvent* event, void* context) {
    SubRattAttackModeView* instance = context;
    if(event->type != InputTypeShort) return false;

    bool consumed = false;
    bool fire = false;
    uint32_t fire_index = 0;

    with_view_model(
        instance->view,
        SubRattAttackModeViewModel * model,
        {
            if(event->key == InputKeyUp) {
                model->cursor = (model->cursor == 0) ?
                                    (uint8_t)(SubRattAttackModeOptionCount - 1) :
                                    (uint8_t)(model->cursor - 1);
                consumed = true;
            } else if(event->key == InputKeyDown) {
                model->cursor = (uint8_t)((model->cursor + 1) % SubRattAttackModeOptionCount);
                consumed = true;
            } else if(event->key == InputKeyOk) {
                fire = true;
                fire_index = model->cursor;
                consumed = true;
            }
        },
        true);

    if(fire && instance->callback) {
        instance->callback(instance->context, fire_index);
    }
    return consumed;
}

SubRattAttackModeView* subratt_attack_mode_view_alloc(void) {
    SubRattAttackModeView* instance = malloc(sizeof(SubRattAttackModeView));
    instance->view = view_alloc();
    instance->callback = NULL;
    instance->context = NULL;
    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(SubRattAttackModeViewModel));
    view_set_draw_callback(instance->view, subratt_attack_mode_view_draw_callback);
    view_set_input_callback(instance->view, subratt_attack_mode_view_input_callback);
    return instance;
}

void subratt_attack_mode_view_free(SubRattAttackModeView* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* subratt_attack_mode_view_get_view(SubRattAttackModeView* instance) {
    furi_assert(instance);
    return instance->view;
}

void subratt_attack_mode_view_set_callback(
    SubRattAttackModeView* instance,
    SubRattAttackModeViewCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->context = context;
}

void subratt_attack_mode_view_set_title(SubRattAttackModeView* instance, const char* title) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        SubRattAttackModeViewModel * model,
        {
            strncpy(model->title, title, sizeof(model->title) - 1);
            model->title[sizeof(model->title) - 1] = '\0';
            model->cursor = SubRattAttackModeOptionBruteForce;
        },
        true);
}
