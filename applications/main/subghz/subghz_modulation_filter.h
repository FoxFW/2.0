#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file subghz_modulation_filter.h
 *
 * Persistent ON/OFF filter for each SubGHz modulation preset.
 * Preset names and count are read dynamically from SubGhzSetting at
 * runtime, so any presets added to setting_user.file are included
 * automatically.
 *
 * Saved to /ext/subghz/modulation_filter.save
 */

/* Enough headroom for built-ins (7) plus any user-defined presets. */
#define SUBGHZ_MOD_FILTER_COUNT  64u
/* SUBGHZ_MOD_FILTER_SAVE_PATH removed — filter saved inside last_subghz.settings */

typedef struct SubGhzModulationFilter SubGhzModulationFilter;

SubGhzModulationFilter* subghz_modulation_filter_alloc(void);
void  subghz_modulation_filter_free(SubGhzModulationFilter* instance);
void  subghz_modulation_filter_load(SubGhzModulationFilter* instance);
void  subghz_modulation_filter_save(SubGhzModulationFilter* instance);
bool  subghz_modulation_filter_is_enabled(const SubGhzModulationFilter* instance, size_t index);
void  subghz_modulation_filter_set_enabled(SubGhzModulationFilter* instance, size_t index, bool enabled);

/** Copy internal enabled[] array out (for saving via last_settings). */
void subghz_modulation_filter_get_raw(const SubGhzModulationFilter* instance,
                                       uint8_t* out, size_t count);

/** Restore internal enabled[] from raw data (after loading last_settings). */
void subghz_modulation_filter_set_raw(SubGhzModulationFilter* instance,
                                       const uint8_t* in, size_t count);

#ifdef __cplusplus
}
#endif
