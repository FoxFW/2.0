#include "subbrute_main_view.h"
#include "../subbrute_i.h"

#include <input/input.h>
#include <gui/elements.h>

#define STATUS_BAR_Y_SHIFT 14
#define TAG "SubBruteMainView"

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

struct SubBruteMainView {
    View* view;
    SubBruteMainViewCallback callback;
    void* context;
    uint8_t index;
    bool is_select_byte;
    bool two_bytes;
    uint64_t key_from_file;
    uint8_t repeat_values[SubBruteAttackTotalCount];
    uint8_t window_position;

    // Hierarchical menu state
    SubBruteMenuLevel menu_level;
    uint8_t selected_brand;
    uint8_t selected_type;
    uint8_t level_index;
    uint8_t level_window_position;
};

typedef struct {
    uint8_t index;
    uint8_t repeat_values[SubBruteAttackTotalCount];
    uint8_t window_position;
    bool is_select_byte;
    bool two_bytes;
    uint64_t key_from_file;

    // Hierarchical menu state
    SubBruteMenuLevel menu_level;
    uint8_t selected_brand;
    uint8_t selected_type;
    uint8_t level_index;
    uint8_t level_window_position;
} SubBruteMainViewModel;

void subbrute_main_view_set_callback(
    SubBruteMainView* instance,
    SubBruteMainViewCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);

    instance->callback = callback;
    instance->context = context;
}

void subbrute_main_view_center_displayed_key(
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

        // For two bytes we need to select prev location
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

                // Redraw prev element with white
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

    // Return normal color
    canvas_set_color(canvas, ColorBlack);
}

void subbrute_main_view_draw_is_byte_selected(Canvas* canvas, SubBruteMainViewModel* model) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, 17, AlignCenter, AlignTop, "Please select values to calc:");

    subbrute_main_view_center_displayed_key(
        canvas, model->key_from_file, model->index, model->two_bytes);

    elements_button_center(canvas, "Select");
    if(model->index > 0) {
        elements_button_left(canvas, " ");
    }
    if(model->index < 7) {
        elements_button_right(canvas, " ");
    }

    // Switch to another mode
    if(model->two_bytes) {
        elements_button_up(canvas, "One byte");
    } else {
        elements_button_up(canvas, "Two bytes");
    }
}

static uint8_t subbrute_main_view_get_level_count(SubBruteMainViewModel* model) {
    if(model->menu_level == SubBruteMenuLevelBrand) {
        return SubBruteBrandCount;
    } else if(model->menu_level == SubBruteMenuLevelType) {
        return subbrute_brand_group(model->selected_brand)->type_count;
    } else {
        return subbrute_brand_group(model->selected_brand)
            ->types[model->selected_type]
            .attack_count;
    }
}

static const char* subbrute_main_view_get_item_name(SubBruteMainViewModel* model, uint8_t pos) {
    if(model->menu_level == SubBruteMenuLevelBrand) {
        return subbrute_brand_group(pos)->name;
    } else if(model->menu_level == SubBruteMenuLevelType) {
        return subbrute_brand_group(model->selected_brand)->types[pos].name;
    } else {
        SubBruteAttacks attack = subbrute_brand_group(model->selected_brand)
                                     ->types[model->selected_type]
                                     .attacks[pos];
        return subbrute_protocol_freq_name(attack);
    }
}

static const char* subbrute_main_view_get_title(SubBruteMainViewModel* model) {
    if(model->menu_level == SubBruteMenuLevelBrand) {
        return SUB_BRUTE_FORCER_VERSION;
    } else if(model->menu_level == SubBruteMenuLevelType) {
        return subbrute_brand_group(model->selected_brand)->name;
    } else {
        const SubBruteBrandGroup* bg = subbrute_brand_group(model->selected_brand);
        if(bg->type_count == 1) {
            return bg->name;
        }
        // Show "Brand > Type" for multi-type brands
        static char title_buf[32];
        snprintf(title_buf, sizeof(title_buf), "%s > %s", bg->name, bg->types[model->selected_type].name);
        return title_buf;
    }
}

void subbrute_main_view_draw_is_ordinary_selected(Canvas* canvas, SubBruteMainViewModel* model) {
    uint16_t screen_width = canvas_width(canvas);
    uint16_t screen_height = canvas_height(canvas);

    // Title bar
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_box(canvas, 0, 0, canvas_width(canvas), STATUS_BAR_Y_SHIFT);
    canvas_invert_color(canvas);
    canvas_draw_str_aligned(
        canvas, 64, 3, AlignCenter, AlignTop, subbrute_main_view_get_title(model));
    canvas_invert_color(canvas);

    // Menu items
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    const uint8_t item_height = 16;
    const uint8_t string_height_offset = 9;

    uint8_t total_items = subbrute_main_view_get_level_count(model);

    for(uint8_t position = 0; position < total_items; ++position) {
        uint8_t item_position = position - model->level_window_position;

        if(item_position < ITEMS_ON_SCREEN) {
            const char* item_name = subbrute_main_view_get_item_name(model, position);

            if(model->level_index == position) {
                canvas_draw_str_aligned(
                    canvas,
                    3,
                    string_height_offset + (item_position * item_height) + STATUS_BAR_Y_SHIFT,
                    AlignLeft,
                    AlignCenter,
                    item_name);

                elements_frame(
                    canvas, 1, 1 + (item_position * item_height) + STATUS_BAR_Y_SHIFT, 124, 15);
            } else {
                canvas_draw_str_aligned(
                    canvas,
                    4,
                    string_height_offset + (item_position * item_height) + STATUS_BAR_Y_SHIFT,
                    AlignLeft,
                    AlignCenter,
                    item_name);
            }

            // Show repeat count only at freq level
            if(model->menu_level == SubBruteMenuLevelFreq) {
                SubBruteAttacks attack = subbrute_brand_group(model->selected_brand)
                                             ->types[model->selected_type]
                                             .attacks[position];
                uint8_t current_repeat_count = model->repeat_values[attack];
                uint8_t min_repeat_count = subbrute_protocol_repeats_count(attack);

                if(current_repeat_count > min_repeat_count) {
#ifdef FW_ORIGIN_Official
                    canvas_set_font(canvas, FontSecondary);
#else
                    canvas_set_font(canvas, FontBatteryPercent);
#endif
                    char buffer[10] = {0};
                    snprintf(buffer, sizeof(buffer), "x%d", current_repeat_count);
                    uint8_t temp_x_offset_repeats =
                        current_repeat_count <= SUBBRUTE_PROTOCOL_MAX_REPEATS ? 15 : 18;

                    canvas_draw_str_aligned(
                        canvas,
                        screen_width - temp_x_offset_repeats,
                        string_height_offset + (item_position * item_height) + STATUS_BAR_Y_SHIFT,
                        AlignLeft,
                        AlignCenter,
                        buffer);
                    canvas_set_font(canvas, FontSecondary);
                }
            }
        }
    }

    elements_scrollbar_pos(
        canvas,
        screen_width,
        STATUS_BAR_Y_SHIFT + 2,
        screen_height - STATUS_BAR_Y_SHIFT,
        model->level_index,
        total_items);
}

void subbrute_main_view_draw(Canvas* canvas, SubBruteMainViewModel* model) {
    if(model->is_select_byte) {
        subbrute_main_view_draw_is_byte_selected(canvas, model);
    } else {
        subbrute_main_view_draw_is_ordinary_selected(canvas, model);
    }
}

bool subbrute_main_view_input_file_protocol(InputEvent* event, SubBruteMainView* instance) {
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
        // Because index is changing
        if(instance->two_bytes && instance->index < 7) {
            instance->index++;
        }

        updated = true;
    } else if(event->key == InputKeyOk) {
      instance->callback(SubBruteCustomEventTypeIndexSelected,
                         instance->context);

        updated = true;
    }
    return updated;
}

static void subbrute_main_view_update_window_position(SubBruteMainView* instance) {
    uint8_t total = 0;
    if(instance->menu_level == SubBruteMenuLevelBrand) {
        total = SubBruteBrandCount;
    } else if(instance->menu_level == SubBruteMenuLevelType) {
        total = subbrute_brand_group(instance->selected_brand)->type_count;
    } else {
        total = subbrute_brand_group(instance->selected_brand)
                    ->types[instance->selected_type]
                    .attack_count;
    }

    instance->level_window_position = instance->level_index;
    if(instance->level_window_position > 0) {
        instance->level_window_position -= 1;
    }
    if(total <= ITEMS_ON_SCREEN) {
        instance->level_window_position = 0;
    } else if(instance->level_window_position >= (total - ITEMS_ON_SCREEN)) {
        instance->level_window_position = (total - ITEMS_ON_SCREEN);
    }
}

static SubBruteAttacks subbrute_main_view_get_current_attack(SubBruteMainView* instance) {
    return subbrute_brand_group(instance->selected_brand)
        ->types[instance->selected_type]
        .attacks[instance->level_index];
}

bool subbrute_main_view_input_ordinary_protocol(
    InputEvent* event,
    SubBruteMainView* instance,
    bool is_short) {

    uint8_t total = 0;
    if(instance->menu_level == SubBruteMenuLevelBrand) {
        total = SubBruteBrandCount;
    } else if(instance->menu_level == SubBruteMenuLevelType) {
        total = subbrute_brand_group(instance->selected_brand)->type_count;
    } else {
        total = subbrute_brand_group(instance->selected_brand)
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
        if(instance->menu_level == SubBruteMenuLevelBrand) {
            if(instance->level_index == SubBruteBrandLoadFile) {
                // Load file - direct action
                instance->callback(SubBruteCustomEventTypeLoadFile, instance->context);
            } else {
                instance->selected_brand = instance->level_index;
                const SubBruteBrandGroup* bg = subbrute_brand_group(instance->selected_brand);
                if(bg->type_count == 1) {
                    // Skip type level
                    instance->selected_type = 0;
                    instance->menu_level = SubBruteMenuLevelFreq;
                } else {
                    instance->menu_level = SubBruteMenuLevelType;
                }
                instance->level_index = 0;
                instance->level_window_position = 0;
            }
            updated = true;
        } else if(instance->menu_level == SubBruteMenuLevelType) {
            instance->selected_type = instance->level_index;
            instance->menu_level = SubBruteMenuLevelFreq;
            instance->level_index = 0;
            instance->level_window_position = 0;
            updated = true;
        } else {
            // Freq level - select the attack
            SubBruteAttacks attack = subbrute_main_view_get_current_attack(instance);
            instance->index = attack;
            instance->callback(SubBruteCustomEventTypeMenuSelected, instance->context);
            updated = true;
        }
    } else if(event->key == InputKeyLeft && is_short) {
        if(instance->menu_level == SubBruteMenuLevelFreq) {
            SubBruteAttacks attack = subbrute_main_view_get_current_attack(instance);
            uint8_t min_repeats = subbrute_protocol_repeats_count(attack);
            uint8_t max_repeats = min_repeats * 3;
            uint8_t current_repeats = instance->repeat_values[attack];
            instance->repeat_values[attack] =
                CLAMP(current_repeats - 1, max_repeats, min_repeats);
            updated = true;
        }
    } else if(event->key == InputKeyRight && is_short) {
        if(instance->menu_level == SubBruteMenuLevelFreq) {
            SubBruteAttacks attack = subbrute_main_view_get_current_attack(instance);
            uint8_t min_repeats = subbrute_protocol_repeats_count(attack);
            uint8_t max_repeats = min_repeats * 3;
            uint8_t current_repeats = instance->repeat_values[attack];
            instance->repeat_values[attack] =
                CLAMP(current_repeats + 1, max_repeats, min_repeats);
            updated = true;
        }
    }

    if(updated) {
        subbrute_main_view_update_window_position(instance);
    }

    return updated;
}

bool subbrute_main_view_input(InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);

    SubBruteMainView* instance = (SubBruteMainView*)context;

    if(event->key == InputKeyBack && event->type == InputTypeShort) {
#ifdef FURI_DEBUG
        FURI_LOG_I(TAG, "InputKey: BACK");
#endif
        if(!instance->is_select_byte && instance->menu_level != SubBruteMenuLevelBrand) {
            // Go up one level
            if(instance->menu_level == SubBruteMenuLevelFreq) {
                const SubBruteBrandGroup* bg = subbrute_brand_group(instance->selected_brand);
                if(bg->type_count == 1) {
                    // Type was skipped, go back to brand
                    instance->menu_level = SubBruteMenuLevelBrand;
                    instance->level_index = instance->selected_brand;
                } else {
                    instance->menu_level = SubBruteMenuLevelType;
                    instance->level_index = instance->selected_type;
                }
            } else if(instance->menu_level == SubBruteMenuLevelType) {
                instance->menu_level = SubBruteMenuLevelBrand;
                instance->level_index = instance->selected_brand;
            }
            subbrute_main_view_update_window_position(instance);

            with_view_model(
                instance->view,
                SubBruteMainViewModel * model,
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
        instance->callback(SubBruteCustomEventTypeBackPressed, instance->context);
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
            updated = subbrute_main_view_input_file_protocol(event, instance);
        }
    } else {
        updated = subbrute_main_view_input_ordinary_protocol(event, instance, is_short);
    }

    if(updated) {
        with_view_model(
            instance->view,
            SubBruteMainViewModel * model,
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
                // Sync all repeat values since attack index may change
                for(size_t i = 0; i < SubBruteAttackTotalCount; i++) {
                    model->repeat_values[i] = instance->repeat_values[i];
                }
            },
            true);
    }

    return updated;
}

void subbrute_main_view_enter(void* context) {
    furi_assert(context);
}

void subbrute_main_view_exit(void* context) {
    furi_assert(context);
}

SubBruteMainView* subbrute_main_view_alloc() {
    SubBruteMainView* instance = malloc(sizeof(SubBruteMainView));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(SubBruteMainViewModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, (ViewDrawCallback)subbrute_main_view_draw);
    view_set_input_callback(instance->view, subbrute_main_view_input);
    view_set_enter_callback(instance->view, subbrute_main_view_enter);
    view_set_exit_callback(instance->view, subbrute_main_view_exit);

    instance->index = 0;
    instance->window_position = 0;
    instance->key_from_file = 0;
    instance->is_select_byte = false;
    instance->two_bytes = false;
    instance->menu_level = SubBruteMenuLevelBrand;
    instance->selected_brand = 0;
    instance->selected_type = 0;
    instance->level_index = 0;
    instance->level_window_position = 0;

    with_view_model(
        instance->view,
        SubBruteMainViewModel * model,
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
        },
        true);

    return instance;
}

void subbrute_main_view_free(SubBruteMainView* instance) {
    furi_assert(instance);

    view_free(instance->view);
    free(instance);
}

View* subbrute_main_view_get_view(SubBruteMainView* instance) {
    furi_assert(instance);

    return instance->view;
}

void subbrute_main_view_set_index(
    SubBruteMainView* instance,
    uint8_t idx,
    const uint8_t* repeats,
    bool is_select_byte,
    bool two_bytes,
    uint64_t key_from_file) {

    furi_assert(instance);
    furi_assert(idx < SubBruteAttackTotalCount);
#ifdef FURI_DEBUG
    FURI_LOG_I(TAG, "Set index: %d, is_select_byte: %d", idx, is_select_byte);
#endif
    for(size_t i = 0; i < SubBruteAttackTotalCount; i++) {
        instance->repeat_values[i] = repeats[i];
    }
    instance->is_select_byte = is_select_byte;
    instance->two_bytes = two_bytes;
    instance->key_from_file = key_from_file;
    instance->index = idx;

    if(!is_select_byte) {
        // Restore hierarchical position from the saved attack index
        uint8_t brand = 0, type = 0, freq_idx = 0;
        subbrute_protocol_find_brand_type(idx, &brand, &type, &freq_idx);
        instance->selected_brand = brand;
        instance->selected_type = type;
        // Start at brand level so user sees the top menu
        instance->menu_level = SubBruteMenuLevelBrand;
        instance->level_index = brand;
        instance->level_window_position = 0;
        subbrute_main_view_update_window_position(instance);
    }

    // Legacy window_position for file byte select mode
    instance->window_position = idx;
    if(!is_select_byte) {
        if(instance->window_position > 0) {
            instance->window_position -= 1;
        }
        if(instance->window_position >= (SubBruteAttackTotalCount - ITEMS_ON_SCREEN)) {
            instance->window_position = (SubBruteAttackTotalCount - ITEMS_ON_SCREEN);
        }
    }

    with_view_model(
        instance->view,
        SubBruteMainViewModel * model,
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

            for(size_t i = 0; i < SubBruteAttackTotalCount; i++) {
                model->repeat_values[i] = repeats[i];
            }
        },
        true);
}

SubBruteAttacks subbrute_main_view_get_index(SubBruteMainView* instance) {
    furi_assert(instance);

    return instance->index;
}

const uint8_t* subbrute_main_view_get_repeats(SubBruteMainView* instance) {
    furi_assert(instance);

    return instance->repeat_values;
}

bool subbrute_main_view_get_two_bytes(SubBruteMainView* instance) {
    furi_assert(instance);

    return instance->two_bytes;
}
