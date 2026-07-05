#pragma once

#include <furi_hal.h>
#include <stdint.h>
#include <stdbool.h>
#include <storage/storage.h>
#include <lib/subghz/types.h>

#define SUBGHZ_LAST_SETTING_FREQUENCY_ANALYZER_TRIGGER        (-93.0f)
// 1 = "AM650"
// "AM270", "AM650", "FM238", "FM12K", "FM476",
#define SUBGHZ_LAST_SETTING_DEFAULT_PRESET                    1
#define SUBGHZ_LAST_SETTING_DEFAULT_FREQUENCY                 433920000
#define SUBGHZ_LAST_SETTING_FREQUENCY_ANALYZER_FEEDBACK_LEVEL 2
#define SUBGHZ_LAST_SETTING_DEFAULT_PRESET_HOPPING_THRESHOLD  (-80.0f)

/* 0 = Oscilloscope/Classic, 1 = Waterfall */
#define SUBGHZ_LAST_SETTING_DEFAULT_VISUALIZER_MODE          0u
#define SUBGHZ_LAST_SETTING_DEFAULT_RAW_ZOOM_LEVEL           0u

typedef struct {
    uint32_t frequency;
    uint32_t preset_index;
    uint32_t frequency_analyzer_feedback_level;
    float frequency_analyzer_trigger;
    bool protocol_file_names;
    bool enable_hopping;
    uint32_t ignore_filter;
    uint32_t filter;
    float rssi;
    bool delete_old_signals;
    float hopping_threshold;
    bool enable_preset_hopping;
    float preset_hopping_threshold;
    bool leds_and_amp;
    uint8_t  tx_power;
    uint32_t visualizer_display_mode; /**< 0=Bar 1=Line */
    uint32_t raw_playback_zoom_level;  /**< 0=full file, higher=narrower window; persists across files */
    /* ── Consolidated filter state ─────────────────────────────────────────
     * protocol_filter_data and mod_filter_data mirror the runtime filter
     * objects and are written/read as part of last_subghz.settings.
     * Eliminates protocol_filter.save and modulation_filter.save. */
    uint8_t protocol_filter_data[256]; /* 1 byte per protocol: 0=off 1=on */
    uint8_t mod_filter_data[64];       /* 1 byte per preset:   0=off 1=on */
    bool    protocol_filter_present;   /* true if loaded from file */
    bool    mod_filter_present;        /* true if loaded from file */
} SubGhzLastSettings;

/* Copy raw filter arrays in/out of the filter objects.
 * Called by subghz.c before save and after load. */
void subghz_last_settings_set_protocol_filter(SubGhzLastSettings* s,
                                               const uint8_t* data, size_t count);
void subghz_last_settings_set_mod_filter(SubGhzLastSettings* s,
                                          const uint8_t* data, size_t count);

SubGhzLastSettings* subghz_last_settings_alloc(void);

void subghz_last_settings_free(SubGhzLastSettings* instance);

void subghz_last_settings_load(SubGhzLastSettings* instance, size_t preset_count);

bool subghz_last_settings_save(SubGhzLastSettings* instance);
