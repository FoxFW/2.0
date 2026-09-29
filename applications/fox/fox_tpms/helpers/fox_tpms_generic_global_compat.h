#pragma once

#ifdef FOX_TPMS_HAS_GENERIC_GLOBAL

#include <lib/subghz/blocks/generic.h>

#define FOX_TPMS_ENDLESS_TX (subghz_block_generic_global.endless_tx)

#else

#define FOX_TPMS_ENDLESS_TX (false)

#endif
