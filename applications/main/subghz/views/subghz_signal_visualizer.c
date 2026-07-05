/**
 * @file subghz_signal_visualizer.c
 * @brief Real-time RF signal visualizer for the SubGHz application.
 *
 * Renders RSSI samples from the CC1101 onto the Flipper's 128×64 display
 * in two modes:
 *   - Bar:  full-screen vertical-bar display with software trigger
 *   - Line:    continuous connected-trace rendering of the live signal
 *
 * Sampling strategy
 * -----------------
 * A high-priority FuriTimer fires every 1 ms and writes a single RSSI
 * reading into a power-of-two ring buffer (VIZ_RAW_BUF_SIZE samples).
 * A second timer fires every ~33 ms (≈30 fps), snapshots the ring buffer
 * into the view model, advances the waterfall matrix, and schedules a
 * canvas redraw via with_view_model(..., true).
 *
 * Trigger
 * -------
 * A software edge trigger watches for the RSSI rising through the user-
 * adjustable threshold.  When armed (RSSI was below threshold) and a
 * rising edge is detected the display latches on that sample index, giving
 * stable captures of burst OOK/ASK transmissions.
 * Up/Down on the D-pad move the threshold in 1 dBm steps.
 * OK toggles between Bar and Line modes. There is no
 * "Classic"/line-waveform mode — it has been removed entirely.
 * Back exits the view.
 */

#include "subghz_signal_visualizer.h"

#include <furi.h>
#include <furi_hal.h>
#include <gui/view.h>
#include <gui/elements.h>
#include <input/input.h>
#include <float_tools.h>
#include <string.h>

#define TAG "SubGhzVisualizer"

/* ── tuneable constants ──────────────────────────────────────────────────── */

/** Ring buffer depth.  Must be a power of two. */
#define VIZ_RAW_BUF_SIZE     1024u
#define VIZ_RAW_BUF_MASK     (VIZ_RAW_BUF_SIZE - 1u)

/** Number of horizontal pixels (= display columns = samples to show). */
#define VIZ_DISP_W           128u

/** Full oscilloscope height (pixels). */
#define VIZ_OSCOPE_H         64u

/** RSSI working range (dBm).  Values outside are clamped. */
#define RSSI_FLOOR           (-100.0f)
#define RSSI_CEIL            (-30.0f)
#define RSSI_SPAN            (RSSI_CEIL - RSSI_FLOOR)   /* 70 dBm */

/** Default trigger threshold (dBm). */
#define TRIGGER_DEFAULT      (-72.0f)
#define TRIGGER_STEP         (1.0f)
#define TRIGGER_MIN          RSSI_FLOOR
#define TRIGGER_MAX          (RSSI_CEIL - TRIGGER_STEP)

/** Sample timer period: 1 ms ≈ 1 kHz. */
#define SAMPLE_TIMER_MS      1u

/** Redraw timer period: ~33 ms ≈ 30 fps. */
#define REDRAW_TIMER_MS      33u

/* ── types ───────────────────────────────────────────────────────────────── */

typedef enum {
    VizModeBar = 0,
    VizModeLine,
    VizModeCount,
} VizMode;

/**
 * View model: written only from the redraw timer callback (under the view
 * mutex via with_view_model); read only from the draw callback (same mutex).
 */
typedef struct {
    /* 128 display samples, RSSI offset from RSSI_FLOOR, range 0..127 */
    uint8_t disp_samples[VIZ_DISP_W];

    VizMode        mode;
    float          trigger_threshold;  /* dBm */
    bool           trigger_active;     /* rising edge was detected this frame */
    char           freq_str[20];       /* e.g. "433.92 MHz" */
    char           mod_str[16];        /* e.g. "AM650" */
} SubGhzSignalVisualizerModel;

/**
 * Opaque view struct.  Fields here are accessed from multiple timer
 * callbacks and must be protected by sample_mutex.
 */
struct SubGhzSignalVisualizer {
    View*                          view;
    SubGhzSignalVisualizerCallback callback;
    void*                          context;
    SubGhzTxRx*                   txrx;

    /* 1 kHz ring buffer -------------------------------------------------- */
    FuriMutex* sample_mutex;
    int8_t     raw_buf[VIZ_RAW_BUF_SIZE]; /* RSSI as int8 (dBm, ~−127..0) */
    uint16_t   write_head;                /* next write position           */

    /* Trigger state (also protected by sample_mutex) --------------------- */
    float trigger_threshold;
    bool  trigger_armed;    /* true if RSSI was below threshold             */
    bool  trigger_active;   /* rising-edge latched this sample window       */
    uint16_t trigger_idx;   /* raw_buf index of the most-recent rising edge */

    /* Timers ------------------------------------------------------------- */
    FuriTimer* sample_timer; /* 1 ms  – fills raw_buf                       */
    FuriTimer* redraw_timer; /* 33 ms – snapshots into model, triggers draw  */

    bool running;
};

/* ── helper: RSSI → display level (0-3) ─────────────────────────────────── */

/* ── helper: RSSI → pixel height (0 = bottom, max = top) ────────────────── */

static inline uint8_t rssi_to_pixel_h(float rssi, uint8_t area_h) {
    if(rssi < RSSI_FLOOR) rssi = RSSI_FLOOR;
    if(rssi > RSSI_CEIL)  rssi = RSSI_CEIL;
    float norm = (rssi - RSSI_FLOOR) / RSSI_SPAN; /* 0.0 .. 1.0 */
    return (uint8_t)(norm * (float)(area_h - 1));
}

/* ── 1 kHz sample timer callback ────────────────────────────────────────── */

static void viz_sample_timer_callback(void* ctx) {
    SubGhzSignalVisualizer* inst = ctx;

    float rssi = subghz_txrx_radio_device_get_rssi(inst->txrx);
    int8_t rssi_i = (rssi < -127.0f) ? -127 : (rssi > 0.0f) ? 0 : (int8_t)rssi;

    furi_mutex_acquire(inst->sample_mutex, FuriWaitForever);

    inst->raw_buf[inst->write_head & VIZ_RAW_BUF_MASK] = rssi_i;

    /* Trigger edge detection: armed when RSSI drops below threshold. */
    if((float)rssi_i < inst->trigger_threshold) {
        inst->trigger_armed = true;
    } else if(inst->trigger_armed) {
        /* Rising edge above threshold while armed → latch. */
        inst->trigger_armed  = false;
        inst->trigger_active = true;
        inst->trigger_idx    = inst->write_head;
    }

    inst->write_head = (inst->write_head + 1u) & VIZ_RAW_BUF_MASK;

    furi_mutex_release(inst->sample_mutex);
}

/* ── 30 fps redraw timer callback ───────────────────────────────────────── */

static void viz_redraw_timer_callback(void* ctx) {
    SubGhzSignalVisualizer* inst = ctx;

    furi_mutex_acquire(inst->sample_mutex, FuriWaitForever);

    /* Choose display start: either a latched trigger index or the newest
     * VIZ_DISP_W samples (free-running). */
    uint16_t start;
    bool trig = inst->trigger_active;
    if(trig) {
        /* Show VIZ_DISP_W/4 samples of pre-trigger + signal. */
        start = (inst->trigger_idx - (VIZ_DISP_W / 4)) & VIZ_RAW_BUF_MASK;
        inst->trigger_active = false; /* consume latch */
    } else {
        /* Free-running: most recent VIZ_DISP_W samples. */
        start = (inst->write_head - VIZ_DISP_W) & VIZ_RAW_BUF_MASK;
    }

    /* Collect VIZ_DISP_W samples from the ring buffer. */
    int8_t snapshot[VIZ_DISP_W];
    for(uint16_t i = 0; i < VIZ_DISP_W; i++) {
        snapshot[i] = inst->raw_buf[(start + i) & VIZ_RAW_BUF_MASK];
    }

    float thr = inst->trigger_threshold;

    furi_mutex_release(inst->sample_mutex);

    /* ---------- update view model ---------- */
    with_view_model(
        inst->view,
        SubGhzSignalVisualizerModel * mdl,
        {
            /* Convert to uint8 offset (0..127) for compact model storage. */
            for(uint16_t i = 0; i < VIZ_DISP_W; i++) {
                float rv = (float)snapshot[i];
                if(rv < RSSI_FLOOR) rv = RSSI_FLOOR;
                if(rv > RSSI_CEIL)  rv = RSSI_CEIL;
                mdl->disp_samples[i] =
                    (uint8_t)((rv - RSSI_FLOOR) / RSSI_SPAN * 127.0f);
            }

            mdl->trigger_threshold = thr;
            mdl->trigger_active    = trig;

            /* Refresh frequency / modulation overlay every frame.
             * FuriString temps are allocated on the heap to avoid VLA. */
            FuriString* fs = furi_string_alloc();
            FuriString* ms = furi_string_alloc();
            subghz_txrx_get_frequency_and_modulation(inst->txrx, fs, ms, false);
            snprintf(mdl->freq_str, sizeof(mdl->freq_str), "%s", furi_string_get_cstr(fs));
            snprintf(mdl->mod_str,  sizeof(mdl->mod_str),  "%s", furi_string_get_cstr(ms));
            furi_string_free(fs);
            furi_string_free(ms);
        },
        true /* trigger redraw */);
}

/* ── draw helpers ─────────────────────────────────────────────────────────── */

/**
 * Draw the live signal display, full 128x64.  Two styles, both reading
 * from the same disp_samples[] live RSSI window — no "Classic" line-
 * waveform mode and no waterfall scrolling history exist anymore:
 *
 *   Bar  — each column is a filled vertical bar (signal "strength" view)
 *   Line — columns are connected with line segments, giving a continuous
 *          trace similar to a spectrum-analyzer envelope. A quiet/empty
 *          signal renders as a flat baseline.
 *
 * Also draws a dashed trigger threshold line and a "TRG" badge when the
 * trigger has just fired.
 */
static void draw_signal_area(Canvas* canvas, SubGhzSignalVisualizerModel* mdl) {
    const uint8_t y_top  = 0;
    const uint8_t area_h = VIZ_OSCOPE_H;

    canvas_draw_frame(canvas, 0, y_top, VIZ_DISP_W, area_h);

    /* Trigger threshold line (dashed, 1 pixel above the frame bottom). */
    float thr_norm = (mdl->trigger_threshold - RSSI_FLOOR) / RSSI_SPAN;
    if(thr_norm < 0.0f) thr_norm = 0.0f;
    if(thr_norm > 1.0f) thr_norm = 1.0f;
    uint8_t thr_y = (uint8_t)((float)(y_top + area_h - 2) -
                               thr_norm * (float)(area_h - 2));
    for(uint8_t x = 1; x < VIZ_DISP_W - 1; x += 4) {
        canvas_draw_dot(canvas, x, thr_y);
        canvas_draw_dot(canvas, x + 1, thr_y);
    }

    uint8_t y_base = (uint8_t)(y_top + area_h - 2);

    if(mdl->mode == VizModeLine) {
        int prev_x = -1, prev_y = y_base;
        for(uint8_t x = 1; x < VIZ_DISP_W - 1; x++) {
            uint8_t h = (uint8_t)((float)mdl->disp_samples[x] / 127.0f *
                                   (float)(area_h - 2));
            int y = y_base - h;
            if(prev_x >= 0) canvas_draw_line(canvas, prev_x, prev_y, x, y);
            prev_x = x;
            prev_y = y;
        }
    } else {
        /* Bar (default): filled vertical bars. */
        for(uint8_t x = 1; x < VIZ_DISP_W - 1; x++) {
            uint8_t h = (uint8_t)((float)mdl->disp_samples[x] / 127.0f *
                                   (float)(area_h - 2));
            if(h == 0) continue;
            canvas_draw_line(canvas, x, y_base, x, (uint8_t)(y_base - h));
        }
    }

    /* Trigger badge. */
    if(mdl->trigger_active) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, (uint8_t)(y_top + 8), "TRG");
    }
}

/* ── canvas draw callback ────────────────────────────────────────────────── */

static void viz_draw_callback(Canvas* canvas, void* model_ptr) {
    SubGhzSignalVisualizerModel* mdl = model_ptr;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* Both Bar and Line modes share the exact same full-screen layout and
     * overlay text — they only differ in how the signal itself is drawn,
     * handled inside draw_signal_area(). */
    draw_signal_area(canvas, mdl);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 7, mdl->freq_str);

    uint8_t mod_w = (uint8_t)(strlen(mdl->mod_str) * 5);
    canvas_draw_str(canvas, (uint8_t)(VIZ_DISP_W - mod_w - 3), 7, mdl->mod_str);

    char thr_label[16];
    snprintf(thr_label, sizeof(thr_label), "T:%.0f", (double)mdl->trigger_threshold);
    canvas_draw_str(canvas, 3, VIZ_OSCOPE_H - 2, thr_label);
}

/* ── input callback ──────────────────────────────────────────────────────── */

static bool viz_input_callback(InputEvent* event, void* ctx) {
    SubGhzSignalVisualizer* inst = ctx;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    switch(event->key) {
    case InputKeyUp:
        /* Raise trigger threshold (less sensitive). */
        furi_mutex_acquire(inst->sample_mutex, FuriWaitForever);
        inst->trigger_threshold += TRIGGER_STEP;
        if(inst->trigger_threshold > TRIGGER_MAX) inst->trigger_threshold = TRIGGER_MAX;
        furi_mutex_release(inst->sample_mutex);
        return true;

    case InputKeyDown:
        /* Lower trigger threshold (more sensitive). */
        furi_mutex_acquire(inst->sample_mutex, FuriWaitForever);
        inst->trigger_threshold -= TRIGGER_STEP;
        if(inst->trigger_threshold < TRIGGER_MIN) inst->trigger_threshold = TRIGGER_MIN;
        furi_mutex_release(inst->sample_mutex);
        return true;

    case InputKeyOk:
        /* Toggle display mode. */
        with_view_model(
            inst->view,
            SubGhzSignalVisualizerModel * mdl,
            { mdl->mode = (VizMode)((mdl->mode + 1u) % VizModeCount); },
            false);
        return true;

    case InputKeyBack:
        if(inst->callback) {
            inst->callback(SubGhzCustomEventViewSignalVisualizerBack, inst->context);
        }
        return true;

    default:
        return false;
    }
}

/* ── public API ──────────────────────────────────────────────────────────── */

SubGhzSignalVisualizer* subghz_signal_visualizer_alloc(SubGhzTxRx* txrx) {
    furi_assert(txrx);

    SubGhzSignalVisualizer* inst = malloc(sizeof(SubGhzSignalVisualizer));
    memset(inst, 0, sizeof(*inst));

    inst->txrx               = txrx;
    inst->trigger_threshold  = TRIGGER_DEFAULT;
    inst->trigger_armed      = false;
    inst->trigger_active     = false;
    inst->trigger_idx        = 0;
    inst->write_head         = 0;
    inst->running            = false;

    inst->sample_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    furi_assert(inst->sample_mutex);

    /* Allocate and configure view. */
    inst->view = view_alloc();
    view_set_context(inst->view, inst);
    view_allocate_model(inst->view, ViewModelTypeLocking,
                        sizeof(SubGhzSignalVisualizerModel));
    view_set_draw_callback(inst->view, viz_draw_callback);
    view_set_input_callback(inst->view, viz_input_callback);

    /* Initialise model defaults. */
    with_view_model(
        inst->view,
        SubGhzSignalVisualizerModel * mdl,
        {
            memset(mdl, 0, sizeof(*mdl));
            mdl->mode              = VizModeBar;
            mdl->trigger_threshold = TRIGGER_DEFAULT;
            strncpy(mdl->freq_str, "---", sizeof(mdl->freq_str));
            strncpy(mdl->mod_str,  "---", sizeof(mdl->mod_str));
        },
        false);

    /* Allocate timers (not started yet). */
    inst->sample_timer = furi_timer_alloc(viz_sample_timer_callback,
                                          FuriTimerTypePeriodic, inst);
    inst->redraw_timer = furi_timer_alloc(viz_redraw_timer_callback,
                                          FuriTimerTypePeriodic, inst);

    FURI_LOG_I(TAG, "Allocated");
    return inst;
}

void subghz_signal_visualizer_free(SubGhzSignalVisualizer* instance) {
    furi_assert(instance);

    subghz_signal_visualizer_stop(instance);

    furi_timer_free(instance->redraw_timer);
    furi_timer_free(instance->sample_timer);

    view_free(instance->view);
    furi_mutex_free(instance->sample_mutex);

    free(instance);
    FURI_LOG_I(TAG, "Freed");
}

View* subghz_signal_visualizer_get_view(SubGhzSignalVisualizer* instance) {
    furi_assert(instance);
    return instance->view;
}

void subghz_signal_visualizer_set_callback(
    SubGhzSignalVisualizer*         instance,
    SubGhzSignalVisualizerCallback  callback,
    void*                           context)
{
    furi_assert(instance);
    furi_assert(callback);
    instance->callback = callback;
    instance->context  = context;
}

void subghz_signal_visualizer_start(SubGhzSignalVisualizer* instance) {
    furi_assert(instance);

    if(instance->running) return;
    instance->running = true;

    /* Put radio into RX if it isn't already. */
    subghz_txrx_rx_start(instance->txrx);

    /* Reset ring buffer and trigger state. */
    furi_mutex_acquire(instance->sample_mutex, FuriWaitForever);
    memset(instance->raw_buf, (int8_t)RSSI_FLOOR, sizeof(instance->raw_buf));
    instance->write_head     = 0;
    instance->trigger_armed  = false;
    instance->trigger_active = false;
    furi_mutex_release(instance->sample_mutex);

    /* Start sample timer first (1 ms), then the slower redraw timer. */
    furi_timer_start(instance->sample_timer, SAMPLE_TIMER_MS);
    furi_timer_start(instance->redraw_timer, REDRAW_TIMER_MS);

    FURI_LOG_I(TAG, "Started – sample %u ms, redraw %u ms",
               SAMPLE_TIMER_MS, REDRAW_TIMER_MS);
}

void subghz_signal_visualizer_stop(SubGhzSignalVisualizer* instance) {
    furi_assert(instance);

    if(!instance->running) return;
    instance->running = false;

    furi_timer_stop(instance->redraw_timer);
    furi_timer_stop(instance->sample_timer);

    FURI_LOG_I(TAG, "Stopped");
}

uint32_t subghz_signal_visualizer_get_mode(SubGhzSignalVisualizer* instance) {
    furi_assert(instance);
    uint32_t mode = 0;
    with_view_model(
        instance->view,
        SubGhzSignalVisualizerModel * mdl,
        { mode = (uint32_t)mdl->mode; },
        false);
    return mode;
}

void subghz_signal_visualizer_set_mode(SubGhzSignalVisualizer* instance, uint32_t mode) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        SubGhzSignalVisualizerModel * mdl,
        { mdl->mode = (VizMode)(mode % VizModeCount); },
        false);
}
