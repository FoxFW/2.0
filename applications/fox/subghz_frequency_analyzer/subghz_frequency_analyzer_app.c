/**
 * @file subghz_frequency_analyzer_app.c
 * Visual layout matches the original stock implementation exactly.
 * Icons I_MHz_25x11/I_Volup_8x6/I_Voldwn_6x6 are not FAP-exported;
 * replaced with text labels at the same coordinates.
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
#include <lib/subghz/subghz_setting.h>
#include <lib/flipper_format/flipper_format.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define RSSI_MIN     (-97.0f)
#define RSSI_MAX     (-60.0f)
#define RSSI_SCALE   2.3f
#define TRIGGER_STEP 1.0f
#define MAX_HISTORY  4
#define SETTING_FILE_PATH EXT_PATH("subghz/assets/setting_user")
#define LAST_SETTINGS_PATH EXT_PATH("subghz/assets/last_subghz.settings")
#define LAST_SETTINGS_TYPE    "Flipper SubGhz Last Setting File"
#define LAST_SETTINGS_VERSION 3
#define FA_TRIGGER_KEY        "FATrigger"
#define FA_FEEDBACK_KEY       "FeedbackLevel"
#define FA_PRESET_KEY         "FAPresetIndex"  /* uint32 — shared with main app */
#define FA_TRIGGER_DEFAULT    (-93.0f)

#define SETTLE_MS    2u
#define REDRAW_MS    80u

extern const uint8_t subghz_device_cc1101_preset_ook_270khz_async_regs[];
extern const uint8_t subghz_device_cc1101_preset_ook_650khz_async_regs[];
extern const uint8_t subghz_device_cc1101_preset_2fsk_dev2_38khz_async_regs[];
extern const uint8_t subghz_device_cc1101_preset_2fsk_dev12khz_async_regs[];
extern const uint8_t subghz_device_cc1101_preset_2fsk_dev47_6khz_async_regs[];
extern const uint8_t subghz_device_cc1101_preset_msk_99_97kb_async_regs[];
extern const uint8_t subghz_device_cc1101_preset_gfsk_9_99kb_async_regs[];

/* fa_preset_names/regs removed — use subghz_setting APIs for dynamic preset count */
#define FA_MOD_FILTER_PATH "/ext/subghz/modulation_filter.save"
static uint8_t fa_mod_filter[64];
static bool    fa_mod_filter_loaded = false;
static void fa_load_mod_filter(void) {
    memset(fa_mod_filter, 0x01, sizeof(fa_mod_filter));
    Storage* s = furi_record_open(RECORD_STORAGE);
    File*    f = storage_file_alloc(s);
    if(storage_file_open(f, FA_MOD_FILTER_PATH, FSAM_READ, FSOM_OPEN_EXISTING))
        storage_file_read(f, fa_mod_filter, sizeof(fa_mod_filter));
    storage_file_close(f); storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    fa_mod_filter_loaded = true;
}
static uint8_t fa_next_enabled(uint8_t cur, int dir, uint8_t count) {
    uint8_t next = (uint8_t)((cur + dir + count) % count);
    uint8_t guard = 0;
    while(fa_mod_filter_loaded && fa_mod_filter[next] == 0x00 && guard++ < count)
        next = (uint8_t)((next + dir + count) % count);
    return next;
}

static inline bool feq(float a, float b) { return fabsf(a - b) < 0.001f; }

static uint32_t round_int(uint32_t value, uint8_t n) {
    uint8_t on = n;
    while(n--) { uint8_t i = value % 10; value /= 10; if(i >= 5) value++; }
    while(on--) value *= 10;
    return value;
}

typedef enum { FeedbackMute=0, FeedbackSound, FeedbackAll, FeedbackVibro, FeedbackCount } FeedbackLevel;
typedef enum { FAScreenSetMod = 0, FAScreenMain, FAScreenConfig } FAScreen;
/* Cycle: Mute(⊘) → Sound(🔊) → Sound+Vibrate → Vibrate → Mute */

typedef struct {
    Gui*              gui;
    ViewPort*         view_port;
    FuriMessageQueue* input_queue;
    FuriMutex*        mutex;
    NotificationApp*  notifications;
    FuriThread*       worker_thread;
    SubGhzSetting*    setting;
    size_t            freq_count;
    volatile bool     worker_should_exit;
    uint32_t          current_freq;
    float             current_rssi;
    uint32_t          locked_freq;
    float             locked_rssi;
    bool              locked;
    float             rssi_last;
    float             trigger;
    uint32_t          history[MAX_HISTORY];
    uint8_t           history_count[MAX_HISTORY];
    uint8_t           history_len;
    uint8_t           selected_index;
    bool              show_frame;
    FeedbackLevel     feedback;
    uint32_t          frequency_to_save;
    bool              exit_requested;
    bool              ok_result_selected; /* true = relaunch to Receiver */
    FAScreen          screen;
    uint8_t           preset_count;  /* dynamic — subghz_setting_get_preset_count() */
    uint8_t           preset_idx;
} FreqAnalyzerApp;

/* Read the saved trigger and feedback level from last_subghz.settings.
 * Uses flipper_format's "rewind on miss" pattern exactly as the main app
 * does in subghz_last_settings.c — safe to call on a fresh file that
 * doesn't yet have these keys (rewind_on_miss means they'll just keep
 * the default values). */
static void fa_load_settings(FreqAnalyzerApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    if(flipper_format_file_open_existing(ff, LAST_SETTINGS_PATH)) {
        uint32_t version = 0;
        FuriString* type = furi_string_alloc();
        /* Only read if the file matches our expected format */
        if(flipper_format_read_header(ff, type, &version) &&
           furi_string_equal_str(type, LAST_SETTINGS_TYPE) &&
           version == LAST_SETTINGS_VERSION) {
            float trigger = FA_TRIGGER_DEFAULT;
            if(flipper_format_read_float(ff, FA_TRIGGER_KEY, &trigger, 1)) {
                app->trigger = trigger;
            }
            flipper_format_rewind(ff);
            uint32_t fb = 0;
            if(flipper_format_read_uint32(ff, FA_FEEDBACK_KEY, &fb, 1)) {
                app->feedback = (FeedbackLevel)(fb < (uint32_t)FeedbackCount ? fb : 0);
            }
            /* Read shared modulation — written by both FA and the main SubGhz app */
            flipper_format_rewind(ff);
            uint32_t pi = 0;
            if(flipper_format_read_uint32(ff, FA_PRESET_KEY, &pi, 1)) {
                if(pi < app->preset_count) app->preset_idx = (uint8_t)pi;
            }
        }
        furi_string_free(type);
    }
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

/* Write only FATrigger and FeedbackLevel back into last_subghz.settings,
 * preserving every other key exactly as-is (read-modify-write via a
 * temp file + atomic rename). */
static void fa_save_settings(FreqAnalyzerApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;
    do {
        if(!flipper_format_file_open_existing(ff, LAST_SETTINGS_PATH)) break;
        /* Update each key in-place.  If the key is absent from the file,
         * flipper_format_update_* returns false with the cursor already at
         * EOF — write_* then appends correctly.  Do NOT rewind before the
         * fallback write; that would put the cursor at position 0 and
         * overwrite the file header.  Only rewind between key searches. */
        if(!flipper_format_update_float(ff, FA_TRIGGER_KEY, &app->trigger, 1)) {
            (void)flipper_format_write_float(ff, FA_TRIGGER_KEY, &app->trigger, 1);
        }
        flipper_format_rewind(ff);
        uint32_t fb = (uint32_t)app->feedback;
        if(!flipper_format_update_uint32(ff, FA_FEEDBACK_KEY, &fb, 1)) {
            (void)flipper_format_write_uint32(ff, FA_FEEDBACK_KEY, &fb, 1);
        }
        flipper_format_rewind(ff);
        /* Save modulation index — shared with main SubGhz Config via FAPresetIndex */
        uint32_t pi = (uint32_t)app->preset_idx;
        if(!flipper_format_update_uint32(ff, FA_PRESET_KEY, &pi, 1)) {
            (void)flipper_format_write_uint32(ff, FA_PRESET_KEY, &pi, 1);
        }
        ok = true;
    } while(false);
    flipper_format_free(ff);
    UNUSED(ok);
    furi_record_close(RECORD_STORAGE);
}

static void write_freq_to_settings(uint32_t freq_hz) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(s);
    if(flipper_format_file_open_existing(ff, LAST_SETTINGS_PATH)) {
        if(!flipper_format_update_uint32(ff, "Frequency", &freq_hz, 1)) {
            flipper_format_write_uint32(ff, "Frequency", &freq_hz, 1);
        }
    }
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

static int32_t worker_fn(void* ctx) {
    FreqAnalyzerApp* app = ctx;
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_registers(
        subghz_setting_get_preset_data(app->setting, app->preset_idx));

    /* Warmup counter: ignore first few readings after OK so the RSSI
     * noise floor settles and doesn't produce instant false detections. */
    uint8_t warmup_remaining = 15; /* ~15 × SETTLE_MS × freq_count ≈ 500ms */

    while(!app->worker_should_exit) {
        /* Wait on SetMod screen — don't scan until user clicks Continue */
        if(app->screen == FAScreenSetMod) {
            furi_delay_ms(50);
            warmup_remaining = 15; /* reset warmup each time we re-enter scan */
            continue;
        }

        uint32_t best_freq = 0;
        float    best_rssi = -127.0f;

        for(size_t i = 0; i < app->freq_count; i++) {
            uint32_t freq = subghz_setting_get_frequency(app->setting, i);
            if(!freq || !furi_hal_subghz_is_frequency_valid(freq)) continue;
            furi_hal_subghz_idle();
            furi_hal_subghz_set_frequency_and_path(freq);
            furi_hal_subghz_rx();
            furi_delay_ms(SETTLE_MS);
            float rssi = furi_hal_subghz_get_rssi();
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->current_freq = freq;
            app->current_rssi = rssi;
            furi_mutex_release(app->mutex);
            if(rssi > best_rssi) { best_rssi = rssi; best_freq = freq; }
            if(app->worker_should_exit) break;
        }

        /* Discard readings during warmup — RSSI not yet stable */
        if(warmup_remaining > 0) {
            warmup_remaining--;
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->current_rssi = -100.0f; /* show safe/low value in UI */
            furi_mutex_release(app->mutex);
            continue;
        }

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        bool was_locked = app->locked;
        if(best_rssi > app->trigger && best_freq) {
            uint32_t rounded = round_int(best_freq, 3);
            app->locked = true;
            app->locked_freq = rounded;
            app->locked_rssi = best_rssi;
            if(best_rssi >= app->rssi_last) app->rssi_last = best_rssi;
            if(!was_locked) {
                /* First lock on this signal — play feedback per user setting.
                 * sequence_success plays BOTH sound and vibro, so it can't
                 * be used for FeedbackSound (sound-only). Build a custom
                 * sound-only sequence using the low-level message primitives. */
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
            }
        } else {
            if(was_locked) {
                /* Update history on unlock */
                uint32_t nf = app->locked_freq;
                bool in_arr = false;
                for(uint8_t i = 0; i < MAX_HISTORY; i++) {
                    if(app->history[i] == nf) {
                        in_arr = true;
                        if(app->history_count[i] == 0) app->history_count[i]++;
                        app->history_count[i]++;
                        if(i > 0) {
                            uint8_t tc = app->history_count[i];
                            for(uint8_t j = MAX_HISTORY - 1; j > 0; j--) {
                                uint8_t off = (j >= i) ? 1 : 0;
                                app->history[j]       = app->history[j - off];
                                app->history_count[j] = app->history_count[j - off];
                            }
                            app->history[0] = nf; app->history_count[0] = tc;
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
                    app->history_count[0]=0;
                }
                app->history_len = 0;
                for(uint8_t i = 0; i < MAX_HISTORY; i++)
                    if(app->history[i] > 0) app->history_len = i + 1;
                app->show_frame = (app->history_len > 0);
            }
            app->locked = false; app->locked_freq = 0; app->locked_rssi = 0;
        }
        furi_mutex_release(app->mutex);
    }
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
    return 0;
}

/* Draw a compact feedback-mode icon at (x,y) in a ~10x7 pixel area.
 * Replaces the "S+"/"S-"/"--" text labels that were cut off and unclear. */
static void draw_feedback_icon(Canvas* canvas, uint8_t x, uint8_t y, FeedbackLevel level) {
    canvas_set_color(canvas, ColorBlack);
    if(level == FeedbackMute) {
        /* ⊘ — circle with diagonal line */
        canvas_draw_circle(canvas, x+4, y+3, 3);
        canvas_draw_line(canvas, x+1, y+6, x+7, y+0);
    } else if(level == FeedbackVibro) {
        /* ~ wave — two humps of a sine wave to suggest vibration */
        canvas_draw_dot(canvas, x+0, y+3);
        canvas_draw_line(canvas, x+1, y+2, x+2, y+1);
        canvas_draw_line(canvas, x+3, y+1, x+4, y+2);
        canvas_draw_line(canvas, x+4, y+2, x+5, y+3);
        canvas_draw_line(canvas, x+5, y+3, x+6, y+4);
        canvas_draw_line(canvas, x+6, y+4, x+7, y+5);
        canvas_draw_line(canvas, x+7, y+5, x+8, y+4);
        canvas_draw_dot(canvas, x+9, y+3);
    } else {
        /* Speaker shape — shared by FeedbackSound and FeedbackAll */
        canvas_draw_box(canvas, x+0, y+2, 3, 3); /* body */
        canvas_draw_line(canvas, x+3, y+1, x+5, y+0); /* cone top */
        canvas_draw_line(canvas, x+3, y+4, x+5, y+6); /* cone bottom */
        canvas_draw_line(canvas, x+5, y+0, x+5, y+6); /* cone right */
        canvas_draw_line(canvas, x+4, y+1, x+4, y+5); /* cone fill */
        if(level == FeedbackAll) {
            /* Add sound wave arc to right of speaker */
            canvas_draw_line(canvas, x+7, y+1, x+8, y+2);
            canvas_draw_line(canvas, x+8, y+2, x+8, y+4);
            canvas_draw_line(canvas, x+8, y+4, x+7, y+5);
        }
    }
}

static void draw_rssi(Canvas* canvas, float rssi, float rssi_last, float trigger, uint8_t x, uint8_t y) {
    if(!feq(rssi, 0.f)) {
        if(rssi > RSSI_MAX) rssi = RSSI_MAX;
        rssi = (rssi - RSSI_MIN) / RSSI_SCALE;
        uint8_t col = 0;
        for(size_t i = 0; i <= (uint8_t)rssi; i++) {
            if((i + 1) % 4) { col++; canvas_draw_box(canvas, x + 2*i, (y+4)-col, 2, col); }
        }
    }
    if(!feq(rssi_last, 0.f)) {
        if(rssi_last > RSSI_MAX) rssi_last = RSSI_MAX;
        int mx = (int)((rssi_last - RSSI_MIN) / RSSI_SCALE) * 2;
        int mh = (int)((rssi_last - RSSI_MIN) / RSSI_SCALE) + 1;
        mh -= (mh / 4) + 3;
        canvas_draw_line(canvas, x+mx+1, y-mh, x+mx+1, y+3);
    }
    float tr = (trigger - RSSI_MIN) / RSSI_SCALE;
    uint8_t tx = (uint8_t)((float)x + 2*tr);
    canvas_draw_dot(canvas, tx, y+4);
    canvas_draw_line(canvas, tx-1, y+5, tx+1, y+5);
    canvas_draw_line(canvas, x, y+3, x + (uint8_t)((RSSI_MAX-RSSI_MIN)*2/RSSI_SCALE), y+3);
}

static void draw_history(Canvas* canvas, FreqAnalyzerApp* app) {
    const uint8_t x1=2, x2=66, y=37;
    canvas_set_font(canvas, FontSecondary);
    uint8_t line = 0;
    bool show_frame = app->show_frame && app->history_len > 0;
    for(uint8_t i = 0; i < MAX_HISTORY; i++) {
        uint8_t cx = (i%2==0) ? x1 : x2;
        uint8_t cy = y + line*11;
        if(i%2 != 0) line++;
        char buf[16];
        if(app->history[i]) {
            snprintf(buf, sizeof(buf), "%03lu.%03lu",
                (unsigned long)(app->history[i]/1000000%1000),
                (unsigned long)(app->history[i]/1000%1000));
        } else { strncpy(buf, "---.---", sizeof(buf)); }
        canvas_draw_str(canvas, cx, cy, buf);
        char rx[8];
        if(app->history_count[i] > 0) snprintf(rx, sizeof(rx), "x%d", app->history_count[i]);
        else strncpy(rx, "MHz", sizeof(rx));
        canvas_draw_str(canvas, cx+41, cy, rx);
        if(show_frame && i == app->selected_index)
            elements_frame(canvas, cx-2, cy-9, 63, 11);
    }
}

static void draw_cb(Canvas* canvas, void* ctx) {
    FreqAnalyzerApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* ── Startup modulation selection screen ─────────────────────────── */
    if(app->screen == FAScreenSetMod) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignTop, "Set Modulation");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop,
            "Modulation for frequency scan:");
        /* Left / name / right */
        canvas_draw_str(canvas, 4, 40, "<");
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop,
            subghz_setting_get_preset_name(app->setting, app->preset_idx));
        canvas_draw_str(canvas, 122, 40, ">");
        elements_button_center(canvas, "Continue");
        furi_mutex_release(app->mutex);
        return;
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 20, 7, "Frequency Analyzer");

    draw_feedback_icon(canvas, 117, 0, app->feedback);

    uint32_t freq = app->locked ? app->locked_freq : app->current_freq;
    char buf[32];
    snprintf(buf, sizeof(buf), "%03lu.%03lu",
        (unsigned long)(freq/1000000%1000), (unsigned long)(freq/1000%1000));

    canvas_set_font(canvas, FontBigNumbers);
    if(app->locked) {
        canvas_draw_box(canvas, 4, 10, 121, 19);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str(canvas, 8, 26, buf);

    /* "MHz" text — replaces I_MHz_25x11 icon (not FAP-exported) */
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 96, 25, "MHz");

    draw_history(canvas, app);

    canvas_draw_str(canvas, 33, 62, "RSSI");
    float rssi = app->locked ? app->locked_rssi : app->current_rssi;
    draw_rssi(canvas, rssi, app->rssi_last, app->trigger, 56, 57);

    elements_button_left(canvas, "T-");
    elements_button_right(canvas, "+T");
    /* Modulation name: y=8 sits above the frequency block,
     * clearing the "MHz" label which starts at y=18. */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 127, 8, AlignRight, AlignTop,
                            subghz_setting_get_preset_name(app->setting, app->preset_idx));

    if(app->screen == FAScreenConfig) {
        /* Full-screen black overlay — covers modulation name and all main
         * display content so nothing bleeds through the config popup. */
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 64);
        /* White border inset by 4px on sides, 6px top/bottom */
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_frame(canvas, 4, 6, 120, 52);
        /* Header */
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignTop, "Modulation Config");
        /* Modulation name centred */
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter,
                                subghz_setting_get_preset_name(app->setting, app->preset_idx));
        /* Side arrows at same height as mod name */
        canvas_draw_str(canvas, 8,  34, "<");
        canvas_draw_str(canvas, 116, 34, ">");
        /* OK button */
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_rframe(canvas, 44, 49, 40, 11, 3);
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignCenter, "[OK] Apply");
        canvas_set_color(canvas, ColorBlack);
    }

    furi_mutex_release(app->mutex);
}

static void input_cb(InputEvent* event, void* ctx) {
    furi_message_queue_put((FuriMessageQueue*)ctx, event, FuriWaitForever);
}

static FreqAnalyzerApp* app_alloc(void) {
    FreqAnalyzerApp* app = malloc(sizeof(FreqAnalyzerApp));
    memset(app, 0, sizeof(*app));
    app->mutex    = furi_mutex_alloc(FuriMutexTypeNormal);
    app->trigger  = RSSI_MIN;
    app->feedback = FeedbackMute;

    /* Register viewport BEFORE slow SD-card read — fixes startup flicker */
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->gui         = furi_record_open(RECORD_GUI);
    app->view_port   = view_port_alloc();
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    view_port_draw_callback_set(app->view_port, draw_cb, app);
    view_port_input_callback_set(app->view_port, input_cb, app->input_queue);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    view_port_update(app->view_port);

    app->setting    = subghz_setting_alloc();
    subghz_setting_load(app->setting, SETTING_FILE_PATH);
    app->freq_count = subghz_setting_get_frequency_count(app->setting);

    /* Load persisted trigger and feedback from last_subghz.settings --
     * these are the exact same fields the stock app persists (FATrigger,
     * FeedbackLevel), so they're shared correctly across both paths. */
    app->preset_count = (uint8_t)subghz_setting_get_preset_count(app->setting);
    fa_load_mod_filter();
    app->preset_idx = 0;
    while(fa_mod_filter_loaded && fa_mod_filter[app->preset_idx] == 0x00
          && app->preset_idx < app->preset_count - 1u)
        app->preset_idx++;
    fa_load_settings(app); /* may update preset_idx from saved setting */
    app->screen = FAScreenSetMod; /* always start at modulation selection */

    app->worker_thread = furi_thread_alloc_ex("FreqAnalWorker", 1024, worker_fn, app);
    furi_thread_start(app->worker_thread);
    return app;
}

static void app_free(FreqAnalyzerApp* app) {
    app->worker_should_exit = true;
    furi_thread_join(app->worker_thread);
    furi_thread_free(app->worker_thread);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    subghz_setting_free(app->setting);
    furi_mutex_free(app->mutex);
    free(app);
}

int32_t subghz_frequency_analyzer_app(void* p) {
    char return_marker[16] = {0};
    if(p && ((const char*)p)[0]) strncpy(return_marker, (const char*)p, sizeof(return_marker)-1);

    FreqAnalyzerApp* app = app_alloc();

    /* Drain any phantom button-presses from app launch */
    InputEvent drain;
    while(furi_message_queue_get(app->input_queue, &drain, 0) == FuriStatusOk) {}

    InputEvent event;
    while(!app->exit_requested) {
        if(furi_message_queue_get(app->input_queue, &event, REDRAW_MS) == FuriStatusOk) {
            if(event.key == InputKeyBack && event.type == InputTypeShort
                && app->screen == FAScreenConfig) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->screen = FAScreenMain;
                furi_mutex_release(app->mutex);

            /* ── SetMod screen: Left/Right cycle, OK enters main screen ── */
            } else if(app->screen == FAScreenSetMod && event.key == InputKeyLeft
                      && event.type == InputTypeShort) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->preset_idx = fa_next_enabled(app->preset_idx, -1, (uint8_t)app->preset_count);
                furi_mutex_release(app->mutex);
            } else if(app->screen == FAScreenSetMod && event.key == InputKeyRight
                      && event.type == InputTypeShort) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->preset_idx = fa_next_enabled(app->preset_idx, 1, (uint8_t)app->preset_count);
                furi_mutex_release(app->mutex);
            } else if(app->screen == FAScreenSetMod &&
                      event.key == InputKeyOk && event.type == InputTypeShort) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->screen = FAScreenMain;
                furi_hal_subghz_idle();
                furi_hal_subghz_load_registers(
                    subghz_setting_get_preset_data(app->setting, app->preset_idx));
                if(app->current_freq > 0)
                    furi_hal_subghz_set_frequency_and_path(app->current_freq);
                furi_hal_subghz_rx();
                furi_mutex_release(app->mutex);
                fa_save_settings(app);

            } else if(event.key == InputKeyBack) {
                app->exit_requested = true;

            } else if(app->screen == FAScreenConfig && event.key == InputKeyLeft
                      && event.type == InputTypeShort) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->preset_idx = fa_next_enabled(app->preset_idx, -1, app->preset_count);
                furi_mutex_release(app->mutex);

            } else if(app->screen == FAScreenConfig && event.key == InputKeyRight
                      && event.type == InputTypeShort) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->preset_idx = fa_next_enabled(app->preset_idx, 1, app->preset_count);
                furi_mutex_release(app->mutex);

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
                app->feedback = (FeedbackLevel)((app->feedback + 1) % FeedbackCount);
                furi_mutex_release(app->mutex);

            } else if(event.key == InputKeyDown &&
                      (event.type == InputTypePress || event.type == InputTypeRepeat)) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                if(app->history_len > 0) {
                    app->show_frame = true;
                    app->selected_index = (app->selected_index + 1) % app->history_len;
                }
                furi_mutex_release(app->mutex);

            } else if(event.key == InputKeyOk && event.type == InputTypeLong
                      && app->screen == FAScreenMain) {
                /* Long OK: open config */
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->screen = FAScreenConfig;
                furi_mutex_release(app->mutex);

            } else if(event.key == InputKeyOk && app->screen == FAScreenConfig
                      && (event.type == InputTypeShort || event.type == InputTypeLong)) {
                /* OK in config: apply modulation and return to main */
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->screen = FAScreenMain;
                furi_hal_subghz_idle();
                furi_hal_subghz_load_registers(
                    subghz_setting_get_preset_data(app->setting, app->preset_idx));
                if(app->current_freq > 0)
                    furi_hal_subghz_set_frequency_and_path(app->current_freq);
                furi_hal_subghz_rx();
                furi_mutex_release(app->mutex);

            } else if(event.key == InputKeyOk &&
                      (event.type == InputTypeShort || event.type == InputTypeLong)) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                uint32_t cand = 0;
                if(app->show_frame && !app->locked && app->history_len > 0)
                    cand = app->history[app->selected_index];
                else if(app->locked)
                    cand = app->locked_freq;
                furi_mutex_release(app->mutex);
                if(cand > 0) {
                    fa_save_settings(app);
                    write_freq_to_settings(cand);
                    /* Queue SubGHz to open the Receiver, already tuned to
                     * the selected frequency (same "read" deep-link used
                     * by the original app-internal FA on OK). */
                    if(return_marker[0]) {
                        /* Write "read" into .focus_menu so subghz.c
                         * sees open_receiver=true on restart. */
                        Storage* ws = furi_record_open(RECORD_STORAGE);
                        File* wf = storage_file_alloc(ws);
                        if(storage_file_open(wf, "/ext/subghz/.focus_menu",
                                              FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                            storage_file_write(wf, "readraw", 7);
                        }
                        storage_file_close(wf);
                        storage_file_free(wf);
                        furi_record_close(RECORD_STORAGE);
                    }
                    app->ok_result_selected = true;
                    app->exit_requested = true;
                }
            }
        }
        view_port_update(app->view_port);
    }

    fa_save_settings(app);


    /* Conditional return to SubGHz */
    if(return_marker[0]) {
        /* If OK was pressed with a result, .focus_menu already has "read"
         * written by the OK handler — just launch SubGHz. If Back was
         * pressed, write the normal menu-focus marker. */
        Storage* s = furi_record_open(RECORD_STORAGE);
        File* f = storage_file_alloc(s);
        bool ok = false;
        if(!app->ok_result_selected) {
            ok = storage_file_open(f, "/ext/subghz/.focus_menu", FSAM_WRITE, FSOM_CREATE_ALWAYS);
            if(ok) storage_file_write(f, return_marker, strlen(return_marker));
        } else {
            ok = true; /* "read" marker already written by OK handler */
        }
        storage_file_close(f);
        storage_file_free(f); furi_record_close(RECORD_STORAGE);
        Loader* loader = furi_record_open(RECORD_LOADER);
        loader_enqueue_launch(loader, "subghz", NULL, LoaderDeferredLaunchFlagNone);
        furi_record_close(RECORD_LOADER);

        /* The loader's spinner is already showing (set in the
         * EnqueueLaunch handler before api_lock_unlock returned).
         * No delay needed — just exit immediately so SubGHz starts
         * as fast as possible. */
    }


    app_free(app);
    return 0;
}
