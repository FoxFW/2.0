#pragma once
#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct InfraredProgressView InfraredProgressView;

typedef enum {
    InfraredProgressViewInputStop,
    InfraredProgressViewInputPause,
    InfraredProgressViewInputResume,
    InfraredProgressViewInputPreviousSignal,
    InfraredProgressViewInputNextSignal,
    InfraredProgressViewInputSendSingle,
    InfraredProgressViewInputSave,
} InfraredProgressViewInput;

typedef void (*InfraredProgressViewInputCallback)(void* context, InfraredProgressViewInput event);

InfraredProgressView* infrared_progress_view_alloc(void);

void infrared_progress_view_free(InfraredProgressView* instance);

View* infrared_progress_view_get_view(InfraredProgressView* instance);

bool infrared_progress_view_set_progress(InfraredProgressView* instance, uint16_t progress);

void infrared_progress_view_set_progress_total(
    InfraredProgressView* instance,
    uint16_t progress_max);

void infrared_progress_view_set_paused(InfraredProgressView* instance, bool is_paused);

void infrared_progress_view_set_input_callback(
    InfraredProgressView* instance,
    InfraredProgressViewInputCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
