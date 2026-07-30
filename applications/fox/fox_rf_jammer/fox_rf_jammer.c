#include "fox_rf_jammer.h"
#include "helpers/radio_device_loader.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_region.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/icon.h>
#include <loader/loader.h>
#include <math.h>
#include <stdlib.h>
#include <storage/storage.h>
#include <string.h>
#include <subghz/devices/devices.h>
#include <lib/subghz/subghz_tx_rx_worker.h>
#include "fox_rf_jammer_icons.h"

#define TAG            "FoxRFJammer"
#define FREQ_MIN       300000000UL
#define FREQ_MAX       928000000UL
#define TX_BUFFER_SIZE 1024

#define FOX_SPLASH_TICK_MS    40
#define FOX_SPLASH_HOLD_MS    2000
#define FOX_SPLASH_FADE_MS    666
#define FOX_SPLASH_GRID_SIDE  16
#define FOX_SPLASH_GRID_TOTAL (FOX_SPLASH_GRID_SIDE * FOX_SPLASH_GRID_SIDE)

static FuriHalRegion s_unlocked_region = {
    .country_code = "FTW",
    .bands_count  = 3,
    .bands = {
        {.start = 299999755, .end = 348000000, .power_limit = 20, .duty_cycle = 50},
        {.start = 386999938, .end = 464000000, .power_limit = 20, .duty_cycle = 50},
        {.start = 778999847, .end = 928000000, .power_limit = 20, .duty_cycle = 50},
    },
};

typedef struct { uint32_t min; uint32_t max; } FreqBand;
static const FreqBand s_bands[] = {
    {300000000UL, 348000000UL},
    {387000000UL, 464000000UL},
    {779000000UL, 928000000UL},
};
#define NUM_BANDS (sizeof(s_bands) / sizeof(s_bands[0]))

static const int DIGIT_TO_CHAR[CURSOR_FREQ_DIGITS] = {0, 1, 2, 4, 5, 6};

static void     fox_splash_draw_cb(Canvas* canvas, void* ctx);
static void     fox_main_draw_cb(Canvas* canvas, void* ctx);
static void     fox_input_cb(InputEvent* ev, void* ctx);
static int32_t  fox_tx_thread(void* ctx);
static bool     fox_freq_valid(uint32_t f);
static uint32_t fox_freq_clamp(uint32_t f, bool up);
static void     fox_stop_tx(FoxRFJammer* app);
static void     fox_start_tx(FoxRFJammer* app);
static void     fox_apply_mod_preset(FoxRFJammer* app);

static bool fox_freq_valid(uint32_t f) {
    for(size_t i = 0; i < NUM_BANDS; i++)
        if(f >= s_bands[i].min && f <= s_bands[i].max) return true;
    return false;
}
static uint32_t fox_freq_clamp(uint32_t f, bool up) {
    if(fox_freq_valid(f)) return f;
    if(up) {
        for(size_t i = 0; i < NUM_BANDS; i++)
            if(f < s_bands[i].min) return s_bands[i].min;
        return s_bands[0].min;
    } else {
        for(int i = (int)NUM_BANDS - 1; i >= 0; i--)
            if(f > s_bands[i].max) return s_bands[i].max;
        return s_bands[NUM_BANDS - 1].max;
    }
}

static int32_t fox_tx_thread(void* ctx) {
    FoxRFJammer* app = ctx;
    uint8_t data[TX_BUFFER_SIZE];

    switch(app->method_idx) {
    case 0:
        memset(data, 0xFF, sizeof(data));
        break;
    case 1:
        for(size_t i = 0; i < sizeof(data); i++) data[i] = (i & 1) ? 0x55 : 0xAA;
        break;
    case 2:
        for(size_t i = 0; i < sizeof(data); i++)
            data[i] = (uint8_t)(127.0f * sinf(2.0f * (float)M_PI * (float)i / (float)sizeof(data)) + 128.0f);
        break;
    case 3:
        for(size_t i = 0; i < sizeof(data); i++) data[i] = (i & 1) ? 0xFF : 0x00;
        break;
    case 4:
        for(size_t i = 0; i < sizeof(data); i++) data[i] = (uint8_t)(255u * i / sizeof(data));
        break;
    case 5:
        for(size_t i = 0; i < sizeof(data); i++) data[i] = (uint8_t)(rand() % 256);
        break;
    case 6: {
        size_t half = sizeof(data) / 2;
        for(size_t i = 0; i < sizeof(data); i++)
            data[i] = (i < half) ? (uint8_t)(i * 255u / half)
                                  : (uint8_t)(255u - (i - half) * 255u / half);
        break;
    }
    case 7:
        for(size_t i = 0; i < sizeof(data); i++)
            data[i] = (uint8_t)(127.0f * sinf(2.0f * (float)M_PI * (float)i *
                                              (1.0f + (float)i / (float)sizeof(data))));
        break;
    case 8:
        for(size_t i = 0; i < sizeof(data); i++) {
            float u1 = ((float)(rand() + 1)) / ((float)RAND_MAX + 2.0f);
            float u2 = ((float)(rand() + 1)) / ((float)RAND_MAX + 2.0f);
            float g  = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
            data[i]  = (uint8_t)(127.0f * g + 128.0f);
        }
        break;
    case 9:
        for(size_t i = 0; i < sizeof(data); i++) data[i] = (i % 10 == 0) ? 0xFF : 0x00;
        break;
    case 10:
        furi_hal_random_fill_buf(data, sizeof(data));
        break;
    default:
        memset(data, 0xFF, sizeof(data));
        break;
    }

    while(app->tx_running && app->subghz_txrx) {
        if(!subghz_tx_rx_worker_write(app->subghz_txrx, data, sizeof(data)))
            furi_delay_ms(20);
        furi_delay_ms(10);
    }
    return 0;
}

static void fox_apply_mod_preset(FoxRFJammer* app) {
    if(!app->device) return;
    subghz_devices_begin(app->device);
    subghz_devices_reset(app->device);
    subghz_devices_idle(app->device);
    switch(app->mod_idx) {
    case 1:  subghz_devices_load_preset(app->device, FuriHalSubGhzPreset2FSKDev238Async, NULL); break;
    case 2:  subghz_devices_load_preset(app->device, FuriHalSubGhzPreset2FSKDev476Async, NULL); break;
    case 3:  subghz_devices_load_preset(app->device, FuriHalSubGhzPresetMSK99_97KbAsync, NULL); break;
    case 4:  subghz_devices_load_preset(app->device, FuriHalSubGhzPresetGFSK9_99KbAsync, NULL); break;
    default: subghz_devices_load_preset(app->device, FuriHalSubGhzPresetOok650Async, NULL); break;
    }
}

static void fox_stop_tx(FoxRFJammer* app) {
    if(!app->tx_running) return;
    app->tx_running = false;
    if(app->tx_thread) {
        furi_thread_join(app->tx_thread);
        furi_thread_free(app->tx_thread);
        app->tx_thread = NULL;
    }
    if(app->subghz_txrx && subghz_tx_rx_worker_is_running(app->subghz_txrx))
        subghz_tx_rx_worker_stop(app->subghz_txrx);
}

static void fox_start_tx(FoxRFJammer* app) {
    if(!app->device || !app->subghz_txrx) return;
    fox_apply_mod_preset(app);
    if(!subghz_tx_rx_worker_start(app->subghz_txrx, app->device, app->frequency)) return;
    app->tx_running = true;
    app->tx_thread  = furi_thread_alloc();
    furi_thread_set_name(app->tx_thread, "Fox Jammer TX");
    furi_thread_set_stack_size(app->tx_thread, 2048);
    furi_thread_set_context(app->tx_thread, app);
    furi_thread_set_callback(app->tx_thread, fox_tx_thread);
    furi_thread_start(app->tx_thread);
}

static uint8_t s_splash_block_order[FOX_SPLASH_GRID_TOTAL];
static uint32_t s_splash_elapsed_ms;

static void fox_splash_shuffle_blocks(void) {
    for(uint32_t i = 0; i < FOX_SPLASH_GRID_TOTAL; i++) {
        s_splash_block_order[i] = (uint8_t)i;
    }
    uint32_t seed = (uint32_t)furi_get_tick() | 1;
    for(uint32_t i = FOX_SPLASH_GRID_TOTAL - 1; i > 0; i--) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        uint32_t j = seed % (i + 1);
        uint8_t tmp = s_splash_block_order[i];
        s_splash_block_order[i] = s_splash_block_order[j];
        s_splash_block_order[j] = tmp;
    }
}

static void fox_splash_draw_cb(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);

    uint8_t icon_w = icon_get_width(&I_fox_64x64);
    uint8_t icon_h = icon_get_height(&I_fox_64x64);
    int32_t x = (128 - (int32_t)icon_w) / 2;
    int32_t y = (64 - (int32_t)icon_h) / 2;
    canvas_draw_icon(canvas, x, y, &I_fox_64x64);

    if(s_splash_elapsed_ms <= FOX_SPLASH_HOLD_MS) return;

    uint32_t fade_elapsed = s_splash_elapsed_ms - FOX_SPLASH_HOLD_MS;
    uint32_t revealed =
        (uint32_t)(((uint64_t)fade_elapsed * FOX_SPLASH_GRID_TOTAL) / FOX_SPLASH_FADE_MS);
    if(revealed > FOX_SPLASH_GRID_TOTAL) revealed = FOX_SPLASH_GRID_TOTAL;

    uint8_t block_px = (uint8_t)(icon_w / FOX_SPLASH_GRID_SIDE);
    if(block_px == 0) block_px = 1;

    canvas_set_color(canvas, ColorWhite);
    for(uint32_t i = 0; i < revealed; i++) {
        uint8_t block = s_splash_block_order[i];
        uint8_t bx = block % FOX_SPLASH_GRID_SIDE;
        uint8_t by = block / FOX_SPLASH_GRID_SIDE;
        canvas_draw_box(canvas, x + bx * block_px, y + by * block_px, block_px, block_px);
    }
    canvas_set_color(canvas, ColorBlack);
}

static void fox_main_draw_cb(Canvas* canvas, void* ctx) {
    FoxRFJammer* app = (FoxRFJammer*)ctx;
    canvas_clear(canvas);

    bool tx_on   = app->tx_active && app->tx_running;
    bool in_mod  = (!tx_on && app->cursor_position == CURSOR_MODULATION);
    bool in_atk  = (!tx_on && app->cursor_position == CURSOR_METHOD);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 2, 0, AlignLeft, AlignTop, "Freq:");
    canvas_draw_str_aligned(canvas, 3, 0, AlignLeft, AlignTop, "Freq:");

    char freq_str[10];
    snprintf(freq_str, sizeof(freq_str), "%03lu.%03lu",
             (unsigned long)(app->frequency / 1000000UL),
             (unsigned long)((app->frequency % 1000000UL) / 1000UL));

    const int CHAR_W  = 11;
    const int FREQ_Y  = 10;
    const int ULINE_Y = FREQ_Y + 11;
    const int sx      = (128 - 7 * CHAR_W) / 2;

    canvas_set_font(canvas, FontPrimary);
    for(int i = 0; i < 7; i++) {
        char ch[2] = {freq_str[i], '\0'};
        canvas_draw_str_aligned(canvas, sx + i * CHAR_W + CHAR_W / 2, FREQ_Y,
                                AlignCenter, AlignTop, ch);
    }
    if(!tx_on && app->cursor_position < CURSOR_FREQ_DIGITS) {
        int ci = DIGIT_TO_CHAR[app->cursor_position];
        int ux = sx + ci * CHAR_W;
        canvas_draw_line(canvas, ux + 2, ULINE_Y, ux + CHAR_W - 2, ULINE_Y);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, sx + 7 * CHAR_W + 3, FREQ_Y + 2,
                            AlignLeft, AlignTop, "MHz");

    canvas_set_font(canvas, FontSecondary);
    if(in_mod) {
        canvas_draw_box(canvas, 0, 23, 128, 11);
        canvas_set_color(canvas, ColorWhite);
    }

    canvas_draw_str_aligned(canvas, 2, 24, AlignLeft, AlignTop, "Mod:");
    canvas_draw_str_aligned(canvas, 3, 24, AlignLeft, AlignTop, "Mod:");
    canvas_draw_str_aligned(canvas, 28, 24, AlignLeft, AlignTop, s_mod_names[app->mod_idx]);
    if(in_mod) {
        canvas_draw_dot(canvas, 107, 25);
        canvas_draw_line(canvas, 106, 26, 108, 26);
        canvas_draw_line(canvas, 105, 27, 109, 27);

        canvas_draw_line(canvas, 105, 29, 109, 29);
        canvas_draw_line(canvas, 106, 30, 108, 30);
        canvas_draw_dot(canvas, 107, 31);
        canvas_set_color(canvas, ColorBlack);
    }

    if(in_atk) {
        canvas_draw_box(canvas, 0, 35, 128, 11);
        canvas_set_color(canvas, ColorWhite);
    }

    canvas_draw_str_aligned(canvas, 2, 36, AlignLeft, AlignTop, "Atk:");
    canvas_draw_str_aligned(canvas, 3, 36, AlignLeft, AlignTop, "Atk:");
    canvas_draw_str_aligned(canvas, 28, 36, AlignLeft, AlignTop, s_method_names[app->method_idx]);
    if(in_atk) {
        canvas_draw_dot(canvas, 107, 37);
        canvas_draw_line(canvas, 106, 38, 108, 38);
        canvas_draw_line(canvas, 105, 39, 109, 39);

        canvas_draw_line(canvas, 105, 41, 109, 41);
        canvas_draw_line(canvas, 106, 42, 108, 42);
        canvas_draw_dot(canvas, 107, 43);
        canvas_set_color(canvas, ColorBlack);
    }

    elements_button_center(canvas, tx_on ? "STOP" : "Start");
}

static void fox_input_cb(InputEvent* ev, void* ctx) {
    furi_message_queue_put(((FoxRFJammer*)ctx)->event_queue, ev, FuriWaitForever);
}

FoxRFJammer* fox_rf_jammer_alloc(void) {
    FoxRFJammer* app = malloc(sizeof(FoxRFJammer));
    if(!app) return NULL;

    app->frequency       = 315000000UL;
    app->cursor_position = 0;
    app->running         = true;
    app->tx_active       = false;
    app->tx_running      = false;
    app->mod_idx         = 0;
    app->method_idx      = 0;
    app->device          = NULL;
    app->subghz_txrx     = NULL;
    app->tx_thread       = NULL;

    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port   = view_port_alloc();
    app->gui         = furi_record_open(RECORD_GUI);

    view_port_draw_callback_set(app->view_port, fox_main_draw_cb, app);
    view_port_input_callback_set(app->view_port, fox_input_cb, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    furi_hal_region_set(&s_unlocked_region);
    furi_hal_power_suppress_charge_enter();

    subghz_devices_init();
    app->subghz_txrx = subghz_tx_rx_worker_alloc();

    return app;
}

void fox_rf_jammer_free(FoxRFJammer* app) {
    furi_assert(app);
    fox_stop_tx(app);
    if(app->subghz_txrx) {
        if(subghz_tx_rx_worker_is_running(app->subghz_txrx))
            subghz_tx_rx_worker_stop(app->subghz_txrx);
        subghz_tx_rx_worker_free(app->subghz_txrx);
        app->subghz_txrx = NULL;
    }
    if(app->device) {
        if(radio_device_loader_is_external(app->device)) {
            if(furi_hal_power_is_otg_enabled()) furi_hal_power_disable_otg();
        } else {
            radio_device_loader_end(app->device);
        }
        app->device = NULL;
    }
    subghz_devices_deinit();
    furi_hal_power_suppress_charge_exit();
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    free(app);
}

int32_t fox_rf_jammer_app(void* p) {
    const char* launch_arg = (p && *(const char*)p) ? (const char*)p : NULL;
    bool launched_from_subghz = launch_arg && (strcmp(launch_arg, "menu:jammer") == 0);

    FoxRFJammer* app = fox_rf_jammer_alloc();
    if(!app) return -1;

    view_port_draw_callback_set(app->view_port, fox_splash_draw_cb, app);
    fox_splash_shuffle_blocks();
    s_splash_elapsed_ms = 0;
    uint32_t splash_total_ms = FOX_SPLASH_HOLD_MS + FOX_SPLASH_FADE_MS;
    while(s_splash_elapsed_ms < splash_total_ms) {
        view_port_update(app->view_port);
        furi_delay_ms(FOX_SPLASH_TICK_MS);
        s_splash_elapsed_ms += FOX_SPLASH_TICK_MS;
    }
    view_port_draw_callback_set(app->view_port, fox_main_draw_cb, app);

    app->device = radio_device_loader_set(NULL, SubGhzRadioDeviceTypeExternalCC1101);
    if(!app->device)
        app->device = radio_device_loader_set(NULL, SubGhzRadioDeviceTypeInternal);
    if(!app->device) { fox_rf_jammer_free(app); return -1; }

    subghz_devices_reset(app->device);
    subghz_devices_idle(app->device);
    fox_apply_mod_preset(app);
    view_port_update(app->view_port);

    InputEvent ev;
    while(app->running) {
        if(furi_message_queue_get(app->event_queue, &ev, 10) != FuriStatusOk) continue;
        if(ev.type != InputTypeShort && ev.type != InputTypeRepeat) continue;

            if(app->tx_active && app->tx_running) {
            if(ev.key == InputKeyOk || ev.key == InputKeyBack) {
                app->tx_active = false;
                fox_stop_tx(app);
                view_port_update(app->view_port);
            }
            continue;
        }

            bool redraw = false;

        switch(ev.key) {
        case InputKeyOk:
            app->tx_active = true;
            fox_start_tx(app);
            redraw = true;
            break;

        case InputKeyBack:
            app->running = false;
            break;

        case InputKeyRight:
            app->cursor_position = (app->cursor_position + 1) % (CURSOR_MAX + 1);
            redraw = true;
            break;

        case InputKeyLeft:
            app->cursor_position = (app->cursor_position == 0)
                                       ? CURSOR_MAX
                                       : app->cursor_position - 1;
            redraw = true;
            break;

        case InputKeyUp:
            if(app->cursor_position == CURSOR_MODULATION) {
                app->mod_idx = (app->mod_idx + 1) % JAM_MOD_COUNT;
                fox_apply_mod_preset(app);
            } else if(app->cursor_position == CURSOR_METHOD) {
                app->method_idx = (app->method_idx + 1) % JAM_METHOD_COUNT;
            } else {
                const uint32_t steps[] = {
                    100000000UL, 10000000UL, 1000000UL,
                    100000UL,    10000UL,    1000UL
                };
                uint32_t f = app->frequency + steps[app->cursor_position];
                if(f > FREQ_MAX) f = FREQ_MIN;
                app->frequency = fox_freq_clamp(f, true);
            }
            redraw = true;
            break;

        case InputKeyDown:
            if(app->cursor_position == CURSOR_MODULATION) {
                app->mod_idx = (app->mod_idx == 0)
                                   ? JAM_MOD_COUNT - 1
                                   : app->mod_idx - 1;
                fox_apply_mod_preset(app);
            } else if(app->cursor_position == CURSOR_METHOD) {
                app->method_idx = (app->method_idx == 0)
                                       ? JAM_METHOD_COUNT - 1
                                       : app->method_idx - 1;
            } else {
                const uint32_t steps[] = {
                    100000000UL, 10000000UL, 1000000UL,
                    100000UL,    10000UL,    1000UL
                };
                uint32_t step = steps[app->cursor_position];
                uint32_t f    = (app->frequency > step + FREQ_MIN)
                                    ? app->frequency - step : FREQ_MAX;
                app->frequency = fox_freq_clamp(f, false);
            }
            redraw = true;
            break;

        default:
            break;
        }

        if(redraw) view_port_update(app->view_port);
    }

    fox_rf_jammer_free(app);

    if(launched_from_subghz) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, "/ext/subghz/.focus_menu", FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            const char* marker = "menu:jammer";
            storage_file_write(f, marker, strlen(marker));
            storage_file_close(f);
        }
        storage_file_free(f);
        furi_record_close(RECORD_STORAGE);
        Loader* loader = furi_record_open(RECORD_LOADER);
        loader_enqueue_launch(loader, "subghz", NULL, LoaderDeferredLaunchFlagNone);
        furi_record_close(RECORD_LOADER);
    }

    return 0;
}
