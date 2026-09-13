#pragma once
/* FoxFW2.0's SubGhzBlockGenericGlobal addition to lib/subghz/blocks/
 * generic.h (RX-decoded counter/button metadata for the Signal Settings
 * editor, Counter/Button BruteForce override flags, and the endless-TX-
 * hold flag used by protocol yield functions). ARF/Unleashed/Momentum each
 * carry the same addition in their own, independently-maintained copy of
 * that shared header - not a port of FoxFW2.0's copy, so unlike
 * subghz_custom_btn_compat.h there's no drift risk in using it directly.
 * Stock's copy has no equivalent at all.
 *
 * Gated by SUBGHZ_GARAGE_HAS_GENERIC_GLOBAL, set per-fork (not uniformly,
 * unlike SUBGHZ_GARAGE_HAS_LIB_EXTENSIONS) by build_all_firmwares.ps1/
 * build_changed_apps.ps1 after copying subghz_garage_COMPATIBLE into each
 * fork's own applications_user - present for ARF/Unleashed/Momentum,
 * stripped for Stock. */

#ifdef SUBGHZ_GARAGE_HAS_GENERIC_GLOBAL

#include <lib/subghz/blocks/generic.h>

static inline void subghz_garage_counter_override_set(uint32_t counter) {
    subghz_block_generic_global_counter_override_set(counter);
}

static inline void subghz_garage_button_override_set(uint8_t button) {
    subghz_block_generic_global_button_override_set(button);
}

static inline bool subghz_garage_counter_override_get(uint32_t* counter) {
    return subghz_block_generic_global_counter_override_get(counter);
}

static inline bool subghz_garage_button_override_get(uint8_t* button) {
    return subghz_block_generic_global_button_override_get(button);
}

#else

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t current_cnt;
    uint32_t new_cnt;
    bool cnt_need_override;
    uint8_t cnt_length_bit;
    bool cnt_is_available;

    uint8_t current_btn;
    uint8_t new_btn;
    bool btn_need_override;
    uint8_t btn_length_bit;
    bool btn_is_available;

    bool endless_tx;
} SubGhzGarageGenericGlobalCompat;

/* Each translation unit gets its own local instance - not shared across
 * this app's separately-loaded plugins, same call as
 * subghz_custom_btn_compat.h. RX-decoded metadata never reaches the Signal
 * Settings editor, counter/button override flags never reach an encoder,
 * and endless_tx always reads false (hold-to-repeat TX degrades to a
 * normal bounded-repeat send). Every other Garage feature is unaffected.
 * __attribute__((unused)): some consuming files only call the override-set
 * wrappers below (themselves no-ops on this branch) and never touch the
 * struct directly, which is otherwise an unused-variable error under
 * -Werror. */
static SubGhzGarageGenericGlobalCompat subghz_block_generic_global __attribute__((unused));

static inline void subghz_garage_counter_override_set(uint32_t counter) {
    UNUSED(counter);
    /* Counter BruteForce needs this to actually vary the transmitted
     * counter - without it, Counter BruteForce compiles and runs but
     * resends the same counter every time; every other Garage feature is
     * unaffected. */
}

static inline void subghz_garage_button_override_set(uint8_t button) {
    UNUSED(button);
    /* Same fallback as subghz_garage_counter_override_set() above, for
     * Button BruteForce / the Signal Settings edit-button flow. */
}

static inline bool subghz_garage_counter_override_get(uint32_t* counter) {
    UNUSED(counter);
    /* Always "not overridden" - matches the real function's meaning given
     * counter_override_set() above is always a no-op on this branch, so
     * there is never an override pending to report. */
    return false;
}

static inline bool subghz_garage_button_override_get(uint8_t* button) {
    UNUSED(button);
    return false;
}

#endif
