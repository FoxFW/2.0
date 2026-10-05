#pragma once

#include <gui/view.h>
#include "../helpers/ir_sense.h"

typedef struct SweepView SweepView;

/* What the sweep screen asks the scene to do. Routed through one callback
 * rather than a setter per key, so adding a key does not add plumbing. */
typedef enum {
    SweepViewEventResetPeak, // OK
    SweepViewEventRenull, // long OK
    SweepViewEventSensUp, // Right — more gain
    SweepViewEventSensDown, // Left — less gain
} SweepViewEvent;

typedef void (*SweepViewCallback)(void* context, SweepViewEvent event);

SweepView* sweep_view_alloc(void);
void sweep_view_free(SweepView* v);
View* sweep_view_get_view(SweepView* v);

void sweep_view_set_callback(SweepView* v, SweepViewCallback cb, void* context);

/* `sensitivity` is the app's setting index (0 High, 1 Medium, 2 Low); the view
 * only displays it, the scene owns it. */
void sweep_view_update(SweepView* v, const IrStats* stats, uint8_t sensitivity);
void sweep_view_tick(SweepView* v);

/* Drop animation and cached readings, so re-entering the sweep does not show a
 * frame of the last one. Takes the live sensitivity because zeroing the model
 * would otherwise label the first frame HIGH whatever the real setting is. */
void sweep_view_reset(SweepView* v, uint8_t sensitivity);
