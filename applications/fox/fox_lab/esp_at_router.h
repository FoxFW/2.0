#pragma once

#include "esp_at.h"
#include <stdbool.h>

typedef struct EspAtRouter EspAtRouter;

typedef void (*EspAtRouterFlprHandler)(void* context, const EspAtMsg* msg);

EspAtRouter*
    esp_at_router_alloc(EspAt* esp_at, EspAtRouterFlprHandler flpr_handler, void* context);
void esp_at_router_free(EspAtRouter* router);

bool esp_at_router_wait_line(EspAtRouter* router, EspAtMsg* out, uint32_t timeout_ms);
