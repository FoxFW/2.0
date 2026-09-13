#pragma once

#include "subghz_custom_btn_compat.h"

/* Same SUBGHZ_GARAGE_HAS_CUSTOM_BTN cdefine gate as subghz_custom_btn_
 * compat.h (see there for why this isn't __has_include-based). Only extra
 * surface custom_btn_i.h adds on top of custom_btn.h is the KeeLoq
 * programming-mode button used by a few protocols' custom-button
 * handling - inert/"off" default is correct on cross-fork builds too. */

#ifdef SUBGHZ_GARAGE_HAS_CUSTOM_BTN
#include <lib/subghz/blocks/custom_btn_i.h>
#else

#define PROG_MODE_OFF              (0U)
#define PROG_MODE_KEELOQ_BFT       (1U)
#define PROG_MODE_KEELOQ_APRIMATIC (2U)
#define PROG_MODE_KEELOQ_DEA_MIO   (3U)

typedef uint8_t ProgMode;

static inline void subghz_custom_btn_set_prog_mode(ProgMode prog_mode) {
    (void)prog_mode;
}

static inline ProgMode subghz_custom_btn_get_prog_mode(void) {
    return PROG_MODE_OFF;
}

#endif
