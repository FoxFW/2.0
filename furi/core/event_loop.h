#pragma once

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {

    FuriEventLoopEventIn = 0x00000001U,

    FuriEventLoopEventOut = 0x00000002U,

    FuriEventLoopEventMask = 0x00000003U,

    FuriEventLoopEventFlagEdge = 0x00000004U,

    FuriEventLoopEventFlagOnce = 0x00000008U,

    FuriEventLoopEventFlagMask = 0xFFFFFFFCU,

    FuriEventLoopEventReserved = UINT32_MAX,
} FuriEventLoopEvent;

typedef struct FuriEventLoop FuriEventLoop;

FuriEventLoop* furi_event_loop_alloc(void);

void furi_event_loop_free(FuriEventLoop* instance);

void furi_event_loop_run(FuriEventLoop* instance);

void furi_event_loop_stop(FuriEventLoop* instance);

typedef void (*FuriEventLoopTickCallback)(void* context);

void furi_event_loop_tick_set(
    FuriEventLoop* instance,
    uint32_t interval,
    FuriEventLoopTickCallback callback,
    void* context);

typedef void (*FuriEventLoopPendingCallback)(void* context);

void furi_event_loop_pend_callback(
    FuriEventLoop* instance,
    FuriEventLoopPendingCallback callback,
    void* context);

typedef void FuriEventLoopObject;

typedef void (*FuriEventLoopEventCallback)(FuriEventLoopObject* object, void* context);

typedef void (*FuriEventLoopThreadFlagsCallback)(void* context);

typedef struct FuriEventFlag FuriEventFlag;

void furi_event_loop_subscribe_event_flag(
    FuriEventLoop* instance,
    FuriEventFlag* event_flag,
    FuriEventLoopEvent event,
    FuriEventLoopEventCallback callback,
    void* context);

typedef struct FuriMessageQueue FuriMessageQueue;

void furi_event_loop_subscribe_message_queue(
    FuriEventLoop* instance,
    FuriMessageQueue* message_queue,
    FuriEventLoopEvent event,
    FuriEventLoopEventCallback callback,
    void* context);

typedef struct FuriStreamBuffer FuriStreamBuffer;

void furi_event_loop_subscribe_stream_buffer(
    FuriEventLoop* instance,
    FuriStreamBuffer* stream_buffer,
    FuriEventLoopEvent event,
    FuriEventLoopEventCallback callback,
    void* context);

typedef struct FuriSemaphore FuriSemaphore;

void furi_event_loop_subscribe_semaphore(
    FuriEventLoop* instance,
    FuriSemaphore* semaphore,
    FuriEventLoopEvent event,
    FuriEventLoopEventCallback callback,
    void* context);

typedef struct FuriMutex FuriMutex;

void furi_event_loop_subscribe_mutex(
    FuriEventLoop* instance,
    FuriMutex* mutex,
    FuriEventLoopEvent event,
    FuriEventLoopEventCallback callback,
    void* context);

void furi_event_loop_subscribe_thread_flags(
    FuriEventLoop* instance,
    FuriEventLoopThreadFlagsCallback callback,
    void* context);

void furi_event_loop_unsubscribe_thread_flags(FuriEventLoop* instance);

void furi_event_loop_unsubscribe(FuriEventLoop* instance, FuriEventLoopObject* object);

bool furi_event_loop_is_subscribed(FuriEventLoop* instance, FuriEventLoopObject* object);

static inline void
    furi_event_loop_maybe_unsubscribe(FuriEventLoop* instance, FuriEventLoopObject* object) {
    if(furi_event_loop_is_subscribed(instance, object))
        furi_event_loop_unsubscribe(instance, object);
}

#ifdef __cplusplus
}
#endif
