#include "subghz_read_raw.h"
#include "../subghz_i.h"

#include <furi.h>
#include <furi_hal.h>
#include <input/input.h>
#include <gui/elements.h>
#include <assets_icons.h>

#define SUBGHZ_READ_RAW_RSSI_HISTORY_SIZE 100
#define TAG                               "SubGhzReadRaw"

/* ── Seek step size (percentage points per Left/Right press while paused) ── */
#define SEEK_STEP_PCT 5u

/* ── Reveal animation: how fast the envelope "builds in" after loading ──── */
#define REVEAL_TIMER_MS      15u
#define REVEAL_COLS_PER_TICK 4u   /* ~100/4 * 15ms ≈ 375ms total reveal time */

/* ── Tick period this app's scene runs its tick timer at (see subghz.c:
 * view_dispatcher_set_tick_event_callback(..., 100)). MUST match — this is
 * what turns total playback duration into a tick count for the position
 * cursor. ── */
#define SCENE_TICK_PERIOD_US 100000u /* 100ms */

/* ── Zoom levels: window span as a percentage of the full file, narrowing
 * by roughly 2/3 per step (same ratio the RAW editor itself uses), capped
 * at a sane minimum so very short captures don't zoom into nothing. ──── */
#define ZOOM_LEVEL_COUNT 5u
/* The actual window-percentage table lives in the scene (where windowed
 * bucketing happens) — the view only needs the level COUNT to clamp
 * Up/Down, so it doesn't duplicate that table. */

struct SubGhzReadRAW {
    View*                  view;
    SubGhzReadRAWCallback  callback;
    void*                  context;
    FuriTimer*             reveal_timer;
};

typedef struct {
    FuriString* frequency_str;
    FuriString* preset_str;
    FuriString* sample_write;
    FuriString* file_name;
    uint8_t*    rssi_history;
    uint8_t     rssi_current;
    bool        rssi_history_end;
    uint8_t     ind_write;
    SubGhzReadRAWStatus    status;
    bool        raw_send_only;
    bool        allow_new;      /* show "New" only after record+save, not when loading an existing file */
    float       raw_threshold_rssi;
    bool        not_showing_samples;
    SubGhzRadioDeviceType device_type;

    /* ── Signal display (Bar / Line — no other modes exist) ──── */
    SubGhzReadRawVizMode viz_mode;
    uint8_t  target_envelope[SUBGHZ_READ_RAW_RSSI_HISTORY_SIZE];
    uint8_t  shown_envelope[SUBGHZ_READ_RAW_RSSI_HISTORY_SIZE];
    uint8_t  reveal_count;
    bool     has_envelope;
    uint8_t  play_pct;
    uint8_t  seek_pct;
    uint32_t tx_tick;
    uint32_t tx_total_ticks;    /* derived from REAL duration, not pulse count */
    uint8_t  zoom_level;        /* 0 = fully zoomed out */
    uint32_t recording_ticks;  /* incremented every scene tick while in REC state */
} SubGhzReadRAWModel;

/* ────────────────────────────────────────────────────────────────────────── */
/*  Public setters                                                            */
/* ────────────────────────────────────────────────────────────────────────── */

void subghz_read_raw_set_callback(
    SubGhzReadRAW* instance,
    SubGhzReadRAWCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);
    instance->callback = callback;
    instance->context  = context;
}

void subghz_read_raw_add_data_statusbar(
    SubGhzReadRAW* instance,
    const char* frequency_str,
    const char* preset_str) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        SubGhzReadRAWModel * model,
        {
            furi_string_set(model->frequency_str, frequency_str);
            furi_string_set(model->preset_str, preset_str);
        },
        true);
}

void subghz_read_raw_set_radio_device_type(
    SubGhzReadRAW* instance,
    SubGhzRadioDeviceType device_type) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        SubGhzReadRAWModel * model,
        { model->device_type = device_type; },
        true);
}

void subghz_read_raw_add_data_rssi(SubGhzReadRAW* instance, float rssi, bool trace) {
    furi_assert(instance);
    uint8_t u_rssi = 0;

    if(rssi < SUBGHZ_RAW_THRESHOLD_MIN) {
        u_rssi = 0;
    } else {
        u_rssi = (uint8_t)((rssi - SUBGHZ_RAW_THRESHOLD_MIN) / 2.7f);
    }

    with_view_model(
        instance->view,
        SubGhzReadRAWModel * model,
        {
            model->rssi_current = u_rssi;
            if(trace) {
                model->rssi_history[model->ind_write++] = u_rssi;
            } else {
                model->rssi_history[model->ind_write] = u_rssi;
            }
            if(model->ind_write >= SUBGHZ_READ_RAW_RSSI_HISTORY_SIZE) {
                model->rssi_history_end = true;
                model->ind_write        = 0;
            }
        },
        true);
}

void subghz_read_raw_update_sample_write(SubGhzReadRAW* instance, size_t sample) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        SubGhzReadRAWModel * model,
        {
            model->not_showing_samples = false;
            furi_string_printf(model->sample_write, "%zu spl.", sample);
        },
        false);
}

void subghz_read_raw_recording_tick(SubGhzReadRAW* instance) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        SubGhzReadRAWModel* model,
        { model->recording_ticks++; },
        false);
}

uint32_t subghz_read_raw_get_recording_ticks(SubGhzReadRAW* instance) {
    furi_assert(instance);
    uint32_t ticks = 0;
    with_view_model(
        instance->view,
        SubGhzReadRAWModel* model,
        { ticks = model->recording_ticks; },
        false);
    return ticks;
}

void subghz_read_raw_stop_send(SubGhzReadRAW* instance) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        SubGhzReadRAWModel * model,
        {
            switch(model->status) {
            case SubGhzReadRAWStatusTXRepeat:
                /* Legacy unsaved-quick-send path only — file playback never
                 * enters TXRepeat anymore (Hold-OK-repeat was removed). */
                instance->callback(SubGhzCustomEventViewReadRAWSendStart, instance->context);
                break;
            case SubGhzReadRAWStatusTX:
                model->status = SubGhzReadRAWStatusIDLE;
                break;
            case SubGhzReadRAWStatusLoadKeyTX:
            case SubGhzReadRAWStatusLoadKeyTXRepeat: /* dead state, kept for enum compat */
            case SubGhzReadRAWStatusLoadKeyTXPaused:
                /* Playback finished naturally (or was stopped) — return to
                 * the idle screen so OK can start it again. No more
                 * auto-repeat; that gesture was unreliable (a long-press
                 * can't be distinguished from a short-press until the
                 * short-press action has already fired) and the user
                 * doesn't want it. */
                model->status   = SubGhzReadRAWStatusLoadKeyIDLE;
                model->play_pct = 0;
                model->tx_tick  = 0;
                break;
            default:
                FURI_LOG_W(TAG, "unknown status");
                model->status = SubGhzReadRAWStatusIDLE;
                break;
            }
        },
        true);
}

void subghz_read_raw_set_viz_mode(SubGhzReadRAW* instance, SubGhzReadRawVizMode mode) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        SubGhzReadRAWModel * model,
        { model->viz_mode = mode; },
        false);
}

void subghz_read_raw_set_allow_new(SubGhzReadRAW* instance, bool allow) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        SubGhzReadRAWModel * model,
        { model->allow_new = allow; },
        false);
}

void subghz_read_raw_set_zoom_level(SubGhzReadRAW* instance, uint8_t zoom_level) {
    furi_assert(instance);
    if(zoom_level >= ZOOM_LEVEL_COUNT) zoom_level = ZOOM_LEVEL_COUNT - 1;
    with_view_model(
        instance->view,
        SubGhzReadRAWModel * model,
        { model->zoom_level = zoom_level; },
        false);
}

uint8_t subghz_read_raw_get_zoom_level(SubGhzReadRAW* instance) {
    furi_assert(instance);
    uint8_t z = 0;
    with_view_model(instance->view, SubGhzReadRAWModel * model, { z = model->zoom_level; }, false);
    return z;
}

uint8_t subghz_read_raw_get_seek_pct(SubGhzReadRAW* instance) {
    furi_assert(instance);
    uint8_t p = 0;
    with_view_model(instance->view, SubGhzReadRAWModel * model, { p = model->seek_pct; }, false);
    return p;
}

/**
 * Returns true once active playback has run at least GRACE_TICKS past its
 * expected completion time. Used as a defensive backup: the underlying TX
 * worker is supposed to fire a natural "end of file" callback, but if for
 * any reason that doesn't happen, the scene can use this to force a clean
 * stop instead of leaving playback stuck at 100% indefinitely.
 */
bool subghz_read_raw_is_playback_overdue(SubGhzReadRAW* instance) {
    furi_assert(instance);
    bool overdue = false;
    with_view_model(
        instance->view,
        SubGhzReadRAWModel * model,
        {
            if(model->status == SubGhzReadRAWStatusLoadKeyTX) {
                const uint32_t GRACE_TICKS = 3; /* ~300ms past expected end */
                overdue = model->tx_tick >= (model->tx_total_ticks + GRACE_TICKS);
            }
        },
        false);
    return overdue;
}

/* ── Reveal animation timer: progressively copies target → shown ─────────── */
static void reveal_timer_cb(void* ctx) {
    SubGhzReadRAW* instance = ctx;
    bool done = false;
    with_view_model(
        instance->view,
        SubGhzReadRAWModel * model,
        {
            if(model->reveal_count < SUBGHZ_READ_RAW_RSSI_HISTORY_SIZE) {
                uint8_t next = (uint8_t)(model->reveal_count + REVEAL_COLS_PER_TICK);
                if(next > SUBGHZ_READ_RAW_RSSI_HISTORY_SIZE)
                    next = SUBGHZ_READ_RAW_RSSI_HISTORY_SIZE;
                for(uint8_t i = model->reveal_count; i < next; i++) {
                    model->shown_envelope[i] = model->has_envelope ? model->target_envelope[i] : 0;
                }
                model->reveal_count = next;
            }
            done = model->reveal_count >= SUBGHZ_READ_RAW_RSSI_HISTORY_SIZE;
        },
        true);

    if(done) furi_timer_stop(instance->reveal_timer);
}

void subghz_read_raw_set_envelope(
    SubGhzReadRAW* instance,
    const uint8_t* envelope,
    uint32_t total_duration_us) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        SubGhzReadRAWModel * model,
        {
            if(envelope) {
                memcpy(model->target_envelope, envelope, SUBGHZ_READ_RAW_RSSI_HISTORY_SIZE);
                model->has_envelope = true;
            } else {
                memset(model->target_envelope, 0, SUBGHZ_READ_RAW_RSSI_HISTORY_SIZE);
                model->has_envelope = false;
            }
            memset(model->shown_envelope, 0, SUBGHZ_READ_RAW_RSSI_HISTORY_SIZE);
            model->reveal_count   = 0;
            model->play_pct       = 0;
            model->seek_pct       = 0;
            model->tx_tick        = 0;

            /* ── THE FIX: derive tick count from REAL duration, not pulse
             * count. Pulse count has no fixed relationship to playback
             * time (a dense burst with many short pulses and a sparse one
             * with few long pulses can have wildly different durations
             * for the same pulse count) — using it made the cursor crawl
             * at a speed completely unrelated to actual playback. ── */
            uint32_t ticks = total_duration_us / SCENE_TICK_PERIOD_US;
            model->tx_total_ticks = (ticks > 0) ? ticks : 1;
        },
        true);

    furi_timer_start(instance->reveal_timer, REVEAL_TIMER_MS);
}

void subghz_read_raw_tick_tx(SubGhzReadRAW* instance) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        SubGhzReadRAWModel * model,
        {
            if(model->status == SubGhzReadRAWStatusLoadKeyTX ||
               model->status == SubGhzReadRAWStatusLoadKeyTXRepeat) {
                model->tx_tick++;
                if(model->tx_total_ticks > 0) {
                    uint32_t pct = (model->tx_tick * 100) / model->tx_total_ticks;
                    model->play_pct = (uint8_t)(pct > 100 ? 100 : pct);
                }
            } else if(model->status == SubGhzReadRAWStatusTX ||
                      model->status == SubGhzReadRAWStatusTXRepeat) {
                /* Quick-send (no saved file): use recording_ticks as the
                 * total duration reference so the cursor tracks approximately
                 * how far through the replay we are. */
                model->tx_tick++;
                uint32_t total = model->recording_ticks > 0 ? model->recording_ticks : 1;
                uint32_t pct   = (model->tx_tick * 100) / total;
                model->play_pct = (uint8_t)(pct > 100 ? 100 : pct);
            }
        },
        true);
}

/* ────────────────────────────────────────────────────────────────────────── */
/*  Draw helpers — recording-phase RSSI history (unchanged from original)    */
/* ────────────────────────────────────────────────────────────────────────── */

static void subghz_read_raw_draw_scale(Canvas* canvas, SubGhzReadRAWModel* model) {
#define SUBGHZ_RAW_TOP_SCALE 15
#define SUBGHZ_RAW_END_SCALE 112  /* keep ticks inside right frame border at x=115 */
    if(!model->rssi_history_end) {
        for(int i = SUBGHZ_RAW_END_SCALE; i > 0; i -= 15) {
            canvas_draw_line(canvas, i, SUBGHZ_RAW_TOP_SCALE, i, SUBGHZ_RAW_TOP_SCALE + 4);
            canvas_draw_line(canvas, i - 5, SUBGHZ_RAW_TOP_SCALE, i - 5, SUBGHZ_RAW_TOP_SCALE + 2);
            canvas_draw_line(canvas, i - 10, SUBGHZ_RAW_TOP_SCALE, i - 10, SUBGHZ_RAW_TOP_SCALE + 2);
        }
    } else {
        for(int i = SUBGHZ_RAW_END_SCALE - model->ind_write % 15; i > -15; i -= 15) {
            canvas_draw_line(canvas, i, SUBGHZ_RAW_TOP_SCALE, i, SUBGHZ_RAW_TOP_SCALE + 4);
            if(SUBGHZ_RAW_END_SCALE > i + 5)
                canvas_draw_line(canvas, i + 5, SUBGHZ_RAW_TOP_SCALE, i + 5, SUBGHZ_RAW_TOP_SCALE + 2);
            if(SUBGHZ_RAW_END_SCALE > i + 10)
                canvas_draw_line(canvas, i + 10, SUBGHZ_RAW_TOP_SCALE, i + 10, SUBGHZ_RAW_TOP_SCALE + 2);
        }
    }
}

static void subghz_read_raw_draw_rssi(Canvas* canvas, SubGhzReadRAWModel* model) {
    uint8_t width = 2;
    /* 85% of the 113px usable frame width = ~96px marker position */
    const uint8_t MARKER_X = 85; /* stop before timer text which can start at ~x=89 */

    /* Flat baseline — always visible even with no signal */
    canvas_draw_line(canvas, 1, 47, 114, 47);

    uint32_t iw = model->ind_write;
    bool wrapped = model->rssi_history_end;

    /* Compute screen position of the marker and draw the waveform.
     *
     * Phase 1 (!wrapped && iw < MARKER_X):
     *   Bars fill left→right.  Marker moves with iw.
     *
     * Phase 2 (!wrapped && iw >= MARKER_X) OR (wrapped):
     *   Marker stays at MARKER_X.  Waveform scrolls: screen_x maps to
     *   history[(iw - MARKER_X + screen_x) % SIZE] so older samples
     *   slide left as new ones arrive on the right. */

    if(!wrapped && iw < MARKER_X) {
        /* Phase 1 — filling */
        for(uint32_t i = 0; i <= iw; i++)
            canvas_draw_line(canvas, (uint8_t)i, 47,
                             (uint8_t)i, 47 - model->rssi_history[i]);
    } else {
        /* Phase 2 / wrapped — scrolling */
        uint32_t base_idx = wrapped
            ? (iw + SUBGHZ_READ_RAW_RSSI_HISTORY_SIZE - MARKER_X)
            : (iw - MARKER_X);
        for(uint8_t screen_x = 1; screen_x <= 113; screen_x++) {
            uint32_t hist_idx =
                (base_idx + screen_x) % SUBGHZ_READ_RAW_RSSI_HISTORY_SIZE;
            canvas_draw_line(canvas, screen_x, 47,
                             screen_x, 47 - model->rssi_history[hist_idx]);
        }
    }

    /* Marker position: during TX (quick-send replay) use play_pct to drive
     * the cursor from left to right.  In all other states keep the original
     * behaviour (marks the live write head / end of recording).             */
    uint8_t px;
    if(model->status == SubGhzReadRAWStatusTX ||
       model->status == SubGhzReadRAWStatusTXRepeat) {
        uint8_t data_end = (!wrapped && iw < MARKER_X) ? (uint8_t)iw : MARKER_X;
        px = (uint8_t)((uint32_t)model->play_pct * data_end / 100);
    } else {
        px = (!wrapped && iw < MARKER_X) ? (uint8_t)iw : MARKER_X;
    }
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_line(canvas, px, 15, px, 47);
    canvas_set_color(canvas, ColorBlack);
    /* Current RSSI bar at marker */
    canvas_draw_line(canvas, px, 47, px, 47 - model->rssi_current);
    /* Dotted vertical line */
    for(uint8_t i = 15; i < 47; i += width * 2)
        canvas_draw_line(canvas, px, i, px, i + width);
    /* Arrowhead pointing down at top */
    canvas_draw_line(canvas, px - 2, 13, px + 2, 13);
    canvas_draw_line(canvas, px - 1, 14, px + 1, 14);
}

static void subghz_read_raw_draw_threshold_rssi(Canvas* canvas, SubGhzReadRAWModel* model) {
    uint8_t x = 114; /* right edge of waveform frame is 115; stay inside */
    uint8_t y = 48;
    if(model->raw_threshold_rssi > SUBGHZ_RAW_THRESHOLD_MIN) {
        y -= (uint8_t)((model->raw_threshold_rssi - SUBGHZ_RAW_THRESHOLD_MIN) / 2.7f);
        uint8_t width = 3;
        for(uint8_t i = 0; i < x; i += width * 2)
            canvas_draw_line(canvas, i, y, i + width, y);
    }
    canvas_draw_line(canvas, x, y - 2, x, y + 2);
    canvas_draw_line(canvas, x - 1, y - 1, x - 1, y + 1);
    canvas_draw_dot(canvas, x - 2, y);
}

/* ────────────────────────────────────────────────────────────────────────── */
/*  Draw helpers — playback envelope: Bar & Line (NO other modes exist)      */
/* ────────────────────────────────────────────────────────────────────────── */

static void subghz_read_raw_draw_envelope(Canvas* canvas, SubGhzReadRAWModel* model) {
    const uint8_t W      = 114;
    const uint8_t TOP    = 15;
    const uint8_t BOTTOM = 39;
    const uint8_t BAR_Y  = 44;
    const uint8_t BAR_H  = 3;

    if(!model->has_envelope && model->reveal_count == 0) {
        /* Blank recording: just draw a flat baseline — no "Playing..." text */
        canvas_draw_line(canvas, 0, BOTTOM, W, BOTTOM);
    } else if(model->viz_mode == SubGhzReadRawVizLine) {
        int prev_x = -1, prev_y = BOTTOM;
        for(uint8_t col = 0; col < model->reveal_count; col++) {
            uint8_t x = (uint8_t)((uint16_t)col * W / 100);
            uint8_t h = (uint8_t)(((uint16_t)model->shown_envelope[col] * (BOTTOM - TOP - 1)) / 255);
            int y = BOTTOM - h;
            if(prev_x >= 0) canvas_draw_line(canvas, prev_x, prev_y, x, y);
            prev_x = x;
            prev_y = y;
        }
        if(model->reveal_count == 0) {
            canvas_draw_line(canvas, 0, BOTTOM, W, BOTTOM);
        }
    } else {
        for(uint8_t col = 0; col < model->reveal_count; col++) {
            uint8_t x = (uint8_t)((uint16_t)col * W / 100);
            uint8_t h = (uint8_t)(((uint16_t)model->shown_envelope[col] * (BOTTOM - TOP - 1)) / 255);
            if(h == 0) continue;
            canvas_draw_line(canvas, x, BOTTOM, x, BOTTOM - h);
        }
    }

    /* ── Position cursor (vertical dashed line) ── */
    uint8_t cursor_x = (uint8_t)((uint16_t)model->play_pct * W / 100);
    for(uint8_t y = TOP; y < BAR_Y; y += 3)
        canvas_draw_dot(canvas, cursor_x, y);

    /* ── Progress bar — frame is W+2 wide so at 100% the fill
     * reaches exactly the inner right edge (fill = W px from x=1). ── */
    canvas_draw_frame(canvas, 0, BAR_Y, W + 2, BAR_H + 2);
    uint8_t fill_w = (uint8_t)((uint16_t)model->play_pct * W / 100);
    if(fill_w > 0) canvas_draw_box(canvas, 1, BAR_Y + 1, fill_w, BAR_H);

    /* ── Seek marker (while paused) ── */
    if(model->status == SubGhzReadRAWStatusLoadKeyTXPaused) {
        uint8_t seek_x = (uint8_t)((uint16_t)model->seek_pct * W / 100);
        canvas_draw_dot(canvas, seek_x, BAR_Y - 1);
        canvas_draw_dot(canvas, seek_x - 1, BAR_Y - 2);
        canvas_draw_dot(canvas, seek_x + 1, BAR_Y - 2);
    }

    /* ── Zoom indicator (only shown when zoomed in) ── */
    if(model->zoom_level > 0) {
        char zb[8];
        snprintf(zb, sizeof(zb), "%ux", model->zoom_level + 1);
        canvas_draw_str_aligned(canvas, W - 2, TOP - 1, AlignRight, AlignBottom, zb);
    }
}

/* ── Compact bargraph for the legacy "send before saving" path. No seek (no
 * file backing exists yet for an in-progress, unsaved recording) — just a
 * clean bar visualisation of the rssi_history captured while recording. ── */
static void subghz_read_raw_draw_legacy_bargraph(Canvas* canvas, SubGhzReadRAWModel* model) {
    const uint8_t W   = 114;
    const uint8_t TOP = 14;
    const uint8_t BOT = 47;

    for(uint8_t x = 0; x < W; x++) {
        uint8_t src = (uint8_t)((uint16_t)x * SUBGHZ_READ_RAW_RSSI_HISTORY_SIZE / W);
        uint8_t v = model->rssi_history[src];
        uint8_t h = v;
        if(h > (BOT - TOP - 1)) h = BOT - TOP - 1;
        if(h == 0) continue;
        canvas_draw_line(canvas, x, BOT, x, BOT - h);
    }
}

/* ────────────────────────────────────────────────────────────────────────── */
/*  Main draw callback                                                        */
/* ────────────────────────────────────────────────────────────────────────── */

void subghz_read_raw_draw(Canvas* canvas, SubGhzReadRAWModel* model) {
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    canvas_draw_str(canvas, 0, 7, furi_string_get_cstr(model->frequency_str));
    canvas_draw_str(canvas, 35, 7, furi_string_get_cstr(model->preset_str));

    if(model->not_showing_samples) {
        canvas_draw_str(canvas, 77, 7,
            (model->device_type == SubGhzRadioDeviceTypeInternal) ? "R: Int" : "R: Ext");
    } else {
        canvas_draw_str(canvas, 70, 7,
            (model->device_type == SubGhzRadioDeviceTypeInternal) ? "I" : "E");
    }

    canvas_draw_str_aligned(canvas, 126, 0, AlignRight, AlignTop,
                            furi_string_get_cstr(model->sample_write));

    /* Thin top border — scale ticks start at y=15 so they don't overlap */
    canvas_draw_line(canvas, 0, 14, 115, 14); /* top border */
    canvas_draw_line(canvas, 0, 48, 115, 48); /* bottom border */
    canvas_draw_line(canvas, 115, 14, 115, 48); /* right border */
    canvas_draw_line(canvas, 0, 14, 0, 48);   /* left border */

    uint8_t signal_mode = 1; /* 0=legacy bargraph, 1=recording RSSI, 2=playback envelope */

    switch(model->status) {
    case SubGhzReadRAWStatusIDLE:
        elements_button_left(canvas, "Erase");
        elements_button_center(canvas, "Send");
        elements_button_right(canvas, "Save");
        break;

    case SubGhzReadRAWStatusLoadKeyIDLE: {
        /* Show the waveform envelope at all times when a file is loaded,
         * not just during playback. The filename is shown at the top of
         * the waveform area (small font) — truncated with "..." if it
         * would overflow the available ~110px / ~22 char width. */
        signal_mode = 2; /* envelope */
        if(model->allow_new) {
            elements_button_left(canvas, "New");
        }
        elements_button_right(canvas, "More");
        elements_button_center(canvas, "Send");

        /* No filename overlay in the waveform area */
        break;
    }

    /* ── File playback. Hold-OK-repeat removed entirely: a short OK press
     * already fires before a long-press can be distinguished, so the
     * gesture was never actually reachable. OK is now a plain
     * play/pause toggle; when playback ends naturally it returns to
     * LoadKeyIDLE so pressing OK again simply starts it over. ── */
    case SubGhzReadRAWStatusLoadKeyTX:
    case SubGhzReadRAWStatusLoadKeyTXRepeat: /* dead state, kept for enum compat */
        signal_mode = 2;
        elements_button_center(canvas, "Pause");
        break;

    case SubGhzReadRAWStatusLoadKeyTXPaused:
        signal_mode = 2;
        elements_button_left(canvas, "Rew");
        elements_button_center(canvas, "Resume");
        elements_button_right(canvas, "Fwd");
        /* PAUSED: moved to the TOP of the envelope frame, bold, no box —
         * it was sitting too low/boxed in before. */
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 57, 16, AlignCenter, AlignTop, "PAUSED");
        canvas_set_font(canvas, FontSecondary);
        break;

    case SubGhzReadRAWStatusTX:
    case SubGhzReadRAWStatusTXRepeat:
        /* Quick-send (no saved file) — keep the RSSI display with its
         * scale ruler + cursor visible during send, same as the recording
         * view. mode=1 shows the frozen RSSI history + scale ticks. */
        signal_mode = 1;
        elements_button_center(canvas, "Hold to repeat");
        break;

    case SubGhzReadRAWStatusStart:
        elements_button_left(canvas, "Config");
        elements_button_center(canvas, "REC");
        signal_mode = 3; /* clean empty envelope — no scale/RSSI before recording */
        break;

    default:
        elements_button_center(canvas, "Stop");
        break;
    }

    /* ── Recording / playback timer ─────────────────────────────────────────
     * Source depends on state:
     *   TX / LoadKeyTX / Paused → tx_tick  (counts up from 0 during replay)
     *   Everything else          → recording_ticks (frozen final duration)
     * Both are in 100 ms units so the format string is identical.          */
    {
        uint32_t timer_ticks = 0;
        bool show_timer = false;

        if(model->status == SubGhzReadRAWStatusTX         ||
           model->status == SubGhzReadRAWStatusTXRepeat    ||
           model->status == SubGhzReadRAWStatusLoadKeyTX   ||
           model->status == SubGhzReadRAWStatusLoadKeyTXRepeat ||
           model->status == SubGhzReadRAWStatusLoadKeyTXPaused) {
            timer_ticks = model->tx_tick;
            show_timer  = true;
        } else if(model->recording_ticks > 0) {
            timer_ticks = model->recording_ticks;
            show_timer  = true;
        }

        if(show_timer) {
            uint32_t secs = timer_ticks / 10;
            uint8_t  mins = (uint8_t)(secs / 60 > 99 ? 99 : secs / 60);
            uint8_t  sec2 = (uint8_t)(secs % 60);
            uint8_t  decs = (uint8_t)(timer_ticks % 10);
            char timer_str[10];
            snprintf(timer_str, sizeof(timer_str), "%u:%02u.%u",
                     (unsigned)mins, (unsigned)sec2, (unsigned)decs);
            canvas_set_color(canvas, ColorBlack);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 113, 17, AlignRight, AlignTop, timer_str);
        }
    }

    if(signal_mode == 0) {
        subghz_read_raw_draw_legacy_bargraph(canvas, model);
    } else if(signal_mode == 2) {
        /* Always draw scale ticks above envelope — user wants them everywhere */
        subghz_read_raw_draw_scale(canvas, model);
        subghz_read_raw_draw_envelope(canvas, model);
    } else if(signal_mode == 1) {
        /* Only draw RSSI/scale for mode 1 — mode 3 (Start state) draws nothing */
        subghz_read_raw_draw_rssi(canvas, model);
        subghz_read_raw_draw_scale(canvas, model);
        subghz_read_raw_draw_threshold_rssi(canvas, model);
        canvas_set_font_direction(canvas, CanvasDirectionBottomToTop);
        canvas_draw_str(canvas, 126, 40, "RSSI");
        canvas_set_font_direction(canvas, CanvasDirectionLeftToRight);
    }
    /* signal_mode == 3: clean empty waveform area for Start state */
}

/* ────────────────────────────────────────────────────────────────────────── */
/*  Input callback                                                            */
/* ────────────────────────────────────────────────────────────────────────── */

bool subghz_read_raw_input(InputEvent* event, void* context) {
    furi_assert(context);
    SubGhzReadRAW* instance = context;

    /* ── OK short press: simple play/pause toggle for file playback.
     * Long-press / repeat handling for OK has been removed entirely —
     * there is no more Hold-OK-repeat gesture. ── */
    if(event->key == InputKeyOk && event->type == InputTypePress) {
        uint8_t ret = false;
        with_view_model(instance->view, SubGhzReadRAWModel * model, {
            switch(model->status) {
            case SubGhzReadRAWStatusIDLE:
                instance->callback(SubGhzCustomEventViewReadRAWSendStart, instance->context);
                model->status   = SubGhzReadRAWStatusTXRepeat;
                model->play_pct = 0;   /* cursor returns to start */
                model->tx_tick  = 0;
                ret = true;
                break;
            case SubGhzReadRAWStatusTX:
                model->status = SubGhzReadRAWStatusTXRepeat;
                break;

            case SubGhzReadRAWStatusLoadKeyIDLE:
                instance->callback(SubGhzCustomEventViewReadRAWSendStart, instance->context);
                model->status   = SubGhzReadRAWStatusLoadKeyTX;
                model->play_pct = 0;
                model->seek_pct = 0;
                model->tx_tick  = 0;
                ret = true;
                break;
            case SubGhzReadRAWStatusLoadKeyTX:
                instance->callback(SubGhzCustomEventViewReadRAWTXPause, instance->context);
                model->status   = SubGhzReadRAWStatusLoadKeyTXPaused;
                model->seek_pct = model->play_pct;
                ret = true;
                break;
            case SubGhzReadRAWStatusLoadKeyTXPaused:
                instance->callback(SubGhzCustomEventViewReadRAWTXResume, instance->context);
                model->status   = SubGhzReadRAWStatusLoadKeyTX;
                model->play_pct = model->seek_pct;
                model->tx_tick  = (uint32_t)model->seek_pct * model->tx_total_ticks / 100;
                ret = true;
                break;
            default:
                break;
            }
        }, ret);
        return true;
    }

    if(event->key == InputKeyOk && event->type == InputTypeRelease) {
        with_view_model(instance->view, SubGhzReadRAWModel * model, {
            if(model->status == SubGhzReadRAWStatusTXRepeat)
                model->status = SubGhzReadRAWStatusTX;
        }, false);
        return true;
    }

    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        with_view_model(instance->view, SubGhzReadRAWModel * model, {
            switch(model->status) {
            case SubGhzReadRAWStatusREC:
                instance->callback(SubGhzCustomEventViewReadRAWIDLE, instance->context);
                model->status = SubGhzReadRAWStatusIDLE;
                break;
            case SubGhzReadRAWStatusLoadKeyTX:
            case SubGhzReadRAWStatusLoadKeyTXRepeat:
            case SubGhzReadRAWStatusLoadKeyTXPaused:
                instance->callback(SubGhzCustomEventViewReadRAWTXRXStop, instance->context);
                model->status = SubGhzReadRAWStatusLoadKeyIDLE;
                break;
            case SubGhzReadRAWStatusTX:
                instance->callback(SubGhzCustomEventViewReadRAWTXRXStop, instance->context);
                model->status = SubGhzReadRAWStatusIDLE;
                break;
            case SubGhzReadRAWStatusLoadKeyIDLE:
                instance->callback(SubGhzCustomEventViewReadRAWBack, instance->context);
                break;
            default:
                instance->callback(SubGhzCustomEventViewReadRAWBack, instance->context);
                break;
            }
        }, true);
        return true;
    }

    /* ── Left: seek backward (paused) / New (idle, allow_new only) / Config ── */
    if(event->key == InputKeyLeft &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        bool fire_erase = false;
        with_view_model(instance->view, SubGhzReadRAWModel * model, {
            if(model->status == SubGhzReadRAWStatusLoadKeyTXPaused) {
                if(model->seek_pct >= SEEK_STEP_PCT) model->seek_pct -= SEEK_STEP_PCT;
                else model->seek_pct = 0;
            } else if(!model->raw_send_only) {
                if(model->status == SubGhzReadRAWStatusStart) {
                    instance->callback(SubGhzCustomEventViewReadRAWConfig, instance->context);
                } else if(model->status == SubGhzReadRAWStatusIDLE) {
                    model->status = SubGhzReadRAWStatusStart;
                    model->rssi_history_end = false;
                    model->ind_write = 0;
                    model->not_showing_samples = true;
                    model->recording_ticks = 0;
                    furi_string_set(model->sample_write, "0 spl.");
                    furi_string_reset(model->file_name);
                    fire_erase = true;
                } else if(model->status == SubGhzReadRAWStatusLoadKeyIDLE && model->allow_new) {
                    model->status = SubGhzReadRAWStatusStart;
                    model->rssi_history_end = false;
                    model->ind_write = 0;
                    model->not_showing_samples = true;
                    model->recording_ticks = 0;
                    furi_string_set(model->sample_write, "0 spl.");
                    furi_string_reset(model->file_name);
                    fire_erase = true;
                }
                /* When allow_new is false (file loaded from Saved), Left
                 * does nothing in LoadKeyIDLE — "New" isn't offered here. */
            }
        }, true);
        if(fire_erase) instance->callback(SubGhzCustomEventViewReadRAWErase, instance->context);
        return true;
    }

    if(event->key == InputKeyRight &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        with_view_model(instance->view, SubGhzReadRAWModel * model, {
            if(model->status == SubGhzReadRAWStatusLoadKeyTXPaused) {
                if(model->seek_pct + SEEK_STEP_PCT <= 95) model->seek_pct += SEEK_STEP_PCT;
                else model->seek_pct = 95;
            } else if(model->status == SubGhzReadRAWStatusIDLE && !model->raw_send_only) {
                instance->callback(SubGhzCustomEventViewReadRAWSave, instance->context);
            } else if(model->status == SubGhzReadRAWStatusLoadKeyIDLE) {
                /* More is always available, regardless of raw_send_only */
                instance->callback(SubGhzCustomEventViewReadRAWMore, instance->context);
            }
        }, true);
        return true;
    }

    /* ── Up / Down: zoom in/out. Only available while PAUSED — zooming
     * during active playback would require either auto-scrolling the
     * window to follow the cursor or letting the cursor run off-screen,
     * both meaningfully more complex/larger for a feature that's really
     * about static inspection (same as the RAW editor itself, which only
     * supports zoom while not actively transmitting). Fires a custom
     * event so the scene can re-derive a windowed envelope from the file,
     * centered on the current seek position; the view only tracks its
     * own zoom_level for display + so the scene can read it back to
     * persist it via subghz_read_raw_get_zoom_level(). ── */
    if((event->key == InputKeyUp || event->key == InputKeyDown) &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        bool paused = false;
        bool zoom_in = (event->key == InputKeyUp);
        with_view_model(instance->view, SubGhzReadRAWModel * model, {
            paused = (model->status == SubGhzReadRAWStatusLoadKeyTXPaused);
            if(paused) {
                if(zoom_in && (unsigned)model->zoom_level + 1u < ZOOM_LEVEL_COUNT) {
                    model->zoom_level++;
                } else if(!zoom_in && model->zoom_level > 0) {
                    model->zoom_level--;
                }
            }
        }, false);
        if(paused && instance->callback) {
            instance->callback(
                zoom_in ? SubGhzCustomEventViewReadRAWZoomIn : SubGhzCustomEventViewReadRAWZoomOut,
                instance->context);
        }
        return true;
    }

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        with_view_model(instance->view, SubGhzReadRAWModel * model, {
            if(model->status == SubGhzReadRAWStatusStart) {
                instance->callback(SubGhzCustomEventViewReadRAWREC, instance->context);
                model->status = SubGhzReadRAWStatusREC;
                model->ind_write = 0;
                model->rssi_history_end = false;
                model->recording_ticks = 0; /* reset timer for each new recording */
            } else if(model->status == SubGhzReadRAWStatusREC) {
                instance->callback(SubGhzCustomEventViewReadRAWIDLE, instance->context);
                model->status = SubGhzReadRAWStatusIDLE;
            }
        }, true);
        return true;
    }

    return true;
}

/* ────────────────────────────────────────────────────────────────────────── */
/*  set_status                                                                */
/* ────────────────────────────────────────────────────────────────────────── */

void subghz_read_raw_set_status(
    SubGhzReadRAW* instance,
    SubGhzReadRAWStatus status,
    const char* file_name,
    float raw_threshold_rssi) {
    furi_assert(instance);
    switch(status) {
    case SubGhzReadRAWStatusStart:
        with_view_model(instance->view, SubGhzReadRAWModel * model, {
            model->status = SubGhzReadRAWStatusStart;
            model->rssi_history_end = false;
            model->ind_write = 0;
            model->not_showing_samples = true;
            model->recording_ticks = 0; /* clear timer — user is starting fresh */
            furi_string_reset(model->file_name);
            furi_string_set(model->sample_write, "0 spl.");
            model->raw_threshold_rssi = raw_threshold_rssi;
        }, true);
        break;
    case SubGhzReadRAWStatusIDLE:
        with_view_model(instance->view, SubGhzReadRAWModel * model,
            { model->status = SubGhzReadRAWStatusIDLE; }, true);
        break;
    case SubGhzReadRAWStatusLoadKeyTX:
        with_view_model(instance->view, SubGhzReadRAWModel * model, {
            model->status = SubGhzReadRAWStatusLoadKeyIDLE;
            model->rssi_history_end = false;
            model->ind_write = 0;
            model->not_showing_samples = true;
            model->play_pct = 0;
            model->seek_pct = 0;
            model->tx_tick  = 0;
            furi_string_set(model->file_name, file_name);
            furi_string_set(model->sample_write, "RAW");
        }, true);
        break;
    case SubGhzReadRAWStatusSaveKey:
        with_view_model(instance->view, SubGhzReadRAWModel * model, {
            model->status = SubGhzReadRAWStatusLoadKeyIDLE;
            if(!model->ind_write) {
                model->not_showing_samples = true;
                furi_string_set(model->file_name, file_name);
                furi_string_set(model->sample_write, "RAW");
            } else {
                furi_string_reset(model->file_name);
            }
        }, true);
        break;
    default:
        FURI_LOG_W(TAG, "unknown status");
        break;
    }
}

/* ────────────────────────────────────────────────────────────────────────── */
/*  Enter / Exit / Alloc / Free                                               */
/* ────────────────────────────────────────────────────────────────────────── */

void subghz_read_raw_enter(void* context) { UNUSED(context); }

void subghz_read_raw_exit(void* context) {
    furi_assert(context);
    SubGhzReadRAW* instance = context;
    with_view_model(instance->view, SubGhzReadRAWModel * model, {
        if(model->status != SubGhzReadRAWStatusIDLE &&
           model->status != SubGhzReadRAWStatusStart &&
           model->status != SubGhzReadRAWStatusLoadKeyIDLE) {
            instance->callback(SubGhzCustomEventViewReadRAWIDLE, instance->context);
            model->status = SubGhzReadRAWStatusStart;
        }
    }, true);
}

SubGhzReadRAW* subghz_read_raw_alloc(bool raw_send_only) {
    SubGhzReadRAW* instance = malloc(sizeof(SubGhzReadRAW));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(SubGhzReadRAWModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, (ViewDrawCallback)subghz_read_raw_draw);
    view_set_input_callback(instance->view, subghz_read_raw_input);
    view_set_enter_callback(instance->view, subghz_read_raw_enter);
    view_set_exit_callback(instance->view, subghz_read_raw_exit);

    with_view_model(instance->view, SubGhzReadRAWModel * model, {
        model->frequency_str = furi_string_alloc();
        model->preset_str    = furi_string_alloc();
        model->sample_write  = furi_string_alloc();
        model->file_name     = furi_string_alloc();
        model->raw_send_only = raw_send_only;
        model->allow_new     = false;
        model->rssi_history  = malloc(SUBGHZ_READ_RAW_RSSI_HISTORY_SIZE * sizeof(uint8_t));
        model->raw_threshold_rssi = -127.0f;
        model->viz_mode       = SubGhzReadRawVizBar;
        model->has_envelope   = false;
        model->reveal_count   = 0;
        model->play_pct       = 0;
        model->seek_pct       = 0;
        model->tx_tick        = 0;
        model->tx_total_ticks = 1;
        model->zoom_level     = 0;
    }, true);

    instance->reveal_timer = furi_timer_alloc(reveal_timer_cb, FuriTimerTypePeriodic, instance);

    return instance;
}

void subghz_read_raw_free(SubGhzReadRAW* instance) {
    furi_assert(instance);
    furi_timer_stop(instance->reveal_timer);
    furi_timer_free(instance->reveal_timer);
    with_view_model(instance->view, SubGhzReadRAWModel * model, {
        furi_string_free(model->frequency_str);
        furi_string_free(model->preset_str);
        furi_string_free(model->sample_write);
        furi_string_free(model->file_name);
        free(model->rssi_history);
    }, true);
    view_free(instance->view);
    free(instance);
}

View* subghz_read_raw_get_view(SubGhzReadRAW* instance) {
    furi_assert(instance);
    return instance->view;
}
