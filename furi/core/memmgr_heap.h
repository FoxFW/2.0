#pragma once

#include <stdint.h>
#include <core/thread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEMMGR_HEAP_UNKNOWN 0xFFFFFFFF

void memmgr_heap_enable_thread_trace(FuriThreadId thread_id);

void memmgr_heap_disable_thread_trace(FuriThreadId thread_id);

size_t memmgr_heap_get_thread_memory(FuriThreadId thread_id);

size_t memmgr_heap_get_max_free_block(void);

void memmgr_heap_printf_free_blocks(void);

/** Callback used by memmgr_heap_write_fragmentation_map() to emit output.
 *
 * furi/core has no knowledge of the storage service (and must not gain a
 * hard link dependency on it -- this file is compiled into targets, such as
 * the updater, that may not link every application service), so the heap
 * map writer never touches a filesystem itself. It hands each finished
 * chunk of text to this callback, and the caller (typically a CLI command
 * that already has a `File*` open via the Storage API) writes it somewhere
 * durable, e.g. by calling storage_file_write() from inside the callback.
 *
 * @param[in] context  opaque pointer supplied by the caller (e.g. an open File*)
 * @param[in] data     bytes to write; not necessarily NUL-terminated
 * @param[in] length   number of bytes in `data`
 */
typedef void (*MemmgrHeapMapWriteCallback)(void* context, const char* data, size_t length);

/** Walk the heap's free list and emit a detailed, human-readable fragmentation
 * snapshot via `write_callback`: total/free/used byte counts, every free
 * block's (address, size), every used region's (address range, size), and a
 * compact bucketed visual map of the whole heap.
 *
 * This makes several passes over the free list (it is the same short list
 * memmgr_heap_printf_free_blocks() walks, so re-walking it is cheap) instead
 * of buffering the free block list in memory, so it stays safe to call
 * regardless of how many free blocks currently exist.
 *
 * A "used region" is only an address range not covered by any free block --
 * the allocator does not tag allocations with a caller, so this can say
 * WHERE memory is used and HOW MUCH, never WHAT is using it, and a single
 * reported region may actually contain several adjacent allocations.
 *
 * @param[in] write_callback  sink for the formatted text; called many times
 * @param[in] write_context   opaque pointer passed through to write_callback
 * @param[in] label           optional caller-supplied label, or NULL/"" for none
 */
void memmgr_heap_write_fragmentation_map(
    MemmgrHeapMapWriteCallback write_callback,
    void* write_context,
    const char* label);

#ifdef __cplusplus
}
#endif
