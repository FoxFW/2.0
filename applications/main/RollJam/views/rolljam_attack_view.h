#pragma once

#include "../rolljam.h"

/*
 * Custom view for attack visualization.
 * Currently the app uses Widget and DialogEx for display.
 * This file is reserved for a future custom canvas-drawn view
 * (e.g., signal waveform display, animated jamming indicator).
 *
 * For now it provides a simple status draw function.
 */

typedef struct {
    const char* phase_text;
    const char* status_text;
    bool jamming;
    bool capturing;
    int signal_count;
} AttackViewState;

// Draw attack status on a canvas (for future custom View use)
void rolljam_attack_view_draw(Canvas* canvas, AttackViewState* state);
