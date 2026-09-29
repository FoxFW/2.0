#pragma once

#include <stdint.h>
#include <core/common_defines.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Icon Icon;

uint16_t icon_get_width(const Icon* instance);

uint16_t icon_get_height(const Icon* instance);

FURI_DEPRECATED const uint8_t* icon_get_data(const Icon* instance);

uint32_t icon_get_frame_count(const Icon* instance);

const uint8_t* icon_get_frame_data(const Icon* instance, uint32_t frame);

#ifdef __cplusplus
}
#endif
