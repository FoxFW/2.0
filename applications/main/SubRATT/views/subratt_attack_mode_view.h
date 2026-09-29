#pragma once

#include <gui/view.h>

typedef struct SubRattAttackModeView SubRattAttackModeView;

typedef void (*SubRattAttackModeViewCallback)(void* context, uint32_t index);

SubRattAttackModeView* subratt_attack_mode_view_alloc(void);

void subratt_attack_mode_view_free(SubRattAttackModeView* instance);

View* subratt_attack_mode_view_get_view(SubRattAttackModeView* instance);

void subratt_attack_mode_view_set_callback(
    SubRattAttackModeView* instance,
    SubRattAttackModeViewCallback callback,
    void* context);

void subratt_attack_mode_view_set_title(SubRattAttackModeView* instance, const char* title);
