#include "subratt_attack_view.h"
#include "../subratt_i.h"

#include <input/input.h>
#include <gui/elements.h>
#include <gui/icon_animation.h>
#include <subghz_randomattack_icons.h>

#define TAG "SubRattAttackView"

struct SubRattAttackView {
    View* view;
    SubRattAttackViewCallback callback;
    void* context;
    SubRattAttacks attack_type;
    uint64_t max_value;
    uint64_t current_step;
    bool is_attacking;
};

typedef struct {
    SubRattAttacks attack_type;
    uint64_t max_value;
    uint64_t current_step;
    uint8_t repeat_count;
    bool is_attacking;
    IconAnimation* icon;
} SubRattAttackViewModel;

void subratt_attack_view_set_callback(
    SubRattAttackView* instance,
    SubRattAttackViewCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);

    instance->callback = callback;
    instance->context = context;
}

bool subratt_attack_view_input(InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);
    SubRattAttackView* instance = context;
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "InputKey: %d", event->key);
#endif

    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        instance->is_attacking = false;
        with_view_model(
            instance->view,
            SubRattAttackViewModel * model,
            { model->is_attacking = false; },
            true);

        instance->callback(SubRattCustomEventTypeBackPressed, instance->context);
        return true;
    }

    bool update = false;

    if(!instance->is_attacking) {
        if(event->type == InputTypeShort && event->key == InputKeyOk) {
#ifdef FURI_DEBUG
            FURI_LOG_D(TAG, "InputKey: %d OK", event->key);
#endif
            instance->is_attacking = true;
            instance->callback(SubRattCustomEventTypeTransmitStarted, instance->context);
            update = true;
        } else if(event->key == InputKeyUp && event->type == InputTypeShort) {
            instance->callback(SubRattCustomEventTypeSaveFile, instance->context);
            update = true;
        } else if(event->key == InputKeyUp && event->type == InputTypeLong) {
            instance->callback(SubRattCustomEventTypeExtraSettings, instance->context);
            update = true;
        } else if(event->key == InputKeyDown) {
            instance->callback(SubRattCustomEventTypeTransmitCustom, instance->context);
            update = true;
        } else if(event->type == InputTypeShort) {
            if(event->key == InputKeyLeft) {
                instance->callback(SubRattCustomEventTypeChangeStepDown, instance->context);
            } else if(event->key == InputKeyRight) {
                instance->callback(SubRattCustomEventTypeChangeStepUp, instance->context);
            }
            update = true;
        } else if(event->type == InputTypeRepeat) {
            if(event->key == InputKeyLeft) {
                instance->callback(SubRattCustomEventTypeChangeStepDownMore, instance->context);
            } else if(event->key == InputKeyRight) {
                instance->callback(SubRattCustomEventTypeChangeStepUpMore, instance->context);
            }
            update = true;
        }
    } else {

        if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
           (event->key == InputKeyOk || event->key == InputKeyBack)) {
            instance->is_attacking = false;
            instance->callback(SubRattCustomEventTypeTransmitNotStarted, instance->context);

            update = true;
        }
    }

    if(update) {
        with_view_model(
            instance->view,
            SubRattAttackViewModel * model,
            {
                if(model->is_attacking != instance->is_attacking) {
                    if(instance->is_attacking) {
                        icon_animation_stop(model->icon);
                        icon_animation_start(model->icon);
                    } else {
                        icon_animation_stop(model->icon);
                    }
                }

                model->attack_type = instance->attack_type;
                model->max_value = instance->max_value;
                model->current_step = instance->current_step;
                model->is_attacking = instance->is_attacking;
            },
            update);
    }

    return update;
}

SubRattAttackView* subratt_attack_view_alloc() {

    SubRattAttackView* instance = calloc(1, sizeof(SubRattAttackView));

    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(SubRattAttackViewModel));
    view_set_context(instance->view, instance);

    instance->attack_type = SubRattAttackTotalCount;
    instance->max_value = 0x00;
    instance->current_step = 0;
    instance->is_attacking = false;

    with_view_model(
        instance->view,
        SubRattAttackViewModel * model,
        {
            model->icon = icon_animation_alloc(&A_Sub1ghz_14);
            view_tie_icon_animation(instance->view, model->icon);
            model->attack_type = instance->attack_type;
            model->max_value = instance->max_value;
            model->current_step = instance->current_step;
            model->is_attacking = instance->is_attacking;
            model->repeat_count = 0;
        },
        false);

    view_set_draw_callback(instance->view, (ViewDrawCallback)subratt_attack_view_draw);
    view_set_input_callback(instance->view, subratt_attack_view_input);
    view_set_enter_callback(instance->view, subratt_attack_view_enter);
    view_set_exit_callback(instance->view, subratt_attack_view_exit);

    return instance;
}

void subratt_attack_view_enter(void* context) {
    furi_assert(context);
    SubRattAttackView* instance = (SubRattAttackView*)context;
    with_view_model(
        instance->view,
        SubRattAttackViewModel * model,
        {
            if(model->is_attacking) {
                icon_animation_start(model->icon);
            }
        },
        true);
}

void subratt_attack_view_free(SubRattAttackView* instance) {
    furi_assert(instance);

    with_view_model(
        instance->view,
        SubRattAttackViewModel * model,
        { icon_animation_free(model->icon); },
        false);

    view_free(instance->view);
    free(instance);
}

View* subratt_attack_view_get_view(SubRattAttackView* instance) {
    furi_assert(instance);
    return instance->view;
}

void subratt_attack_view_set_current_step(SubRattAttackView* instance, uint64_t current_step) {
    furi_assert(instance);

    instance->current_step = current_step;
    with_view_model(
        instance->view,
        SubRattAttackViewModel * model,
        { model->current_step = current_step; },
        true);
}

void subratt_attack_view_init_values(
    SubRattAttackView* instance,
    uint8_t index,
    uint64_t max_value,
    uint64_t current_step,
    bool is_attacking,
    uint8_t extra_repeats) {
#ifdef FURI_DEBUG
    FURI_LOG_I(
        TAG,
        "INIT, attack_type: %d, max_value: %lld, current_step: %lld, extra_repeats: %d",
        index,
        max_value,
        current_step,
        extra_repeats);
#endif
    instance->attack_type = index;
    instance->max_value = max_value;
    instance->current_step = current_step;
    instance->is_attacking = is_attacking;

    with_view_model(
        instance->view,
        SubRattAttackViewModel * model,
        {
            model->max_value = max_value;
            model->attack_type = index;
            model->current_step = current_step;
            model->is_attacking = is_attacking;
            model->repeat_count = extra_repeats;
            if(is_attacking) {
                icon_animation_start(model->icon);
            } else {
                icon_animation_stop(model->icon);
            }
        },
        true);
}

void subratt_attack_view_exit(void* context) {
    furi_assert(context);
    SubRattAttackView* instance = context;
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "subratt_attack_view_exit");
#endif
    with_view_model(
        instance->view,
        SubRattAttackViewModel * model,
        { icon_animation_stop(model->icon); },
        false);
}

void subratt_attack_view_draw(Canvas* canvas, void* context) {
    furi_assert(context);
    SubRattAttackViewModel* model = (SubRattAttackViewModel*)context;
    char buffer[64] = {0};

    const char* attack_name = NULL;
    attack_name = subratt_protocol_name(model->attack_type);

    if(model->is_attacking) {
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, attack_name);
    }

    const uint8_t y_frequency = 17;
    if(model->max_value > 9999) {
        canvas_set_font(canvas, FontBigNumbers);
        snprintf(buffer, sizeof(buffer), "%05d/", (int)model->current_step);
        canvas_draw_str_aligned(canvas, 5, y_frequency, AlignLeft, AlignTop, buffer);

        canvas_set_font(canvas, FontPrimary);
        snprintf(buffer, sizeof(buffer), "%05d", (int)model->max_value);
        canvas_draw_str_aligned(canvas, 107, y_frequency + 13, AlignRight, AlignBottom, buffer);
    } else if(model->max_value <= 0xFF) {
        canvas_set_font(canvas, FontBigNumbers);
        snprintf(
            buffer, sizeof(buffer), "%03d/%03d", (int)model->current_step, (int)model->max_value);
        canvas_draw_str_aligned(canvas, 64, y_frequency, AlignCenter, AlignTop, buffer);
    } else {
        canvas_set_font(canvas, FontBigNumbers);
        snprintf(
            buffer, sizeof(buffer), "%04d/%04d", (int)model->current_step, (int)model->max_value);
        canvas_draw_str_aligned(canvas, 64, y_frequency, AlignCenter, AlignTop, buffer);
    }
    canvas_set_font(canvas, FontSecondary);

    memset(buffer, 0, sizeof(buffer));
    if(!model->is_attacking) {
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignBottom, attack_name);

        snprintf(
            buffer,
            sizeof(buffer),
            "x%d",
            model->repeat_count);
        canvas_draw_str_aligned(canvas, 60, 6, AlignCenter, AlignCenter, buffer);

        elements_button_left(canvas, "-1");
        elements_button_right(canvas, "+1");
        elements_button_center(canvas, "Start");
        elements_button_up(canvas, "Save");
        elements_button_down(canvas, "Resend");
    } else {

        const uint8_t icon_h_offset = 0;
        const uint8_t icon_width_with_offset =
            icon_animation_get_width(model->icon) + icon_h_offset;
        const uint8_t icon_v_offset = icon_animation_get_height(model->icon);
        const uint8_t x = canvas_width(canvas);
        const uint8_t y = canvas_height(canvas);
        canvas_draw_icon_animation(
            canvas, x - icon_width_with_offset, y - icon_v_offset, model->icon);

        float progress_value = (float)model->current_step / (float)model->max_value;
        elements_progress_bar(canvas, 8, 37, 110, progress_value > 1 ? 1 : progress_value);

        snprintf(
            buffer,
            sizeof(buffer),
            "x%d",
            model->repeat_count);
        canvas_draw_str(canvas, 4, y - 8, buffer);
        canvas_draw_str(canvas, 4, y - 1, "repeats");

        elements_button_center(canvas, "Stop");
    }
}
