#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>
#include <expansion/expansion.h>

typedef enum {
    FlasherViewDetect,
    FlasherViewConnect,
    FlasherViewMenu,
    FlasherViewBoard,
    FlasherViewFiles,
    FlasherViewPrepare,
    FlasherViewProgress,
    FlasherViewTerminal,
    FlasherViewInput,
    FlasherViewResult,
} FlasherView;

typedef enum {
    FlasherEventDetectOk       = 0,
    FlasherEventDetectFail     = 1,
    FlasherEventMenuFirmware   = 2,
    FlasherEventMenuCustom     = 3,
    FlasherEventMenuTerminal   = 4,
    FlasherEventBoardGo        = 5,
    FlasherEventFilesGo        = 6,
    FlasherEventPrepareGo      = 7,
    FlasherEventFlashProgress  = 8,
    FlasherEventFlashDone      = 9,
    FlasherEventFlashFail      = 10,
    FlasherEventTerminalCmd      = 11,
    FlasherEventCmdSent          = 12,
    FlasherEventTerminalUpdate   = 13,
    FlasherEventBootNotDetected    = 14,
    FlasherEventPrepareCancel    = 15,
    FlasherEventDetectSkip       = 16,
    FlasherEventPrepareContinue  = 17,
    FlasherEventPrepareAutoDetected = 18,
    FlasherEventResultDwellDone     = 19,
} FlasherEvent;

#define FLASHER_BOARD_COUNT 9

typedef struct {
    const char*  label;
    const char*  folder;
    uint32_t     boot_addr;
} FlasherBoard;

extern const FlasherBoard k_flasher_boards[FLASHER_BOARD_COUNT];

#define FLASHER_STATUS_LEN 80

typedef struct {
    volatile uint8_t  progress;
    volatile bool     done;
    volatile bool     success;
    char              status[FLASHER_STATUS_LEN];
    FuriMutex*        mutex;
} FlasherWorkerState;

#define FLASHER_TERM_LOG_LEN  512

#define FLASHER_PATH_LEN 256
#define FLASHER_CMD_LEN  128
#define FLASHER_DATA_DIR EXT_PATH("apps_data/fox_esp_flasher")
#define FLASHER_BAUDRATE 115200U

#define FLASHER_FAST_BAUDRATE 921600U

typedef struct FlasherApp FlasherApp;
struct FlasherApp {
    Gui*             gui;
    ViewDispatcher*  view_dispatcher;
    Storage*         storage;
    DialogsApp*      dialogs;
    NotificationApp* notifications;

    Expansion*           expansion;

    FuriHalSerialHandle* serial_handle;
    FuriThread*          uart_rx_thread;
    FuriStreamBuffer*    uart_rx_stream;

    size_t  pin_option_index;
    uint8_t connect_selected;

    uint8_t board_index;
    bool    board_custom;

    char file_bootloader[FLASHER_PATH_LEN];
    char file_partitions[FLASHER_PATH_LEN];
    char file_firmware[FLASHER_PATH_LEN];
    uint8_t files_selected;

    FuriThread*        flash_thread;
    FlasherWorkerState worker_state;
    FuriStreamBuffer*  flash_rx_stream;

    char   term_log[FLASHER_TERM_LOG_LEN];
    size_t term_log_len;
    char   last_cmd[FLASHER_CMD_LEN];
    char   cmd_buf[FLASHER_CMD_LEN];

    FlasherView current_view;
    bool        flashing_active;
    bool        esp32_in_bootloader;
    bool        prepare_is_startup;
    bool        auto_install_pending;

    FuriThread*        prepare_poll_thread;
    volatile bool       prepare_poll_running;

    FuriTimer*  result_dwell_timer;
    bool        flash_done_pending;
    bool        last_result_success;

    View*       detect_view;
    View*       connect_view;
    View*       menu_view;
    View*       board_view;
    View*       files_view;
    View*       prepare_view;
    View*       progress_view;
    View*       terminal_view;
    View*       result_view;
    TextInput*  text_input;
};

View* view_detect_alloc(FlasherApp* app);
void  view_detect_free(View* v);
void  view_detect_set_probing(View* v, bool probing);
void  view_detect_set_found(View* v, bool found);

View* view_connect_alloc(FlasherApp* app);
void  view_connect_free(View* v);

View* view_menu_alloc(FlasherApp* app);
void  view_menu_free(View* v);

View* view_board_alloc(FlasherApp* app);
void  view_board_free(View* v);
void  view_board_refresh(View* v);

View* view_files_alloc(FlasherApp* app);
void  view_files_free(View* v);
void  view_files_refresh(View* v);
void  view_files_select_install(View* v);

View* view_prepare_alloc(FlasherApp* app);
void  view_prepare_free(View* v);
void  view_prepare_refresh(View* v);
void  view_prepare_set_startup(View* v);
void  view_prepare_set_error(View* v);

View* view_progress_alloc(FlasherApp* app);
void  view_progress_free(View* v);
void  view_progress_refresh(View* v);

View* view_terminal_alloc(FlasherApp* app);
void  view_terminal_free(View* v);
void  view_terminal_refresh(View* v);
void  view_terminal_reset_scroll(View* v);
void  view_terminal_append(FlasherApp* app, const char* str, size_t len);

View* view_result_alloc(FlasherApp* app);
void  view_result_free(View* v);
void  view_result_set(View* v, bool success, uint8_t board_index);

void flasher_switch_view(FlasherApp* app, FlasherView v);

void flasher_draw_ok_button(
    Canvas* canvas, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t radius, const char* label);

void flasher_terminal_back(FlasherApp* app);

void flasher_uart_open(FlasherApp* app);
void flasher_uart_close(FlasherApp* app);
void flasher_uart_pause_rx(FlasherApp* app);
void flasher_uart_resume_rx(FlasherApp* app);

uint32_t flasher_uart_enter_bootloader(FlasherApp* app);
void flasher_uart_tx(FlasherApp* app, const uint8_t* data, size_t len);
void flasher_uart_set_br(FlasherApp* app, uint32_t baud);
bool flasher_uart_check_bootloader(FlasherApp* app);
void flasher_uart_get_and_reset_rx_errors(uint32_t* overrun, uint32_t* frame, uint32_t* noise);

void flasher_prepare_poll_start(FlasherApp* app);
void flasher_prepare_poll_stop(FlasherApp* app);

void flasher_worker_start(FlasherApp* app);
void flasher_worker_stop(FlasherApp* app);

void flasher_worker_log(const char* str);

size_t      flasher_pin_option_count(void);
const char* flasher_pin_option_label(size_t index);
