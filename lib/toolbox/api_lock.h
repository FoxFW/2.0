#pragma once
#include <furi/furi.h>

typedef FuriEventFlag* FuriApiLock;

#define API_LOCK_EVENT (1U << 0)

#define api_lock_alloc_locked() furi_event_flag_alloc()

#define api_lock_wait_unlock(_lock) \
    furi_event_flag_wait(_lock, API_LOCK_EVENT, FuriFlagWaitAny, FuriWaitForever)

#define api_lock_free(_lock) furi_event_flag_free(_lock)

#define api_lock_unlock(_lock) furi_event_flag_set(_lock, API_LOCK_EVENT)

#define api_lock_wait_unlock_and_free(_lock) \
    api_lock_wait_unlock(_lock);             \
    api_lock_free(_lock);

#define api_lock_wait_unlock_with_timeout(_lock, _timeout) \
    furi_event_flag_wait(_lock, API_LOCK_EVENT, FuriFlagWaitAny, _timeout)

#define api_lock_is_locked(_lock) (!(furi_event_flag_get(_lock) & API_LOCK_EVENT))

#define api_lock_relock(_lock) furi_event_flag_clear(_lock, API_LOCK_EVENT)
