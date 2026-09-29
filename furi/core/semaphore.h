#pragma once

#include "base.h"
#include "thread.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FuriSemaphore FuriSemaphore;

FuriSemaphore* furi_semaphore_alloc(uint32_t max_count, uint32_t initial_count);

void furi_semaphore_free(FuriSemaphore* instance);

FuriStatus furi_semaphore_acquire(FuriSemaphore* instance, uint32_t timeout);

FuriStatus furi_semaphore_release(FuriSemaphore* instance);

uint32_t furi_semaphore_get_count(FuriSemaphore* instance);

uint32_t furi_semaphore_get_space(FuriSemaphore* instance);

#ifdef __cplusplus
}
#endif
