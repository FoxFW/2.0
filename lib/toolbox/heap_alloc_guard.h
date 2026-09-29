#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void heap_alloc_guard_init(void);

void heap_alloc_guard_lock(void);

void heap_alloc_guard_unlock(void);

#ifdef __cplusplus
}
#endif
