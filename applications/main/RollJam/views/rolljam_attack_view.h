#pragma once

#include "../rolljam.h"

typedef struct {
    const char* phase_text;
    const char* status_text;
    bool jamming;
    bool capturing;
    int signal_count;
} AttackViewState;

void rolljam_attack_view_draw(Canvas* canvas, AttackViewState* state);
