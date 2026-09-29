/*
 * Clemsa MV-12 MASTERcode remote for Flipper Zero.
 *
 * WHY THIS EXISTS
 * ---------------
 * The Flipper decodes Clemsa MasterCode perfectly and still cannot open the
 * door -- neither native emulation nor RAW replay, on any preset or frequency.
 * That has been an open complaint since 2023 (flipperzero-firmware#3182).
 *
 * The reason is that the MV-12 uses "carrier over carrier", like a 38 kHz IR
 * remote. Inside every "carrier on" period it chops the 433.92 MHz carrier at a
 * ~65 us chip rate -- a ~7.7 kHz subcarrier. The receiver requires it. A solid
 * gated carrier, which is what Mastercode emulation and RAW replay both emit,
 * is ignored.
 *
 * Slicer-based receivers smooth the chopping away, so a capture looks like
 * textbook Mastercode and the subcarrier never appears in the .sub. That is why
 * the bits always verified correct while the door stayed shut.
 *
 * Credit: jesusvallejo (github.com/jesusvallejo/ESPHOME_GARAGE) for the answer,
 * skotopes for independently spotting the second carrier on physical remotes.
 *
 * WAVEFORM
 * --------
 *   chip     = 65 us
 *   bit slot = 48 chips = 3120 us
 *   bit '1'  = 16 subcarrier cycles, then 16 chips silence (burst 2015 us)
 *   bit '0'  =  8 subcarrier cycles, then 32 chips silence (burst  975 us)
 *   frame    = 36 bits MSB first, then ~16 ms silence
 *   payload  = [35:20] preamble 0xB7E0 | [19:4] serial | [3:2] button | [1:0] 00
 *
 * The serial is not a factory secret: it is the 8 trinary DIP switches, two
 * bits per switch (down=00, middle=10, up=11), switch 1 in the lowest pair.
 * Set yours under Settings -> DIP switches; nothing is hard-coded per remote.
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>
#include <toolbox/level_duration.h>
#include <subghz/devices/cc1101_configs.h>

#define TAG "ClemsaMV12"

/* ------------------------------------------------------------------ waveform */
#define NBITS          36
#define BIT_SLOT_CHIPS 48
#define ONE_CYCLES     16
#define ZERO_CYCLES    8
#define PREAMBLE       0xB7E0UL
#define GUARD_US       16000

/* Worst case is the all-ones serial: preamble contributes 10 ones, serial 16,
 * button 1 -> 27 one-bits at 32 entries each plus 9 zero-bits at 16 = 1008. */
#define MAX_LD 1152

/* ------------------------------------------------------------------ settings */
#define CLEMSA_DIR       EXT_PATH("apps_data/clemsa_mv12")
#define CLEMSA_SETTINGS  CLEMSA_DIR "/settings.bin"
#define SETTINGS_MAGIC   0x4D
#define SETTINGS_VERSION 1

#define FREQ_MIN  433050000UL
#define FREQ_MAX  434790000UL
#define FREQ_STEP 10000UL

typedef struct {
    uint8_t dip[8]; /* 0 = down '0', 1 = middle 'F', 2 = up '1' */
    uint16_t chip_us;
    uint8_t reps;
    uint32_t frequency;
} ClemsaSettings;

static const ClemsaSettings SETTINGS_DEFAULT = {
    .dip = {0, 0, 0, 0, 0, 0, 0, 0}, /* neutral placeholder -> serial 0x0000 */
    .chip_us = 65,
    .reps = 10,
    .frequency = 433920000UL,
};

static const char TRIT_LABEL[3] = {'0', 'F', '1'};
static const uint8_t TRIT_BITS[3] = {0x0, 0x2, 0x3};

/* ----------------------------------------------------------------------- app */
typedef enum {
    ViewMain,
    ViewConfig,
    ViewDip,
} ClemsaViewId;

typedef enum {
    CfgDip,
    CfgChip,
    CfgReps,
    CfgFreq,
    CfgCount,
} ClemsaCfgItem;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* input_queue;
    NotificationApp* notifications;
    FuriMutex* mutex;

    ClemsaSettings st;
    ClemsaViewId view;
    uint8_t door; /* 0 = door 1, 1 = door 2 */
    uint8_t cfg_sel;
    uint8_t dip_sel;
    bool sending;
    bool tx_failed; /* last press was refused by the region / invalid frequency */
    bool running;

    LevelDuration ld[MAX_LD];
    size_t ld_count;
    size_t ld_index;
    uint8_t tx_rep;
} ClemsaApp;

/* ------------------------------------------------------------------- payload */
static uint16_t clemsa_serial(const ClemsaSettings* s) {
    uint16_t v = 0;
    for(uint8_t i = 0; i < 8; i++) v |= (uint16_t)TRIT_BITS[s->dip[i]] << (2 * i);
    return v;
}

static uint64_t clemsa_payload(const ClemsaSettings* s, uint8_t door) {
    uint64_t p = (uint64_t)PREAMBLE << 20;
    p |= (uint64_t)clemsa_serial(s) << 4;
    p |= (uint64_t)(door == 0 ? 0x2 : 0x1) << 2; /* door1 = '10', door2 = '01' */
    return p;
}

static void clemsa_dip_str(const ClemsaSettings* s, char* out) {
    for(uint8_t i = 0; i < 8; i++) out[i] = TRIT_LABEL[s->dip[i]];
    out[8] = '\0';
}

/* Flipper's reduced printf has no reliable %llX, so split the 36-bit word. */
static void clemsa_payload_str(uint64_t p, char* out, size_t n) {
    snprintf(
        out,
        n,
        "0x%01lX%08lX",
        (unsigned long)((p >> 32) & 0xF),
        (unsigned long)(p & 0xFFFFFFFF));
}

/* ---------------------------------------------------------------- modulation */
/* Emits strictly alternating levels, exactly like a RAW .sub: for each bit,
 * `cycles` HIGH chips interleaved with single LOW chips, then one long LOW that
 * absorbs the final cycle's LOW plus the slot's remaining silence. */
static size_t clemsa_build(LevelDuration* out, size_t cap, uint64_t payload, uint16_t chip) {
    size_t n = 0;
    for(int8_t i = NBITS - 1; i >= 0; i--) {
        uint8_t cycles = ((payload >> i) & 1ULL) ? ONE_CYCLES : ZERO_CYCLES;
        for(uint8_t c = 0; c < cycles; c++) {
            if(n + 2 >= cap) return n;
            out[n++] = level_duration_make(true, chip);
            if(c + 1 < cycles) out[n++] = level_duration_make(false, chip);
        }
        if(n >= cap) return n;
        uint32_t tail = (uint32_t)(1 + BIT_SLOT_CHIPS - 2 * cycles) * chip;
        if(i == 0) tail += GUARD_US;
        out[n++] = level_duration_make(false, tail);
    }
    return n;
}

static LevelDuration clemsa_tx_yield(void* ctx) {
    ClemsaApp* app = ctx;
    if(app->ld_index >= app->ld_count) {
        app->tx_rep++;
        if(app->tx_rep >= app->st.reps) return level_duration_reset();
        app->ld_index = 0;
    }
    return app->ld[app->ld_index++];
}

static bool clemsa_transmit(ClemsaApp* app) {
    uint64_t payload = clemsa_payload(&app->st, app->door);
    app->ld_count = clemsa_build(app->ld, MAX_LD, payload, app->st.chip_us);
    app->ld_index = 0;
    app->tx_rep = 0;

    if(!furi_hal_subghz_is_frequency_valid(app->st.frequency)) {
        FURI_LOG_E(TAG, "frequency %lu not valid", (unsigned long)app->st.frequency);
        return false;
    }

    furi_hal_power_suppress_charge_enter();

    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    /* Unleashed / newer official firmware dropped the FuriHalSubGhzPreset enum
     * from the HAL; the register tables are exported instead. On older official
     * builds this line is furi_hal_subghz_load_preset(FuriHalSubGhzPresetOok650Async).
     * AM650 rather than AM270 because the 65 us chips need the wider filter. */
    furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_ook_650khz_async_regs);
    furi_hal_subghz_set_frequency_and_path(app->st.frequency);
    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_cc1101_g0, false);

    /* Returns false when the region forbids TX here. Without this check the
     * wait below would spin forever, and the input queue is not serviced during
     * a transmit -- the app would hang with no way out. */
    bool started = furi_hal_subghz_start_async_tx(clemsa_tx_yield, app);
    if(started) {
        /* One frame is ~128 ms; bound the wait generously in case the async TX
         * stalls, so a radio fault degrades to a failed press, not a lockup. */
        uint32_t timeout_ms = (uint32_t)app->st.reps * 200 + 1000;
        uint32_t waited = 0;
        while(!furi_hal_subghz_is_async_tx_complete() && waited < timeout_ms) {
            furi_delay_ms(5);
            waited += 5;
        }
        if(waited >= timeout_ms) FURI_LOG_E(TAG, "async tx timed out");
        furi_hal_subghz_stop_async_tx();
    } else {
        FURI_LOG_E(TAG, "tx not allowed at %lu Hz", (unsigned long)app->st.frequency);
    }

    furi_hal_subghz_sleep();
    furi_hal_power_suppress_charge_exit();
    return started;
}

/* -------------------------------------------------------------------- config */
static const char* clemsa_cfg_name(ClemsaCfgItem item) {
    switch(item) {
    case CfgDip:  return "DIP switches";
    case CfgChip: return "Chip period";
    case CfgReps: return "Repeats";
    case CfgFreq: return "Frequency";
    default:      return "";
    }
}

static void clemsa_cfg_value(const ClemsaApp* app, ClemsaCfgItem item, char* out, size_t n) {
    switch(item) {
    case CfgDip:
        clemsa_dip_str(&app->st, out);
        break;
    case CfgChip:
        snprintf(out, n, "%u us", (unsigned)app->st.chip_us);
        break;
    case CfgReps:
        snprintf(out, n, "%u", (unsigned)app->st.reps);
        break;
    case CfgFreq:
        snprintf(
            out,
            n,
            "%lu.%02lu MHz",
            (unsigned long)(app->st.frequency / 1000000UL),
            (unsigned long)((app->st.frequency % 1000000UL) / 10000UL));
        break;
    default:
        out[0] = '\0';
        break;
    }
}

static void clemsa_cfg_adjust(ClemsaApp* app, ClemsaCfgItem item, int dir) {
    switch(item) {
    case CfgChip: {
        int v = app->st.chip_us + dir;
        if(v < 55) v = 55;
        if(v > 80) v = 80;
        app->st.chip_us = (uint16_t)v;
        break;
    }
    case CfgReps: {
        int v = app->st.reps + dir;
        if(v < 3) v = 3;
        if(v > 40) v = 40;
        app->st.reps = (uint8_t)v;
        break;
    }
    case CfgFreq: {
        int64_t v = (int64_t)app->st.frequency + (int64_t)dir * FREQ_STEP;
        if(v < (int64_t)FREQ_MIN) v = FREQ_MIN;
        if(v > (int64_t)FREQ_MAX) v = FREQ_MAX;
        app->st.frequency = (uint32_t)v;
        break;
    }
    default:
        break;
    }
}

static void clemsa_settings_load(ClemsaApp* app) {
    if(!saved_struct_load(
           CLEMSA_SETTINGS, &app->st, sizeof(ClemsaSettings), SETTINGS_MAGIC, SETTINGS_VERSION)) {
        FURI_LOG_I(TAG, "no saved settings, using defaults");
        app->st = SETTINGS_DEFAULT;
    }
    /* A corrupt or hand-edited file must not be able to index out of bounds. */
    for(uint8_t i = 0; i < 8; i++)
        if(app->st.dip[i] > 2) app->st.dip[i] = 0;
    if(app->st.chip_us < 55 || app->st.chip_us > 80) app->st.chip_us = SETTINGS_DEFAULT.chip_us;
    if(app->st.reps < 3 || app->st.reps > 40) app->st.reps = SETTINGS_DEFAULT.reps;
    if(app->st.frequency < FREQ_MIN || app->st.frequency > FREQ_MAX)
        app->st.frequency = SETTINGS_DEFAULT.frequency;
}

static void clemsa_settings_save(ClemsaApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, CLEMSA_DIR);
    furi_record_close(RECORD_STORAGE);

    if(!saved_struct_save(
           CLEMSA_SETTINGS, &app->st, sizeof(ClemsaSettings), SETTINGS_MAGIC, SETTINGS_VERSION)) {
        FURI_LOG_E(TAG, "settings save failed");
    }
}

/* --------------------------------------------------------------------- views */
static void clemsa_draw_main(Canvas* canvas, ClemsaApp* app) {
    char buf[32];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 9, "Clemsa MV-12");
    clemsa_dip_str(&app->st, buf);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, buf);
    canvas_draw_line(canvas, 0, 12, 127, 12);

    for(uint8_t i = 0; i < 2; i++) {
        int x = i ? 68 : 4;
        bool sel = (app->door == i);
        if(sel) {
            canvas_draw_rbox(canvas, x, 16, 56, 26, 3);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, x, 16, 56, 26, 3);
        }
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, x + 28, 26, AlignCenter, AlignCenter, i ? "DOOR 2" : "DOOR 1");
        canvas_set_font(canvas, FontSecondary);
        if(sel) {
            const char* label = "OK to open";
            if(app->sending) label = "SENDING";
            else if(app->tx_failed) label = "TX BLOCKED";
            canvas_draw_str_aligned(canvas, x + 28, 36, AlignCenter, AlignCenter, label);
            canvas_set_color(canvas, ColorBlack);
        }
    }

    canvas_set_font(canvas, FontSecondary);
    snprintf(
        buf,
        sizeof(buf),
        "%lu.%02lu  %uus  x%u",
        (unsigned long)(app->st.frequency / 1000000UL),
        (unsigned long)((app->st.frequency % 1000000UL) / 10000UL),
        (unsigned)app->st.chip_us,
        (unsigned)app->st.reps);
    canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignBottom, buf);
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "OK send   Up config");
}

static void clemsa_draw_config(Canvas* canvas, ClemsaApp* app) {
    char buf[24];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 9, "Settings");
    canvas_draw_line(canvas, 0, 12, 127, 12);

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < CfgCount; i++) {
        int y = 16 + i * 10;
        bool sel = (i == app->cfg_sel);
        if(sel) {
            canvas_draw_box(canvas, 0, y, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 3, y + 8, clemsa_cfg_name(i));
        clemsa_cfg_value(app, i, buf, sizeof(buf));
        canvas_draw_str_aligned(canvas, 125, y + 8, AlignRight, AlignBottom, buf);
        if(sel) canvas_set_color(canvas, ColorBlack);
    }

    canvas_draw_str_aligned(
        canvas, 64, 63, AlignCenter, AlignBottom,
        app->cfg_sel == CfgDip ? "OK edit   Back save" : "< > change   Back save");
}

static void clemsa_draw_dip(Canvas* canvas, ClemsaApp* app) {
    char buf[24];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 9, "DIP");
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "serial 0x%04X", (unsigned)clemsa_serial(&app->st));
    canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, buf);
    canvas_draw_line(canvas, 0, 12, 127, 12);

    /* Eight switches laid out the way they sit on the remote: '1' up, 'F'
     * middle, '0' down. */
    for(uint8_t i = 0; i < 8; i++) {
        int x = 8 + i * 14;
        char label[2] = {(char)('1' + i), '\0'};

        if(i == app->dip_sel) {
            canvas_draw_box(canvas, x, 14, 12, 9);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(canvas, x + 6, 22, AlignCenter, AlignBottom, label);
        if(i == app->dip_sel) canvas_set_color(canvas, ColorBlack);

        canvas_draw_frame(canvas, x, 24, 12, 30);
        for(uint8_t slot = 0; slot < 3; slot++) {
            uint8_t trit = 2 - slot;
            int cy = 24 + slot * 10;
            bool on = (app->st.dip[i] == trit);
            if(on) {
                canvas_draw_box(canvas, x + 1, cy + 1, 10, 8);
                canvas_set_color(canvas, ColorWhite);
            }
            char t[2] = {TRIT_LABEL[trit], '\0'};
            canvas_draw_str_aligned(canvas, x + 6, cy + 9, AlignCenter, AlignBottom, t);
            if(on) canvas_set_color(canvas, ColorBlack);
        }
    }

    clemsa_payload_str(clemsa_payload(&app->st, app->door), buf, sizeof(buf));
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, buf);
}

static void clemsa_draw_callback(Canvas* canvas, void* ctx) {
    ClemsaApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    switch(app->view) {
    case ViewMain:   clemsa_draw_main(canvas, app);   break;
    case ViewConfig: clemsa_draw_config(canvas, app); break;
    case ViewDip:    clemsa_draw_dip(canvas, app);    break;
    }

    furi_mutex_release(app->mutex);
}

static void clemsa_input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, FuriWaitForever);
}

/* --------------------------------------------------------------------- input */
/* Returns true when a transmit was requested; the caller runs it outside the
 * mutex so the GUI thread can keep painting the "SENDING" state. */
static bool clemsa_handle_input(ClemsaApp* app, InputEvent* event) {
    bool transmit = false;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    switch(app->view) {
    case ViewMain:
        switch(event->key) {
        case InputKeyLeft:  app->door = 0; break;
        case InputKeyRight: app->door = 1; break;
        case InputKeyOk:    transmit = true; break;
        case InputKeyUp:    app->view = ViewConfig; break;
        case InputKeyBack:  app->running = false; break;
        default: break;
        }
        break;

    case ViewConfig:
        switch(event->key) {
        case InputKeyUp:
            app->cfg_sel = (app->cfg_sel + CfgCount - 1) % CfgCount;
            break;
        case InputKeyDown:
            app->cfg_sel = (app->cfg_sel + 1) % CfgCount;
            break;
        case InputKeyLeft:
            if(app->cfg_sel == CfgDip) app->view = ViewDip;
            else clemsa_cfg_adjust(app, app->cfg_sel, -1);
            break;
        case InputKeyRight:
            if(app->cfg_sel == CfgDip) app->view = ViewDip;
            else clemsa_cfg_adjust(app, app->cfg_sel, +1);
            break;
        case InputKeyOk:
            if(app->cfg_sel == CfgDip) app->view = ViewDip;
            break;
        case InputKeyBack:
            clemsa_settings_save(app);
            app->view = ViewMain;
            break;
        default: break;
        }
        break;

    case ViewDip:
        switch(event->key) {
        case InputKeyLeft:
            app->dip_sel = (app->dip_sel + 7) % 8;
            break;
        case InputKeyRight:
            app->dip_sel = (app->dip_sel + 1) % 8;
            break;
        case InputKeyUp:
            app->st.dip[app->dip_sel] = (app->st.dip[app->dip_sel] + 1) % 3;
            break;
        case InputKeyDown:
            app->st.dip[app->dip_sel] = (app->st.dip[app->dip_sel] + 2) % 3;
            break;
        case InputKeyOk:
        case InputKeyBack:
            app->view = ViewConfig;
            break;
        default: break;
        }
        break;
    }

    furi_mutex_release(app->mutex);
    return transmit;
}

/* ---------------------------------------------------------------------- main */
int32_t clemsa_mv12_app(void* p) {
    UNUSED(p);

    ClemsaApp* app = malloc(sizeof(ClemsaApp));
    app->view = ViewMain;
    app->door = 0;
    app->cfg_sel = 0;
    app->dip_sel = 0;
    app->sending = false;
    app->tx_failed = false;
    app->running = true;
    app->ld_count = 0;
    app->ld_index = 0;
    app->tx_rep = 0;

    clemsa_settings_load(app);

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, clemsa_draw_callback, app);
    view_port_input_callback_set(app->view_port, clemsa_input_callback, app->input_queue);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    FURI_LOG_I(TAG, "serial 0x%04X chip %u us", (unsigned)clemsa_serial(&app->st),
               (unsigned)app->st.chip_us);

    InputEvent event;
    while(app->running) {
        if(furi_message_queue_get(app->input_queue, &event, 100) != FuriStatusOk) continue;
        if(event.type != InputTypeShort && event.type != InputTypeRepeat) continue;

        bool transmit = clemsa_handle_input(app, &event);
        view_port_update(app->view_port);

        if(transmit) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->sending = true;
            furi_mutex_release(app->mutex);
            view_port_update(app->view_port);

            notification_message(app->notifications, &sequence_blink_start_magenta);
            bool ok = clemsa_transmit(app);
            notification_message(app->notifications, &sequence_blink_stop);
            if(!ok) notification_message(app->notifications, &sequence_error);

            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->sending = false;
            app->tx_failed = !ok;
            furi_mutex_release(app->mutex);
            view_port_update(app->view_port);
        }
    }

    clemsa_settings_save(app);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
