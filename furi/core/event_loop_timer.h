#pragma once

#include "event_loop.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FuriEventLoopTimerTypeOnce = 0,
    FuriEventLoopTimerTypePeriodic = 1,
} FuriEventLoopTimerType;

typedef void (*FuriEventLoopTimerCallback)(void* context);

typedef struct FuriEventLoopTimer FuriEventLoopTimer;

FuriEventLoopTimer* furi_event_loop_timer_alloc(
    FuriEventLoop* instance,
    FuriEventLoopTimerCallback callback,
    FuriEventLoopTimerType type,
    void* context);

void furi_event_loop_timer_free(FuriEventLoopTimer* timer);

void furi_event_loop_timer_start(FuriEventLoopTimer* timer, uint32_t interval);

void furi_event_loop_timer_restart(FuriEventLoopTimer* timer);

void furi_event_loop_timer_stop(FuriEventLoopTimer* timer);

uint32_t furi_event_loop_timer_get_remaining_time(const FuriEventLoopTimer* timer);

uint32_t furi_event_loop_timer_get_interval(const FuriEventLoopTimer* timer);

bool furi_event_loop_timer_is_running(const FuriEventLoopTimer* timer);

#ifdef __cplusplus
}
#endif
