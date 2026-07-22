#include "../subghz_i.h"
#include "../views/subghz_read_raw.h"
#include <dolphin/dolphin.h>
#include <lib/subghz/protocols/raw.h>
#include <toolbox/path.h>
#include <stdlib.h>
#include <string.h>

#define RAW_FILE_NAME "RAW_"
#define TAG           "SubGhzSceneReadRAW"

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

void subghz_scene_read_raw_on_enter(void* context) {
    SubGhz* subghz = context;
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

    furi_check(subghz_txrx_load_decoder_by_name_protocol(subghz->txrx, SUBGHZ_PROTOCOL_RAW_NAME));

    //set filter RAW feed
    subghz_txrx_receiver_set_filter(subghz->txrx, SubGhzProtocolFlag_RAW);
    furi_string_free(file_name);

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdReadRAW);
}

bool subghz_scene_read_raw_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    bool consumed = false;
    SubGhzProtocolDecoderRAW* decoder_raw =
        (SubGhzProtocolDecoderRAW*)subghz_txrx_get_decoder(subghz->txrx);
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SubGhzCustomEventViewReadRAWBack:

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
            subghz_last_settings_save(subghz->last_settings);
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
                SubGhzRadioPreset preset = subghz_txrx_get_preset(subghz->txrx);
                if(subghz_protocol_raw_save_to_file_init(decoder_raw, RAW_FILE_NAME, &preset)) {
                    dolphin_deed(DolphinDeedSubGhzRawRec);
                    subghz_txrx_rx_start(subghz->txrx);
                    subghz->state_notifications = SubGhzNotificationStateRx;
                    /* recording_ticks reset in view input handler */
                    subghz_rx_key_state_set(subghz, SubGhzRxKeyStateAddKey);
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
            break;
        }
    }
    return consumed;
}

void subghz_scene_read_raw_on_exit(void* context) {
    SubGhz* subghz = context;

    //Stop CC1101
    subghz_txrx_stop(subghz->txrx);
    subghz->state_notifications = SubGhzNotificationStateIDLE;
    notification_message(subghz->notifications, &sequence_reset_rgb);

    //filter restoration
    subghz_txrx_receiver_set_filter(subghz->txrx, subghz->filter);
}
