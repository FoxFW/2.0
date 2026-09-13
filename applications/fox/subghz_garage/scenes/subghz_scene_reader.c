/**
 * @file subghz_scene_reader.c
 * @brief Combined Read (auto capture+decode) and Read RAW (manual
 *        record/load/send) scene implementation.
 *
 * Both already drove the same subghz_read_raw view (SubGhzViewIdReadRAW).
 * This merges their scene-level logic into one translation unit to drop
 * the duplicate setup code two separate scene files each carried (status
 * bar, protocol group load, callback wiring, view switch) instead of one.
 *
 * Still two distinct registered scenes - SubGhzSceneReceiver ("Read") and
 * SubGhzSceneReadRAW ("Read RAW"), see subghz_scene.h - so every existing
 * scene_manager_get_scene_state()/has_previous_scene() check elsewhere in
 * this app (the save flow, Receiver Config, More RAW, etc.) that keys off
 * one or the other scene ID keeps working unchanged. SubGhzSceneReceiver's
 * own on_enter/on_event/on_exit are now thin wrappers: on_enter sets
 * subghz->reader_read_mode before delegating into SubGhzSceneReadRAW's
 * handlers below, which check the flag first and branch to Read's logic
 * (subghz_scene_reader_read_on_enter/on_event/on_exit, moved here
 * unchanged from the old subghz_scene_receiver.c) before falling through
 * to Read RAW's own unchanged body. on_exit clears the flag so a later
 * direct entry into SubGhzSceneReadRAW never sees a stale true.
 */

#include "../subghz_i.h"
#include "../helpers/subghz_debug_log.h"
#include <dolphin/dolphin.h>
#include <lib/subghz/protocols/raw.h>
#include <toolbox/path.h>
#include <furi.h>
#include <furi/core/memmgr.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define TAG "SubGhzSceneReader"

/* ========================================================================
 * Read RAW - manual record/load/send, waveform envelope + zoom, save flow.
 * Unchanged from the old subghz_scene_read_raw.c except for the
 * reader_read_mode dispatch added at the top of on_enter/on_event.
 * ======================================================================== */

#define RAW_FILE_NAME "RAW_"

bool subghz_scene_read_raw_update_filename(SubGhz* subghz) {
    bool ret = false;
    //set the path to read the file
    FuriString* temp_str = furi_string_alloc();
    do {
        FlipperFormat* fff_data = subghz_txrx_get_fff_data(subghz->txrx);
        if(!flipper_format_rewind(fff_data)) {
            FURI_LOG_E(TAG, "Rewind error");
            break;
        }

        if(!flipper_format_read_string(fff_data, "File_name", temp_str)) {
            FURI_LOG_E(TAG, "Missing File_name");
            break;
        }

        furi_string_set(subghz->file_path, temp_str);

        ret = true;
    } while(false);

    furi_string_free(temp_str);
    return ret;
}

static void subghz_scene_read_raw_update_statusbar(void* context) {
    furi_assert(context);
    SubGhz* subghz = context;

    FuriString* frequency_str = furi_string_alloc();
    FuriString* modulation_str = furi_string_alloc();

#ifdef SUBGHZ_EXT_PRESET_NAME
    subghz_txrx_get_frequency_and_modulation(subghz->txrx, frequency_str, modulation_str, true);
#else
    subghz_txrx_get_frequency_and_modulation(subghz->txrx, frequency_str, modulation_str, false);
#endif
    subghz_read_raw_add_data_statusbar(
        subghz->subghz_read_raw,
        furi_string_get_cstr(frequency_str),
        furi_string_get_cstr(modulation_str));

    furi_string_free(frequency_str);
    furi_string_free(modulation_str);

    subghz_read_raw_set_radio_device_type(
        subghz->subghz_read_raw, subghz_txrx_radio_device_get(subghz->txrx));
}

void subghz_scene_read_raw_callback(SubGhzCustomEvent event, void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, event);
}

void subghz_scene_read_raw_callback_end_tx(void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(
        subghz->view_dispatcher, SubGhzCustomEventViewReadRAWSendStop);
}


/* RAW .sub files use repeated "RAW_Data:" text lines (not a FlipperFormat array).
 * Reads as plain text; produces a 100-byte HIGH/LOW preview for TX timeline display. */
#define ENVELOPE_PREVIEW_SAMPLES 2048u

typedef struct {
    File*  file;
    uint8_t buf[128];
    size_t len;
    size_t pos;
    bool   eof;
} SubghzRawLineReader;

static bool subghz_raw_lr_read_line(SubghzRawLineReader* lr, FuriString* out) {
    furi_string_reset(out);
    bool any = false;
    while(true) {
        if(lr->pos >= lr->len) {
            if(lr->eof) break;
            lr->len = storage_file_read(lr->file, lr->buf, sizeof(lr->buf));
            lr->pos = 0;
            if(lr->len == 0) {
                lr->eof = true;
                break;
            }
        }
        char c = (char)lr->buf[lr->pos++];
        if(c == '\n') {
            any = true;
            break;
        }
        if(c == '\r') continue;
        furi_string_push_back(out, c);
        any = true;
    }
    return any;
}

/* zoom_level: 0 = full file (no windowing). center_pct: 0-100, the
 * playback position to center the zoom window on (only used if
 * zoom_level > 0). Reads the file once; total_us (the FULL file's real
 * duration) is always computed and reported back via *out_total_us so the
 * caller can pass it to subghz_read_raw_set_envelope() — TX cursor speed
 * must always reflect the whole file's real duration, never the zoomed
 * display window, since actual transmission timing doesn't change when
 * you zoom the display. */
static const uint8_t ENVELOPE_ZOOM_WINDOW_PCT[5] = {100, 60, 36, 22, 13};

static void subghz_scene_read_raw_load_envelope_ex(
    SubGhz* subghz, uint8_t zoom_level, uint8_t center_pct, uint64_t* out_total_us) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File*    file     = storage_file_alloc(storage);

    uint8_t  preview[100];
    memset(preview, 0, sizeof(preview));
    uint32_t total_samples = 0;
    bool     ok            = false;
    uint64_t total_us      = 0;
    if(out_total_us) *out_total_us = 0;

    int32_t* buf = malloc(sizeof(int32_t) * ENVELOPE_PREVIEW_SAMPLES);

    if(buf && storage_file_open(
                  file, furi_string_get_cstr(subghz->file_path),
                  FSAM_READ, FSOM_OPEN_EXISTING)) {

        SubghzRawLineReader lr = {.file = file, .len = 0, .pos = 0, .eof = false};
        FuriString* line = furi_string_alloc();

        /* Read ALL entries for accurate total_us.
         * Noise entries from blank recordings fill the preview buffer before
         * our silence padding is reached — previously this caused total_us to
         * be far too small.  Now we count abs(v) for every entry regardless of
         * whether it fits in buf[], then use buf[] only for the visual preview. */
        while(subghz_raw_lr_read_line(&lr, line)) {
            const char* s = furi_string_get_cstr(line);
            if(strncmp(s, "RAW_Data:", 9) == 0) {
                const char* p = s + 9;
                char* end;
                while(*p) {
                    long v = strtol(p, &end, 10);
                    if(end == p) {
                        if(*p == '\0') break;
                        p++;
                        continue;
                    }
                    p = end;
                    /* Always accumulate duration from every entry */
                    total_us += (uint64_t)(v < 0 ? -(int64_t)v : (int64_t)v);
                    /* Only store in preview buffer if space remains */
                    if(total_samples < ENVELOPE_PREVIEW_SAMPLES)
                        buf[total_samples++] = (int32_t)v;
                }
            }
        }

        furi_string_free(line);
        storage_file_close(file);

        if(total_samples > 0) {
            /* total_us already accumulated inline above (all entries, not
             * just the preview-buffer subset) — nothing more to sum here. */
            if(out_total_us) *out_total_us = total_us;

            if(total_us > 0) {
                /* Display window: full file when zoom_level==0, otherwise a
                 * narrower span centered on center_pct (same 2/3-ish ratio
                 * the RAW editor itself uses for zoom steps). */
                uint64_t disp_start = 0;
                uint64_t disp_end   = total_us;
                if(zoom_level > 0 && zoom_level < 5) {
                    uint64_t window_span =
                        (total_us * ENVELOPE_ZOOM_WINDOW_PCT[zoom_level]) / 100;
                    if(window_span < 1) window_span = 1;
                    uint64_t center = ((uint64_t)center_pct * total_us) / 100;
                    uint64_t half   = window_span / 2;
                    disp_start = (center > half) ? (center - half) : 0;
                    disp_end   = disp_start + window_span;
                    if(disp_end > total_us) {
                        disp_end   = total_us;
                        disp_start = (disp_end > window_span) ? (disp_end - window_span) : 0;
                    }
                }
                uint64_t disp_span = (disp_end > disp_start) ? (disp_end - disp_start) : 1;

                /* Duty-cycle intensity per 1% time-bucket WITHIN the display
                 * window: accumulate how much "on" (HIGH/carrier) time falls
                 * inside each bucket, as a fraction of that bucket's span.
                 * This is far more informative than a flat HIGH/LOW flag —
                 * dense pulse bursts show as tall bars, sparse/gappy regions
                 * show as short ones, and OOK/ASK pulse structure is
                 * actually visible at a glance. */
                uint32_t bucket_on_us[100] = {0};
                uint64_t current_us = 0;

                for(uint32_t i = 0; i < total_samples; i++) {
                    int32_t  val = buf[i];
                    uint64_t dur = (uint64_t)(val < 0 ? -val : val);
                    bool     hi  = (val > 0);
                    uint64_t seg_start = current_us;
                    uint64_t seg_end   = current_us + dur;
                    current_us = seg_end;

                    if(!hi || dur == 0) continue;

                    /* Clip this pulse segment to the display window. */
                    uint64_t cs = seg_start > disp_start ? seg_start : disp_start;
                    uint64_t ce = seg_end   < disp_end   ? seg_end   : disp_end;
                    if(ce <= cs) continue; /* entirely outside the window */

                    uint8_t b0 = (uint8_t)(((cs - disp_start) * 100) / disp_span);
                    uint8_t b1 = (uint8_t)(((ce - disp_start) * 100) / disp_span);
                    if(b1 > 99) b1 = 99;
                    for(uint8_t b = b0; b <= b1; b++) {
                        uint64_t bstart = disp_start + ((uint64_t)b * disp_span / 100);
                        uint64_t bend   = disp_start + ((uint64_t)(b + 1) * disp_span / 100);
                        uint64_t ov_s   = cs > bstart ? cs : bstart;
                        uint64_t ov_e   = ce < bend   ? ce : bend;
                        if(ov_e > ov_s) bucket_on_us[b] += (uint32_t)(ov_e - ov_s);
                    }
                }

                for(uint8_t b = 0; b < 100; b++) {
                    uint64_t bstart = disp_start + ((uint64_t)b * disp_span / 100);
                    uint64_t bend   = disp_start + ((uint64_t)(b + 1) * disp_span / 100);
                    uint64_t span   = (bend > bstart) ? (bend - bstart) : 1;
                    uint32_t inten  = (uint32_t)((uint64_t)bucket_on_us[b] * 255 / span);
                    preview[b] = (uint8_t)(inten > 255 ? 255 : inten);
                }
                ok = true;
            }
        }
    } else if(file) {
        storage_file_close(file);
    }

    if(buf) free(buf);
    if(file) storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    /* THE FIX: pass the FULL file's real duration (microseconds), not
     * pulse count — pulse count has no fixed relationship to playback
     * time, which made the position cursor crawl at an unrelated speed. */
    subghz_read_raw_set_envelope(
        subghz->subghz_read_raw,
        ok ? preview : NULL,
        (uint32_t)total_us);
}

/* Convenience wrapper: full-file load (no zoom window), used on initial
 * file load and whenever zoom resets to level 0. */
static void subghz_scene_read_raw_load_envelope(SubGhz* subghz) {
    subghz_scene_read_raw_load_envelope_ex(subghz, 0, 0, NULL);
}

/* Re-derive a zoomed envelope centered on the given playback position,
 * preserving the FULL file's real duration for TX cursor timing (which
 * never changes with zoom — only the displayed window does). */
static void subghz_scene_read_raw_load_envelope_zoomed(
    SubGhz* subghz, uint8_t zoom_level, uint8_t center_pct) {
    subghz_scene_read_raw_load_envelope_ex(subghz, zoom_level, center_pct, NULL);
}

/* ========================================================================
 * Read - auto capture+decode. Moved here unchanged from the old
 * subghz_scene_receiver.c. Only reachable via reader_read_mode - see the
 * dispatch at the top of subghz_scene_read_raw_on_enter/on_event below.
 *
 * Two internal states (tracked via scene_manager_set_scene_state, same
 * convention Decode RAW already uses for itself):
 *
 *  Listening  - subghz_read_raw's view drives the screen (same visual as
 *               Read RAW's own recording screen), RX is running with only
 *               the RAW decoder loaded (SUBGHZ_PROTOCOL_RAW_NAME), raw
 *               pulses accumulate into a fixed scratch file. Each tick,
 *               compares the raw sample-write count to the previous tick's
 *               - once it's stopped growing for SUBGHZ_AUTO_READ_SILENCE_
 *               TICKS ticks after having seen at least one pulse, the
 *               transmission is considered finished.
 *  Decoding   - RX is stopped, the scratch file is fed through the file-
 *               encoder-worker + subghz_receiver_decode() loop (the same
 *               core mechanism Decode RAW uses, minus its scene-specific
 *               UI/retry plumbing - reused directly here would entangle
 *               this scene with DecodeRAW's own state/failure-scene
 *               assumptions). Ensures the active protocol group's plugin is
 *               loaded for the very first time this decode actually needs
 *               it (subghz_txrx_get_receiver() does this internally).
 *
 * On success (history grew): jump to the existing SubGhzSceneReceiverInfo
 * screen for the newly-decoded item - already has Save/Discard(Back)/Send,
 * reused as-is. On failure: brief auto-dismiss "couldn't decode" popup,
 * then straight back to Listening. Either way the scratch file is deleted
 * right after the decode attempt - it only ever exists to feed the decode
 * pass; a successful Save (from ReceiverInfo) serializes the DECODED
 * protocol's own state, not this raw file.
 * ======================================================================== */

#define AUTO_READ_TMP_FILE_NAME "auto_read_tmp"

/* How many Tick-handler ticks (~100ms each, matching this app's tick
 * period) of zero new raw samples - after having seen at least one -
 * before a transmission is considered finished. Needs on-device tuning:
 * too short risks cutting a signal off mid-repeat-gap, too long just makes
 * the screen sit on "Decoding..." longer than necessary after a real
 * signal already finished. */
#define SUBGHZ_AUTO_READ_SILENCE_TICKS 3

/* How many file-encoder samples to feed through subghz_receiver_decode()
 * per tick during Decoding - same batch size Decode RAW itself uses. */
#define SUBGHZ_AUTO_READ_DECODE_SAMPLES_PER_TICK 400

/* Idle Start screen auto-start countdown, in whole seconds - the Start
 * button shows "Start (3)" -> "Start (2)" -> "Start (1)" then starts
 * listening on its own, same as an OK press. See s_auto_start_countdown_sec
 * below. */
#define SUBGHZ_AUTO_START_COUNTDOWN_SEC 3

/* Low-RAM watchdog debounce - user reported a brief (a tick or two) heap
 * dip while CLI was already connected flashed "Uh Oh!" and auto-recovered
 * near-instantly, which on lab.flipper.net's CLI manifests as a frozen,
 * un-reconnectable session rather than a clean disconnect (the low-RAM
 * screen's own recovery path locks out new CLI sessions once it believes
 * USB/qFlipper was actually closed - see subghz_scene_low_ram_warning.c -
 * so a spike that resolves itself before the user could act still trips
 * that lockout). Require the trigger condition to hold for a full second
 * (10 ticks at this app's 100ms tick period) before actually bailing to
 * the warning screen, so a momentary spike doesn't fire it at all. */
#define SUBGHZ_LOW_RAM_TRIGGER_TICKS 10

/* How long to let the just-torn-down Listening session's memory actually
 * land before re-checking heap for the final bail/resume decision below -
 * mirrors subghz_low_ram_mitigate()'s own settle-then-recheck pattern
 * (subghz.c). Confirmed necessary via device logs: a read taken
 * immediately after subghz_txrx_stop() returned was still reporting
 * "still low" with values the very next scene's own read - reached well
 * under 300ms later - already showed comfortably recovered. */
#define SUBGHZ_LOW_RAM_TICK_BAIL_RECHECK_WAIT_MS 300

/* Belt-and-suspenders cap on top of the SUBGHZ_GARAGE_WORKER_RAM_COST
 * margin check below - if the Tick handler still finds itself resuming
 * Listening this many times in one visit, stop trusting the numbers and
 * force a full bail to the Warning screen instead (higher recover bar,
 * 1.5s minimum dwell, manual Close) rather than risk a fast, silent
 * auto-retry loop for a cause the margin check didn't anticipate. */
#define SUBGHZ_LOW_RAM_RESUME_ATTEMPTS_MAX 3

typedef enum {
    /* Idle Start screen shown (SubGhzReadRAWStatusStart) - RX not running,
     * no protocol group loaded, nothing to clean up. Only Listening/
     * Decoding actually have a live RX/file/decode session. */
    SubGhzReceiverAutoStateStart,
    SubGhzReceiverAutoStateListening,
    SubGhzReceiverAutoStateDecoding,
} SubGhzReceiverAutoState;

/* Silence-detection bookkeeping - file-local since this scene is
 * effectively a singleton within one app instance. Reset in subghz_scene_receiver_start_
 * listening(). */
static uint32_t s_auto_last_sample_count = 0;
static uint32_t s_auto_silence_ticks = 0;
static bool s_auto_has_activity = false;
static uint16_t s_auto_history_count_before_decode = 0;
/* Diagnostic only - counts ticks since the last start_listening() call, so
 * a periodic heap log can be emitted without flooding the log every tick.
 * See the user's regression report ("things have gotten worse") - the
 * previous listening-loop had almost no logging at all, so a fast heap
 * decline while just sitting there produced zero trace of where it went. */
static uint32_t s_auto_tick_count = 0;
/* Low-RAM debounce - consecutive ticks the trigger condition has held true.
 * Reset in subghz_scene_receiver_start_listening() and any tick the
 * condition doesn't hold. See SUBGHZ_LOW_RAM_TRIGGER_TICKS. */
static uint32_t s_low_ram_tick_count = 0;
/* Hard cap on the Tick handler's own "stop, wait, recovered? resume"
 * cycle within one Read visit - see SUBGHZ_LOW_RAM_RESUME_ATTEMPTS_MAX.
 * Reset on fresh scene entry (subghz_scene_reader_read_on_enter()) only -
 * NOT by start_listening() itself, since the whole point is counting how
 * many times THAT function got called by this internal resume path
 * specifically, not by a deliberate user action. */
static uint32_t s_low_ram_resume_attempts = 0;
/* Start-screen auto-start countdown bookkeeping - seconds remaining and a
 * 100ms sub-tick counter (10 ticks = 1 second, matching this app's tick
 * period). Both reset in subghz_scene_reader_read_show_start(), which runs
 * on every genuine fresh visit to the idle Start screen (first entry,
 * return from Config with nothing running, return from the No Match
 * widget). An OK press skips straight to start_listening() without
 * touching these - they just get reset next time the Start screen shows. */
static uint8_t s_auto_start_countdown_sec = 0;
static uint8_t s_auto_start_subtick = 0;
/* Set true when Back cancels an in-progress countdown, so the Tick handler
 * stops running it entirely - just zeroing the seconds counter isn't
 * enough on its own, since the Tick handler's own "hit zero -> go" check
 * can't otherwise tell a genuinely-finished countdown apart from one that
 * was cancelled early and is simply sitting at 0. Reset alongside the
 * countdown itself in subghz_scene_reader_read_show_start(). */
static bool s_auto_start_cancelled = false;

/* Same countdown, for manual Read RAW's own idle Start screen (reached
 * when subghz->reader_read_mode is false - a separate on_event/on_enter
 * pair from Read's above, see subghz_scene_read_raw_on_event() dispatch).
 * Manual Read RAW has no auto_state concept at all, so this can't reuse
 * the statics above - reset in subghz_scene_read_raw_on_enter()'s default
 * case and in the Erase ("New") custom event handler, both of which are
 * the only two places that ever (re)set status to Start for this mode.
 * s_manual_start_countdown_fired guards against sending the synthetic
 * REC event (see the Tick handler below) more than once, since the model
 * status doesn't flip away from Start synchronously the instant that
 * event is queued. */
static uint8_t s_manual_start_countdown_sec = 0;
static uint8_t s_manual_start_subtick = 0;
static bool s_manual_start_countdown_fired = false;
/* Same purpose as s_auto_start_cancelled above, for manual Read RAW's own
 * countdown. Reset alongside it in on_enter()'s default case and the Erase
 * handler. */
static bool s_manual_start_cancelled = false;

const NotificationSequence subghz_sequence_rx = {
    &message_green_255,

    &message_display_backlight_on,

    &message_vibro_on,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,

    &message_delay_50,
    NULL,
};

static void subghz_scene_receiver_callback(SubGhzCustomEvent event, void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, event);
}

static void subghz_scene_receiver_add_to_history_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    furi_assert(context);
    SubGhz* subghz = context;

    subghz_debug_log_write("add_to_history_callback: enter, decoder_base=%p", (void*)decoder_base);
    SubGhzRadioPreset preset = subghz_txrx_get_preset(subghz->txrx);
    subghz_history_add_to_history(
        subghz->history, decoder_base, &preset, SUBGHZ_LOW_RAM_FREE_HEAP_READ);
    subghz_debug_log_write("add_to_history_callback: added, resetting receiver");
    subghz_receiver_reset(receiver);
    subghz_debug_log_write("add_to_history_callback: exit");
}

static void subghz_scene_receiver_delete_tmp_file(SubGhz* subghz) {
    UNUSED(subghz);
    FuriString* temp_str = furi_string_alloc();
    furi_string_printf(
        temp_str, "%s/%s%s", SUBGHZ_RAW_FOLDER, AUTO_READ_TMP_FILE_NAME, SUBGHZ_APP_FILENAME_EXTENSION);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_remove(storage, furi_string_get_cstr(temp_str));
    furi_record_close(RECORD_STORAGE);
    furi_string_free(temp_str);
}

/* Idle Start screen - shown on every fresh entry (and every return from
 * Config) instead of launching straight into RX. Gives the user a
 * deliberate moment to reach Config (the "Config" button is drawn for
 * SubGhzReadRAWStatusStart, same screen manual Read RAW's own idle state
 * already uses) before committing to a capture, rather than being dropped
 * straight into a live RX session the instant this scene opens. No
 * protocol group is loaded and no heap is committed yet at this point -
 * that only happens once the user actually presses Start/OK, which fires
 * SubGhzCustomEventViewReadRAWREC and runs subghz_scene_receiver_start_
 * listening() below. */
static void subghz_scene_reader_read_show_start(SubGhz* subghz) {
    scene_manager_set_scene_state(
        subghz->scene_manager, SubGhzSceneReceiver, SubGhzReceiverAutoStateStart);

    float threshold_rssi = subghz_threshold_rssi_get(subghz->threshold_rssi);

    FuriString* frequency_str = furi_string_alloc();
    FuriString* modulation_str = furi_string_alloc();
    subghz_txrx_get_frequency_and_modulation(subghz->txrx, frequency_str, modulation_str, true);
    subghz_read_raw_add_data_statusbar(
        subghz->subghz_read_raw,
        furi_string_get_cstr(frequency_str),
        furi_string_get_cstr(modulation_str));
    furi_string_free(frequency_str);
    furi_string_free(modulation_str);
    subghz_read_raw_set_radio_device_type(
        subghz->subghz_read_raw, subghz_txrx_radio_device_get(subghz->txrx));

    subghz_read_raw_set_callback(subghz->subghz_read_raw, subghz_scene_receiver_callback, subghz);
    subghz_read_raw_set_viz_mode(
        subghz->subghz_read_raw,
        (SubGhzReadRawVizMode)subghz->last_settings->visualizer_display_mode);

    subghz_read_raw_set_status(subghz->subghz_read_raw, SubGhzReadRAWStatusStart, "", threshold_rssi);

    s_auto_start_countdown_sec = SUBGHZ_AUTO_START_COUNTDOWN_SEC;
    s_auto_start_subtick = 0;
    s_auto_start_cancelled = false;
    subghz_read_raw_set_start_countdown(subghz->subghz_read_raw, s_auto_start_countdown_sec);

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdReadRAW);
}

/* switch_view: whether the RAW view (SubGhzViewIdReadRAW) still needs to be
 * made the active view, or is already showing. view_dispatcher_switch_to_
 * view() unconditionally runs the OLD current view's exit callback before
 * the new one's enter callback (view_dispatcher_set_current_view() has no
 * same-view short-circuit) - subghz_read_raw_exit() treats any status other
 * than IDLE/Start/LoadKeyIDLE as "still recording, abandoned without going
 * through the view's own Back/OK" and self-fires ViewReadRAWIDLE to clean
 * up. Calling switch_to_view(ReadRAW) while ReadRAW is ALREADY the active
 * view (status already REC, set moments earlier by the Start screen's own
 * OK-press handler or by subghz_read_raw_set_status() just above) makes
 * that view briefly "exit itself," firing a spurious IDLE that the scene
 * then processes as "user wants to exit Read" - undoing the recording that
 * had just genuinely started. Pass false from any caller where the RAW
 * view is already known to be on screen. */
static void subghz_scene_receiver_start_listening(SubGhz* subghz, bool switch_view) {
    s_auto_last_sample_count = 0;
    s_auto_silence_ticks = 0;
    s_auto_has_activity = false;
    s_auto_tick_count = 0;
    s_low_ram_tick_count = 0;

    scene_manager_set_scene_state(
        subghz->scene_manager, SubGhzSceneReceiver, SubGhzReceiverAutoStateListening);

    float threshold_rssi = subghz_threshold_rssi_get(subghz->threshold_rssi);

    FuriString* frequency_str = furi_string_alloc();
    FuriString* modulation_str = furi_string_alloc();
    subghz_txrx_get_frequency_and_modulation(subghz->txrx, frequency_str, modulation_str, true);
    subghz_read_raw_add_data_statusbar(
        subghz->subghz_read_raw,
        furi_string_get_cstr(frequency_str),
        furi_string_get_cstr(modulation_str));
    furi_string_free(frequency_str);
    furi_string_free(modulation_str);
    subghz_read_raw_set_radio_device_type(
        subghz->subghz_read_raw, subghz_txrx_radio_device_get(subghz->txrx));

    subghz_read_raw_set_callback(subghz->subghz_read_raw, subghz_scene_receiver_callback, subghz);
    subghz_read_raw_set_viz_mode(
        subghz->subghz_read_raw,
        (SubGhzReadRawVizMode)subghz->last_settings->visualizer_display_mode);

    /* This is the choke point every real "about to run the heavy
     * rx-restart chain" path goes through - Start/REC button press,
     * Config-resume, silence-timeout auto-restart, decode-failure restart
     * - so this is deliberately where subghz_low_ram_mitigate() lives, not
     * on_enter (which only shows the idle Start screen and must never
     * touch qFlipper on its own). MUST run before the protocol-group-load
     * call right below, not after it - that load has its own threshold
     * check (SUBGHZ_TXRX_RECEIVER_REBUILD_FREE_HEAP, helpers/subghz_
     * txrx.c) and previously ran first, so a still-connected qFlipper
     * (its screen-stream alone costs ~25KB) could fail the group load
     * before ever getting a chance to be disconnected here - surfaced as
     * "Loading Error" with qFlipper visibly still connected/mirroring,
     * never having been kicked off like it should have. Disconnects
     * CLI/RPC unconditionally before the group-load/worker-alloc/rx_start
     * chain below, which costs another 12-15KB beyond this point per
     * device logs - real headroom matters here, not just a bare pass over
     * the threshold. Falling through to the warning screen below is now a
     * rare last resort - see subghz_low_ram_mitigate()'s own comment
     * (subghz_i.h). */
    if(subghz_low_ram_mitigate(subghz, SUBGHZ_LOW_RAM_FREE_HEAP_READ)) {
        /* One canonical read, reused below - see the matching comment on
         * the Tick handler's own bail block for why a fresh read per log
         * line was misleading (and could even contradict the "still low"
         * claim it was printed next to). */
        size_t bail_free_heap = memmgr_get_free_heap();
        if(furi_get_tick() < subghz->low_ram_grace_until_ms) {
            /* Still inside the post-recovery grace window (subghz_i.h) -
             * the warning screen just proved heap was fine a moment ago,
             * so tripping the SAME zero-debounce check again this fast
             * means that recovery didn't really hold, not a fresh problem
             * worth re-showing "Uh Oh!" for. Bounce to the idle Start
             * screen quietly instead of back into the warning screen -
             * the user can retry deliberately once real headroom has had
             * a chance to actually settle. */
            FURI_LOG_W(
                TAG,
                "start_listening: free heap %zu still low within post-recovery grace, backing out to Start",
                bail_free_heap);
            subghz_debug_log_write(
                "start_listening: free heap %zu still low within post-recovery grace, backing out to Start",
                bail_free_heap);
            subghz_scene_reader_read_show_start(subghz);
            return;
        }
        FURI_LOG_W(
            TAG,
            "start_listening: free heap %zu still low after mitigation, bailing to warning",
            bail_free_heap);
        subghz_debug_log_write(
            "start_listening: free heap %zu still low after mitigation, bailing to warning",
            bail_free_heap);
        scene_manager_next_scene(subghz->scene_manager, SubGhzSceneLowRamWarning);
        return;
    }

    /* Reset before attempting the load, not just trust whatever state it's
     * in: subghz_txrx_ensure_protocol_group() latches a "gave up on this
     * group" flag after two failed attempts and short-circuits to an
     * immediate failure on every call after that, until the group
     * actually changes - by design, to avoid hammering an already-
     * fragile heap. But Read calls start_listening() far more often than
     * any other screen (every re-entry, every popup dismiss, every
     * decode-failure retry), so a single low-RAM moment anywhere earlier
     * in the session could latch this and then silently block every
     * subsequent Read attempt for the rest of the app's life, long after
     * heap actually recovered - previously via furi_check() below
     * crashing the whole app outright the next time this ran. Safe to
     * give the group a genuine fresh attempt every time rather than trust
     * a potentially very stale latch. */
    subghz_txrx_reset_protocol_load_failed(subghz->txrx);
    if(!subghz_txrx_load_decoder_by_name_protocol(subghz->txrx, SUBGHZ_PROTOCOL_RAW_NAME)) {
        FURI_LOG_E(TAG, "start_listening: protocol group failed to load");
        /* SubGhzSceneProtocolLoadError already exists purpose-built for
         * exactly this (see its own file header) - Exit/Retry, where
         * Retry fully relaunches the app for a clean heap. More
         * appropriate than the generic SD-card-style ShowError message. */
        scene_manager_next_scene(subghz->scene_manager, SubGhzSceneProtocolLoadError);
        return;
    }
    subghz_txrx_receiver_set_filter(subghz->txrx, SubGhzProtocolFlag_RAW);
    subghz_txrx_set_rx_callback(subghz->txrx, NULL, subghz);

    SubGhzProtocolDecoderRAW* decoder_raw =
        (SubGhzProtocolDecoderRAW*)subghz_txrx_get_decoder(subghz->txrx);
    SubGhzRadioPreset preset = subghz_txrx_get_preset(subghz->txrx);

    /* Defensive: close out any still-open raw capture from a session this
     * scene never got to clean up itself - specifically the Tick handler's
     * low-RAM bail (mid-Listening, straight to LowRamWarning with RX still
     * running and the scratch file still open for write - on_exit only
     * tears down Decoding state, not Listening, since Config's own round
     * trip deliberately leaves Listening's RX running). Without this,
     * save_to_file_init()'s storage_simply_remove()/file_open_always() on
     * the SAME file path fails because the old handle is still holding it
     * open - surfaces later as a bogus "Function requires an SD card."
     * error, and the abandoned Storage record/FlipperFormat/RX worker are
     * why free heap doesn't recover on its own either. subghz_protocol_
     * raw_save_to_file_stop() is already a safe no-op if nothing was open
     * (file_is_open == RAWFileIsOpenClose), so this costs nothing on the
     * normal fresh-entry path. */
    subghz_txrx_stop(subghz->txrx);
    subghz_protocol_raw_save_to_file_stop(decoder_raw);

    if(subghz_protocol_raw_save_to_file_init(decoder_raw, AUTO_READ_TMP_FILE_NAME, &preset)) {
        FURI_LOG_I(TAG, "start_listening: rx_start");
        subghz_debug_log_write("start_listening: rx_start");
        subghz_txrx_rx_start(subghz->txrx);
        subghz->state_notifications = SubGhzNotificationStateRx;

        /* Re-apply the saved Hopping on/off preference every time a fresh
         * listening session actually begins, rather than trusting whatever
         * subghz->txrx's hopper_state happens to still be from a previous
         * scene visit. This is the single choke point every "about to run
         * the heavy rx-restart chain" path goes through (see the big
         * comment above), so it covers the OK-press, Config-resume, and
         * low-RAM-recovery-resume paths uniformly. The Tick handler below
         * is what actually advances the hop once this is set - see its own
         * comment. */
        subghz_txrx_hopper_set_state(
            subghz->txrx,
            subghz->last_settings->enable_hopping ? SubGhzHopperStateRunning :
                                                     SubGhzHopperStateOFF);

        subghz_read_raw_set_status(subghz->subghz_read_raw, SubGhzReadRAWStatusREC, "", threshold_rssi);
        if(switch_view) {
            view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdReadRAW);
        }
    } else if(subghz_low_ram_mitigate(subghz, SUBGHZ_LOW_RAM_FREE_HEAP_READ)) {
        /* Re-check rather than trust the gate above blindly: that check and
         * this failure aren't atomic - a qFlipper reconnect's one-time
         * screen-stream session cost (~3.5-4KB, paid on its own RPC thread
         * the instant StartScreenStream arrives, regardless of suppression
         * - see rpc_gui_screen_suppress.h) can land in the handful of
         * instructions between them and tip a reading that had just barely
         * cleared the threshold into an actual save_to_file_init failure.
         * subghz_low_ram_mitigate() gives the soft-lock a chance to free
         * that RAM before conceding. Without this, that race surfaced as
         * the misleading SD-card message even though the card was never
         * the problem - route it to the same warning screen a slower heap
         * decline would have hit instead. */
        size_t bail_free_heap = memmgr_get_free_heap();
        if(furi_get_tick() < subghz->low_ram_grace_until_ms) {
            /* Same post-recovery grace check as the mitigate() bail above -
             * see that comment. */
            FURI_LOG_W(
                TAG,
                "start_listening: save_to_file_init failed with free heap %zu within post-recovery grace, backing out to Start",
                bail_free_heap);
            subghz_debug_log_write(
                "start_listening: save_to_file_init failed with free heap %zu within post-recovery grace, backing out to Start",
                bail_free_heap);
            subghz_scene_receiver_delete_tmp_file(subghz);
            subghz_scene_reader_read_show_start(subghz);
            return;
        }
        FURI_LOG_W(
            TAG,
            "start_listening: save_to_file_init failed with free heap %zu still low after mitigation",
            bail_free_heap);
        subghz_debug_log_write(
            "start_listening: save_to_file_init failed with free heap %zu still low after mitigation",
            bail_free_heap);
        scene_manager_next_scene(subghz->scene_manager, SubGhzSceneLowRamWarning);
    } else {
        furi_string_set(subghz->error_str, "Function requires\nan SD card.");
        scene_manager_next_scene(subghz->scene_manager, SubGhzSceneShowError);
    }
}

static bool subghz_scene_receiver_decode_next(SubGhz* subghz) {
    LevelDuration level_duration;
    SubGhzReceiver* receiver = subghz_txrx_get_receiver(subghz->txrx);
    for(uint32_t read = SUBGHZ_AUTO_READ_DECODE_SAMPLES_PER_TICK; read > 0; --read) {
        level_duration =
            subghz_file_encoder_worker_get_level_duration(subghz->decode_raw_file_worker_encoder);
        if(!level_duration_is_reset(level_duration)) {
            if(level_duration_is_wait(level_duration)) {
                return true;
            }
            bool level = level_duration_get_level(level_duration);
            uint32_t duration = level_duration_get_duration(level_duration);
            if(duration > 1000000) {
                FURI_LOG_E(TAG, "decode_next: LD overflow: %ld", duration);
                return true;
            }
            subghz_receiver_decode(receiver, level, duration);
        } else {
            return false; // EOF
        }
    }
    return true; // more samples pending
}

static void subghz_scene_receiver_no_match_widget_cb(
    GuiButtonType result,
    InputType type,
    void* context) {
    SubGhz* subghz = context;
    if(type != InputTypeShort) return;

    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(
            subghz->view_dispatcher, SubGhzCustomEventReceiverNoMatchBack);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(
            subghz->view_dispatcher, SubGhzCustomEventReceiverNoMatchSave);
    }
}

static void subghz_scene_receiver_show_decode_failed_popup(SubGhz* subghz) {
    subghz_ensure_widget(subghz);
    Widget* widget = subghz->widget;
    widget_add_string_multiline_element(
        widget, 64, 8, AlignCenter, AlignTop, FontPrimary, "No Match");
    widget_add_string_multiline_element(
        widget, 64, 20, AlignCenter, AlignTop, FontSecondary, "Couldn't decode\nthat signal.");
    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Back", subghz_scene_receiver_no_match_widget_cb, subghz);
    widget_add_button_element(
        widget, GuiButtonTypeRight, "Save", subghz_scene_receiver_no_match_widget_cb, subghz);
    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdWidget);
}

static void subghz_scene_receiver_decode_finish(SubGhz* subghz) {
    subghz_debug_log_write("decode_finish: enter");
    subghz_txrx_set_rx_callback(subghz->txrx, NULL, subghz);
    if(subghz_file_encoder_worker_is_running(subghz->decode_raw_file_worker_encoder)) {
        subghz_debug_log_write("decode_finish: stopping worker");
        subghz_file_encoder_worker_stop(subghz->decode_raw_file_worker_encoder);
    }
    subghz_debug_log_write("decode_finish: freeing worker");
    subghz_file_encoder_worker_free(subghz->decode_raw_file_worker_encoder);
    subghz->decode_raw_file_worker_encoder = NULL;

    subghz_scene_receiver_delete_tmp_file(subghz);
    subghz_debug_log_write("decode_finish: tmp file deleted");

    uint16_t new_count = subghz_history_get_item(subghz->history);
    subghz_debug_log_write(
        "decode_finish: history count now %d (was %d before)", new_count, s_auto_history_count_before_decode);
    if(new_count > s_auto_history_count_before_decode) {
        FURI_LOG_I(TAG, "decode_finish: matched, opening info for idx %d", new_count - 1);
        subghz->idx_menu_chosen = new_count - 1;
        dolphin_deed(DolphinDeedSubGhzReceiverInfo);
        scene_manager_next_scene(subghz->scene_manager, SubGhzSceneReceiverInfo);
    } else {
        FURI_LOG_I(TAG, "decode_finish: no match");
        subghz_scene_receiver_show_decode_failed_popup(subghz);
    }
    subghz_debug_log_write("decode_finish: exit");
}

static void subghz_scene_receiver_stop_and_decode(SubGhz* subghz) {
    subghz_debug_log_write("stop_and_decode: enter");
    subghz_txrx_stop(subghz->txrx);
    subghz_debug_log_write("stop_and_decode: txrx_stop done");
    subghz->state_notifications = SubGhzNotificationStateIDLE;

    SubGhzProtocolDecoderRAW* decoder_raw =
        (SubGhzProtocolDecoderRAW*)subghz_txrx_get_decoder(subghz->txrx);
    size_t spl_count = subghz_protocol_raw_get_sample_write(decoder_raw);
    subghz_debug_log_write("stop_and_decode: decoder_raw=%p spl_count=%zu", (void*)decoder_raw, spl_count);
    subghz_protocol_raw_save_to_file_stop(decoder_raw);
    subghz_debug_log_write("stop_and_decode: save_to_file_stop done");

    if(spl_count == 0) {
        subghz_debug_log_write("stop_and_decode: spl_count 0, restarting listening");
        subghz_scene_receiver_delete_tmp_file(subghz);
        /* RAW view is already showing (this only runs mid-Listening) - no
         * view switch needed, see start_listening()'s switch_view comment. */
        subghz_scene_receiver_start_listening(subghz, false);
        return;
    }

    /* Same reasoning as the checkpoints in start_listening()/on_enter above
     * - allocating the file-encoder worker and running the decode loop
     * below needs real headroom, not just a bare pass over the threshold.
     * subghz_low_ram_mitigate() tries a soft-lock first before conceding. */
    if(subghz_low_ram_mitigate(subghz, SUBGHZ_LOW_RAM_FREE_HEAP_READ)) {
        FURI_LOG_W(
            TAG, "stop_and_decode: free heap %zu still low after mitigation, skipping decode",
            memmgr_get_free_heap());
        subghz_debug_log_write(
            "stop_and_decode: free heap %zu still low after mitigation, skipping decode",
            memmgr_get_free_heap());
        subghz_scene_receiver_delete_tmp_file(subghz);
        scene_manager_next_scene(subghz->scene_manager, SubGhzSceneLowRamWarning);
        return;
    }

    FuriString* temp_str = furi_string_alloc();
    furi_string_printf(
        temp_str, "%s/%s%s", SUBGHZ_RAW_FOLDER, AUTO_READ_TMP_FILE_NAME, SUBGHZ_APP_FILENAME_EXTENSION);
    subghz_protocol_raw_gen_fff_data(
        subghz_txrx_get_fff_data(subghz->txrx),
        furi_string_get_cstr(temp_str),
        subghz_txrx_radio_device_get_name(subghz->txrx));
    furi_string_set(subghz->file_path, temp_str);
    furi_string_free(temp_str);
    subghz_debug_log_write("stop_and_decode: fff_data generated, file_path set");

    s_auto_history_count_before_decode = subghz_history_get_item(subghz->history);
    subghz_debug_log_write(
        "stop_and_decode: history_count_before=%d, allocating file encoder worker",
        s_auto_history_count_before_decode);

    subghz->decode_raw_file_worker_encoder = subghz_file_encoder_worker_alloc();
    subghz_debug_log_write(
        "stop_and_decode: worker=%p allocated", (void*)subghz->decode_raw_file_worker_encoder);
    FuriString* file_name = furi_string_alloc();
    if(!flipper_format_rewind(subghz_txrx_get_fff_data(subghz->txrx)) ||
       !flipper_format_read_string(
           subghz_txrx_get_fff_data(subghz->txrx), "File_name", file_name) ||
       !subghz_file_encoder_worker_start(
           subghz->decode_raw_file_worker_encoder,
           furi_string_get_cstr(file_name),
           subghz_txrx_radio_device_get_name(subghz->txrx))) {
        FURI_LOG_E(TAG, "stop_and_decode: couldn't start decode worker");
        subghz_debug_log_write("stop_and_decode: couldn't start decode worker, restarting listening");
        furi_string_free(file_name);
        subghz_file_encoder_worker_free(subghz->decode_raw_file_worker_encoder);
        subghz->decode_raw_file_worker_encoder = NULL;
        subghz_scene_receiver_delete_tmp_file(subghz);
        /* Same as the spl_count==0 case above - RAW view already showing. */
        subghz_scene_receiver_start_listening(subghz, false);
        return;
    }
    subghz_debug_log_write("stop_and_decode: file encoder worker started");
    furi_string_free(file_name);
    furi_delay_ms(100); //worker needs a moment to open the file before we start reading it

    subghz_txrx_set_rx_callback(subghz->txrx, subghz_scene_receiver_add_to_history_callback, subghz);
    subghz_txrx_receiver_set_filter(subghz->txrx, SubGhzProtocolFlag_Decodable);
    subghz_debug_log_write("stop_and_decode: rx_callback + filter set");

    scene_manager_set_scene_state(
        subghz->scene_manager, SubGhzSceneReceiver, SubGhzReceiverAutoStateDecoding);
    /* No dedicated "Decoding..." status exists on this view (SaveKey/
     * LoadKeyIDLE are for the manual save-then-reload flow, not a generic
     * progress message) - IDLE is the simplest safe display and this phase
     * is brief (400 samples/tick, a garage/gate remote's capture is small)
     * so the visual gap is minor. */
    subghz_read_raw_set_status(
        subghz->subghz_read_raw, SubGhzReadRAWStatusIDLE, "",
        subghz_threshold_rssi_get(subghz->threshold_rssi));
    FURI_LOG_I(TAG, "stop_and_decode: decoding started, %zu samples", spl_count);
    subghz_debug_log_write("stop_and_decode: decoding started, %zu samples", spl_count);
}

static void subghz_scene_reader_read_on_enter(SubGhz* subghz) {
    FURI_LOG_I(TAG, "on_enter");

    /* Fresh visit - the Tick handler's own resume-attempt count from any
     * earlier visit no longer applies. See SUBGHZ_LOW_RAM_RESUME_ATTEMPTS_
     * MAX. */
    s_low_ram_resume_attempts = 0;

    /* Suppress qFlipper's screen-stream for the whole time we're anywhere
     * in Read (this scene, its Config excursion, and the Low RAM Warning
     * bounce - each re-suppresses on its own on_enter, see rpc_gui_screen_
     * suppress.h). Deliberately just suppression here, NOT the CLI/RPC
     * disconnect - merely entering Read (showing the Start screen, not yet
     * capturing) should always succeed cheaply and never touch qFlipper's
     * connection. subghz_low_ram_mitigate() (the actual CLI/RPC
     * disconnect) lives inside subghz_scene_receiver_start_listening()
     * instead, which is the single choke point every real "about to run
     * the heavy rx-restart chain" path goes through - the Start/REC button
     * press below, the Config-resume branch right below, silence-timeout
     * auto-restart, and decode-failure restart all call it, so it doesn't
     * need duplicating here too. */
    rpc_gui_screen_stream_set_suppressed(true);

    subghz_ensure_history(subghz);

    /* Fresh entry (or return from the No Match screen's Back button) reads
     * as Start (scene state defaults to 0, same value as
     * SubGhzReceiverAutoStateStart, so a first-ever entry lands here too)
     * and shows the idle Start screen. Returning from Config while a
     * capture was running (auto_state still Listening - the Config-entry
     * case above already stopped RX rather than leaving it running) instead
     * restarts listening directly: start_listening() begins a fresh
     * recording immediately, no extra Start press needed, same as if RX
     * had been left running through Config and discarded here instead. */
    SubGhzReceiverAutoState auto_state = (SubGhzReceiverAutoState)
        scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneReceiver);
    if(auto_state == SubGhzReceiverAutoStateListening) {
        /* Genuine switch needed here - the previously-active view was
         * Config's (or, on the very first entry, whatever launched Read),
         * never RAW itself. */
        subghz_scene_receiver_start_listening(subghz, true);
    } else {
        subghz_scene_reader_read_show_start(subghz);
    }

    FURI_LOG_I(TAG, "on_enter: done");
}

static bool subghz_scene_reader_read_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        /* Only reachable while the No Match widget is on screen - Listening's
         * Back goes through the read_raw view's own input handler (converts
         * to ViewReadRAWIDLE, handled below) before it would ever reach here
         * as a raw Back event. Treat the hardware Back key the same as the
         * widget's own on-screen "Back" button: return to the Start screen,
         * rather than falling through to the framework default (which would
         * pop this whole scene off the stack and exit Read unexpectedly). */
        if(subghz->widget) {
            widget_reset(subghz->widget);
        }
        subghz_scene_reader_read_show_start(subghz);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SubGhzCustomEventViewReadRAWBack:
            /* Reachable if Back is pressed while the view's status isn't
             * REC (e.g. mid-Decoding, status==IDLE) - falls through to the
             * same handler as ViewReadRAWIDLE, which branches on the
             * current auto-state to clean up correctly either way. */
        case SubGhzCustomEventViewReadRAWIDLE: {
            /* ViewReadRAWIDLE only fires here from a genuine Back (or OK)
             * press while the view's status==REC - our own silence-driven
             * stop-and-decode transition is called directly from the Tick
             * handler below, never through this event. Either event means
             * "user wants to exit Read". */
            SubGhzReceiverAutoState auto_state = (SubGhzReceiverAutoState)
                scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneReceiver);
            subghz_debug_log_write(
                "exit_read: enter, event=%lu auto_state=%d",
                (unsigned long)event.event,
                (int)auto_state);

            if(auto_state == SubGhzReceiverAutoStateStart && s_auto_start_countdown_sec > 0) {
                /* Countdown running on the Start screen - Back cancels it
                 * and stays put, rather than exiting Read. */
                s_auto_start_countdown_sec = 0;
                s_auto_start_cancelled = true;
                subghz_read_raw_set_start_countdown(subghz->subghz_read_raw, 0);
                consumed = true;
                break;
            }

            /* Reset to Start immediately - this branch always exits Read
             * entirely (search_and_switch_to_previous_scene below), so
             * nothing else will reset it. Without this, auto_state stayed
             * whatever it was (Listening, almost always) after leaving, so
             * the NEXT time Read was entered on_enter's Listening check read
             * true from stale leftover state and skipped the Start screen,
             * jumping straight into a fresh recording regardless of how
             * this exit happened (OK or Back) - every subsequent Read entry
             * for the rest of the app session, not just the next one. */
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneReceiver, SubGhzReceiverAutoStateStart);

            if(auto_state == SubGhzReceiverAutoStateDecoding) {
                subghz_txrx_set_rx_callback(subghz->txrx, NULL, subghz);
                if(subghz->decode_raw_file_worker_encoder) {
                    if(subghz_file_encoder_worker_is_running(subghz->decode_raw_file_worker_encoder)) {
                        subghz_file_encoder_worker_stop(subghz->decode_raw_file_worker_encoder);
                    }
                    subghz_file_encoder_worker_free(subghz->decode_raw_file_worker_encoder);
                    subghz->decode_raw_file_worker_encoder = NULL;
                }
            } else if(auto_state == SubGhzReceiverAutoStateListening) {
                subghz_debug_log_write("exit_read: stopping txrx");
                subghz_txrx_stop(subghz->txrx);
                subghz_debug_log_write("exit_read: txrx_stop done");
                subghz->state_notifications = SubGhzNotificationStateIDLE;
                SubGhzProtocolDecoderRAW* decoder_raw =
                    (SubGhzProtocolDecoderRAW*)subghz_txrx_get_decoder(subghz->txrx);
                subghz_debug_log_write("exit_read: decoder_raw=%p", (void*)decoder_raw);
                subghz_protocol_raw_save_to_file_stop(decoder_raw);
                subghz_debug_log_write("exit_read: save_to_file_stop done");
                subghz_txrx_hopper_set_state(subghz->txrx, SubGhzHopperStateOFF);
                subghz_debug_log_write("exit_read: hopper off done");
                subghz_txrx_set_rx_callback(subghz->txrx, NULL, subghz);
                subghz_debug_log_write("exit_read: rx_callback cleared");
            }
            /* else: SubGhzReceiverAutoStateStart - the Start screen, before
             * Start/OK was ever pressed. No decoder was loaded and no RX was
             * running this visit (subghz_scene_reader_read_show_start()
             * only sets up the view - see its comment), so there's nothing
             * to tear down here. Falling straight through to the shared
             * cleanup below used to instead call subghz_txrx_get_decoder()
             * and hand whatever garbage/stale pointer it returned (the
             * struct is plain malloc()'d, decoder_result is only ever
             * written by a real protocol load) to subghz_protocol_raw_
             * save_to_file_stop(), which furi_check()s its argument - a
             * guaranteed or near-guaranteed crash on Back-before-Start. */
            subghz_scene_receiver_delete_tmp_file(subghz);
            subghz_debug_log_write("exit_read: tmp file deleted");

            subghz_txrx_set_default_preset(subghz->txrx, subghz->last_settings->frequency);
            subghz_debug_log_write("exit_read: default preset set, switching scene");
            if(!scene_manager_search_and_switch_to_previous_scene(
                   subghz->scene_manager, SubGhzSceneStart)) {
                scene_manager_stop(subghz->scene_manager);
                view_dispatcher_stop(subghz->view_dispatcher);
            }
            subghz_debug_log_write("exit_read: scene switch done");
            consumed = true;
            break;
        }

        case SubGhzCustomEventViewReadRAWConfig: {
            /* Keyed by SubGhzSceneReadRAW (not SubGhzSceneReceiver) to match
             * manual Read RAW's own Config-entry case below - Receiver
             * Config's on_enter checks this exact key to decide whether to
             * show the RSSI Threshold item, so Read needs to set the same
             * one it reads. */
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneReadRAW, SubGhzCustomEventManagerSet);

            /* Stop RX before leaving, rather than leaving it running in the
             * background for the whole Config visit (the old shared on_exit
             * behavior, still correct for manual Read RAW's genuine resume-
             * in-place recording). Read's own on_enter always discards
             * whatever was captured and restarts fresh on return (see its
             * comment) regardless of whether RX kept running through Config
             * or not, so leaving it running bought nothing here - just
             * pointless radio time and SD writes for a capture that was
             * always going to be thrown away. auto_state is left as
             * Listening so on_enter still auto-resumes a fresh recording
             * immediately on return, same as before. */
            SubGhzReceiverAutoState auto_state = (SubGhzReceiverAutoState)
                scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneReceiver);
            if(auto_state == SubGhzReceiverAutoStateListening) {
                SubGhzProtocolDecoderRAW* decoder_raw =
                    (SubGhzProtocolDecoderRAW*)subghz_txrx_get_decoder(subghz->txrx);
                subghz_txrx_stop(subghz->txrx);
                subghz_protocol_raw_save_to_file_stop(decoder_raw);
                subghz_scene_receiver_delete_tmp_file(subghz);
            }

            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneReceiverConfig);
            consumed = true;
            break;
        }

        case SubGhzCustomEventViewReadRAWREC:
            /* OK pressed on the Start screen - RAW view already showing
             * (Start status), no switch needed. This was the exact bug: a
             * redundant switch_to_view() here made the view briefly "exit
             * itself" while status was already REC, self-firing a spurious
             * ViewReadRAWIDLE that killed the recording within ~500ms of
             * every single Start press - see start_listening()'s comment. */
            subghz_scene_receiver_start_listening(subghz, false);
            consumed = true;
            break;

        case SubGhzCustomEventReceiverNoMatchBack:
            widget_reset(subghz->widget);
            subghz_scene_reader_read_show_start(subghz);
            consumed = true;
            break;

        case SubGhzCustomEventReceiverNoMatchSave:
            widget_reset(subghz->widget);
            if(subghz_file_available(subghz) && subghz_scene_read_raw_update_filename(subghz)) {
                scene_manager_set_scene_state(
                    subghz->scene_manager, SubGhzSceneReadRAW, SubGhzCustomEventManagerSetRAW);
                subghz_rx_key_state_set(subghz, SubGhzRxKeyStateBack);
                scene_manager_next_scene(subghz->scene_manager, SubGhzSceneSaveName);
            } else {
                subghz_scene_reader_read_show_start(subghz);
            }
            consumed = true;
            break;

        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        SubGhzReceiverAutoState auto_state = (SubGhzReceiverAutoState)scene_manager_get_scene_state(
            subghz->scene_manager, SubGhzSceneReceiver);

        /* Low-RAM watchdog: qFlipper's screen-stream is the main thing
         * observed pushing free heap this low during normal Read use -
         * see rpc_gui_screen_suppress.h for the targeted fix (suppresses
         * just that, not all of USB/CLI, so this watchdog can keep
         * watching heap over a live CLI connection instead of killing it
         * outright).
         *
         * Only while actually mid-capture/decode (Listening or Decoding) -
         * never while sitting idle on the Start screen. This Tick handler
         * runs on every tick regardless of auto_state, and previously ran
         * this whole check unconditionally too: after enough heap had
         * already been used up by earlier Read/Read RAW cycles, simply
         * sitting on the idle Start screen (Start not yet pressed) still
         * accumulated ticks here and disconnected qFlipper on its own
         * within about a second of opening Read - before the user had done
         * anything, contradicting the "CLI disconnects at Start press, not
         * before" design. Manual Read RAW never showed this because its own
         * on_enter has no equivalent per-Tick watchdog at all - the actual
         * choke-point checks (start_listening(), stop_and_decode()) still
         * run exactly once each, right where the heavy chain begins,
         * independent of this.
         *
         * Single unified threshold now (SUBGHZ_LOW_RAM_FREE_HEAP_READ) - require
         * SUBGHZ_LOW_RAM_TRIGGER_TICKS consecutive ticks before acting at
         * all, so a momentary spike (CLI's own baseline cost briefly
         * touching the edge, a GC-style dip, etc.) doesn't trigger
         * anything. Once debounced, try subghz_low_ram_mitigate() (soft-
         * locks CLI/RPC, which frees real RAM if qFlipper's screen-stream
         * was the cause) before conceding - only bail to the warning
         * screen if heap is still low after that. */
        if(auto_state != SubGhzReceiverAutoStateStart) {
            size_t free_heap = memmgr_get_free_heap();
            bool low_ram_condition = free_heap < SUBGHZ_LOW_RAM_FREE_HEAP_READ;

            if(low_ram_condition) {
                s_low_ram_tick_count++;
            } else {
                s_low_ram_tick_count = 0;
            }

            if(s_low_ram_tick_count >= SUBGHZ_LOW_RAM_TRIGGER_TICKS &&
               !subghz_low_ram_mitigate(subghz, SUBGHZ_LOW_RAM_FREE_HEAP_READ)) {
                s_low_ram_tick_count = 0;
            }

            if(s_low_ram_tick_count >= SUBGHZ_LOW_RAM_TRIGGER_TICKS) {
                uint32_t sustained_ticks = s_low_ram_tick_count;
                s_low_ram_tick_count = 0;

                /* Cheap early out - if heap already cleared by the time we
                 * actually get here, don't even touch the running session. */
                if(memmgr_get_free_heap() >= SUBGHZ_LOW_RAM_FREE_HEAP_READ) {
                    FURI_LOG_I(
                        TAG,
                        "tick: free heap %zu recovered by bail time (was low for %lu ticks) - skipping warning",
                        memmgr_get_free_heap(),
                        (unsigned long)sustained_ticks);
                    return true;
                }

                /* Was mid-Listening (actively recording) when this fired -
                 * on_exit only tears down Decoding state (Config's own
                 * round trip deliberately leaves Listening's RX/file
                 * running so a signal keeps accumulating across a Config
                 * visit), so nothing else stops this session on the way
                 * out. Recovery (or the grace fallback below) always
                 * restarts fresh anyway (never resumes this exact session),
                 * so the abandoned RX worker + open scratch file need to be
                 * torn down here or they never get closed - previously left
                 * free heap unable to recover on its own, and the next
                 * start_listening() call's file remove+reopen failed with a
                 * misleading "requires an SD card" error since the old
                 * handle was still holding the file open. */
                bool was_listening = (auto_state == SubGhzReceiverAutoStateListening);
                if(was_listening) {
                    SubGhzProtocolDecoderRAW* decoder_raw =
                        (SubGhzProtocolDecoderRAW*)subghz_txrx_get_decoder(subghz->txrx);
                    subghz_txrx_stop(subghz->txrx);
                    subghz_protocol_raw_save_to_file_stop(decoder_raw);
                    subghz_scene_receiver_delete_tmp_file(subghz);
                }

                /* Re-check after tearing the session down, not just before -
                 * device logs confirmed the RX worker's ~10.8KB doesn't
                 * always show up in a read taken immediately after
                 * subghz_txrx_stop() returns, but reliably does within a
                 * couple hundred ms (same settle-then-recheck pattern
                 * subghz_low_ram_mitigate() already uses for its own
                 * teardown). One canonical read from here on, reused for
                 * every check/log below. */
                furi_delay_ms(SUBGHZ_LOW_RAM_TICK_BAIL_RECHECK_WAIT_MS);
                size_t bail_free_heap = memmgr_get_free_heap();

                /* Resuming means re-allocating the RX worker, which costs
                 * SUBGHZ_GARAGE_WORKER_RAM_COST all over again - checking
                 * against the bare SUBGHZ_LOW_RAM_FREE_HEAP_READ trip point
                 * here isn't enough margin, since that cost alone is bigger
                 * than the entire 10000-14000 hysteresis gap. A heap that
                 * had just barely cleared 14000 would drop straight back
                 * under 10000 the instant the worker reallocated, and this
                 * whole stop/recover/resume cycle would repeat forever. Only
                 * attempt the direct resume if there's real margin left
                 * over after paying that cost again - and cap how many
                 * times this loop is allowed to try, in case some other,
                 * unanticipated cost keeps eating the margin too. */
                bool safe_to_resume = bail_free_heap >=
                                      (SUBGHZ_LOW_RAM_FREE_HEAP + SUBGHZ_GARAGE_WORKER_RAM_COST);
                if(safe_to_resume && s_low_ram_resume_attempts >= SUBGHZ_LOW_RAM_RESUME_ATTEMPTS_MAX) {
                    FURI_LOG_W(
                        TAG,
                        "tick: free heap %zu recovered but already resumed %lu times this visit - forcing full bail",
                        bail_free_heap,
                        (unsigned long)s_low_ram_resume_attempts);
                    subghz_debug_log_write(
                        "tick: free heap %zu recovered but already resumed %lu times this visit - forcing full bail",
                        bail_free_heap,
                        (unsigned long)s_low_ram_resume_attempts);
                    safe_to_resume = false;
                }
                if(safe_to_resume) {
                    s_low_ram_resume_attempts++;
                    FURI_LOG_I(
                        TAG,
                        "tick: free heap %zu recovered with margin after teardown (was low for %lu ticks) - resuming (attempt %lu)",
                        bail_free_heap,
                        (unsigned long)sustained_ticks,
                        (unsigned long)s_low_ram_resume_attempts);
                    subghz_debug_log_write(
                        "tick: free heap %zu recovered with margin after teardown (was low for %lu ticks) - resuming (attempt %lu)",
                        bail_free_heap,
                        (unsigned long)sustained_ticks,
                        (unsigned long)s_low_ram_resume_attempts);
                    if(was_listening) {
                        subghz_scene_receiver_start_listening(subghz, false);
                    }
                    return true;
                }
                if(furi_get_tick() < subghz->low_ram_grace_until_ms) {
                    /* Same post-recovery grace check as start_listening()'s
                     * own bails (subghz_i.h) - this debounced watchdog
                     * already required a full sustained second of low heap
                     * before getting here, but that second can still fall
                     * entirely inside the grace window right after a
                     * recovery resumed Listening. Treat it the same way:
                     * back out to Start quietly instead of re-showing
                     * "Uh Oh!" so soon after the last one cleared. */
                    FURI_LOG_W(
                        TAG,
                        "tick: free heap %zu still low within post-recovery grace, backing out to Start",
                        bail_free_heap);
                    subghz_debug_log_write(
                        "tick: free heap %zu still low within post-recovery grace, backing out to Start",
                        bail_free_heap);
                    subghz_scene_reader_read_show_start(subghz);
                    return true;
                }
                FURI_LOG_W(
                    TAG,
                    "tick: free heap %zu still low after mitigation, sustained for %lu ticks, bailing to warning",
                    bail_free_heap,
                    (unsigned long)sustained_ticks);
                subghz_debug_log_write(
                    "tick: free heap %zu still low after mitigation, sustained for %lu ticks, bailing to warning",
                    bail_free_heap,
                    (unsigned long)sustained_ticks);
                scene_manager_next_scene(subghz->scene_manager, SubGhzSceneLowRamWarning);
                return true;
            }
        } else {
            s_low_ram_tick_count = 0;
        }

        if(auto_state == SubGhzReceiverAutoStateListening) {
            subghz_read_raw_recording_tick(subghz->subghz_read_raw);

            SubGhzProtocolDecoderRAW* decoder_raw =
                (SubGhzProtocolDecoderRAW*)subghz_txrx_get_decoder(subghz->txrx);
            size_t sample_count = subghz_protocol_raw_get_sample_write(decoder_raw);
            subghz_read_raw_update_sample_write(subghz->subghz_read_raw, sample_count);

            float rssi_value = subghz_txrx_radio_device_get_rssi(subghz->txrx);
            SubGhzThresholdRssiData ret_rssi =
                subghz_threshold_get_rssi_data(subghz->threshold_rssi, rssi_value);
            subghz_read_raw_add_data_rssi(subghz->subghz_read_raw, ret_rssi.rssi, true);

            if(sample_count > s_auto_last_sample_count) {
                /* The RAW decoder writes samples for any RF transition,
                 * ambient noise included - gate the START of a confirmed
                 * activity window on the RSSI threshold too, so a noise
                 * blip alone never begins a capture. Once activity has
                 * genuinely started, drop back to pure sample growth so a
                 * real transmission's natural mid-signal RSSI dips don't
                 * prematurely cut it off. */
                if(s_auto_has_activity || ret_rssi.is_above) {
                    s_auto_has_activity = true;
                    s_auto_silence_ticks = 0;
                }
            } else if(s_auto_has_activity) {
                s_auto_silence_ticks++;
            }
            s_auto_last_sample_count = sample_count;
            s_auto_tick_count++;

            if(s_auto_has_activity && s_auto_silence_ticks >= SUBGHZ_AUTO_READ_SILENCE_TICKS) {
                FURI_LOG_I(
                    TAG,
                    "listening: silence threshold reached at tick %lu, %zu samples captured",
                    (unsigned long)s_auto_tick_count,
                    sample_count);
                subghz_debug_log_write(
                    "listening: silence threshold reached at tick %lu, %zu samples captured",
                    (unsigned long)s_auto_tick_count,
                    sample_count);
                subghz_scene_receiver_stop_and_decode(subghz);
            } else {
                notification_message(subghz->notifications, &sequence_blink_cyan_10);
            }

            /* Frequency hopping (see Settings > Hopping, subghz_scene_
             * receiver_config.c): while genuinely idle with no activity
             * detected yet this visit, let the hopper engine (helpers/
             * subghz_txrx.c - stock's own, unmodified, already-linked-in
             * hopper state machine) scan across the frequency table looking
             * for a live signal, retuning at most once per tick. The
             * instant s_auto_has_activity flips true above (a capture has
             * genuinely started), this stops being called at all, freezing
             * the radio on the frequency that just produced it for the
             * remainder of the capture - hopper_update() itself is also
             * gated on hopper_state (a no-op switch-case return when
             * Hopping is OFF), so this costs nothing extra when the toggle
             * is off. */
            if(!s_auto_has_activity) {
                subghz_txrx_hopper_update(subghz->txrx, subghz->last_settings->hopping_threshold);
            }
        } else if(auto_state == SubGhzReceiverAutoStateStart && !s_auto_start_cancelled) {
            /* Idle Start screen countdown - see SUBGHZ_AUTO_START_COUNTDOWN_
             * SEC and s_auto_start_countdown_sec above. An OK press
             * (SubGhzCustomEventViewReadRAWREC above) calls start_listening()
             * directly and never touches these statics, so it always skips
             * straight past whatever the countdown is doing. */
            s_auto_start_subtick++;
            if(s_auto_start_subtick >= 10) {
                s_auto_start_subtick = 0;
                if(s_auto_start_countdown_sec > 0) {
                    s_auto_start_countdown_sec--;
                }
                if(s_auto_start_countdown_sec == 0) {
                    subghz_scene_receiver_start_listening(subghz, false);
                    return true;
                }
                subghz_read_raw_set_start_countdown(
                    subghz->subghz_read_raw, s_auto_start_countdown_sec);
            }
        } else if(subghz->decode_raw_file_worker_encoder) {
            subghz_debug_log_write("decode tick: calling decode_next");
            bool more = subghz_scene_receiver_decode_next(subghz);
            subghz_debug_log_write("decode tick: decode_next returned %d", (int)more);
            if(!more) {
                subghz_debug_log_write("decode tick: calling decode_finish");
                subghz_scene_receiver_decode_finish(subghz);
                subghz_debug_log_write("decode tick: decode_finish returned");
            }
        }
        /* On a failed match, decode_finish() frees decode_raw_file_worker_
         * encoder and shows the No Match widget via view_dispatcher_switch_
         * to_view() without changing scene or scene state - Tick routes by
         * scene, not view, so state stayed Decoding and the next Tick called
         * decode_next() against a freed worker (furi_assert is a no-op
         * outside FURI_DEBUG builds, so nothing caught it before the deref).
         * The NULL guard above is the fix - state only changes once the
         * widget's Back or Save button is pressed (see NoMatchBack/
         * NoMatchSave above), both of which eventually land back on the
         * Start screen. */
    }
    return consumed;
}

static void subghz_scene_reader_read_on_exit(SubGhz* subghz) {
    /* Un-suppress unconditionally - if this exit is a hop to Config or the
     * Low RAM Warning screen, both re-suppress in their own on_enter, so
     * there's no live-frame gap while genuinely still mid-Read. ReceiverInfo
     * deliberately stays un-suppressed (a static post-decode display,
     * reached only once heap already proved adequate to finish a decode -
     * no reason to hide it from a connected qFlipper). */
    rpc_gui_screen_stream_set_suppressed(false);
    subghz_cli_soft_unlock(subghz);

    /* Listening state needs no RX cleanup here - every path that leaves
     * this scene while still Listening already stopped RX itself before
     * getting here (the Config-entry case in on_event, and the Tick
     * handler's low-RAM bail points), rather than leaving a session running
     * in the background for wherever this scene is headed next. Only
     * Decoding needs defensive cleanup here - shouldn't normally still be
     * mid-decode when this scene is left (decode_finish() already cleans
     * up before this scene ever pushes another one), but safe to guard. */
    SubGhzReceiverAutoState auto_state = (SubGhzReceiverAutoState)scene_manager_get_scene_state(
        subghz->scene_manager, SubGhzSceneReceiver);
    if(auto_state == SubGhzReceiverAutoStateDecoding) {
        subghz_txrx_set_rx_callback(subghz->txrx, NULL, subghz);
        if(subghz->decode_raw_file_worker_encoder) {
            if(subghz_file_encoder_worker_is_running(subghz->decode_raw_file_worker_encoder)) {
                subghz_file_encoder_worker_stop(subghz->decode_raw_file_worker_encoder);
            }
            subghz_file_encoder_worker_free(subghz->decode_raw_file_worker_encoder);
            subghz->decode_raw_file_worker_encoder = NULL;
        }
        subghz_scene_receiver_delete_tmp_file(subghz);
    }
    if(subghz->popup) {
        popup_reset(subghz->popup);
    }
    if(subghz->widget) {
        widget_reset(subghz->widget);
    }
}

/* ========================================================================
 * Registered scene entry points.
 * ======================================================================== */

void subghz_scene_read_raw_on_enter(void* context) {
    SubGhz* subghz = context;

    /* Auto Read's Stop (OK/Back during REC) always exits Read entirely -
     * manual Read RAW's Stop genuinely lands on the IDLE/Erase/Send/Save
     * screen. See subghz_read_raw_set_auto_capture_mode()'s comment. */
    subghz_read_raw_set_auto_capture_mode(subghz->subghz_read_raw, subghz->reader_read_mode);

    if(subghz->reader_read_mode) {
        subghz_scene_reader_read_on_enter(subghz);
        return;
    }

    FURI_LOG_I(TAG, "on_enter");

    /* Same reasoning as Read's on_enter above - see rpc_gui_screen_
     * suppress.h. This scene is also used for viewing/sending an
     * already-saved file (not just live recording), which isn't nearly as
     * RAM-sensitive, but there's no harm suppressing there too - simpler
     * than splitting behavior by subghz_rx_key_state_get(). No proactive
     * CLI lock here - entering this scene doesn't itself allocate the
     * heavy RX worker/file chain (that only happens on REC, already
     * gated by subghz_low_ram_mitigate() in the REC handler below). */
    rpc_gui_screen_stream_set_suppressed(true);

    FuriString* file_name = furi_string_alloc();

    float threshold_rssi = subghz_threshold_rssi_get(subghz->threshold_rssi);
    switch(subghz_rx_key_state_get(subghz)) {
    case SubGhzRxKeyStateBack:
        subghz_read_raw_set_status(
            subghz->subghz_read_raw, SubGhzReadRAWStatusIDLE, "", threshold_rssi);
        break;
    case SubGhzRxKeyStateRAWLoad:
    case SubGhzRxKeyStateRAWMore:
        path_extract_filename(subghz->file_path, file_name, true);
        subghz_read_raw_set_status(
            subghz->subghz_read_raw,
            SubGhzReadRAWStatusLoadKeyTX,
            furi_string_get_cstr(file_name),
            threshold_rssi);
        /* Loading an EXISTING file (from Saved Menu, or returning from
         * the More menu) — there's no in-progress recording session to
         * discard, so don't offer "New" here. */
        subghz_read_raw_set_allow_new(subghz->subghz_read_raw, false);

        /* Apply the persisted zoom level (remembered across files/sessions
         * per the user's request), centered at the start of the file. */
        {
            uint8_t zoom = (uint8_t)subghz->last_settings->raw_playback_zoom_level;
            subghz_read_raw_set_zoom_level(subghz->subghz_read_raw, zoom);
            if(zoom > 0) {
                subghz_scene_read_raw_load_envelope_zoomed(subghz, zoom, 0);
            } else {
                subghz_scene_read_raw_load_envelope(subghz);
            }
        }
        break;
    case SubGhzRxKeyStateRAWSave:
        path_extract_filename(subghz->file_path, file_name, true);
        subghz_read_raw_set_status(
            subghz->subghz_read_raw,
            SubGhzReadRAWStatusSaveKey,
            furi_string_get_cstr(file_name),
            threshold_rssi);
        /* Just finished recording and saving in THIS session — offer
         * "New" so the user can immediately start another recording
         * without backing all the way out and back in. */
        subghz_read_raw_set_allow_new(subghz->subghz_read_raw, true);
        break;
    default:
        subghz_read_raw_set_status(
            subghz->subghz_read_raw, SubGhzReadRAWStatusStart, "", threshold_rssi);
        s_manual_start_countdown_sec = SUBGHZ_AUTO_START_COUNTDOWN_SEC;
        s_manual_start_subtick = 0;
        s_manual_start_countdown_fired = false;
        s_manual_start_cancelled = false;
        subghz_read_raw_set_start_countdown(subghz->subghz_read_raw, s_manual_start_countdown_sec);
        break;
    }

    if((subghz_rx_key_state_get(subghz) != SubGhzRxKeyStateBack) &&
       (subghz_rx_key_state_get(subghz) != SubGhzRxKeyStateRAWLoad)) {
        subghz_rx_key_state_set(subghz, SubGhzRxKeyStateIDLE);

        if(furi_string_empty(file_name)) {
            subghz_txrx_set_preset_internal(
                subghz->txrx,
                subghz->last_settings->frequency,
                subghz->last_settings->preset_index,
                subghz->last_settings->tx_power);
        }
    }
    subghz_scene_read_raw_update_statusbar(subghz);

    //set callback view raw
    subghz_read_raw_set_callback(subghz->subghz_read_raw, subghz_scene_read_raw_callback, subghz);

    //apply the global Bar/Line display preference
    //(same setting used by the Signal Visualizer — one consistent
    // visual language across the whole app, no "Classic" mode exists)
    subghz_read_raw_set_viz_mode(
        subghz->subghz_read_raw,
        (SubGhzReadRawVizMode)subghz->last_settings->visualizer_display_mode);

    /* Reset the "gave up on this group" latch before attempting the load,
     * not just trust furi_check() to hold - see Read's matching comment
     * above (Read shares this same SubGhzTxRx instance, so a low-RAM
     * failure there could latch this and previously crashed this screen
     * outright the next time it ran, long after heap recovered). */
    subghz_txrx_reset_protocol_load_failed(subghz->txrx);
    if(!subghz_txrx_load_decoder_by_name_protocol(subghz->txrx, SUBGHZ_PROTOCOL_RAW_NAME)) {
        furi_string_free(file_name);
        scene_manager_next_scene(subghz->scene_manager, SubGhzSceneProtocolLoadError);
        return;
    }

    //set filter RAW feed
    subghz_txrx_receiver_set_filter(subghz->txrx, SubGhzProtocolFlag_RAW);
    furi_string_free(file_name);

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdReadRAW);
}

bool subghz_scene_read_raw_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;

    if(subghz->reader_read_mode) {
        return subghz_scene_reader_read_on_event(context, event);
    }

    bool consumed = false;
    SubGhzProtocolDecoderRAW* decoder_raw =
        (SubGhzProtocolDecoderRAW*)subghz_txrx_get_decoder(subghz->txrx);
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SubGhzCustomEventViewReadRAWBack:

            if(subghz_read_raw_get_status(subghz->subghz_read_raw) == SubGhzReadRAWStatusStart &&
               s_manual_start_countdown_sec > 0) {
                /* Countdown running on the Start screen - Back cancels it
                 * and stays put, rather than exiting Read RAW. */
                s_manual_start_countdown_sec = 0;
                s_manual_start_cancelled = true;
                subghz_read_raw_set_start_countdown(subghz->subghz_read_raw, 0);
                consumed = true;
                break;
            }

            subghz_txrx_stop(subghz->txrx);
            //Stop save file
            subghz_protocol_raw_save_to_file_stop(decoder_raw);
            subghz->state_notifications = SubGhzNotificationStateIDLE;
            //needed save?
            if((subghz_rx_key_state_get(subghz) == SubGhzRxKeyStateAddKey) ||
               (subghz_rx_key_state_get(subghz) == SubGhzRxKeyStateBack)) {
                subghz_rx_key_state_set(subghz, SubGhzRxKeyStateExit);
                if(subghz_scene_read_raw_update_filename(subghz)) {
                    furi_string_set(subghz->file_path_tmp, subghz->file_path);
                } else {
                    furi_string_reset(subghz->file_path_tmp);
                }
                scene_manager_next_scene(subghz->scene_manager, SubGhzSceneNeedSaving);
            } else {
                //Restore default setting
                if(subghz->raw_send_only) {
                    subghz_txrx_set_default_preset(subghz->txrx, 0);
                } else {
                    subghz_txrx_set_default_preset(subghz->txrx, subghz->last_settings->frequency);
                }
                if(!scene_manager_search_and_switch_to_previous_scene(
                       subghz->scene_manager, SubGhzSceneSaved)) {
                    if(!scene_manager_search_and_switch_to_previous_scene(
                           subghz->scene_manager, SubGhzSceneStart)) {
                        scene_manager_stop(subghz->scene_manager);
                        view_dispatcher_stop(subghz->view_dispatcher);
                    }
                }
            }
            consumed = true;
            break;

        case SubGhzCustomEventViewReadRAWTXPause:
            /* User pressed OK during file TX — stop the TX worker and park. */
            subghz_txrx_stop(subghz->txrx);
            subghz->state_notifications = SubGhzNotificationStateIDLE;
            notification_message(subghz->notifications, &sequence_reset_rgb);
            consumed = true;
            break;

        case SubGhzCustomEventViewReadRAWTXResume: {
            /* User pressed OK while paused — restart TX from the beginning.
             * (Precise seek from an arbitrary position is not yet supported
             *  by the SubGHz TX worker; we restart from the file start.) */
            FlipperFormat* fff = subghz_txrx_get_fff_data(subghz->txrx);
            if(fff) {
                flipper_format_rewind(fff);
                if(subghz_tx_start(subghz, fff)) {
                    subghz->state_notifications = SubGhzNotificationStateTx;
                }
            }
            consumed = true;
            break;
        }

        case SubGhzCustomEventViewReadRAWZoomIn:
        case SubGhzCustomEventViewReadRAWZoomOut: {
            /* The view already updated its own zoom_level on Up/Down; read
             * it back, re-derive the envelope for the new window centered
             * on the current seek position, and persist the new level so
             * it carries over to the next file (per the user's request). */
            uint8_t zoom = subghz_read_raw_get_zoom_level(subghz->subghz_read_raw);
            uint8_t center = subghz_read_raw_get_seek_pct(subghz->subghz_read_raw);
            if(zoom > 0) {
                subghz_scene_read_raw_load_envelope_zoomed(subghz, zoom, center);
            } else {
                subghz_scene_read_raw_load_envelope(subghz);
            }
            subghz->last_settings->raw_playback_zoom_level = zoom;
            subghz_garage_last_settings_save(subghz->last_settings);
            consumed = true;
            break;
        }

        case SubGhzCustomEventViewReadRAWTXRXStop:
            subghz_txrx_stop(subghz->txrx);
            subghz->state_notifications = SubGhzNotificationStateIDLE;
            consumed = true;
            break;

        case SubGhzCustomEventViewReadRAWConfig:
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneReadRAW, SubGhzCustomEventManagerSet);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneReceiverConfig);
            consumed = true;
            break;

        case SubGhzCustomEventViewReadRAWErase:
            if((subghz_rx_key_state_get(subghz) == SubGhzRxKeyStateAddKey) ||
               (subghz_rx_key_state_get(subghz) == SubGhzRxKeyStateBack)) {
                if(subghz_scene_read_raw_update_filename(subghz)) {
                    furi_string_set(subghz->file_path_tmp, subghz->file_path);
                    subghz_delete_file(subghz);
                }
            }
            subghz_rx_key_state_set(subghz, SubGhzRxKeyStateIDLE);
            notification_message(subghz->notifications, &sequence_reset_rgb);
            /* "New" - view just moved status back to Start (see the Left-
             * arrow handler in views/subghz_read_raw.c), so the auto-start
             * countdown re-arms fresh here too. */
            s_manual_start_countdown_sec = SUBGHZ_AUTO_START_COUNTDOWN_SEC;
            s_manual_start_subtick = 0;
            s_manual_start_countdown_fired = false;
            s_manual_start_cancelled = false;
            subghz_read_raw_set_start_countdown(subghz->subghz_read_raw, s_manual_start_countdown_sec);
            consumed = true;
            break;

        case SubGhzCustomEventViewReadRAWMore:
            if(subghz_file_available(subghz)) {
                if(subghz_scene_read_raw_update_filename(subghz)) {
                    scene_manager_set_scene_state(
                        subghz->scene_manager, SubGhzSceneReadRAW, SubGhzCustomEventManagerSet);
                    if(subghz_rx_key_state_get(subghz) != SubGhzRxKeyStateRAWLoad) {
                        subghz_rx_key_state_set(subghz, SubGhzRxKeyStateRAWMore);
                    }
                    scene_manager_next_scene(subghz->scene_manager, SubGhzSceneMoreRAW);
                    consumed = true;
                } else {
                    furi_crash("SubGhz: RAW file name update error.");
                }
            } else {
                if(!scene_manager_search_and_switch_to_previous_scene(
                       subghz->scene_manager, SubGhzSceneStart)) {
                    scene_manager_stop(subghz->scene_manager);
                    view_dispatcher_stop(subghz->view_dispatcher);
                }
            }
            break;

        case SubGhzCustomEventViewReadRAWSendStart:

            if(subghz_file_available(subghz) && subghz_scene_read_raw_update_filename(subghz)) {
                //start send
                subghz->state_notifications = SubGhzNotificationStateIDLE;
                if(!subghz_tx_start(subghz, subghz_txrx_get_fff_data(subghz->txrx))) {
                    subghz_rx_key_state_set(subghz, SubGhzRxKeyStateBack);
                    subghz_read_raw_set_status(
                        subghz->subghz_read_raw,
                        SubGhzReadRAWStatusIDLE,
                        "",
                        subghz_threshold_rssi_get(subghz->threshold_rssi));
                } else {
                    if(scene_manager_has_previous_scene(subghz->scene_manager, SubGhzSceneSaved) ||
                       !scene_manager_has_previous_scene(subghz->scene_manager, SubGhzSceneStart)) {
                        dolphin_deed(DolphinDeedSubGhzSend);
                    }
                    // set callback end tx
                    subghz_txrx_set_raw_file_encoder_worker_callback_end(
                        subghz->txrx, subghz_scene_read_raw_callback_end_tx, subghz);
                    subghz->state_notifications = SubGhzNotificationStateTx;
                }
            } else {
                if(!scene_manager_search_and_switch_to_previous_scene(
                       subghz->scene_manager, SubGhzSceneStart)) {
                    scene_manager_stop(subghz->scene_manager);
                    view_dispatcher_stop(subghz->view_dispatcher);
                }
            }
            consumed = true;
            break;

        case SubGhzCustomEventViewReadRAWSendStop:
            subghz->state_notifications = SubGhzNotificationStateIDLE;
            subghz_txrx_stop(subghz->txrx);
            subghz_read_raw_stop_send(subghz->subghz_read_raw);
            consumed = true;
            break;

        case SubGhzCustomEventViewReadRAWIDLE:
            subghz_txrx_stop(subghz->txrx);
            size_t spl_count = subghz_protocol_raw_get_sample_write(decoder_raw);

            subghz_protocol_raw_save_to_file_stop(decoder_raw);

            FuriString* temp_str = furi_string_alloc();
            furi_string_printf(
                temp_str,
                "%s/%s%s",
                SUBGHZ_RAW_FOLDER,
                RAW_FILE_NAME,
                SUBGHZ_APP_FILENAME_EXTENSION);

            /* ── Silence padding ──────────────────────────────────────────
             * save_to_file_stop only writes RAW_Data when ind_write > 0.
             * A blank recording (no CC1101 transitions) has NO RAW_Data
             * line.  We must write a complete "RAW_Data: -N..." line in
             * that case rather than trying to extend a non-existent one.
             *
             * For recordings with some data, we extend the existing line
             * by seeking to the byte before the final \n and appending.
             *
             * Either way the total silence appended = recording_ticks *
             * 100 000 µs, so a 20-second blank plays for 20 seconds. */
            {
                /* Use recording_ticks — proven correct (powers the on-screen timer).
                 * Each tick is ~100ms. Minimum 1 tick so sub-100ms recordings
                 * still get some silence rather than zero padding. */
                uint32_t rec_ticks  = subghz_read_raw_get_recording_ticks(subghz->subghz_read_raw);
                uint32_t expected_us = (rec_ticks > 0 ? rec_ticks : 1u) * 100000u;

                if(expected_us > 0) {
                    Storage* pad_storage  = furi_record_open(RECORD_STORAGE);
                    File*    pad_file     = storage_file_alloc(pad_storage);

                    if(storage_file_open(
                           pad_file,
                           furi_string_get_cstr(temp_str),
                           FSAM_READ_WRITE,
                           FSOM_OPEN_EXISTING)) {

                        uint64_t fsz = storage_file_size(pad_file);

                        /* Check whether a RAW_Data line already exists by
                         * scanning the file for the key.  We do a simple
                         * linear scan since files are small (<64 KB). */
                        bool has_raw_data = false;
                        if(fsz >= 10u) {
                            uint8_t  scan_buf[256];
                            storage_file_seek(pad_file, 0, true);
                            uint16_t n_read = storage_file_read(pad_file, scan_buf, sizeof(scan_buf) - 1);
                            scan_buf[n_read] = '\0';
                            /* Check entire file for RAW_Data key */
                            uint64_t remaining = fsz;
                            has_raw_data = (strstr((char*)scan_buf, "RAW_Data") != NULL);
                            if(!has_raw_data && remaining > (uint64_t)n_read) {
                                /* Scan rest of file in chunks */
                                while(remaining > (uint64_t)n_read && !has_raw_data) {
                                    n_read = storage_file_read(pad_file, scan_buf, sizeof(scan_buf) - 1);
                                    if(n_read == 0) break;
                                    scan_buf[n_read] = '\0';
                                    has_raw_data = (strstr((char*)scan_buf, "RAW_Data") != NULL);
                                }
                            }
                        }

                        /* Helper lambda to write silence chunks */
                        uint32_t rem = expected_us;

                        /* Write silence as:  1 -(total-1) 1
                         * • Bookend 1µs positive pulses keep the raw TX
                         *   engine running for the full duration — pure
                         *   silence (all-negative) can cause some encoder
                         *   implementations to stop early.
                         * • ONE large negative entry avoids the encoder's
                         *   preload-buffer limit that truncates many small
                         *   -32000 entries to only ~2 seconds of playback.
                         * • The 1µs spikes are too short to affect signal
                         *   decoding if there is real content in the file. */
                        /* Write silence as chunks of at most 999 999 µs each.
                         * The TX file-encoder worker (subghz_file_encoder_worker.c)
                         * silently CLAMPS any value outside ±1 000 000 µs to ±100 µs.
                         * One large -4 999 998 entry therefore became 0.1 ms instead
                         * of 5 seconds.  Multiple ≤999 999 µs chunks bypass the clamp
                         * entirely and each plays at its true duration. */
                        const uint32_t SILENCE_CHUNK = 999999u;
                        char  buf[32];
                        int   wn;
                        bool  first_chunk = true;
                        uint32_t rem2 = rem; /* rem is expected_us */

                        /* Seek position depends on whether a RAW_Data line exists */
                        if(has_raw_data) {
                            storage_file_seek(pad_file, (uint32_t)(fsz - 1u), true);
                        } else {
                            storage_file_seek(pad_file, (uint32_t)fsz, true);
                        }

                        while(rem2 > 0) {
                            uint32_t chunk = (rem2 > SILENCE_CHUNK) ? SILENCE_CHUNK : rem2;
                            uint32_t sil   = (chunk > 1u) ? chunk - 1u : 0u;
                            if(first_chunk) {
                                if(has_raw_data) {
                                    wn = sil ? snprintf(buf, sizeof(buf), " 1 -%lu", (unsigned long)sil)
                                             : snprintf(buf, sizeof(buf), " 1");
                                } else {
                                    wn = sil ? snprintf(buf, sizeof(buf), "RAW_Data: 1 -%lu", (unsigned long)sil)
                                             : snprintf(buf, sizeof(buf), "RAW_Data: 1");
                                }
                                first_chunk = false;
                            } else {
                                wn = sil ? snprintf(buf, sizeof(buf), " 1 -%lu", (unsigned long)sil)
                                         : snprintf(buf, sizeof(buf), " 1");
                            }
                            if(wn > 0) storage_file_write(pad_file, buf, (size_t)wn);
                            rem2 -= chunk;
                        }
                        /* Trailing carrier pulse + newline */
                        storage_file_write(pad_file, " 1\n", 3u);
                    }

                    storage_file_close(pad_file);
                    storage_file_free(pad_file);
                    furi_record_close(RECORD_STORAGE);
                }
            }
            subghz_protocol_raw_gen_fff_data(
                subghz_txrx_get_fff_data(subghz->txrx),
                furi_string_get_cstr(temp_str),
                subghz_txrx_radio_device_get_name(subghz->txrx));
            /* Set file_path so subghz_file_available() returns true.
             * Without this the path only lives in fff_data and Send/Save
             * always fail with "File not available". */
            furi_string_set(subghz->file_path, temp_str);
            furi_string_free(temp_str);

            /* Load envelope now so tx_total_ticks is set correctly for the
             * send-without-saving path (Send from Erase/Send/Save screen).
             * Without this the progress bar jumps to 100% instantly because
             * tx_total_ticks stays at its default of 1. */
            subghz_scene_read_raw_load_envelope(subghz);

            if(spl_count > 0) {
                notification_message(subghz->notifications, &sequence_set_green_255);
            } else {
                notification_message(subghz->notifications, &sequence_reset_rgb);
            }

            subghz->state_notifications = SubGhzNotificationStateIDLE;
            subghz_rx_key_state_set(subghz, SubGhzRxKeyStateAddKey);

            consumed = true;
            break;

        case SubGhzCustomEventViewReadRAWREC:
            if(subghz_rx_key_state_get(subghz) != SubGhzRxKeyStateIDLE) {
                scene_manager_next_scene(subghz->scene_manager, SubGhzSceneNeedSaving);
            } else {
                /* REC pressed - about to run the same heavy alloc chain as
                 * Read's start_listening(), so this is the choke point for
                 * manual Read RAW too. Unconditional, before attempting
                 * save_to_file_init below - not just the reactive recheck
                 * on failure further down. */
                subghz_low_ram_mitigate(subghz, SUBGHZ_LOW_RAM_FREE_HEAP_READ);

                SubGhzRadioPreset preset = subghz_txrx_get_preset(subghz->txrx);
                if(subghz_protocol_raw_save_to_file_init(decoder_raw, RAW_FILE_NAME, &preset)) {
                    dolphin_deed(DolphinDeedSubGhzRawRec);
                    subghz_txrx_rx_start(subghz->txrx);
                    subghz->state_notifications = SubGhzNotificationStateRx;
                    /* recording_ticks reset in view input handler */
                    subghz_rx_key_state_set(subghz, SubGhzRxKeyStateAddKey);
                } else if(subghz_low_ram_mitigate(subghz, SUBGHZ_LOW_RAM_FREE_HEAP_READ)) {
                    /* Same reasoning as Read's start_listening() above - a
                     * save_to_file_init failure with heap already this low
                     * is far more likely OOM (possibly a qFlipper
                     * reconnect's one-time session cost landing right
                     * here) than an actual missing/unwritable SD card -
                     * subghz_low_ram_mitigate() tries a soft-lock first,
                     * then route to the low-RAM warning instead of the
                     * misleading SD-card message if still low. */
                    FURI_LOG_W(
                        TAG,
                        "REC: save_to_file_init failed with free heap %zu still low after mitigation",
                        memmgr_get_free_heap());
                    scene_manager_next_scene(subghz->scene_manager, SubGhzSceneLowRamWarning);
                } else {
                    furi_string_set(subghz->error_str, "Function requires\nan SD card.");
                    scene_manager_next_scene(subghz->scene_manager, SubGhzSceneShowError);
                }
            }
            consumed = true;
            break;

        case SubGhzCustomEventViewReadRAWSave:
            if(subghz_file_available(subghz) && subghz_scene_read_raw_update_filename(subghz)) {
                scene_manager_set_scene_state(
                    subghz->scene_manager, SubGhzSceneReadRAW, SubGhzCustomEventManagerSetRAW);
                subghz_rx_key_state_set(subghz, SubGhzRxKeyStateBack);
                scene_manager_next_scene(subghz->scene_manager, SubGhzSceneSaveName);
            } else {
                if(!scene_manager_search_and_switch_to_previous_scene(
                       subghz->scene_manager, SubGhzSceneStart)) {
                    scene_manager_stop(subghz->scene_manager);
                    view_dispatcher_stop(subghz->view_dispatcher);
                }
            }
            consumed = true;
            break;

        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        switch(subghz->state_notifications) {
        case SubGhzNotificationStateRx:
            notification_message(subghz->notifications, &sequence_blink_cyan_10);
            subghz_read_raw_recording_tick(subghz->subghz_read_raw);

            subghz_read_raw_update_sample_write(
                subghz->subghz_read_raw, subghz_protocol_raw_get_sample_write(decoder_raw));

            SubGhzThresholdRssiData ret_rssi = subghz_threshold_get_rssi_data(
                subghz->threshold_rssi, subghz_txrx_radio_device_get_rssi(subghz->txrx));
            subghz_read_raw_add_data_rssi(
                subghz->subghz_read_raw, ret_rssi.rssi, true /* always advance marker */);
            /* Do NOT pause recording during silence — we want the full
             * duration saved so playback takes as long as the recording. */
            break;
        case SubGhzNotificationStateTx:
            notification_message(subghz->notifications, &sequence_blink_magenta_10);
            subghz_read_raw_tick_tx(subghz->subghz_read_raw);

            /* Defensive backup: the TX worker is supposed to fire a
             * natural "end of file" callback (registered in SendStart,
             * see subghz_scene_read_raw_callback_end_tx) that stops
             * playback automatically. If that hasn't happened ~300ms past
             * when the file should have finished, force the same proven
             * stop sequence ourselves rather than leaving the cursor
             * stuck at 100% until the user manually presses OK. */
            if(subghz_read_raw_is_playback_overdue(subghz->subghz_read_raw)) {
                view_dispatcher_send_custom_event(
                    subghz->view_dispatcher, SubGhzCustomEventViewReadRAWSendStop);
            }
            break;
        default:
            /* state_notifications==IDLE covers the Start screen as well as
             * post-record IDLE and LoadKeyIDLE - only actually run the
             * countdown while genuinely sitting on Start, hence the status
             * getter rather than trusting state_notifications alone. */
            if(!s_manual_start_countdown_fired && !s_manual_start_cancelled &&
               subghz_read_raw_get_status(subghz->subghz_read_raw) ==
                   SubGhzReadRAWStatusStart) {
                s_manual_start_subtick++;
                if(s_manual_start_subtick >= 10) {
                    s_manual_start_subtick = 0;
                    if(s_manual_start_countdown_sec > 0) {
                        s_manual_start_countdown_sec--;
                        subghz_read_raw_set_start_countdown(
                            subghz->subghz_read_raw, s_manual_start_countdown_sec);
                    }
                    if(s_manual_start_countdown_sec == 0) {
                        s_manual_start_countdown_fired = true;
                        /* Set REC status synchronously here, same as the
                         * view's own OK-press handler does - the REC event
                         * handler below only runs the recording start-up
                         * chain (SD file/RX/mitigate), it never touches
                         * model->status itself. Without this the view would
                         * keep showing the Start screen for one more frame
                         * after the countdown already fired. */
                        subghz_read_raw_set_status(
                            subghz->subghz_read_raw,
                            SubGhzReadRAWStatusREC,
                            "",
                            subghz_threshold_rssi_get(subghz->threshold_rssi));
                        view_dispatcher_send_custom_event(
                            subghz->view_dispatcher, SubGhzCustomEventViewReadRAWREC);
                    }
                }
            }
            break;
        }
    }
    return consumed;
}

void subghz_scene_read_raw_on_exit(void* context) {
    SubGhz* subghz = context;

    if(subghz->reader_read_mode) {
        subghz_scene_reader_read_on_exit(subghz);
        return;
    }

    rpc_gui_screen_stream_set_suppressed(false);
    subghz_cli_soft_unlock(subghz);

    //Stop CC1101
    subghz_txrx_stop(subghz->txrx);
    subghz->state_notifications = SubGhzNotificationStateIDLE;
    notification_message(subghz->notifications, &sequence_reset_rgb);

    //filter restoration
    subghz_txrx_receiver_set_filter(subghz->txrx, subghz->filter);
}

void subghz_scene_receiver_on_enter(void* context) {
    SubGhz* subghz = context;
    subghz->reader_read_mode = true;
    subghz_scene_read_raw_on_enter(context);
}

bool subghz_scene_receiver_on_event(void* context, SceneManagerEvent event) {
    return subghz_scene_read_raw_on_event(context, event);
}

void subghz_scene_receiver_on_exit(void* context) {
    SubGhz* subghz = context;
    subghz_scene_read_raw_on_exit(context);
    subghz->reader_read_mode = false;
}
