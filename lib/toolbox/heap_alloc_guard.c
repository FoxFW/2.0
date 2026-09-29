#include "heap_alloc_guard.h"

#include <furi.h>

static FuriMutex* heap_alloc_guard_mutex = NULL;

static FuriMutex* heap_alloc_guard_get_mutex(void) {
    if(!heap_alloc_guard_mutex) {
        furi_kernel_lock();
        if(!heap_alloc_guard_mutex) {
            heap_alloc_guard_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
        }
        furi_kernel_unlock();
    }
    return heap_alloc_guard_mutex;
}

void heap_alloc_guard_init(void) {
    heap_alloc_guard_get_mutex();
}

void heap_alloc_guard_lock(void) {
    furi_mutex_acquire(heap_alloc_guard_get_mutex(), FuriWaitForever);
}

void heap_alloc_guard_unlock(void) {
    furi_mutex_release(heap_alloc_guard_mutex);
}
