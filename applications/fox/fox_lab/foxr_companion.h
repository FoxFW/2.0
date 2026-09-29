#pragma once

#include "esp_at.h"
#include <gui/gui.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct FoxrCompanion FoxrCompanion;

#define FOXR_LOG_LINES     6
#define FOXR_LOG_LINE_MAX  40

FoxrCompanion* foxr_companion_alloc(EspAt* esp_at, Gui* gui, volatile bool* restart_pending_flag);

void foxr_companion_free(FoxrCompanion* companion);

void foxr_companion_dispatch(void* context, const EspAtMsg* msg);

uint32_t foxr_companion_log_version(FoxrCompanion* companion);
void foxr_companion_log_snapshot(
    FoxrCompanion* companion,
    char out[FOXR_LOG_LINES][FOXR_LOG_LINE_MAX],
    size_t* out_count);
