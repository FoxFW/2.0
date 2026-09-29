#pragma once

#include "../subratt_custom_event.h"
#include <gui/view.h>
#include <input/input.h>
#include <gui/elements.h>

typedef void (*SubRattAttackViewCallback)(SubRattCustomEvent event, void* context);
typedef struct SubRattAttackView SubRattAttackView;

void subratt_attack_view_set_callback(
    SubRattAttackView* instance,
    SubRattAttackViewCallback callback,
    void* context);

SubRattAttackView* subratt_attack_view_alloc();

void subratt_attack_view_free(SubRattAttackView* instance);

View* subratt_attack_view_get_view(SubRattAttackView* instance);

void subratt_attack_view_set_current_step(SubRattAttackView* instance, uint64_t current_step);

void subratt_attack_view_init_values(
    SubRattAttackView* instance,
    uint8_t index,
    uint64_t max_value,
    uint64_t current_step,
    bool is_attacking,
    uint8_t extra_repeats);
