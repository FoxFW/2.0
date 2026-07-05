// protocols/protocol_items.h
#pragma once

#include <lib/subghz/types.h>

#include "keeloq.h"

typedef enum {
    GDRProtocolRegistryFilterAM = 0,
    GDRProtocolRegistryFilterFM,
} GDRProtocolRegistryFilter;

GDRProtocolRegistryFilter gdr_get_protocol_registry_filter_for_preset(
    const uint8_t* preset_data,
    size_t preset_data_size);

const char*
    gdr_get_protocol_registry_filter_name(GDRProtocolRegistryFilter filter);

#ifdef ENABLE_TIMING_TUNER_SCENE
// Timing information for protocol analysis
typedef struct {
    const char* name;
    uint32_t te_short;
    uint32_t te_long;
    uint32_t te_delta;
    uint32_t min_count_bit;
} GDRProtocolTiming;

// Get timing info for a protocol by name (returns NULL if not found)
const GDRProtocolTiming* gdr_get_protocol_timing(const char* protocol_name);

// Get timing info by index (for iteration)
const GDRProtocolTiming* gdr_get_protocol_timing_by_index(size_t index);

// Get number of protocols with timing info
size_t gdr_get_protocol_timing_count(void);
#endif
