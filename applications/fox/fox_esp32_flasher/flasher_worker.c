#include "fox_esp32_flasher.h"

#ifndef SERIAL_FLASHER_INTERFACE_UART
#define SERIAL_FLASHER_INTERFACE_UART
#endif

#define SERIAL_FLASHER_RESET_HOLD_TIME_MS 100

#include "esp_loader_io.h"
#include "esp_loader.h"

#include <furi_hal_gpio.h>
#include <furi_hal_rtc.h>
#include <string.h>
#include <stdio.h>

#define TAG "FlasherWorker"

#define FLASHER_DEBUG_LOG_ENABLED 1

#define FLASHER_TURBO_ENABLED 1

const FlasherBoard k_flasher_boards[FLASHER_BOARD_COUNT] = {
    {"ESP32 Classic",  "esp32",   0x1000},
    {"ESP32-WROOM",   "esp32",   0x1000},
    {"ESP32-WROVER",  "esp32",   0x1000},
    {"ESP32-CAM",     "esp32",   0x1000},
    {"ESP32-S2",      "esp32s2", 0x1000},
    {"ESP32-S3",      "esp32s3", 0x0000},
    {"ESP32-C3",      "esp32c3", 0x0000},
    {"ESP32-C5",      "esp32c5", 0x2000},
    {"ESP32-C6",      "esp32c6", 0x0000},
};

static FlasherApp*       g_app           = NULL;
static FuriStreamBuffer* g_flash_rx      = NULL;
static File*             g_log_file      = NULL;

static FuriTimer* g_timer          = NULL;
static uint32_t   g_remaining_time = 0;

static uint32_t g_session_start_tick = 0;

static void log_write(const char* str) {
    if(!g_log_file || !str || !str[0]) return;
    storage_file_write(g_log_file, str, strlen(str));
}

static void log_flush(void) {
    if(g_log_file) storage_file_sync(g_log_file);
}

void flasher_worker_log(const char* str) {
    log_write(str);
    log_flush();
}

esp_loader_error_t loader_port_write(const uint8_t* data, uint16_t size, uint32_t timeout) {
    UNUSED(timeout);
    if(g_app) flasher_uart_tx(g_app, data, size);
    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_port_read(uint8_t* data, uint16_t size, uint32_t timeout) {
    if(!g_flash_rx) {
        return ESP_LOADER_ERROR_TIMEOUT;
    }
    size_t got = furi_stream_buffer_receive(g_flash_rx, data, size, timeout);
    return (got < size) ? ESP_LOADER_ERROR_TIMEOUT : ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_port_change_transmission_rate(uint32_t rate) {
    if(g_app && g_app->serial_handle) furi_hal_serial_set_br(g_app->serial_handle, rate);
    return ESP_LOADER_SUCCESS;
}

void loader_port_delay_ms(uint32_t ms) {
    furi_delay_ms(ms);
}

static void timer_callback(void* context) {
    UNUSED(context);
    if(g_remaining_time > 0) {
        g_remaining_time--;
    }
}

void loader_port_start_timer(uint32_t ms) {
    g_remaining_time = ms;
    if(g_timer) furi_timer_start(g_timer, 1);
}

uint32_t loader_port_remaining_time(void) {
    return g_remaining_time;
}

void loader_port_enter_bootloader(void) {
    loader_port_delay_ms(100);
}

void loader_port_reset_target(void) {
    furi_hal_gpio_init(&gpio_ext_pb2, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_ext_pb2, false);
    loader_port_delay_ms(250);
    furi_hal_gpio_write(&gpio_ext_pb2, true);
    furi_hal_gpio_init(&gpio_ext_pb2, GpioModeInput, GpioPullNo, GpioSpeedLow);
    loader_port_delay_ms(100);
}

void loader_port_debug_print(const char* str) {
    if(!g_app) return;
    log_write(str);
    log_flush();
    view_terminal_append(g_app, str, strlen(str));
    view_dispatcher_send_custom_event(g_app->view_dispatcher, FlasherEventTerminalUpdate);
}

void loader_port_spi_set_cs(uint32_t level) {
    UNUSED(level);
}

static void worker_set_progress_value(FlasherApp* app, uint8_t pct, const char* msg) {
    furi_mutex_acquire(app->worker_state.mutex, FuriWaitForever);
    app->worker_state.progress = pct;
    if(msg) snprintf(app->worker_state.status, FLASHER_STATUS_LEN, "%s", msg);
    furi_mutex_release(app->worker_state.mutex);
}

static void worker_set_progress(FlasherApp* app, uint8_t pct, const char* msg) {
    worker_set_progress_value(app, pct, msg);
    view_dispatcher_send_custom_event(app->view_dispatcher, FlasherEventFlashProgress);
}

static esp_loader_error_t flash_one_file(
    FlasherApp* app,
    const char* path,
    uint32_t addr,
    uint8_t step,
    uint8_t total_steps) {
    char msg[96];
    esp_loader_error_t err;
    static uint8_t payload[1024];

    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        loader_port_debug_print("Cannot open file\n");
        storage_file_free(f);
        return ESP_LOADER_ERROR_FAIL;
    }

    uint64_t total_size = storage_file_size(f);

    snprintf(msg, sizeof(msg), "Erasing flash...this may take a while (%u/%u)\n", step, total_steps);
    loader_port_debug_print(msg);

    err = esp_loader_flash_start(addr, total_size, sizeof(payload));
    if(err != ESP_LOADER_SUCCESS) {
        snprintf(msg, sizeof(msg), "Erasing flash failed with error %d\n", err);
        loader_port_debug_print(msg);
        storage_file_close(f);
        storage_file_free(f);
        return err;
    }

    loader_port_debug_print("Start programming\n");

    uint64_t remaining     = total_size;
    uint64_t last_updated  = remaining;
    uint32_t last_term_tick = furi_get_tick();
    while(remaining > 0) {
        size_t to_read = (remaining < sizeof(payload)) ? (size_t)remaining : sizeof(payload);
        uint16_t n = storage_file_read(f, payload, to_read);
        err = esp_loader_flash_write(payload, n);
        if(err != ESP_LOADER_SUCCESS) {
            snprintf(msg, sizeof(msg), "Packet could not be written! Error: %u\n", err);
            loader_port_debug_print(msg);
            storage_file_close(f);
            storage_file_free(f);
            return err;
        }
        remaining -= n;

        if((furi_get_tick() - last_term_tick) >= 500) {
            last_term_tick = furi_get_tick();

            uint8_t file_pct = (uint8_t)(((total_size - remaining) * 100ULL) / total_size);
            uint8_t term_pct = (uint8_t)(((step - 1) * 100U / total_steps) + (file_pct / total_steps));
            uint32_t cur_addr = addr + (uint32_t)(total_size - remaining);
            char tmsg[48];
            snprintf(
                tmsg,
                sizeof(tmsg),
                "Writing 0x%06lX... (%u%%)\n",
                (unsigned long)cur_addr,
                term_pct);
            view_terminal_append(app, tmsg, strlen(tmsg));
            view_dispatcher_send_custom_event(app->view_dispatcher, FlasherEventTerminalUpdate);

            log_write(tmsg);
            log_flush();
        }

        if((last_updated - remaining) > 50000 || remaining == 0) {
            uint8_t file_pct = (uint8_t)(((total_size - remaining) * 100ULL) / total_size);
            uint8_t overall  = (uint8_t)(((step - 1) * 100U / total_steps) + (file_pct / total_steps));
            worker_set_progress(app, overall, NULL);

            snprintf(msg, sizeof(msg), "[t] step=%u file_pct=%u elapsed_ms=%lu heap_free=%u\n",
                     step, file_pct, (unsigned long)(furi_get_tick() - g_session_start_tick),
                     (unsigned)memmgr_get_free_heap());
            log_write(msg);
            log_flush();

            last_updated = remaining;
        }
    }

    storage_file_close(f);
    storage_file_free(f);

    snprintf(msg, sizeof(msg), "Finished programming (%u/%u) elapsed_ms=%lu\n",
             step, total_steps, (unsigned long)(furi_get_tick() - g_session_start_tick));
    loader_port_debug_print(msg);
    return ESP_LOADER_SUCCESS;
}

static int32_t flash_thread_fn(void* context) {
    FlasherApp* app = context;
    bool boot_not_detected = false;
    bool turbo_active = false;

    app->flash_rx_stream = g_flash_rx = furi_stream_buffer_alloc(4096, 1);
    g_timer               = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, app);
    g_session_start_tick   = furi_get_tick();

    log_write("=== flash session start ===\n");
    {
        size_t heap_free  = memmgr_get_free_heap();
        size_t heap_total = memmgr_get_total_heap();
        FuriThreadList* tlist = furi_thread_list_alloc();
        furi_thread_enumerate(tlist);
        size_t thread_count = furi_thread_list_size(tlist);

        char msg[96];
        snprintf(
            msg,
            sizeof(msg),
            "[env] heap_free=%u heap_total=%u threads=%u\n",
            (unsigned)heap_free,
            (unsigned)heap_total,
            (unsigned)thread_count);
        log_write(msg);

        for(size_t i = 0; i < thread_count; i++) {
            FuriThreadListItem* item = furi_thread_list_get_at(tlist, i);
            char tmsg[64];
            snprintf(
                tmsg,
                sizeof(tmsg),
                "[thr] %-20s prio=%u stack=%u\n",
                item->name ? item->name : "?",
                (unsigned)item->priority,
                (unsigned)item->stack_size);
            log_write(tmsg);
        }

        furi_thread_list_free(tlist);
    }
    log_flush();

    notification_message(app->notifications, &sequence_set_only_blue_255);

    notification_message(app->notifications, &sequence_display_backlight_enforce_on);

    const FlasherBoard* board = &k_flasher_boards[app->board_index];
    char path_boot[FLASHER_PATH_LEN];
    char path_part[FLASHER_PATH_LEN];
    char path_boot0[FLASHER_PATH_LEN];
    char path_fw[FLASHER_PATH_LEN];

    if(app->board_custom) {
        snprintf(path_boot,  sizeof(path_boot),  "%s", app->file_bootloader);
        snprintf(path_part,  sizeof(path_part),  "%s", app->file_partitions);
        snprintf(path_boot0, sizeof(path_boot0), FLASHER_DATA_DIR "/%s/boot_app0.bin", board->folder);
        snprintf(path_fw,    sizeof(path_fw),    "%s", app->file_firmware);
    } else {
        snprintf(path_boot,  sizeof(path_boot),  FLASHER_DATA_DIR "/%s/bootloader.bin", board->folder);
        snprintf(path_part,  sizeof(path_part),  FLASHER_DATA_DIR "/%s/partitions.bin", board->folder);
        snprintf(path_boot0, sizeof(path_boot0), FLASHER_DATA_DIR "/%s/boot_app0.bin",  board->folder);
        snprintf(path_fw,    sizeof(path_fw),    FLASHER_DATA_DIR "/%s/firmware.bin",   board->folder);
    }

    {
        const char* paths[4] = {path_boot, path_part, path_boot0, path_fw};
        const char* names[4] = {"bootloader.bin", "partitions.bin",
                                 "boot_app0.bin",  "firmware.bin"};
        char msg[64];
        for(int i = 0; i < 4; i++) {
            if(storage_common_stat(app->storage, paths[i], NULL) != FSE_OK) {
                snprintf(msg, sizeof(msg), "Missing: %s", names[i]);
                worker_set_progress(app, 0, msg);
                snprintf(msg, sizeof(msg), "File not found: %s\n", paths[i]);
                loader_port_debug_print(msg);
                loader_port_debug_print("Reflash FoxFW to restore SD card files\n");
                goto fail;
            }
        }
    }

    worker_set_progress(app, 0, "Entering bootloader...");
    log_write("=== boot-entry start ===\n");
    log_flush();

    if(!app->esp32_in_bootloader) {
        uint32_t drained = flasher_uart_enter_bootloader(app);
        char dmsg[48];
        snprintf(dmsg, sizeof(dmsg), "boot-entry drained=%lu\n", (unsigned long)drained);
        log_write(dmsg);
        if(drained > 32) {
            snprintf(dmsg, sizeof(dmsg), "!! %lu stray bytes before connect\n", (unsigned long)drained);
            loader_port_debug_print(dmsg);
        }
    } else {
        flasher_uart_resume_rx(app);

        furi_delay_ms(50);
        uint32_t drained = 0;
        uint8_t discard;
        while(furi_stream_buffer_receive(g_flash_rx, &discard, 1, 0)) drained++;
        char dmsg[48];
        snprintf(dmsg, sizeof(dmsg), "boot-entry drained=%lu (pre-confirmed)\n", (unsigned long)drained);
        log_write(dmsg);
    }
    log_write("=== boot-entry done ===\n");
    log_flush();

    worker_set_progress(app, 2, "Connecting...");
    log_write("=== connect start ===\n");
    {
        char bmsg[48];
        snprintf(bmsg, sizeof(bmsg), "[cfg] baud=%u (connect/boot-entry rate)\n", (unsigned)FLASHER_BAUDRATE);
        log_write(bmsg);
    }
    log_flush();
    esp_loader_connect_args_t cfg = ESP_LOADER_CONNECT_DEFAULT();
    esp_loader_error_t err = esp_loader_connect(&cfg);
    log_write("=== connect returned ===\n");
    log_flush();
    if(err != ESP_LOADER_SUCCESS) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Connect failed: %u\n", err);
        loader_port_debug_print(msg);
        boot_not_detected = true;
        goto fail;
    }
    loader_port_debug_print("Connected\n");

#if FLASHER_TURBO_ENABLED
    {
        log_write("[turbo] requesting fast baud (921600) after connect\n");
        log_flush();
        esp_loader_error_t turbo_err = esp_loader_change_transmission_rate(FLASHER_FAST_BAUDRATE);
        if(turbo_err == ESP_LOADER_SUCCESS) {
            flasher_uart_set_br(app, FLASHER_FAST_BAUDRATE);
            turbo_active = true;
            char tmsg[48];
            snprintf(tmsg, sizeof(tmsg), "[turbo] now at baud=%u\n", (unsigned)FLASHER_FAST_BAUDRATE);
            log_write(tmsg);
        } else {
            char tmsg[72];
            snprintf(
                tmsg,
                sizeof(tmsg),
                "[turbo] change_transmission_rate failed: %u (staying at %u)\n",
                turbo_err,
                (unsigned)FLASHER_BAUDRATE);
            log_write(tmsg);
        }
        log_flush();
    }
#endif

    worker_set_progress(app, 5, "Flashing...");

    uint8_t n_steps = 4;
    uint8_t step    = 1;

    err = flash_one_file(app, path_boot,  board->boot_addr, step++, n_steps);
    if(err != ESP_LOADER_SUCCESS) goto fail;

    err = flash_one_file(app, path_part,  0x8000,           step++, n_steps);
    if(err != ESP_LOADER_SUCCESS) goto fail;

    err = flash_one_file(app, path_boot0, 0xE000,           step++, n_steps);
    if(err != ESP_LOADER_SUCCESS) goto fail;

    err = flash_one_file(app, path_fw,    0x10000,          step++, n_steps);
    if(err != ESP_LOADER_SUCCESS) goto fail;

    loader_port_debug_print("Resetting...\n");
    loader_port_reset_target();
    worker_set_progress(app, 100, "Done!");

    furi_mutex_acquire(app->worker_state.mutex, FuriWaitForever);
    app->worker_state.done = app->worker_state.success = true;
    furi_mutex_release(app->worker_state.mutex);

    notification_message(app->notifications, &sequence_set_vibro_on);
    loader_port_delay_ms(80);
    notification_message(app->notifications, &sequence_reset_vibro);
    notification_message(app->notifications, &sequence_reset_blue);
    goto cleanup;

fail:
    furi_mutex_acquire(app->worker_state.mutex, FuriWaitForever);
    app->worker_state.done    = true;
    app->worker_state.success = false;

    if(app->worker_state.progress >= 100) app->worker_state.progress = 99;
    snprintf(app->worker_state.status, FLASHER_STATUS_LEN, "Flash Failed");
    furi_mutex_release(app->worker_state.mutex);
    notification_message(app->notifications, &sequence_reset_blue);

cleanup:

    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);

#if FLASHER_TURBO_ENABLED
    if(turbo_active) {
        flasher_uart_set_br(app, FLASHER_BAUDRATE);
        log_write("[turbo] restored baud to 115200\n");
    }
#endif
    log_write("=== flash session end ===\n");
    log_flush();

    flasher_uart_pause_rx(app);

    FuriStreamBuffer* rx_to_free = g_flash_rx;
    app->flash_rx_stream = NULL;
    g_flash_rx = NULL;
    furi_stream_buffer_free(rx_to_free);

    if(g_timer) {
        furi_timer_free(g_timer);
        g_timer = NULL;
    }

    flasher_uart_resume_rx(app);

    FlasherEvent ev = app->worker_state.success ? FlasherEventFlashDone
                    : boot_not_detected        ? FlasherEventBootNotDetected
                                               : FlasherEventFlashFail;
    view_dispatcher_send_custom_event(app->view_dispatcher, ev);
    return 0;
}

void flasher_worker_start(FlasherApp* app) {
    if(app->flash_thread) {
        return;
    }
    g_app = app;

#if FLASHER_DEBUG_LOG_ENABLED
    if(!g_log_file) {
        storage_simply_mkdir(app->storage, FLASHER_DATA_DIR "/logs");

        DateTime dt;
        furi_hal_rtc_get_datetime(&dt);
        char log_path[FLASHER_PATH_LEN];
        snprintf(
            log_path,
            sizeof(log_path),
            FLASHER_DATA_DIR "/logs/%04u%02u%02u_%02u%02u%02u.log",
            (unsigned)dt.year,
            (unsigned)dt.month,
            (unsigned)dt.day,
            (unsigned)dt.hour,
            (unsigned)dt.minute,
            (unsigned)dt.second);

        g_log_file = storage_file_alloc(app->storage);
        if(!storage_file_open(g_log_file, log_path, FSAM_WRITE, FSOM_OPEN_ALWAYS)) {
            storage_file_free(g_log_file);
            g_log_file = NULL;
        }
    }
#endif

    furi_mutex_acquire(app->worker_state.mutex, FuriWaitForever);
    app->worker_state.progress  = 0;
    app->worker_state.done      = false;
    app->worker_state.success   = false;
    app->worker_state.status[0] = '\0';
    furi_mutex_release(app->worker_state.mutex);

    app->flash_thread = furi_thread_alloc_ex("FoxFlashWorker", 8192, flash_thread_fn, app);
    furi_thread_start(app->flash_thread);
}

void flasher_worker_stop(FlasherApp* app) {
    if(!app->flash_thread) return;
    furi_thread_join(app->flash_thread);
    furi_thread_free(app->flash_thread);
    app->flash_thread = NULL;
    g_app = NULL;

    if(g_log_file) {
        storage_file_close(g_log_file);
        storage_file_free(g_log_file);
        g_log_file = NULL;
    }
}
