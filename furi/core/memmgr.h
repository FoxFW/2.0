#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FURI_MEMMGR_GUARD 1

size_t memmgr_get_free_heap(void);

size_t memmgr_get_total_heap(void);

size_t memmgr_get_minimum_free_heap(void);

void* aligned_malloc(size_t size, size_t alignment);

void aligned_free(void* p);

void* memmgr_alloc_from_pool(size_t size);

size_t memmgr_pool_get_free(void);

size_t memmgr_pool_get_max_block(void);

#ifdef __cplusplus
}
#endif
