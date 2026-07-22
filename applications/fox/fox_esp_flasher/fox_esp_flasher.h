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

typedef enum {
    FlasherViewDetect,    /* "Detecting..." / "ESP32 not found" + Settings/Retry */
    FlasherViewConnect,   /* Pin selector + Retry (like fox_chat connect_settings) */
    FlasherViewMenu,      /* Main menu — 3 big Fox-style buttons */
    FlasherViewBoard,     /* Board selector + Install / Select Files action button */
    FlasherViewFiles,     /* Custom install: 3 file-path rows + Install button */
    FlasherViewProgress,  /* Flash progress bar + "DO NOT DISCONNECT" + View Terminal */
    FlasherViewTerminal,  /* Terminal — 2 output lines + Send Command button */
    FlasherViewInput,     /* TextInput reused for Send Command */
    FlasherViewResult,    /* Success / Error result screen */
} FlasherView;

typedef enum {
    FlasherEventDetectOk       = 0,
    FlasherEventDetectFail     = 1,
    FlasherEventMenuFirmware   = 2,
    FlasherEventMenuCustom     = 3,
    FlasherEventMenuTerminal   = 4,
    FlasherEventBoardGo        = 5,
    FlasherEventFilesGo        = 6,
    FlasherEventFlashProgress  = 7,
    FlasherEventFlashDone      = 8,
    FlasherEventFlashFail      = 9,
    FlasherEventTerminalCmd    = 10,
    FlasherEventCmdSent        = 11,
    FlasherEventTerminalUpdate = 12,
} FlasherEvent;

#define FLASHER_BOARD_COUNT 8

typedef struct {
    const char*  label;           /* shown on board selector screen */
    const char*  folder;          /* subfolder in apps_data/fox_esp_flasher/ */
    uint32_t     boot_addr;       /* bootloader flash address */
} FlasherBoard;

extern const FlasherBoard k_flasher_boards[FLASHER_BOARD_COUNT];

#define FLASHER_STATUS_LEN 80

typedef struct {
    volatile uint8_t  progress;          /* 0 – 100 */
    volatile bool     done;
    volatile bool     success;
    char              status[FLASHER_STATUS_LEN]; /* last status line (worker writes) */
    FuriMutex*        mutex;
} FlasherWorkerState;

#define FLASHER_TERM_LOG_LEN  512

#define FLASHER_PATH_LEN 256
#define FLASHER_CMD_LEN  128
#define FLASHER_DATA_DIR EXT_PATH("apps_data/fox_esp_flasher")
#define FLASHER_BAUDRATE 115200U

typedef struct FlasherApp FlasherApp;
struct FlasherApp {
    Gui*             gui;
    ViewDispatcher*  view_dispatcher;
    Storage*         storage;
    DialogsApp*      dialogs;
    NotificationApp* notifications;

    /* UART (shared: AT mode for detect/terminal, binary for flash) */
    FuriHalSerialHandle* serial_handle;
    FuriThread*          uart_rx_thread;
    FuriStreamBuffer*    uart_rx_stream; /* regular AT / terminal RX */

    /* Detect / connect */
    bool    detect_probing;   /* true while background probe thread is running */
    bool    detect_found;     /* result of last probe */
    FuriThread* probe_thread;
    size_t  pin_option_index; /* for connect settings */
    uint8_t connect_selected;

    /* Board selection */
    uint8_t board_index;      /* 0 – FLASHER_BOARD_COUNT-1 */
    bool    board_custom;     /* true = custom .bin install */

    /* Custom file paths (custom install mode) */
    char file_bootloader[FLASHER_PATH_LEN];
    char file_partitions[FLASHER_PATH_LEN];
    char file_firmware[FLASHER_PATH_LEN];
    uint8_t files_selected;   /* bitmask: bit0=bootloader, bit1=partitions, bit2=firmware */

    /* Flash worker */
    FuriThread*        flash_thread;
    FlasherWorkerState worker_state;
    FuriStreamBuffer*  flash_rx_stream; /* consumed by esp_loader lib during flash */
    FuriTimer*         flash_timer;     /* loader_port_start_timer() implementation */

    /* Terminal */
    char   term_log[FLASHER_TERM_LOG_LEN];
    size_t term_log_len;
    char   last_cmd[FLASHER_CMD_LEN];
    char   cmd_buf[FLASHER_CMD_LEN];

    /* Navigation tracking (no view_dispatcher_get_current_view in public API) */
    FlasherView current_view;
    bool        flashing_active; /* true while flash worker thread is running */

    /* Views */
    View*       detect_view;
    View*       connect_view;
    View*       menu_view;
    View*       board_view;
    View*       files_view;
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

View* view_progress_alloc(FlasherApp* app);
void  view_progress_free(View* v);
void  view_progress_refresh(View* v);

View* view_terminal_alloc(FlasherApp* app);
void  view_terminal_free(View* v);
void  view_terminal_refresh(View* v);
void  view_terminal_append(FlasherApp* app, const char* str, size_t len);

View* view_result_alloc(FlasherApp* app);
void  view_result_free(View* v);
void  view_result_set(View* v, bool success, uint8_t board_index);

void flasher_uart_open(FlasherApp* app);
void flasher_uart_close(FlasherApp* app);
void flasher_uart_tx(FlasherApp* app, const uint8_t* data, size_t len);
bool flasher_uart_probe(FlasherApp* app);  /* blocking AT probe, returns true if Fox fw found */

void flasher_worker_start(FlasherApp* app);
void flasher_worker_stop(FlasherApp* app);

size_t      flasher_pin_option_count(void);
const char* flasher_pin_option_label(size_t index);
