#pragma once
/* FoxFW2.0's custom-button-remap system (lib/subghz/blocks/custom_btn.h/
 * .c). Gated by SUBGHZ_GARAGE_HAS_CUSTOM_BTN, a cdefine set only on
 * FoxFW2.0's own native build (see application.fam; stripped for the
 * _COMPATIBLE cross-fork variant by sync_apps_from_foxfw.ps1) - NOT
 * __has_include. An earlier version of this file (task #102) ported an
 * early snapshot of custom_btn.h to ARF/Unleashed/Momentum's own lib/
 * subghz/blocks/ trees so __has_include could find a real one there, but
 * FoxFW2.0's copy has grown new functions since (multi-page support,
 * set_original/set_max moving from custom_btn_i.h to the public header)
 * that those one-time snapshots were never updated to match - confirmed
 * by direct inspection, each fork's ported copy is a different, stale
 * subset. Trusting "the header exists" as "the header is current" was
 * the actual bug; this always uses the safe fallback below on every
 * cross-fork build instead, regardless of what that fork's own copy
 * happens to contain, so this can't keep drifting out of sync again. */

#ifdef SUBGHZ_GARAGE_HAS_CUSTOM_BTN
#include <lib/subghz/blocks/custom_btn.h>
#else

/* Safe defaults matching how these protocols behaved before this feature
 * existed: no remap ever active, every decoder just uses its own
 * standard button. */

#include <stdbool.h>
#include <stdint.h>

#define SUBGHZ_CUSTOM_BTN_OK    (0U)
#define SUBGHZ_CUSTOM_BTN_UP    (1U)
#define SUBGHZ_CUSTOM_BTN_DOWN  (2U)
#define SUBGHZ_CUSTOM_BTN_LEFT  (3U)
#define SUBGHZ_CUSTOM_BTN_RIGHT (4U)

static inline bool subghz_custom_btn_set(uint8_t btn_id) {
    (void)btn_id;
    return false;
}

static inline uint8_t subghz_custom_btn_get(void) {
    return SUBGHZ_CUSTOM_BTN_OK;
}

static inline uint8_t subghz_custom_btn_get_original(void) {
    return SUBGHZ_CUSTOM_BTN_OK;
}

static inline void subghz_custom_btn_set_original(uint8_t btn_code) {
    (void)btn_code;
}

static inline void subghz_custom_btn_set_max(uint8_t b) {
    (void)b;
}

static inline void subghz_custom_btns_reset(void) {
}

static inline bool subghz_custom_btn_is_allowed(void) {
    return false;
}

static inline void subghz_custom_btn_set_long(bool v) {
    (void)v;
}

static inline bool subghz_custom_btn_get_long(void) {
    return false;
}

static inline void subghz_custom_btn_set_pages(bool enabled) {
    (void)enabled;
}

static inline bool subghz_custom_btn_has_pages(void) {
    return false;
}

static inline void subghz_custom_btn_set_page(uint8_t page) {
    (void)page;
}

static inline uint8_t subghz_custom_btn_get_page(void) {
    return 0;
}

static inline void subghz_custom_btn_set_max_pages(uint8_t n) {
    (void)n;
}

static inline uint8_t subghz_custom_btn_get_max_pages(void) {
    return 0;
}

static inline const char*
    subghz_custom_btn_get_label_for_proto(const char* proto_name, uint8_t btn_dir) {
    (void)proto_name;
    (void)btn_dir;
    return NULL;
}

#endif
