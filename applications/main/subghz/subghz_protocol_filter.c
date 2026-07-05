/**
 * @file subghz_protocol_filter.c
 * @brief Persistent per-protocol ON/OFF filter for the SubGHz app.
 *
 * Storage format: a raw array of uint8_t, one byte per protocol index.
 *   0x01 = enabled (default)
 *   0x00 = disabled
 *
 * Using one byte per entry (rather than a packed bitmask) keeps the read
 * and write code simple and the file size negligible (<256 bytes).
 */

#include "subghz_protocol_filter.h"
#include <stdlib.h>
#include <string.h>
#include <storage/storage.h>
#include <furi.h>

#define TAG "SubGhzProtocolFilter"

/* ── internal struct ─────────────────────────────────────────────────────── */

struct SubGhzProtocolFilter {
    uint8_t enabled[SUBGHZ_FILTER_MAX_PROTOCOLS]; /* 0x00=off  0x01=on */
};

/* ── public API ──────────────────────────────────────────────────────────── */

SubGhzProtocolFilter* subghz_protocol_filter_alloc(void) {
    SubGhzProtocolFilter* inst = malloc(sizeof(SubGhzProtocolFilter));
    furi_assert(inst);
    /* All enabled by default */
    memset(inst->enabled, 0x01, sizeof(inst->enabled));
    return inst;
}

void subghz_protocol_filter_free(SubGhzProtocolFilter* instance) {
    furi_assert(instance);
    free(instance);
}

void subghz_protocol_filter_save(SubGhzProtocolFilter* instance) {
    /* No-op: filter is saved inside last_subghz.settings via subghz_save_all(). */
    (void)instance;
}

void subghz_protocol_filter_load(SubGhzProtocolFilter* instance) {
    /* No-op: filter is loaded from last_subghz.settings in subghz.c on startup. */
    (void)instance;
}


void subghz_protocol_filter_reset(SubGhzProtocolFilter* instance) {
    furi_assert(instance);
    memset(instance->enabled, 0x01, sizeof(instance->enabled));
}

bool subghz_protocol_filter_is_enabled(const SubGhzProtocolFilter* instance, size_t index) {
    furi_assert(instance);
    if(index >= SUBGHZ_FILTER_MAX_PROTOCOLS) return true; /* unknown → allow */
    return instance->enabled[index] != 0x00;
}

void subghz_protocol_filter_set_enabled(SubGhzProtocolFilter* instance,
                                         size_t index, bool enabled) {
    furi_assert(instance);
    if(index >= SUBGHZ_FILTER_MAX_PROTOCOLS) return;
    instance->enabled[index] = enabled ? 0x01 : 0x00;
}

size_t subghz_protocol_filter_enabled_count(const SubGhzProtocolFilter* instance,
                                              size_t total_count) {
    furi_assert(instance);
    size_t count = 0;
    size_t limit = total_count < SUBGHZ_FILTER_MAX_PROTOCOLS
                       ? total_count
                       : SUBGHZ_FILTER_MAX_PROTOCOLS;
    for(size_t i = 0; i < limit; i++) {
        if(instance->enabled[i] != 0x00) count++;
    }
    return count;
}


void subghz_protocol_filter_get_raw(const SubGhzProtocolFilter* instance,
                                     uint8_t* out, size_t count) {
    furi_assert(instance && out);
    size_t n = count < SUBGHZ_FILTER_MAX_PROTOCOLS ? count : SUBGHZ_FILTER_MAX_PROTOCOLS;
    memcpy(out, instance->enabled, n);
}

void subghz_protocol_filter_set_raw(SubGhzProtocolFilter* instance,
                                     const uint8_t* in, size_t count) {
    furi_assert(instance && in);
    size_t n = count < SUBGHZ_FILTER_MAX_PROTOCOLS ? count : SUBGHZ_FILTER_MAX_PROTOCOLS;
    memcpy(instance->enabled, in, n);
}
