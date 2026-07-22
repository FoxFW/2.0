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

/* Requested as "apps_assets/fox_chameleon/logs/" - substituted here for
   "/ext/apps_data/fox_chameleon/logs", for two reasons: (1) Flipper's own
   storage.h reserves STORAGE_APP_ASSETS_PATH_PREFIX ("/assets") for
   read-only bundled resources shipped with an app, not files the app
   writes at runtime - "apps_data" is Flipper's actual convention for
   that; and (2) it keeps this new logs/ folder a sibling of
   config.txt/dumps/debug, all of which this app already keeps under
   /ext/apps_data/fox_chameleon. Flag this back to the user if a literal
   apps_assets path was actually intended. */
#define FOX_CHAMELEON_LOG_DIR "/ext/apps_data/fox_chameleon/logs"

/* Confirmed against real hardware during this project's own bring-up
   (a real Chameleon Ultra, a real GET_APP_VERSION round trip) - used as
   config.txt's default service/write_char/notify_char whenever it
   doesn't already specify its own, so a fresh install has something
   working to try without requiring manual setup first. Still the
   standard Nordic UART Service UUIDs this project assumed throughout,
   just no longer merely assumed. */
#define FOX_CHAMELEON_DEFAULT_SERVICE_UUID     "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define FOX_CHAMELEON_DEFAULT_WRITE_CHAR_UUID  "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define FOX_CHAMELEON_DEFAULT_NOTIFY_CHAR_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

#define CHAMELEON_CANDIDATE_MAX 8

typedef enum {
    FoxChameleonViewSettings,
    FoxChameleonViewMenu,
    /* The interactive "no ESP32 response" failure screen only -
       widget_add_string_multiline_element() + Retry/Skip buttons in
       action_check_esp32(). Every other status/response display in this
       app (the running command log) uses FoxChameleonViewTerminal
       instead - see app_render_log() in main.c. */
    FoxChameleonViewMessage,
    FoxChameleonViewSlotMenu,
    FoxChameleonViewCandidateMenu,
    FoxChameleonViewConnectionMenu,
    /* The "Terminal" - a custom scrollable, persistent-per-session log
       view with its own black header bar, replacing what used to be a
       plain Widget text box. See terminal_draw_cb()/terminal_input_cb()
       and app_render_log() in main.c. */
    FoxChameleonViewTerminal,
} FoxChameleonView;

/* Index 0 is a single menu slot that does double duty, per
   render_main_menu() in main.c: labelled "Connect" and running
   action_connect() while app->ble_initialized is false, or labelled
   "Connection" and opening the Connection submenu
   (FoxChameleonViewConnectionMenu) once it's true.

   Deliberately keyed on ble_initialized, NOT ble_connected - this fixes
   a real bug from an earlier revision: with the label keyed on
   ble_connected, disconnecting from a Chameleon Ultra (while the ESP32's
   BLE stack was still up) flipped item 0 back to "Connect", and pressing
   it re-ran the whole Connect flow - including re-trying the just-saved
   MAC - which reconnected straight back to the device the user had just
   disconnected from. ble_initialized instead tracks "has BLEINIT
   succeeded on this ESP32 session" and only goes false again via
   Disconnect BLE in the Connection submenu below, so item 0 stays
   "Connection" (never auto-reconnecting) the whole time BLE is up,
   whether or not a specific Chameleon Ultra is currently linked.

   Everything below item 0 (Get firmware version ... Dump slot to SD) is
   now only shown while ble_connected is true - those all require an
   actual linked Chameleon Ultra, so render_main_menu() hides them
   entirely rather than leaving them visible-but-guaranteed-to-fail. Only
   item 0 and MenuIndexDisconnectAndQuit remain while disconnected.

   MenuIndexDisconnectAndQuit is always the last item: a full teardown
   (BLEDISC + freeing the esp_at/UART session) followed by exiting the
   app back to the Flipper's app list - distinct from anything in the
   Connection submenu, which only ever resets BLE state, never quits. */
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

/* Connection submenu, reached via the "Connection" item whenever
   ble_initialized - see action_open_connection_menu()/
   connection_submenu_callback() in main.c.

   Connect to C.U: reconnects using the MAC already saved in config.txt -
   does nothing if none is saved (logs and points at Search for C.U
   instead). If already connected to a Chameleon Ultra, this disconnects
   it first (BLEDISC) before reconnecting - real-hardware behavior for
   sending BLECONN while already connected was never confirmed, so
   disconnecting first is the safer assumption, not a verified
   requirement.

   Search for C.U: re-runs the same BLESCAN flow as the very first
   Connect - auto-connects if exactly one Chameleon Ultra answers,
   otherwise shows the selectable list. Also disconnects first if already
   connected, for the same reason as above. With only one Chameleon
   Ultra nearby, this and Connect to C.U end up doing the same thing -
   expected, not a bug, per the user's own description.

   Disconnect BLE: turns BLE off for this session - BLEDISC if currently
   connected, then clears ble_connected and ble_initialized - without
   freeing the underlying esp_at/UART session, so the ESP32 stays
   claimed and item 0 simply reverts to "Connect" on the main menu
   (reachable immediately, no need to revisit Settings or press Start
   again). Full teardown + quitting the app is a separate, more drastic
   action - see MenuIndexDisconnectAndQuit above. */
typedef enum {
    ConnectionMenuIndexConnectToCU,
    ConnectionMenuIndexSearchForCU,
    ConnectionMenuIndexDisconnectBLE,
} ConnectionMenuIndex;

/* Row indices for the Settings screen's custom View (not a Submenu -
   see settings_draw_cb()/settings_input_cb() in main.c). Up/Down moves
   app->settings_selected between these three rows; Left/Right adjusts
   the Pins/Baud value directly on whichever row is selected; OK only
   does anything on the Start row. */
typedef enum {
    SettingsIndexPins,
    SettingsIndexBaud,
    SettingsIndexStart,
} SettingsIndex;

/* One BLESCAN result whose advertised name contained "Chameleon". Kept
   as a plain fixed-size array on App, not a dynamic list - there's no
   realistic scenario with more than a handful of Chameleon Ultras
   nearby at once, and this avoids needing dynamic allocation for
   something the on-screen list can only show CHAMELEON_CANDIDATE_MAX
   of anyway. */
typedef struct {
    char mac[24];
    int rssi;
} ChameleonCandidate;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;

    /* Settings - a custom View, not a Submenu, so Left/Right can adjust
       Pins/Baud in place ("<" / ">" boxes) instead of needing OK presses
       - which, with the old Submenu, also had the side effect of
       jumping the selection back to the top of the list after each
       press. See settings_draw_cb()/settings_input_cb() in main.c. */
    View* settings_view;
    size_t settings_selected;

    Submenu* submenu;
    Submenu* slot_submenu;
    Submenu* connection_submenu;
    Widget* widget;

    /* Two-line-per-entry selectable list for choosing among multiple
       BLESCAN candidates, matching the "Favourites" list style from
       Fox File Browser (ffb.c: ffv_fav_draw_cb/ffv_fav_input_cb) - a
       real, already-working pattern from a sibling project, not a
       Submenu. Submenu items are single-line only; this is a custom
       View with its own Canvas draw callback instead, which is why it
       needs its own selection/scroll fields below rather than an
       index handed to it by the view dispatcher. */
    View* candidate_view;
    size_t candidate_selected;
    size_t candidate_scroll;

    EspAt* esp_at;

    /* Chosen on the Settings screen (or by app_probe_default_uart() at
       launch - see app_alloc() in main.c), applied once when "Start" is
       pressed or the auto-probe succeeds - see pin_options/baud_options
       in main.c. esp_at is NULL until then. */
    size_t pin_option_index;
    size_t baud_option_index;

    /* Loaded from config.txt on the SD card if present, written back to
       it (via app_save_config() in main.c) whenever a MAC is
       discovered or chosen - config.txt is no longer something the
       user is expected to hand-edit before first use. service/
       write_char/notify_char are BLE UUIDs, not GATT indices - the Fox
       ESP32 Firmware protocol selects characteristics by UUID (see
       fox_esp32_firmware's README), unlike ESP-AT's index-based
       AT+BLEGATTCWR this app used previously. */
    FuriString* chameleon_mac;
    FuriString* gatt_service_uuid;
    FuriString* gatt_write_char_uuid;
    FuriString* gatt_notify_char_uuid;

    /* Populated by action_scan_for_chameleons() in main.c whenever
       Connect can't use a saved MAC (none saved, or it stopped
       working) - see that function for the full flow. */
    ChameleonCandidate candidates[CHAMELEON_CANDIDATE_MAX];
    size_t candidate_count;

    /* esp32_detected gates the menu: it's set once action_check_esp32()
       (or the launch-time auto-probe in app_alloc()) gets a plain OK
       back for a plain AT. Until it's true, the only reachable screens
       are settings and the message view - there is no menu to fall
       back to, so navigation_callback treats Back on those as "exit the
       app" rather than "go to menu". */
    bool esp32_detected;

    /* ble_initialized: BLEINIT has succeeded on the current esp_at
       session - this is what gates the main menu's "Connect" ->
       "Connection" label switch (see MenuIndex's comment above for why
       it's this flag and not ble_connected). Cleared only by Disconnect
       BLE in the Connection submenu, or by claiming a fresh esp_at
       session in action_start().

       ble_connected: a specific Chameleon Ultra is currently linked
       (BLECONN + BLESVC + BLECHAR all confirmed). Independent of
       ble_initialized - BLE can be (and often is) initialized with no
       device currently connected, e.g. between Search for C.U finding a
       list and the user picking one. Also gates which main menu items
       render_main_menu() shows - see MenuIndex's comment above. */
    bool ble_initialized;
    bool ble_connected;
    FoxChameleonView current_view;

    /* Terminal - see app_render_log()/app_log()/app_terminal_start_session()
       in main.c. log is the full persisted-per-session transcript (all
       appended, not cleared between individual actions - only
       app_terminal_start_session() clears it, at each new
       ESP32 Firmware bring-up and each fresh Connect attempt).
       It's capped at FOX_TERMINAL_LOG_MAX_CHARS and trimmed from the
       front when exceeded, purely to bound RAM use on a Flipper - the
       SD card copy at terminal_log_path is never trimmed. */
    FuriString* log;
    FuriString* terminal_log_path;
    View* terminal_view;
    /* Index, in wrapped display lines (not raw log lines), of the
       topmost visible row - see terminal_draw_cb()'s line-wrapping in
       main.c. Reset to "show the bottom" every time app_render_log()
       runs with new content; Up/Down while viewing move it manually. */
    size_t terminal_scroll;
} App;
