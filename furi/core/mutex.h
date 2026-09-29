#pragma once

#include "base.h"
#include "thread.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FuriMutexTypeNormal,
    FuriMutexTypeRecursive,
} FuriMutexType;

typedef struct FuriMutex FuriMutex;

FuriMutex* furi_mutex_alloc(FuriMutexType type);

void furi_mutex_free(FuriMutex* instance);

FuriStatus furi_mutex_acquire(FuriMutex* instance, uint32_t timeout);

FuriStatus furi_mutex_release(FuriMutex* instance);

FuriThreadId furi_mutex_get_owner(FuriMutex* instance);

#ifdef __cplusplus
}
#endif
