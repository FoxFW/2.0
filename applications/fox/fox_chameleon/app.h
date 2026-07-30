#pragma once

#include <furi.h>
#include <furi_hal_serial_types.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>

#include "esp_at.h"

#define FOX_CHAMELEON_CONFIG_DIR  "/ext/apps_data/fox_chameleon"
#define FOX_CHAMELEON_CONFIG_FILE "/ext/apps_data/fox_chameleon/config.txt"
#define FOX_CHAMELEON_DUMP_DIR    "/ext/apps_data/fox_chameleon/dumps"
#define FOX_CHAMELEON_DUMP_FILE \
    "/ext/apps_data/fox_chameleon/dumps/slot_dump.bin"

#define FOX_CHAMELEON_LOG_DIR "/ext/apps_data/fox_chameleon/logs"

#define FOX_CHAMELEON_DEFAULT_SERVICE_UUID     "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define FOX_CHAMELEON_DEFAULT_WRITE_CHAR_UUID  "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define FOX_CHAMELEON_DEFAULT_NOTIFY_CHAR_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

#define CHAMELEON_CANDIDATE_MAX 8

typedef enum {
    FoxChameleonViewSettings,
    FoxChameleonViewMenu,

    FoxChameleonViewMessage,
    FoxChameleonViewSlotMenu,
    FoxChameleonViewCandidateMenu,
    FoxChameleonViewConnectionMenu,

    FoxChameleonViewTerminal,
} FoxChameleonView;

typedef enum {
    MenuIndexConnect,
    MenuIndexGetVersion,
    MenuIndexGetGitVersion,
    MenuIndexGetBattery,
    MenuIndexGetSlot,
    MenuIndexGetModel,
    MenuIndexGetEnabledSlots,
    MenuIndexGetChipId,
    MenuIndexGetAddress,
    MenuIndexSelectSlot,
    MenuIndexEnterReaderMode,
    MenuIndexEnterEmulatorMode,
    MenuIndexGetDeviceMode,
    MenuIndexDetectMifareSupport,
    MenuIndexScanCard,
    MenuIndexReadSlotBlock0,
    MenuIndexReadCardWithDictionary,
    MenuIndexDumpSlotToSd,
    MenuIndexDisconnectAndQuit,
} MenuIndex;

typedef enum {
    ConnectionMenuIndexConnectToCU,
    ConnectionMenuIndexSearchForCU,
    ConnectionMenuIndexDisconnectBLE,
} ConnectionMenuIndex;

typedef enum {
    SettingsIndexPins,
    SettingsIndexBaud,
    SettingsIndexStart,
} SettingsIndex;

typedef struct {
    char mac[24];
    int rssi;
} ChameleonCandidate;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;

    View* settings_view;
    size_t settings_selected;

    Submenu* submenu;
    Submenu* slot_submenu;
    Submenu* connection_submenu;
    Widget* widget;

    View* candidate_view;
    size_t candidate_selected;
    size_t candidate_scroll;

    EspAt* esp_at;

    size_t pin_option_index;
    size_t baud_option_index;

    FuriString* chameleon_mac;
    FuriString* gatt_service_uuid;
    FuriString* gatt_write_char_uuid;
    FuriString* gatt_notify_char_uuid;

    ChameleonCandidate candidates[CHAMELEON_CANDIDATE_MAX];
    size_t candidate_count;

    bool esp32_detected;

    bool ble_initialized;
    bool ble_connected;
    FoxChameleonView current_view;

    FuriString* log;
    FuriString* terminal_log_path;
    View* terminal_view;

    size_t terminal_scroll;
} App;
