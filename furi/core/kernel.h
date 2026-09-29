#pragma once

#include <core/base.h>

#ifdef __cplusplus
extern "C" {
#endif

bool furi_kernel_is_irq_or_masked(void);

bool furi_kernel_is_running(void);

int32_t furi_kernel_lock(void);

int32_t furi_kernel_unlock(void);

int32_t furi_kernel_restore_lock(int32_t lock);

uint32_t furi_kernel_get_tick_frequency(void);

void furi_delay_tick(uint32_t ticks);

FuriStatus furi_delay_until_tick(uint32_t tick);

uint32_t furi_get_tick(void);

uint32_t furi_ms_to_ticks(uint32_t milliseconds);

void furi_delay_ms(uint32_t milliseconds);

void furi_delay_us(uint32_t microseconds);

#ifdef __cplusplus
}
#endif
