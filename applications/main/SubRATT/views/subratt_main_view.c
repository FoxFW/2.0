#include "subratt_main_view.h"
#include "../subratt_i.h"

#include <input/input.h>
#include <gui/elements.h>

#define STATUS_BAR_Y_SHIFT 14
#define TAG "SubRattMainView"

#define ITEMS_ON_SCREEN 3
#define ITEMS_INTERVAL 1
#define ITEM_WIDTH 14
#define ITEM_Y 27
#define ITEM_HEIGHT 13
#define TEXT_X 6
#define TEXT_Y 37
#define TEXT_INTERVAL 3
#define TEXT_WIDTH 12
#define ITEM_FRAME_RADIUS 2

#define MENU_ROW_X   8
#define MENU_ROW_W   112
#define MENU_ROW_H   22
#define MENU_ROW_R   3
#define MENU_ROW_VIS 2

struct SubRattMainView {
    View* view;
    SubRattMainViewCallback callback;
    void* context;
    uint8_t index;
    bool is_select_byte;
    bool two_bytes;
    uint64_t key_from_file;
    uint8_t repeat_values[SubRattAttackTotalCount];
    uint8_t window_position;

    SubRattMenuLevel menu_level;
    uint8_t selected_brand;
    uint8_t selected_type;
    uint8_t level_index;
    uint8_t level_window_position;
};

typedef struct {
    uint8_t index;
    uint8_t repeat_values[SubRattAttackTotalCount];
    uint8_t window_position;
    bool is_select_byte;
    bool two_bytes;
    uint64_t key_from_file;

    SubRattMenuLevel menu_level;
    uint8_t selected_brand;
    uint8_t selected_type;
    uint8_t level_index;
    uint8_t level_window_position;
} SubRattMainViewModel;

void subratt_main_view_set_callback(
    SubRattMainView* instance,
    SubRattMainViewCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);

    instance->callback = callback;
    instance->context = context;
}

void subratt_main_view_center_displayed_key(
    Canvas* canvas,
    uint64_t key,
    uint8_t index,
    bool two_bytes) {
    uint8_t text_x = TEXT_X;
    uint8_t item_x = TEXT_X - ITEMS_INTERVAL;
    canvas_set_font(canvas, FontSecondary);

    for(int i = 0; i < 8; i++) {
        char current_value[3] = {0};
        uint8_t byte_value = (uint8_t)(key >> 8 * (7 - i)) & 0xFF;
        snprintf(current_value, sizeof(current_value), "%02X", byte_value);

        if(!two_bytes && i == index) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_rbox(
                canvas, item_x - 1, ITEM_Y, ITEM_WIDTH + 1, ITEM_HEIGHT, ITEM_FRAME_RADIUS);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, text_x, TEXT_Y, current_value);
        } else if(two_bytes && (i == index || i == index - 1)) {
            if(i == index) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_rbox(
                    canvas,
                    item_x - ITEMS_INTERVAL - ITEM_WIDTH - 1,
                    ITEM_Y,
                    ITEM_WIDTH * 2 + ITEMS_INTERVAL * 2 + 1,
                    ITEM_HEIGHT,
                    ITEM_FRAME_RADIUS);

                canvas_set_color(canvas, ColorWhite);
                canvas_draw_str(canvas, text_x, TEXT_Y, current_value);

                memset(current_value, 0, sizeof(current_value));
                byte_value = (uint8_t)(key >> 8 * (7 - i + 1)) & 0xFF;
                snprintf(current_value, sizeof(current_value), "%02X", byte_value);
                canvas_draw_str(
                    canvas, text_x - (TEXT_WIDTH + TEXT_INTERVAL), TEXT_Y, current_value);
            } else {
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_str(canvas, text_x, TEXT_Y, current_value);
            }
        } else {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_str(canvas, text_x, TEXT_Y, current_value);
        }
        text_x = text_x + TEXT_WIDTH + TEXT_INTERVAL;
        item_x = item_x + ITEM_WIDTH + ITEMS_INTERVAL;
    }

    canvas_set_color(canvas, ColorBlack);
}

void subratt_main_view_draw_is_byte_selected(Canvas* canvas, SubRattMainViewModel* model) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, 17, AlignCenter, AlignTop, "Please select values to calc:");

    subratt_main_view_center_displayed_key(
        canvas, model->key_from_file, model->index, model->two_bytes);

    elements_button_center(canvas, "Select");
    if(model->index > 0) {
        elements_button_left(canvas, " ");
    }
    if(model->index < 7) {
        elements_button_right(canvas, " ");
    }

    if(model->two_bytes) {
        elements_button_up(canvas, "One byte");
    } else {
        elements_button_up(canvas, "Two bytes");
    }
}

static uint8_t subratt_main_view_get_level_count(SubRattMainViewModel* model) {
    if(model->menu_level == SubRattMenuLevelBrand) {
        return SubRattBrandCount;
    } else if(model->menu_level == SubRattMenuLevelType) {
        return subratt_brand_group(model->selected_brand)->type_count;
    } else {
        return subratt_brand_group(model->selected_brand)
            ->types[model->selected_type]
            .attack_count;
    }
}

static const char* subratt_main_view_get_item_name(SubRattMainViewModel* model, uint8_t pos) {
    if(model->menu_level == SubRattMenuLevelBrand) {
        return subratt_brand_group(pos)->name;
    } else if(model->menu_level == SubRattMenuLevelType) {
        return subratt_brand_group(model->selected_brand)->types[pos].name;
    } else {
        SubRattAttacks attack = subratt_brand_group(model->selected_brand)
                                     ->types[model->selected_type]
                                     .attacks[pos];
        return subratt_protocol_freq_name(attack);
    }
}

static const char* subratt_main_view_get_title(SubRattMainViewModel* model) {
    if(model->menu_level == SubRattMenuLevelBrand) {
        return SUB_RATT_APP_TITLE;
    } else if(model->menu_level == SubRattMenuLevelType) {
        return subratt_brand_group(model->selected_brand)->name;
    } else {
        const SubRattBrandGroup* bg = subratt_brand_group(model->selected_brand);
        if(bg->type_count == 1) {
            return bg->name;
        }

        static char title_buf[32];
        snprintf(title_buf, sizeof(title_buf), "%s > %s", bg->name, bg->types[model->selected_type].name);
        return title_buf;
    }
}

void subratt_main_view_draw_is_ordinary_selected(Canvas* canvas, SubRattMainViewModel* model) {
    uint16_t screen_width = canvas_width(canvas);
    uint16_t screen_height = canvas_height(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_box(canvas, 0, 0, canvas_width(canvas), STATUS_BAR_Y_SHIFT);
    canvas_invert_color(canvas);
    canvas_draw_str_aligned(
        canvas, 64, 3, AlignCenter, AlignTop, subratt_main_view_get_title(model));
    canvas_invert_color(canvas);

    uint8_t total_items = subratt_main_view_get_level_count(model);

    for(uint8_t position = 0; position < total_items; ++position) {
        uint8_t item_position = position - model->level_window_position;
        if(item_position >= MENU_ROW_VIS) continue;

        const char* item_name = subratt_main_view_get_item_name(model, position);
        bool selected = (model->level_index == position);

        int32_t by = STATUS_BAR_Y_SHIFT + item_position * MENU_ROW_H + 1;
        int32_t bh = MENU_ROW_H - 2;

        canvas_set_color(canvas, ColorBlack);
        if(selected) {
            canvas_draw_rbox(canvas, MENU_ROW_X, by, MENU_ROW_W, bh, MENU_ROW_R);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, MENU_ROW_X, by, MENU_ROW_W, bh, MENU_ROW_R);
        }

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, MENU_ROW_X + MENU_ROW_W / 2, by + bh / 2, AlignCenter, AlignCenter, item_name);

        if(model->menu_level == SubRattMenuLevelFreq) {
            SubRattAttacks attack = subratt_brand_group(model->selected_brand)
                                         ->types[model->selected_type]
                                         .attacks[position];
            uint8_t current_repeat_count = model->repeat_values[attack];
            uint8_t min_repeat_count = subratt_protocol_repeats_count(attack);

            if(current_repeat_count > min_repeat_count) {
#ifdef FW_ORIGIN_Official
                canvas_set_font(canvas, FontSecondary);
#else
                canvas_set_font(canvas, FontBatteryPercent);
#endif
                char buffer[10] = {0};
                snprintf(buffer, sizeof(buffer), "x%d", current_repeat_count);

                canvas_draw_str_aligned(
                    canvas, MENU_ROW_X + MENU_ROW_W - 3, by + 3, AlignRight, AlignTop, buffer);
            }
        }
        canvas_set_color(canvas, ColorBlack);
    }

    if(total_items > MENU_ROW_VIS) {
        elements_scrollbar_pos(
            canvas,
            screen_width,
            STATUS_BAR_Y_SHIFT,
            screen_height - STATUS_BAR_Y_SHIFT,
            model->level_index,
            total_items);
    }
}

void subratt_main_view_draw(Canvas* canvas, SubRattMainViewModel* model) {
    if(model->is_select_byte) {
        subratt_main_view_draw_is_byte_selected(canvas, model);
    } else {
        subratt_main_view_draw_is_ordinary_selected(canvas, model);
    }
}

bool subratt_main_view_input_file_protocol(InputEvent* event, SubRattMainView* instance) {
    bool updated = false;
    if(event->key == InputKeyLeft) {
        if((instance->index > 0 && !instance->two_bytes) ||
           (instance->two_bytes && instance->index > 1)) {
            instance->index--;
        }

        updated = true;
    } else if(event->key == InputKeyRight) {
        if(instance->index < 7) {
            instance->index++;
        }

        updated = true;
    } else if(event->key == InputKeyUp) {
        instance->two_bytes = !instance->two_bytes;

        if(instance->two_bytes && instance->index < 7) {
            instance->index++;
        }

        updated = true;
    } else if(event->key == InputKeyOk) {
      instance->callback(SubRattCustomEventTypeIndexSelected,
                         instance->context);

        updated = true;
    }
    return updated;
}

static void subratt_main_view_update_window_position(SubRattMainView* instance) {
    uint8_t total = 0;
    if(instance->menu_level == SubRattMenuLevelBrand) {
        total = SubRattBrandCount;
    } else if(instance->menu_level == SubRattMenuLevelType) {
        total = subratt_brand_group(instance->selected_brand)->type_count;
    } else {
        total = subratt_brand_group(instance->selected_brand)
                    ->types[instance->selected_type]
                    .attack_count;
    }

    instance->level_window_position = instance->level_index;
    if(instance->level_window_position > 0) {
        instance->level_window_position -= 1;
    }
    if(total <= MENU_ROW_VIS) {
        instance->level_window_position = 0;
    } else if(instance->level_window_position >= (total - MENU_ROW_VIS)) {
        instance->level_window_position = (total - MENU_ROW_VIS);
    }
}

static SubRattAttacks subratt_main_view_get_current_attack(SubRattMainView* instance) {
    return subratt_brand_group(instance->selected_brand)
        ->types[instance->selected_type]
        .attacks[instance->level_index];
}

bool subratt_main_view_input_ordinary_protocol(
    InputEvent* event,
    SubRattMainView* instance,
    bool is_short) {

    uint8_t total = 0;
    if(instance->menu_level == SubRattMenuLevelBrand) {
        total = SubRattBrandCount;
    } else if(instance->menu_level == SubRattMenuLevelType) {
        total = subratt_brand_group(instance->selected_brand)->type_count;
    } else {
        total = subratt_brand_group(instance->selected_brand)
                    ->types[instance->selected_type]
                    .attack_count;
    }

    const uint8_t max_index = total - 1;
    bool updated = false;

    if(event->key == InputKeyUp && is_short) {
        if(instance->level_index == 0) {
            instance->level_index = max_index;
        } else {
            instance->level_index--;
        }
        updated = true;
    } else if(event->key == InputKeyDown && is_short) {
        if(instance->level_index == max_index) {
            instance->level_index = 0;
        } else {
            instance->level_index++;
        }
        updated = true;
    } else if(event->key == InputKeyOk && is_short) {
        if(instance->menu_level == SubRattMenuLevelBrand) {
            if(instance->level_index == SubRattBrandLoadFile) {

                instance->callback(SubRattCustomEventTypeLoadFile, instance->context);
            } else if(instance->level_index == SubRattBrandLoadSavedKeys) {

                instance->callback(SubRattCustomEventTypeLoadSavedKeys, instance->context);
            } else {
                instance->selected_brand = instance->level_index;
                const SubRattBrandGroup* bg = subratt_brand_group(instance->selected_brand);
                if(bg->type_count == 1) {

                    instance->selected_type = 0;
                    instance->menu_level = SubRattMenuLevelFreq;
                } else {
                    instance->menu_level = SubRattMenuLevelType;
                }
                instance->level_index = 0;
                instance->level_window_position = 0;
            }
            updated = true;
        } else if(instance->menu_level == SubRattMenuLevelType) {
            instance->selected_type = instance->level_index;
            instance->menu_level = SubRattMenuLevelFreq;
            instance->level_index = 0;
            instance->level_window_position = 0;
            updated = true;
        } else {

            SubRattAttacks attack = subratt_main_view_get_current_attack(instance);
            instance->index = attack;
            instance->callback(SubRattCustomEventTypeMenuSelected, instance->context);
            updated = true;
        }
    } else if(event->key == InputKeyLeft && is_short) {
        if(instance->menu_level == SubRattMenuLevelFreq) {
            SubRattAttacks attack = subratt_main_view_get_current_attack(instance);
            uint8_t min_repeats = subratt_protocol_repeats_count(attack);
            uint8_t max_repeats = min_repeats * 3;
            uint8_t current_repeats = instance->repeat_values[attack];
            instance->repeat_values[attack] =
                CLAMP(current_repeats - 1, max_repeats, min_repeats);
            updated = true;
        }
    } else if(event->key == InputKeyRight && is_short) {
        if(instance->menu_level == SubRattMenuLevelFreq) {
            SubRattAttacks attack = subratt_main_view_get_current_attack(instance);
            uint8_t min_repeats = subratt_protocol_repeats_count(attack);
            uint8_t max_repeats = min_repeats * 3;
            uint8_t current_repeats = instance->repeat_values[attack];
            instance->repeat_values[attack] =
                CLAMP(current_repeats + 1, max_repeats, min_repeats);
            updated = true;
        }
    }

    if(updated) {
        subratt_main_view_update_window_position(instance);
    }

    return updated;
}

bool subratt_main_view_input(InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);

    SubRattMainView* instance = (SubRattMainView*)context;

    if(event->key == InputKeyBack && event->type == InputTypeShort) {
#ifdef FURI_DEBUG
        FURI_LOG_I(TAG, "InputKey: BACK");
#endif
        if(!instance->is_select_byte && instance->menu_level != SubRattMenuLevelBrand) {

            if(instance->menu_level == SubRattMenuLevelFreq) {
                const SubRattBrandGroup* bg = subratt_brand_group(instance->selected_brand);
                if(bg->type_count == 1) {

                    instance->menu_level = SubRattMenuLevelBrand;
                    instance->level_index = instance->selected_brand;
                } else {
                    instance->menu_level = SubRattMenuLevelType;
                    instance->level_index = instance->selected_type;
                }
            } else if(instance->menu_level == SubRattMenuLevelType) {
                instance->menu_level = SubRattMenuLevelBrand;
                instance->level_index = instance->selected_brand;
            }
            subratt_main_view_update_window_position(instance);

            with_view_model(
                instance->view,
                SubRattMainViewModel * model,
                {
                    model->menu_level = instance->menu_level;
                    model->selected_brand = instance->selected_brand;
                    model->selected_type = instance->selected_type;
                    model->level_index = instance->level_index;
                    model->level_window_position = instance->level_window_position;
                },
                true);
            return true;
        }
        instance->callback(SubRattCustomEventTypeBackPressed, instance->context);
        return false;
    }

#ifdef FURI_DEBUG
    FURI_LOG_D(
        TAG,
        "InputKey: %d, extra_repeats: %d",
        event->key,
        instance->repeat_values[instance->index]);
#endif

    bool updated = false;
    bool is_short = (event->type == InputTypeShort) || (event->type == InputTypeRepeat);

    if(instance->is_select_byte) {
        if(is_short) {
            updated = subratt_main_view_input_file_protocol(event, instance);
        }
    } else {
        updated = subratt_main_view_input_ordinary_protocol(event, instance, is_short);
    }

    if(updated) {
        with_view_model(
            instance->view,
            SubRattMainViewModel * model,
            {
                model->index = instance->index;
                model->window_position = instance->window_position;
                model->key_from_file = instance->key_from_file;
                model->is_select_byte = instance->is_select_byte;
                model->two_bytes = instance->two_bytes;
                model->menu_level = instance->menu_level;
                model->selected_brand = instance->selected_brand;
                model->selected_type = instance->selected_type;
                model->level_index = instance->level_index;
                model->level_window_position = instance->level_window_position;

                for(size_t i = 0; i < SubRattAttackTotalCount; i++) {
                    model->repeat_values[i] = instance->repeat_values[i];
                }
            },
            true);
    }

    return updated;
}

void subratt_main_view_enter(void* context) {
    furi_assert(context);
}

void subratt_main_view_exit(void* context) {
    furi_assert(context);
}

SubRattMainView* subratt_main_view_alloc() {

    SubRattMainView* instance = calloc(1, sizeof(SubRattMainView));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(SubRattMainViewModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, (ViewDrawCallback)subratt_main_view_draw);
    view_set_input_callback(instance->view, subratt_main_view_input);
    view_set_enter_callback(instance->view, subratt_main_view_enter);
    view_set_exit_callback(instance->view, subratt_main_view_exit);

    instance->index = 0;
    instance->window_position = 0;
    instance->key_from_file = 0;
    instance->is_select_byte = false;
    instance->two_bytes = false;
    instance->menu_level = SubRattMenuLevelBrand;
    instance->selected_brand = 0;
    instance->selected_type = 0;
    instance->level_index = 0;
    instance->level_window_position = 0;

    with_view_model(
        instance->view,
        SubRattMainViewModel * model,
        {
            model->index = instance->index;
            model->window_position = instance->window_position;
            model->key_from_file = instance->key_from_file;
            model->is_select_byte = instance->is_select_byte;
            model->two_bytes = instance->two_bytes;
            model->menu_level = instance->menu_level;
            model->selected_brand = instance->selected_brand;
            model->selected_type = instance->selected_type;
            model->level_index = instance->level_index;
            model->level_window_position = instance->level_window_position;
            for(size_t i = 0; i < SubRattAttackTotalCount; i++) {
                model->repeat_values[i] = instance->repeat_values[i];
            }
        },
        true);

    return instance;
}

void subratt_main_view_free(SubRattMainView* instance) {
    furi_assert(instance);

    view_free(instance->view);
    free(instance);
}

View* subratt_main_view_get_view(SubRattMainView* instance) {
    furi_assert(instance);

    return instance->view;
}

void subratt_main_view_set_index(
    SubRattMainView* instance,
    uint8_t idx,
    const uint8_t* repeats,
    bool is_select_byte,
    bool two_bytes,
    uint64_t key_from_file) {

    furi_assert(instance);
    furi_assert(idx < SubRattAttackTotalCount);
#ifdef FURI_DEBUG
    FURI_LOG_I(TAG, "Set index: %d, is_select_byte: %d", idx, is_select_byte);
#endif
    for(size_t i = 0; i < SubRattAttackTotalCount; i++) {
        instance->repeat_values[i] = repeats[i];
    }
    instance->is_select_byte = is_select_byte;
    instance->two_bytes = two_bytes;
    instance->key_from_file = key_from_file;
    instance->index = idx;

    if(!is_select_byte) {

        uint8_t brand = 0, type = 0, freq_idx = 0;
        subratt_protocol_find_brand_type(idx, &brand, &type, &freq_idx);
        instance->selected_brand = brand;
        instance->selected_type = type;

        instance->menu_level = SubRattMenuLevelBrand;
        instance->level_index = brand;
        instance->level_window_position = 0;
        subratt_main_view_update_window_position(instance);
    }

    instance->window_position = idx;
    if(!is_select_byte) {
        if(instance->window_position > 0) {
            instance->window_position -= 1;
        }
        if(instance->window_position >= (SubRattAttackTotalCount - ITEMS_ON_SCREEN)) {
            instance->window_position = (SubRattAttackTotalCount - ITEMS_ON_SCREEN);
        }
    }

    with_view_model(
        instance->view,
        SubRattMainViewModel * model,
        {
            model->index = instance->index;
            model->window_position = instance->window_position;
            model->key_from_file = instance->key_from_file;
            model->is_select_byte = instance->is_select_byte;
            model->two_bytes = instance->two_bytes;
            model->menu_level = instance->menu_level;
            model->selected_brand = instance->selected_brand;
            model->selected_type = instance->selected_type;
            model->level_index = instance->level_index;
            model->level_window_position = instance->level_window_position;

            for(size_t i = 0; i < SubRattAttackTotalCount; i++) {
                model->repeat_values[i] = repeats[i];
            }
        },
        true);
}

SubRattAttacks subratt_main_view_get_index(SubRattMainView* instance) {
    furi_assert(instance);

    return instance->index;
}

const uint8_t* subratt_main_view_get_repeats(SubRattMainView* instance) {
    furi_assert(instance);

    return instance->repeat_values;
}

bool subratt_main_view_get_two_bytes(SubRattMainView* instance) {
    furi_assert(instance);

    return instance->two_bytes;
}
