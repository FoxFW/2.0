#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file subghz_protocol_filter.h
 *
 * Persistent ON/OFF filter for each entry in the SubGHz protocol registry.
 *
 * The filter is a bitmask array: one bit per protocol index.  A protocol
 * whose bit is SET is ENABLED (the default).  A protocol whose bit is
 * CLEARED is disabled and will be skipped during Read decoding.
 *
 * State is saved to /ext/subghz/protocol_filter.save as a plain binary
 * blob.  Missing or corrupt files are silently treated as "all enabled".
 *
 * Maximum protocols supported: SUBGHZ_FILTER_MAX_PROTOCOLS (256).
 * Flipper firmware currently has ~62; the buffer has room to grow.
 */

#define SUBGHZ_FILTER_MAX_PROTOCOLS 256u
/* SUBGHZ_FILTER_SAVE_PATH removed — filter is now saved inside last_subghz.settings */

typedef struct SubGhzProtocolFilter SubGhzProtocolFilter;

/** Allocate and initialise the filter (all protocols enabled). */
SubGhzProtocolFilter* subghz_protocol_filter_alloc(void);

/** Free the filter instance. */
void subghz_protocol_filter_free(SubGhzProtocolFilter* instance);

/** Load filter state from SD card.  Silent no-op if the file is missing. */
void subghz_protocol_filter_load(SubGhzProtocolFilter* instance);

/** Save current filter state to SD card. */
void subghz_protocol_filter_save(SubGhzProtocolFilter* instance);

/** Returns true if the protocol at the given registry index is enabled. */
bool subghz_protocol_filter_is_enabled(const SubGhzProtocolFilter* instance, size_t index);

/** Enable or disable the protocol at the given registry index. */
void subghz_protocol_filter_set_enabled(SubGhzProtocolFilter* instance, size_t index, bool enabled);

/** Enable all protocols (reset to default). */
void subghz_protocol_filter_reset(SubGhzProtocolFilter* instance);

/** Returns how many protocols are currently enabled. */
size_t subghz_protocol_filter_enabled_count(const SubGhzProtocolFilter* instance,
                                             size_t total_count);

/** Copy internal enabled[] array out (for saving via last_settings). */
void subghz_protocol_filter_get_raw(const SubGhzProtocolFilter* instance,
                                     uint8_t* out, size_t count);

/** Restore internal enabled[] array from raw data (after loading last_settings). */
void subghz_protocol_filter_set_raw(SubGhzProtocolFilter* instance,
                                     const uint8_t* in, size_t count);

#ifdef __cplusplus
}
#endif
