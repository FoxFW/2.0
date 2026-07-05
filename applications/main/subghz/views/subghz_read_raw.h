#pragma once

#include <gui/view.h>
#include "../helpers/subghz_types.h"
#include "../helpers/subghz_custom_event.h"

typedef struct SubGhzReadRAW SubGhzReadRAW;

typedef void (*SubGhzReadRAWCallback)(SubGhzCustomEvent event, void* context);

typedef enum {
    SubGhzReadRAWStatusStart,
    SubGhzReadRAWStatusIDLE,
    SubGhzReadRAWStatusREC,
    SubGhzReadRAWStatusTX,
    SubGhzReadRAWStatusTXRepeat,

    SubGhzReadRAWStatusLoadKeyIDLE,
    SubGhzReadRAWStatusLoadKeyTX,
    SubGhzReadRAWStatusLoadKeyTXRepeat,
    SubGhzReadRAWStatusLoadKeyTXPaused,
    SubGhzReadRAWStatusSaveKey,
} SubGhzReadRAWStatus;

/** Visual style for the signal display: Bar = vertical bars by intensity.
 *  Line = continuous connected-line trace, like a spectrum analyzer
 *  envelope. No "Classic"/binary waveform mode exists — removed entirely. */
typedef enum {
    SubGhzReadRawVizBar  = 0,
    SubGhzReadRawVizLine = 1,
} SubGhzReadRawVizMode;

void subghz_read_raw_set_callback(
    SubGhzReadRAW* subghz_read_raw,
    SubGhzReadRAWCallback callback,
    void* context);

SubGhzReadRAW* subghz_read_raw_alloc(bool raw_send_only);
void subghz_read_raw_free(SubGhzReadRAW* subghz_static);

void subghz_read_raw_add_data_statusbar(
    SubGhzReadRAW* instance,
    const char* frequency_str,
    const char* preset_str);

void subghz_read_raw_set_radio_device_type(
    SubGhzReadRAW* instance,
    SubGhzRadioDeviceType device_type);

void subghz_read_raw_update_sample_write(SubGhzReadRAW* instance, size_t sample);
void subghz_read_raw_recording_tick(SubGhzReadRAW* instance);
uint32_t subghz_read_raw_get_recording_ticks(SubGhzReadRAW* instance);
void subghz_read_raw_stop_send(SubGhzReadRAW* instance);
void subghz_read_raw_add_data_rssi(SubGhzReadRAW* instance, float rssi, bool trace);

void subghz_read_raw_set_status(
    SubGhzReadRAW* instance,
    SubGhzReadRAWStatus status,
    const char* file_name,
    float raw_threshold_rssi);

/**
 * Controls whether the "New" button appears in the LoadKeyIDLE screen's
 * left slot. Loading an existing file from Saved should NOT offer "New"
 * (there's no in-progress recording session to discard) — only the
 * post-record-then-save flow should. Defaults to false.
 */
void subghz_read_raw_set_allow_new(SubGhzReadRAW* instance, bool allow);

/** Select Bargraph or Waterfall rendering. Driven from the same global
 *  Radio Settings > Visualizer preference used by the Signal Visualizer,
 *  so the whole app shares one consistent visual language. */
void subghz_read_raw_set_viz_mode(SubGhzReadRAW* instance, SubGhzReadRawVizMode mode);

/**
 * Set the signal envelope to display during playback: 100 intensity values
 * (0-255, duty-cycle of "on" time within that 1% time-slice — NOT a binary
 * HIGH/LOW flag, so bar height actually reflects pulse density). Triggers
 * a ~400ms left-to-right reveal animation so the display visibly "builds"
 * in front of the user rather than popping in fully formed.
 *
 * @param envelope          100-byte intensity array, or NULL to clear/show
 *                           a generic "Playing..." placeholder if unavailable
 * @param total_duration_us The REAL playback duration in microseconds.
 *                           This drives the position cursor's speed — it
 *                           must be actual wall-clock duration, not pulse
 *                           count (pulse count has no fixed relationship
 *                           to playback time; using it made the cursor
 *                           crawl at the wrong, unrelated speed).
 */
void subghz_read_raw_set_envelope(
    SubGhzReadRAW* instance,
    const uint8_t* envelope,
    uint32_t total_duration_us);

/**
 * Advance the TX position cursor by one tick. Call this from the scene's
 * TX tick handler at the SAME fixed period the scene's tick timer runs at
 * (this app's tick period is 100ms — see view_dispatcher_set_tick_event_callback).
 */
void subghz_read_raw_tick_tx(SubGhzReadRAW* instance);

/**
 * Set the zoom level (0 = fully zoomed out / whole file, higher = narrower
 * window). Persists across files via the caller (Radio Settings-style
 * last_settings field) — this function only applies it to the current
 * envelope; the caller is responsible for re-requesting envelope data for
 * the new window via the SubGhzCustomEventViewReadRAWZoomIn/Out events.
 */
void subghz_read_raw_set_zoom_level(SubGhzReadRAW* instance, uint8_t zoom_level);
uint8_t subghz_read_raw_get_zoom_level(SubGhzReadRAW* instance);

/** Returns the current seek position (0-100), valid while paused — used
 *  by the scene to center a zoom window on the right spot. */
uint8_t subghz_read_raw_get_seek_pct(SubGhzReadRAW* instance);

/** Returns true if active playback has run past its expected completion
 *  time without the natural TX-end callback having stopped it yet. The
 *  scene uses this as a defensive backup trigger. */
bool subghz_read_raw_is_playback_overdue(SubGhzReadRAW* instance);

View* subghz_read_raw_get_view(SubGhzReadRAW* subghz_static);
