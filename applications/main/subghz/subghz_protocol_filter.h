#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SUBGHZ_FILTER_MAX_PROTOCOLS 256u

typedef struct SubGhzProtocolFilter SubGhzProtocolFilter;

SubGhzProtocolFilter* subghz_protocol_filter_alloc(void);

void subghz_protocol_filter_free(SubGhzProtocolFilter* instance);

void subghz_protocol_filter_load(SubGhzProtocolFilter* instance);

void subghz_protocol_filter_save(SubGhzProtocolFilter* instance);

bool subghz_protocol_filter_is_enabled(const SubGhzProtocolFilter* instance, size_t index);

void subghz_protocol_filter_set_enabled(SubGhzProtocolFilter* instance, size_t index, bool enabled);

void subghz_protocol_filter_reset(SubGhzProtocolFilter* instance);

size_t subghz_protocol_filter_enabled_count(const SubGhzProtocolFilter* instance,
                                             size_t total_count);

void subghz_protocol_filter_get_raw(const SubGhzProtocolFilter* instance,
                                     uint8_t* out, size_t count);

void subghz_protocol_filter_set_raw(SubGhzProtocolFilter* instance,
                                     const uint8_t* in, size_t count);

#ifdef __cplusplus
}
#endif
