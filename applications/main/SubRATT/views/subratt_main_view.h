#pragma once

#include "../subratt_custom_event.h"
#include "../subratt_protocols.h"
#include <gui/view.h>
#include <input/input.h>
#include <gui/elements.h>

typedef enum {
    SubRattMenuLevelBrand,
    SubRattMenuLevelType,
    SubRattMenuLevelFreq,
} SubRattMenuLevel;

typedef void (*SubRattMainViewCallback)(SubRattCustomEvent event, void* context);
typedef struct SubRattMainView SubRattMainView;

void subratt_main_view_set_callback(
    SubRattMainView* instance,
    SubRattMainViewCallback callback,
    void* context);

SubRattMainView* subratt_main_view_alloc();

void subratt_main_view_free(SubRattMainView* instance);

View* subratt_main_view_get_view(SubRattMainView* instance);

void subratt_main_view_set_index(
    SubRattMainView* instance,
    uint8_t idx,
    const uint8_t* repeats,
    bool is_select_byte,
    bool two_bytes,
    uint64_t key_from_file);

SubRattAttacks subratt_main_view_get_index(SubRattMainView* instance);

const uint8_t* subratt_main_view_get_repeats(SubRattMainView* instance);

bool subratt_main_view_get_two_bytes(SubRattMainView* instance);

void subratt_attack_view_enter(void* context);

void subratt_attack_view_exit(void* context);

bool subratt_attack_view_input(InputEvent* event, void* context);

void subratt_attack_view_draw(Canvas* canvas, void* context);
