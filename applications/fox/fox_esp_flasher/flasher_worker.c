#include "fox_esp_flasher.h"

#ifndef SERIAL_FLASHER_INTERFACE_UART
#define SERIAL_FLASHER_INTERFACE_UART
#endif

#define SERIAL_FLASHER_RESET_HOLD_TIME_MS 100

#include "esp_loader_io.h"
#include "esp_loader.h"

#include <furi_hal_gpio.h>
#include <string.h>
#include <stdio.h>

#define TAG "FlasherWorker"

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

static FlasherApp*       g_app          = NULL;
static FuriStreamBuffer* g_flash_rx     = NULL;
static uint32_t          g_remaining_ms = 0;

static void timer_tick(void* context) {
    UNUSED(context);
    if(g_remaining_ms > 0) g_remaining_ms--;
}

static void _initDTR(void) {
    furi_hal_gpio_init(&gpio_ext_pc3, GpioModeOutputPushPull, GpioPullDown, GpioSpeedVeryHigh);
    furi_hal_gpio_init(&gpio_ext_pc1, GpioModeOutputPushPull, GpioPullDown, GpioSpeedVeryHigh);
}

static void _initRTS(void) {
    furi_hal_gpio_init(&gpio_ext_pb2, GpioModeOutputPushPull, GpioPullDown, GpioSpeedVeryHigh);
    furi_hal_gpio_init(&gpio_ext_pc0, GpioModeOutputPushPull, GpioPullDown, GpioSpeedVeryHigh);
}

static void _setDTR(bool state) {
    furi_hal_gpio_write(&gpio_ext_pc1, state);
    furi_hal_gpio_write(&gpio_ext_pc3, state);
}

static void _setRTS(bool state) {
    furi_hal_gpio_write(&gpio_ext_pb2, state);
    furi_hal_gpio_write(&gpio_ext_pc0, state);
}

static void _deinitDTR(void) {
    furi_hal_gpio_init(&gpio_ext_pc3, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_ext_pc1, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

static void _deinitRTS(void) {
    furi_hal_gpio_init(&gpio_ext_pb2, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_ext_pc0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

esp_loader_error_t loader_port_write(const uint8_t* data, uint16_t size, uint32_t timeout) {
    UNUSED(timeout);
    if(g_app) flasher_uart_tx(g_app, data, size);
    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_port_read(uint8_t* data, uint16_t size, uint32_t timeout) {
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

void loader_port_start_timer(uint32_t ms) {
    g_remaining_ms = ms;
    if(g_app) furi_timer_start(g_app->flash_timer, 1);
}

uint32_t loader_port_remaining_time(void) {
    return g_remaining_ms;
}

void loader_port_enter_bootloader(void) {
    /* ESP32 bootloader entry is handled manually by the user on the Prepare
     * screen (hold BOOT, tap RST, release BOOT) before pressing OK.
     * Nothing to do here — short delay for UART to settle before sync. */
    loader_port_delay_ms(100);
}

void loader_port_reset_target(void) {
    _setDTR(true);
    loader_port_delay_ms(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
    _setDTR(false);
}

void loader_port_debug_print(const char* str) {
    if(!g_app) return;
    view_terminal_append(g_app, str, strlen(str));
    view_dispatcher_send_custom_event(g_app->view_dispatcher, FlasherEventTerminalUpdate);
}

void loader_port_spi_set_cs(uint32_t level) {
    UNUSED(level);
}

static void worker_set_progress(FlasherApp* app, uint8_t pct, const char* msg) {
    furi_mutex_acquire(app->worker_state.mutex, FuriWaitForever);
    app->worker_state.progress = pct;
    if(msg) snprintf(app->worker_state.status, FLASHER_STATUS_LEN, "%s", msg);
    furi_mutex_release(app->worker_state.mutex);
    view_dispatcher_send_custom_event(app->view_dispatcher, FlasherEventFlashProgress);
}

static esp_loader_error_t flash_one_file(
    FlasherApp* app,
    const char* path,
    uint32_t addr,
    uint8_t step,
    uint8_t total_steps) {
    char msg[64];
    esp_loader_error_t err;
    static uint8_t payload[1024];

    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        loader_port_debug_print("Cannot open file\n");
        storage_file_free(f);
        return ESP_LOADER_ERROR_FAIL;
    }

    uint64_t total_size = storage_file_size(f);
    snprintf(msg, sizeof(msg), "Erasing... (%u/%u)\n", step, total_steps);
    loader_port_debug_print(msg);

    err = esp_loader_flash_start(addr, total_size, sizeof(payload));
    if(err != ESP_LOADER_SUCCESS) {
        snprintf(msg, sizeof(msg), "Erase failed: %d\n", err);
        loader_port_debug_print(msg);
        storage_file_close(f);
        storage_file_free(f);
        return err;
    }

    snprintf(msg, sizeof(msg), "Writing... (%u/%u)\n", step, total_steps);
    loader_port_debug_print(msg);

    uint64_t remaining = total_size;
    while(remaining > 0) {
        size_t to_read = (remaining < sizeof(payload)) ? (size_t)remaining : sizeof(payload);
        uint16_t n = storage_file_read(f, payload, to_read);
        err = esp_loader_flash_write(payload, n);
        if(err != ESP_LOADER_SUCCESS) {
            snprintf(msg, sizeof(msg), "Write error: %d\n", err);
            loader_port_debug_print(msg);
            storage_file_close(f);
            storage_file_free(f);
            return err;
        }
        remaining -= n;

        uint8_t file_pct = (uint8_t)(((total_size - remaining) * 100ULL) / total_size);
        uint8_t overall  = (uint8_t)(((step - 1) * 100U / total_steps) + (file_pct / total_steps));
        worker_set_progress(app, overall, NULL);
    }

    storage_file_close(f);
    storage_file_free(f);
    snprintf(msg, sizeof(msg), "Done (%u/%u)\n", step, total_steps);
    loader_port_debug_print(msg);
    return ESP_LOADER_SUCCESS;
}

static int32_t flash_thread_fn(void* context) {
    FlasherApp* app = context;

    app->flash_rx_stream = g_flash_rx = furi_stream_buffer_alloc(2048, 1);
    app->flash_timer = furi_timer_alloc(timer_tick, FuriTimerTypePeriodic, NULL);

    _setDTR(false); _initDTR();
    _setRTS(false); _initRTS();

    furi_hal_gpio_init_simple(&gpio_swclk, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_swclk, true);

    notification_message(app->notifications, &sequence_set_only_blue_255);

    worker_set_progress(app, 0, "Entering bootloader...");
    loader_port_enter_bootloader();

    worker_set_progress(app, 2, "Connecting...");
    esp_loader_connect_args_t cfg = ESP_LOADER_CONNECT_DEFAULT();
    esp_loader_error_t err = esp_loader_connect(&cfg);
    if(err != ESP_LOADER_SUCCESS) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Connect failed: %u\n", err);
        loader_port_debug_print(msg);
        goto fail;
    }
    loader_port_debug_print("Connected\n");
    worker_set_progress(app, 5, "Flashing...");

    const FlasherBoard* board = &k_flasher_boards[app->board_index];
    char path_boot[FLASHER_PATH_LEN];
    char path_part[FLASHER_PATH_LEN];
    char path_boot0[FLASHER_PATH_LEN];
    char path_fw[FLASHER_PATH_LEN];

    if(app->board_custom) {
        snprintf(path_boot,  sizeof(path_boot),  "%s", app->file_bootloader);
        snprintf(path_part,  sizeof(path_part),  "%s", app->file_partitions);
        path_boot0[0] = '\0';
        snprintf(path_fw,    sizeof(path_fw),    "%s", app->file_firmware);
    } else {
        snprintf(path_boot,  sizeof(path_boot),  FLASHER_DATA_DIR "/%s/bootloader.bin", board->folder);
        snprintf(path_part,  sizeof(path_part),  FLASHER_DATA_DIR "/%s/partitions.bin", board->folder);
        snprintf(path_boot0, sizeof(path_boot0), FLASHER_DATA_DIR "/%s/boot_app0.bin",  board->folder);
        snprintf(path_fw,    sizeof(path_fw),    FLASHER_DATA_DIR "/%s/firmware.bin",   board->folder);
    }

    uint8_t n_steps = app->board_custom ? 3 : 4;
    uint8_t step = 1;

    err = flash_one_file(app, path_boot, board->boot_addr, step++, n_steps);
    if(err != ESP_LOADER_SUCCESS) goto fail;

    err = flash_one_file(app, path_part, 0x8000, step++, n_steps);
    if(err != ESP_LOADER_SUCCESS) goto fail;

    if(!app->board_custom) {
        err = flash_one_file(app, path_boot0, 0xE000, step++, n_steps);
        if(err != ESP_LOADER_SUCCESS) goto fail;
    }

    err = flash_one_file(app, path_fw, 0x10000, step++, n_steps);
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
    furi_mutex_release(app->worker_state.mutex);
    notification_message(app->notifications, &sequence_reset_blue);

cleanup:
    _deinitDTR();
    _deinitRTS();
    furi_hal_gpio_init_simple(&gpio_swclk, GpioModeAnalog);

    /* Null both pointers before freeing — UART IRQ reads flash_rx_stream to decide
     * which buffer to write to, so clearing it prevents a use-after-free. */
    FuriStreamBuffer* rx_to_free = g_flash_rx;
    app->flash_rx_stream = NULL;
    g_flash_rx = NULL;
    furi_timer_free(app->flash_timer);
    app->flash_timer = NULL;
    furi_stream_buffer_free(rx_to_free);

    FlasherEvent ev = app->worker_state.success ? FlasherEventFlashDone : FlasherEventFlashFail;
    view_dispatcher_send_custom_event(app->view_dispatcher, ev);
    return 0;
}

void flasher_worker_start(FlasherApp* app) {
    g_app = app;
    furi_mutex_acquire(app->worker_state.mutex, FuriWaitForever);
    app->worker_state.progress   = 0;
    app->worker_state.done       = false;
    app->worker_state.success    = false;
    app->worker_state.status[0]  = '\0';
    furi_mutex_release(app->worker_state.mutex);
    app->flash_thread = furi_thread_alloc_ex("FoxFlashWorker", 4096, flash_thread_fn, app);
    furi_thread_start(app->flash_thread);
}

void flasher_worker_stop(FlasherApp* app) {
    if(!app->flash_thread) return;
    furi_thread_join(app->flash_thread);
    furi_thread_free(app->flash_thread);
    app->flash_thread = NULL;
    g_app = NULL;
}
