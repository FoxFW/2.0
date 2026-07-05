/**
 * @file subghz_modulation_analyzer_app.c
 *
 * Two-screen app:
 *
 * SCREEN 1 — Frequency Setup
 *   Title "Set Frequency" (bold)
 *   Large digit-by-digit frequency editor: XXX.XX (MHz)
 *   Left/Right move the cursor between digits, Up/Down change the digit.
 *   "[OK] Continue" at the bottom, Back exits the app.
 *   Frequency is persisted to /ext/subghz/.modana_freq so it's remembered.
 *
 * SCREEN 2 — Modulation Scanning
 *   Same layout as the stock Frequency Analyzer, with:
 *     - Fixed frequency shown top-right (the one set on Screen 1)
 *     - Current modulation preset shown large on the left (abbreviated)
 *     - Modulation history list (detected presets, 4 slots)
 *     - RSSI bar graph + trigger controls
 *   Back returns to Screen 1.
 *   OK on a detected preset: writes Frequency + Preset to last_subghz.settings
 *   and relaunches SubGHz with "read" to open the Receiver pre-configured.
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/elements.h>
#include <input/input.h>
#include <loader/loader.h>
#include <storage/storage.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <lib/flipper_format/flipper_format.h>
#include <lib/subghz/subghz_setting.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ── Verified register tables from cc1101_configs.h ── */
extern const uint8_t subghz_device_cc1101_preset_ook_270khz_async_regs[];
extern const uint8_t subghz_device_cc1101_preset_ook_650khz_async_regs[];
extern const uint8_t subghz_device_cc1101_preset_2fsk_dev2_38khz_async_regs[];
extern const uint8_t subghz_device_cc1101_preset_2fsk_dev12khz_async_regs[];
extern const uint8_t subghz_device_cc1101_preset_2fsk_dev47_6khz_async_regs[];
extern const uint8_t subghz_device_cc1101_preset_msk_99_97kb_async_regs[];
extern const uint8_t subghz_device_cc1101_preset_gfsk_9_99kb_async_regs[];

/* Preset arrays removed — names/data loaded dynamically from SubGhzSetting.
 * Adding new modulations requires no changes to this file. */

/* ── Modulation filter (mirrors SUBGHZ_MOD_FILTER_COUNT from main app) ── */
#define MA_MOD_FILTER_COUNT 64u
#define MA_MOD_FILTER_PATH "/ext/subghz/modulation_filter.save"

static uint8_t g_mod_filter[MA_MOD_FILTER_COUNT]; /* 0=disabled, 1=enabled */
static bool    g_mod_filter_loaded = false;

static void ma_load_mod_filter(void) {
    memset(g_mod_filter, 0x01, sizeof(g_mod_filter)); /* default: all on */
    Storage* s = furi_record_open(RECORD_STORAGE);
    File*    f = storage_file_alloc(s);
    if(storage_file_open(f, MA_MOD_FILTER_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(f, g_mod_filter, sizeof(g_mod_filter));
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    g_mod_filter_loaded = true;
}

/* Returns next enabled preset index, or current if all disabled */
static uint8_t ma_next_enabled_preset(uint8_t current, uint8_t count) {
    if(!g_mod_filter_loaded) return (uint8_t)((current + 1) % count);
    uint8_t next  = (uint8_t)((current + 1) % count);
    uint8_t guard = 0;
    while(g_mod_filter[next] == 0x00 && guard++ < count)
        next = (uint8_t)((next + 1) % count);
    return next;
}
#define RSSI_MIN      (-97.0f)
#define RSSI_MAX      (-60.0f)
#define RSSI_SCALE    2.3f
#define TRIGGER_STEP  1.0f
#define MAX_HISTORY   4
#define DWELL_MS      120u
#define REDRAW_MS     80u

#define MODANA_FREQ_FILE EXT_PATH("subghz/.modana_freq")
#define LAST_SETTINGS_PATH EXT_PATH("subghz/assets/last_subghz.settings")
#define LAST_SETTINGS_TYPE    "Flipper SubGhz Last Setting File"
#define LAST_SETTINGS_VERSION 3

static inline bool feq(float a, float b) { return fabsf(a-b) < 0.001f; }

typedef enum { ScreenFreqSetup, ScreenScanning } AppScreen;
typedef enum { FeedbackMute=0, FeedbackSound, FeedbackAll, FeedbackVibro, FeedbackCount } FeedbackLevel;
/* Cycle: Mute(⊘) → Sound(🔊) → Sound+Vibrate → Vibrate → Mute */

/* Frequency digits: d[0..2] = MHz hundreds/tens/ones, d[3..4] = kHz */
/* Display: d[0]d[1]d[2].d[3]d[4] MHz */

typedef struct {
    Gui*              gui;
    ViewPort*         view_port;
    FuriMessageQueue* input_queue;
    FuriMutex*        mutex;
    NotificationApp*  notifications;
    FuriTimer*        dwell_timer;

    AppScreen  screen;

    /* Frequency setup */
    uint8_t    freq_digits[5]; /* 0-9 each */
    uint8_t    cursor;         /* 0-4, which digit is selected */

    /* Scanning state */
    uint32_t   scan_freq;      /* Hz, derived from freq_digits */
    uint8_t    preset_idx;     /* current scanning preset */
    float      current_rssi;
    bool       locked;
    uint8_t    locked_preset;
    float      locked_rssi;
    float      rssi_last;
    float      trigger;

    uint8_t    history[MAX_HISTORY];
    uint8_t    history_count[MAX_HISTORY];
    uint8_t    history_len;
    uint8_t    selected_index;
    bool       show_frame;

    FeedbackLevel feedback;
    bool       exit_requested;
    bool       ok_result_selected;
    SubGhzSetting* setting;     /* loaded at alloc, freed at free */
    uint8_t    preset_count;   /* subghz_setting_get_preset_count() */
} ModAnalApp;

/* ── Persistence ──────────────────────────────────────────────────────── */

static uint32_t digits_to_hz(const uint8_t* d) {
    /* d[0..2] = MHz part, d[3..4] = 10kHz / 100kHz part */
    uint32_t mhz = (uint32_t)d[0]*100 + d[1]*10 + d[2];
    uint32_t khz = (uint32_t)d[3]*10  + d[4]; /* in units of 10kHz */
    return mhz * 1000000u + khz * 10000u;
}

static void hz_to_digits(uint32_t hz, uint8_t* d) {
    uint32_t mhz = hz / 1000000u;
    uint32_t rem = (hz % 1000000u) / 10000u; /* 0-99 */
    d[0] = (uint8_t)(mhz / 100 % 10);
    d[1] = (uint8_t)(mhz / 10  % 10);
    d[2] = (uint8_t)(mhz       % 10);
    d[3] = (uint8_t)(rem / 10  % 10);
    d[4] = (uint8_t)(rem       % 10);
}

static void save_freq(ModAnalApp* app) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(s);
    if(flipper_format_file_open_existing(ff, LAST_SETTINGS_PATH)) {
        uint32_t freq_hz = digits_to_hz(app->freq_digits);
        if(!flipper_format_update_uint32(ff, "Frequency", &freq_hz, 1)) {
            flipper_format_write_uint32(ff, "Frequency", &freq_hz, 1);
        }
    }
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

static void load_freq(ModAnalApp* app) {
    /* Default: 433.92 MHz */
    hz_to_digits(433920000u, app->freq_digits);
    Storage* s = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(s);
    if(flipper_format_file_open_existing(ff, LAST_SETTINGS_PATH)) {
        uint32_t version = 0;
        FuriString* type = furi_string_alloc();
        if(flipper_format_read_header(ff, type, &version) &&
           furi_string_equal_str(type, LAST_SETTINGS_TYPE) &&
           version == LAST_SETTINGS_VERSION) {
            uint32_t freq_hz = 0;
            if(flipper_format_read_uint32(ff, "Frequency", &freq_hz, 1) && freq_hz > 0) {
                hz_to_digits(freq_hz, app->freq_digits);
            }
        }
        furi_string_free(type);
    }
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

static void load_trigger(ModAnalApp* app) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(s);
    if(flipper_format_file_open_existing(ff, LAST_SETTINGS_PATH)) {
        uint32_t version = 0;
        FuriString* type = furi_string_alloc();
        if(flipper_format_read_header(ff, type, &version) &&
           furi_string_equal_str(type, LAST_SETTINGS_TYPE) &&
           version == LAST_SETTINGS_VERSION) {
            float trigger = RSSI_MIN;
            if(flipper_format_read_float(ff, "FATrigger", &trigger, 1)) {
                app->trigger = trigger;
            }
            flipper_format_rewind(ff);
            uint32_t fb = 0;
            if(flipper_format_read_uint32(ff, "FeedbackLevel", &fb, 1)) {
                app->feedback = (FeedbackLevel)(fb < (uint32_t)FeedbackCount ? fb : 0);
            }
            flipper_format_rewind(ff);
            uint32_t pi = 0;
            if(flipper_format_read_uint32(ff, "FAPresetIndex", &pi, 1)) {
                if(pi < (uint32_t)app->preset_count) app->preset_idx = (uint8_t)pi;
            }
        }
        furi_string_free(type);
    }
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

static void save_trigger(ModAnalApp* app) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(s);
    if(flipper_format_file_open_existing(ff, LAST_SETTINGS_PATH)) {
        /* No rewind before the fallback write — cursor is at EOF after a
         * failed update, so write_* appends correctly.  Rewind is only
         * needed between separate key searches.                          */
        if(!flipper_format_update_float(ff, "FATrigger", &app->trigger, 1)) {
            flipper_format_write_float(ff, "FATrigger", &app->trigger, 1);
        }
        flipper_format_rewind(ff);
        uint32_t fb = (uint32_t)app->feedback;
        if(!flipper_format_update_uint32(ff, "FeedbackLevel", &fb, 1)) {
            flipper_format_write_uint32(ff, "FeedbackLevel", &fb, 1);
        }
        flipper_format_rewind(ff);
        uint32_t pi = (uint32_t)app->preset_idx;
        if(!flipper_format_update_uint32(ff, "FAPresetIndex", &pi, 1)) {
            flipper_format_write_uint32(ff, "FAPresetIndex", &pi, 1);
        }
    }
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

/* Write selected freq+preset to last_subghz.settings so SubGHz opens
 * the Receiver already tuned to the right frequency and modulation. */
static void write_result_to_settings(ModAnalApp* app, uint32_t freq_hz, uint8_t preset_idx) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(s);
    if(flipper_format_file_open_existing(ff, LAST_SETTINGS_PATH)) {
        if(!flipper_format_update_uint32(ff, "Frequency", &freq_hz, 1)) {
            flipper_format_write_uint32(ff, "Frequency", &freq_hz, 1);
        }
        /* Preset is stored by name in last_subghz.settings */
        flipper_format_rewind(ff);
        FuriString* preset_str = furi_string_alloc_set(
            subghz_setting_get_preset_name(app->setting, preset_idx));
        if(!flipper_format_update_string(ff, "Preset", preset_str)) {
            flipper_format_write_string(ff, "Preset", preset_str);
        }
        furi_string_free(preset_str);
    }
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

/* ── Dwell timer: sample → hop to next modulation ─────────────────────── */

static void dwell_timer_cb(void* ctx) {
    ModAnalApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    /* Safety guard: only run if we're actually on the scanning screen with
     * a valid frequency set. If scan_freq is 0 (setup screen, or timer
     * fired before start_scanning set it), calling
     * furi_hal_subghz_set_frequency_and_path(0) triggers a furi_check
     * assert on the invalid frequency. */
    if(app->screen != ScreenScanning || app->scan_freq == 0) {
        furi_mutex_release(app->mutex);
        return;
    }

    float rssi = furi_hal_subghz_get_rssi();
    bool was_locked = app->locked;

    if(rssi > app->trigger) {
        if(!app->locked) {
            app->locked        = true;
            app->locked_preset = app->preset_idx;
            app->locked_rssi   = rssi;
            /* First lock — play feedback per user setting.
             * sequence_success plays BOTH sound and vibro, so we use a
             * custom sound-only sequence for FeedbackSound. */
            if(app->feedback == FeedbackSound) {
                static const NotificationSequence seq_sound_only = {
                    &message_note_c5,
                    &message_delay_50,
                    &message_sound_off,
                    NULL,
                };
                notification_message(app->notifications, &seq_sound_only);
            } else if(app->feedback == FeedbackAll) {
                notification_message(app->notifications, &sequence_success);
            } else if(app->feedback == FeedbackVibro) {
                notification_message(app->notifications, &sequence_single_vibro);
            }
        } else if(rssi > app->locked_rssi) {
            app->locked_rssi = rssi;
        }
        if(rssi > app->rssi_last) app->rssi_last = rssi;
    } else {
        if(was_locked && app->locked) {
            uint8_t nf = app->locked_preset;
            bool in_arr = false;
            for(uint8_t i = 0; i < MAX_HISTORY; i++) {
                if(app->history_count[i] > 0 && app->history[i] == nf) {
                    in_arr = true;
                    app->history_count[i]++;
                    if(i > 0) {
                        uint8_t tc = app->history_count[i];
                        for(uint8_t j = MAX_HISTORY-1; j > 0; j--) {
                            uint8_t off = (j >= i) ? 1 : 0;
                            app->history[j]       = app->history[j-off];
                            app->history_count[j] = app->history_count[j-off];
                        }
                        app->history[0]=nf; app->history_count[0]=tc;
                    }
                    break;
                }
            }
            if(!in_arr) {
                app->history[3]=app->history[2]; app->history[2]=app->history[1];
                app->history[1]=app->history[0]; app->history[0]=nf;
                app->history_count[3]=app->history_count[2];
                app->history_count[2]=app->history_count[1];
                app->history_count[1]=app->history_count[0];
                app->history_count[0]=1;
            }
            app->history_len = 0;
            for(uint8_t i = 0; i < MAX_HISTORY; i++)
                if(app->history_count[i] > 0) app->history_len = i + 1;
            app->show_frame = (app->history_len > 0);
        }
        app->locked = false;
    }

    app->current_rssi = rssi;
    app->preset_idx = ma_next_enabled_preset(app->preset_idx, app->preset_count);
    if(furi_hal_subghz_is_frequency_valid(app->scan_freq)) {
        furi_hal_subghz_idle();
        furi_hal_subghz_load_registers(
            subghz_setting_get_preset_data(app->setting, app->preset_idx));
        furi_hal_subghz_set_frequency_and_path(app->scan_freq);
        furi_hal_subghz_rx();
    }

    furi_mutex_release(app->mutex);
}

/* ── Draw helpers ─────────────────────────────────────────────────────── */

static void draw_feedback_icon(Canvas* canvas, uint8_t x, uint8_t y, FeedbackLevel level) {
    canvas_set_color(canvas, ColorBlack);
    if(level == FeedbackMute) {
        canvas_draw_circle(canvas, x+4, y+3, 3);
        canvas_draw_line(canvas, x+1, y+6, x+7, y+0);
    } else if(level == FeedbackVibro) {
        /* ~ wave */
        canvas_draw_dot(canvas, x+0, y+3);
        canvas_draw_line(canvas, x+1, y+2, x+2, y+1);
        canvas_draw_line(canvas, x+3, y+1, x+4, y+2);
        canvas_draw_line(canvas, x+4, y+2, x+5, y+3);
        canvas_draw_line(canvas, x+5, y+3, x+6, y+4);
        canvas_draw_line(canvas, x+6, y+4, x+7, y+5);
        canvas_draw_line(canvas, x+7, y+5, x+8, y+4);
        canvas_draw_dot(canvas, x+9, y+3);
    } else {
        canvas_draw_box(canvas, x+0, y+2, 3, 3);
        canvas_draw_line(canvas, x+3, y+1, x+5, y+0);
        canvas_draw_line(canvas, x+3, y+4, x+5, y+6);
        canvas_draw_line(canvas, x+5, y+0, x+5, y+6);
        canvas_draw_line(canvas, x+4, y+1, x+4, y+5);
        if(level == FeedbackAll) {
            canvas_draw_line(canvas, x+7, y+1, x+8, y+2);
            canvas_draw_line(canvas, x+8, y+2, x+8, y+4);
            canvas_draw_line(canvas, x+8, y+4, x+7, y+5);
        }
    }
}

static void draw_rssi(Canvas* c, float rssi, float rssi_last, float trigger, uint8_t x, uint8_t y) {
    if(!feq(rssi, 0.f)) {
        if(rssi > RSSI_MAX) rssi = RSSI_MAX;
        rssi = (rssi - RSSI_MIN) / RSSI_SCALE;
        uint8_t col = 0;
        for(size_t i = 0; i <= (uint8_t)rssi; i++) {
            if((i+1)%4) { col++; canvas_draw_box(c, x+2*i, (y+4)-col, 2, col); }
        }
    }
    if(!feq(rssi_last, 0.f)) {
        if(rssi_last > RSSI_MAX) rssi_last = RSSI_MAX;
        int mx = (int)((rssi_last-RSSI_MIN)/RSSI_SCALE)*2;
        int mh = (int)((rssi_last-RSSI_MIN)/RSSI_SCALE)+1;
        mh -= (mh/4)+3;
        canvas_draw_line(c, x+mx+1, y-mh, x+mx+1, y+3);
    }
    float tr = (trigger-RSSI_MIN)/RSSI_SCALE;
    uint8_t tx = (uint8_t)((float)x + 2*tr);
    canvas_draw_dot(c, tx, y+4);
    canvas_draw_line(c, tx-1, y+5, tx+1, y+5);
    canvas_draw_line(c, x, y+3, x+(uint8_t)((RSSI_MAX-RSSI_MIN)*2/RSSI_SCALE), y+3);
}

static void draw_freq_setup(Canvas* canvas, ModAnalApp* app) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignTop, "Set Frequency");

    /* Digit display: 3 digits, dot, 2 digits, " MHz"
     * FontBigNumbers digits are ~12px wide, ~18px tall.
     * Horizontal: 5 digits×13 + dot×5 + "MHz"×~22 ≈ 92px total.
     * Centred on 128px screen: start_x = (128-92)/2 ≈ 18.
     * Vertical: title ends ~y=18, Continue button occupies bottom ~10px.
     * Available centre: (18+54)/2 = 36, baseline = 36+18 = 54 — but that
     * crowds the button. Use y=44 which visually centres in the gap and
     * matches the layout seen in the screenshot (was too high at y=30). */
    /* Horizontal centering: total width = 5 digits×13 + dot×6 + "MHz"×~24 = 95px.
     * Centre on 128px screen: (128-95)/2 = 16. Use 17 for a clean pixel. */
    const uint8_t digit_w = 13;
    const uint8_t dot_w   = 6;
    const uint8_t start_x = 17;
    const uint8_t digit_y = 40; /* was 30, move down for better vertical centering */

    canvas_set_font(canvas, FontBigNumbers);
    uint8_t x = start_x;
    for(uint8_t i = 0; i < 5; i++) {
        if(i == 3) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, x, digit_y, ".");
            x += dot_w;
            canvas_set_font(canvas, FontBigNumbers);
        }
        char ch[2] = {'0' + app->freq_digits[i], 0};
        canvas_draw_str(canvas, x, digit_y, ch);
        if(i == app->cursor) {
            /* Cursor underline — 2px below the digit baseline */
            canvas_draw_box(canvas, x-1, digit_y+2, digit_w-1, 2);
        }
        x += digit_w;
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, x+2, digit_y-2, "MHz");

    elements_button_center(canvas, "Continue");
}

static void draw_scanning(Canvas* canvas, ModAnalApp* app) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    canvas_draw_str(canvas, 17, 7, "Modulation Analyzer");

    draw_feedback_icon(canvas, 117, 0, app->feedback);

    /* Current modulation display — split into text prefix (FontPrimary)
     * + numeric suffix (FontBigNumbers) because FontBigNumbers only renders
     * digits, not letters. Without this split, "AM650" would show as "650"
     * (only the digits). */
    uint8_t disp_idx = app->locked ? app->locked_preset : app->preset_idx;
    /* Preset name from SubGhzSetting — any modulation works automatically */
    const char* preset_display_name =
        subghz_setting_get_preset_name(app->setting, disp_idx);
    bool is_inverted = app->locked;
    if(is_inverted) {
        canvas_draw_box(canvas, 4, 10, 75, 19);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }
    /* Preset name display — uses SubGhzSetting name so new modulations
     * appear automatically without any code change. */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 8, 20, preset_display_name);
    canvas_set_color(canvas, ColorBlack);

    /* Fixed frequency display at top right */
    canvas_set_font(canvas, FontSecondary);
    char fstr[12];
    uint32_t f = app->scan_freq;
    snprintf(fstr, sizeof(fstr), "%lu.%02lu",
        (unsigned long)(f/1000000u),
        (unsigned long)((f%1000000u)/10000u));
    canvas_draw_str(canvas, 90, 16, fstr);
    canvas_draw_str(canvas, 90, 25, "MHz");

    /* History */
    const uint8_t x1=2, x2=66, y=37;
    uint8_t line = 0;
    bool show_frame = app->show_frame && app->history_len > 0;
    for(uint8_t i = 0; i < MAX_HISTORY; i++) {
        uint8_t cx = (i%2==0) ? x1 : x2;
        uint8_t cy = y + line*11;
        if(i%2 != 0) line++;
        uint8_t hidx = app->history[i];
        const char* name = (app->history_count[i]>0 && hidx < app->preset_count)
            ? subghz_setting_get_preset_name(app->setting, hidx) : "------";
        canvas_draw_str(canvas, cx, cy, name);
        if(app->history_count[i] > 1) {
            char rx[8]; snprintf(rx, sizeof(rx), "x%d", app->history_count[i]);
            canvas_draw_str(canvas, cx+41, cy, rx);
        }
        if(show_frame && i==app->selected_index)
            elements_frame(canvas, cx-2, cy-9, 63, 11);
    }

    canvas_draw_str(canvas, 33, 62, "RSSI");
    float rssi = app->locked ? app->locked_rssi : app->current_rssi;
    draw_rssi(canvas, rssi, app->rssi_last, app->trigger, 56, 57);
    elements_button_left(canvas, "T-");
    elements_button_right(canvas, "+T");
}

static void draw_cb(Canvas* canvas, void* ctx) {
    ModAnalApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(app->screen == ScreenFreqSetup) {
        draw_freq_setup(canvas, app);
    } else {
        draw_scanning(canvas, app);
    }
    furi_mutex_release(app->mutex);
}

static void input_cb(InputEvent* event, void* ctx) {
    furi_message_queue_put((FuriMessageQueue*)ctx, event, FuriWaitForever);
}

/* ── App lifecycle ──────────────────────────────────────────────────────── */

static ModAnalApp* app_alloc(void) {
    ModAnalApp* app = malloc(sizeof(ModAnalApp));
    memset(app, 0, sizeof(*app));
    app->mutex    = furi_mutex_alloc(FuriMutexTypeNormal);
    app->trigger  = RSSI_MIN;
    app->feedback = FeedbackMute;
    app->screen   = ScreenFreqSetup;
    load_freq(app);
    load_trigger(app);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->gui         = furi_record_open(RECORD_GUI);
    app->view_port   = view_port_alloc();
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    view_port_draw_callback_set(app->view_port, draw_cb, app);
    view_port_input_callback_set(app->view_port, input_cb, app->input_queue);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    view_port_update(app->view_port);

    app->dwell_timer = furi_timer_alloc(dwell_timer_cb, FuriTimerTypePeriodic, app);

    /* Load SubGhzSetting so preset names/data are available dynamically */
    app->setting = subghz_setting_alloc();
    subghz_setting_load(app->setting, EXT_PATH("subghz/assets/setting_user.file"));
    app->preset_count = (uint8_t)subghz_setting_get_preset_count(app->setting);
    ma_load_mod_filter();

    /* Start on first enabled modulation */
    while(g_mod_filter_loaded && g_mod_filter[app->preset_idx] == 0x00
          && app->preset_idx < app->preset_count - 1u)
        app->preset_idx++;

    return app;
}

static void start_scanning(ModAnalApp* app) {
    app->scan_freq  = digits_to_hz(app->freq_digits);
    app->preset_idx = 0;
    app->locked     = false;
    app->current_rssi = 0;
    furi_hal_subghz_idle();
        furi_hal_subghz_load_registers(
            subghz_setting_get_preset_data(app->setting, 0));
    furi_hal_subghz_set_frequency_and_path(app->scan_freq);
    furi_hal_subghz_rx();
    furi_timer_start(app->dwell_timer, DWELL_MS);
    app->screen = ScreenScanning;
}

static void stop_scanning(ModAnalApp* app) {
    furi_timer_stop(app->dwell_timer);
    furi_hal_subghz_idle();
    app->screen = ScreenFreqSetup;
}

static void app_free(ModAnalApp* app) {
    furi_timer_stop(app->dwell_timer);
    furi_timer_free(app->dwell_timer);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    subghz_setting_free(app->setting);
        furi_mutex_free(app->mutex);
    free(app);
}

int32_t subghz_modulation_analyzer_app(void* p) {
    char return_marker[16] = {0};
    if(p && ((const char*)p)[0]) strncpy(return_marker, (const char*)p, sizeof(return_marker)-1);

    furi_hal_subghz_reset();
    furi_hal_subghz_idle();

    ModAnalApp* app = app_alloc();

    InputEvent drain;
    while(furi_message_queue_get(app->input_queue, &drain, 0) == FuriStatusOk) {}
    furi_delay_ms(150);
    while(furi_message_queue_get(app->input_queue, &drain, 0) == FuriStatusOk) {}

    InputEvent event;
    while(!app->exit_requested) {
        if(furi_message_queue_get(app->input_queue, &event, REDRAW_MS) == FuriStatusOk) {

            /* ── Screen 1: Frequency Setup ── */
            if(app->screen == ScreenFreqSetup) {
                if(event.key == InputKeyBack && event.type == InputTypeShort) {
                    app->exit_requested = true;

                } else if(event.key == InputKeyOk && event.type == InputTypeShort) {
                    uint32_t candidate = digits_to_hz(app->freq_digits);
                    if(furi_hal_subghz_is_frequency_valid(candidate)) {
                        save_freq(app);
                        start_scanning(app);
                    }

                } else if((event.key == InputKeyLeft || event.key == InputKeyRight) &&
                           (event.type == InputTypeShort || event.type == InputTypeRepeat)) {
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    if(event.key == InputKeyLeft) {
                        if(app->cursor > 0) app->cursor--;
                    } else {
                        if(app->cursor < 4) app->cursor++;
                    }
                    furi_mutex_release(app->mutex);

                } else if((event.key == InputKeyUp || event.key == InputKeyDown) &&
                           (event.type == InputTypeShort || event.type == InputTypeRepeat)) {
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    uint8_t* d = &app->freq_digits[app->cursor];
                    if(event.key == InputKeyUp) {
                        *d = (*d + 1) % 10;
                    } else {
                        *d = (*d == 0) ? 9 : (*d - 1);
                    }
                    furi_mutex_release(app->mutex);
                }

            /* ── Screen 2: Scanning ── */
            } else {
                if(event.key == InputKeyBack && event.type == InputTypeShort) {
                    stop_scanning(app);

                } else if(event.key == InputKeyLeft &&
                          (event.type == InputTypePress || event.type == InputTypeRepeat)) {
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    app->trigger -= TRIGGER_STEP;
                    if(app->trigger < RSSI_MIN) app->trigger = RSSI_MIN;
                    furi_mutex_release(app->mutex);

                } else if(event.key == InputKeyRight &&
                          (event.type == InputTypePress || event.type == InputTypeRepeat)) {
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    app->trigger += TRIGGER_STEP;
                    if(app->trigger > RSSI_MAX) app->trigger = RSSI_MAX;
                    furi_mutex_release(app->mutex);

                } else if(event.key == InputKeyUp && event.type == InputTypePress) {
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    app->feedback = (FeedbackLevel)((app->feedback+1) % FeedbackCount);
                    furi_mutex_release(app->mutex);

                } else if(event.key == InputKeyDown &&
                          (event.type == InputTypePress || event.type == InputTypeRepeat)) {
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    if(app->history_len > 0) {
                        app->show_frame = true;
                        app->selected_index = (app->selected_index+1) % app->history_len;
                    }
                    furi_mutex_release(app->mutex);

                } else if(event.key == InputKeyOk &&
                          (event.type == InputTypeShort || event.type == InputTypeLong)) {
                    /* Write selected preset + frequency to last_settings,
                     * then return to SubGHz's Receiver already tuned. */
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    uint8_t chosen_preset = 0;
                    uint32_t chosen_freq  = app->scan_freq;
                    if(app->show_frame && !app->locked && app->history_len > 0) {
                        chosen_preset = app->history[app->selected_index];
                    } else if(app->locked) {
                        chosen_preset = app->locked_preset;
                    }
                    furi_mutex_release(app->mutex);
                    stop_scanning(app);
                    save_trigger(app);
                    write_result_to_settings(app, chosen_freq, chosen_preset);
                    /* Relaunch SubGHz directly into the Receiver */
                    if(return_marker[0]) {
                        /* Write "read" as the focus so subghz opens receiver */
                        Storage* s = furi_record_open(RECORD_STORAGE);
                        File* f = storage_file_alloc(s);
                        if(storage_file_open(f, "/ext/subghz/.focus_menu", FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                            storage_file_write(f, "readraw", 7);
                        }
                        storage_file_close(f);
                        storage_file_free(f);
                        furi_record_close(RECORD_STORAGE);
                    }
                    app->ok_result_selected = true;
                    app->exit_requested = true;
                }
            }
        }
        view_port_update(app->view_port);
    }

    /* Stop the dwell timer BEFORE sleeping the radio — otherwise it can
     * fire during the hourglass loop and call furi_hal_subghz_load_registers
     * on a sleeping radio, causing another furi_check crash. */
    furi_timer_stop(app->dwell_timer);
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
    save_trigger(app);

    /* Conditional return to SubGHz */
    if(return_marker[0]) {
        if(!app->ok_result_selected) {
            /* Back pressed (no result selected) — write menu-focus marker
             * so SubGHz returns to "Modulation Analyzer" in the Start menu. */
            Storage* s = furi_record_open(RECORD_STORAGE);
            File* f = storage_file_alloc(s);
            if(storage_file_open(f, "/ext/subghz/.focus_menu", FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                storage_file_write(f, return_marker, strlen(return_marker));
            }
            storage_file_close(f);
            storage_file_free(f);
            furi_record_close(RECORD_STORAGE);
        }
        /* OK result path already wrote "read" to .focus_menu — nothing
         * more needed here. */

        Loader* loader = furi_record_open(RECORD_LOADER);
        loader_enqueue_launch(loader, "subghz", NULL, LoaderDeferredLaunchFlagNone);
        furi_record_close(RECORD_LOADER);

        /* The loader's spinner is already showing — exit immediately. */
    }

    app_free(app);
    return 0;
}
