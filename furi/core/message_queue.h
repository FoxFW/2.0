#pragma once

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FuriMessageQueue FuriMessageQueue;

FuriMessageQueue* furi_message_queue_alloc(uint32_t msg_count, uint32_t msg_size);

void furi_message_queue_free(FuriMessageQueue* instance);

FuriStatus
    furi_message_queue_put(FuriMessageQueue* instance, const void* msg_ptr, uint32_t timeout);

FuriStatus furi_message_queue_get(FuriMessageQueue* instance, void* msg_ptr, uint32_t timeout);

uint32_t furi_message_queue_get_capacity(FuriMessageQueue* instance);

uint32_t furi_message_queue_get_message_size(FuriMessageQueue* instance);

uint32_t furi_message_queue_get_count(FuriMessageQueue* instance);

uint32_t furi_message_queue_get_space(FuriMessageQueue* instance);

FuriStatus furi_message_queue_reset(FuriMessageQueue* instance);

#ifdef __cplusplus
}
#endif
