#pragma once

#include <furi.h>
#include <furi_hal_serial_types.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>

#include "esp_at.h"
#include "fox_splash.h"

typedef enum {
    FoxDownloaderViewSplash,
    FoxDownloaderViewMenu,
    FoxDownloaderViewMessage,
    FoxDownloaderViewTerminal,
    FoxDownloaderViewNetworkList,
    FoxDownloaderViewTextInput,
    FoxDownloaderViewStationList,
    FoxDownloaderViewConnectSettings,
    FoxDownloaderViewSavedWifiList,
    FoxDownloaderViewDownloadFound,
    FoxDownloaderViewDownloadProgress,
    FoxDownloaderViewDownloadSettings,
    FoxDownloaderViewDownloadResume,
    FoxDownloaderViewCatalogAppList,
    FoxDownloaderViewMyAppsList,
    FoxDownloaderViewGithubFileList,
    FoxDownloaderViewCatalogDisclaimer,
} FoxDownloaderView;

typedef enum {
    MenuContextMain,
    MenuContextWifi,
    MenuContextWifiConnection,
    MenuContextWifiRecon,
    MenuContextWifiAttacks,
    MenuContextWifiSavedAction,
    MenuContextCatalog,
} MenuContext;

typedef enum {
    MenuMainWifi,
    MenuMainCatalog,
    MenuMainMyApps,
    MenuMainGithub,
    MenuMainFileDownloader,
    MenuMainSettings,
    MenuMainTerminal,
    MenuMainTerminalCommand,
} MenuMainIndex;

typedef enum {
    TextInputPurposeNone,
    TextInputPurposePassword,
    TextInputPurposeBeaconSsids,
    TextInputPurposeTerminalCommand,
    TextInputPurposeSavedWifiEditPassword,
    TextInputPurposeHttpDownloadUrl,
    TextInputPurposeGithubRepo,
} TextInputPurpose;

#define FOX_WIFI_NETWORK_MAX 24
#define FOX_WIFI_SSID_MAX    33

typedef struct {
    char ssid[FOX_WIFI_SSID_MAX];
    int rssi;
    bool secure;
    bool saved;
    int scan_index;
} FoxWifiNetwork;

#define FOX_SAVED_WIFI_MAX 8
#define FOX_WIFI_PASS_MAX  64

typedef struct {
    char ssid[FOX_WIFI_SSID_MAX];
    char password[FOX_WIFI_PASS_MAX];
} FoxSavedWifi;

#define FOX_STATION_MAX     16
#define FOX_STATION_MAC_MAX 18

typedef struct {
    char mac[FOX_STATION_MAC_MAX];
    int rssi;
} FoxStation;

#define FOX_TEXT_INPUT_BUFFER_MAX 320

#define FOX_DOWNLOAD_DIR      "/ext/downloads"
#define FOX_DOWNLOAD_DATA_DIR "/ext/apps_data/fox_file_downloader"
#define FOX_DOWNLOAD_BAUD     115200U
#define FOX_DOWNLOAD_NAME_MAX 64
#define FOX_DOWNLOAD_TYPE_MAX 48
#define FOX_DOWNLOAD_ERR_MAX  64
#define FOX_DOWNLOAD_PATH_MAX 160

// How long a single connect attempt (the [DOWNLOAD/START] wait) is allowed
// to take before giving up - shared between url_download.c (the actual
// wait) and download_progress_view.c (which fills the Connecting screen's
// bar based on how much of this budget has elapsed, since there's nothing
// else - no bytes yet - to show real progress with).
#define FOX_DOWNLOAD_CONNECT_TIMEOUT_MS 28000

#define FOX_APPS_DIR "/ext/apps"

typedef struct {
    uint32_t baud;
    uint8_t retry_attempts;
    uint16_t timeout_sec;
    bool auto_lower_baud;
} DownloaderSettings;

typedef enum {
    // A plain URL download connects once and starts streaming into the
    // .download file immediately - there's no separate "check" connection
    // that gets torn down and reopened. app->download_confirmed tracks
    // whether the user has actually opted in yet (via the Install button
    // on the merged Connecting/Downloading progress screen): false right
    // up until Install is pressed, which is also the point a resume
    // marker first gets written. Backing out or cancelling before that
    // point just deletes whatever got downloaded so far, since it was
    // never something the user asked to keep - see download_pending_cancel
    // and the DownloadPurposeFile branch in main.c's WORKER_DONE handler.
    // Catalog installs and GitHub file downloads skip this entirely -
    // download_found_confirm() marks them confirmed immediately, since
    // they only ever make one connection to begin with.
    DownloadPurposeFile,
    DownloadPurposeCatalogPage,
    DownloadPurposeCatalogInstall,
    DownloadPurposeGithubRepoInfo,
    DownloadPurposeGithubTree,
    DownloadPurposeGithubFile,
} DownloadPurpose;

typedef enum {
    CatalogCategoryBluetooth,
    CatalogCategoryGames,
    CatalogCategoryGPIO,
    CatalogCategoryInfrared,
    CatalogCategoryIButton,
    CatalogCategoryMedia,
    CatalogCategoryNFC,
    CatalogCategoryRFID,
    CatalogCategorySubGHz,
    CatalogCategoryTools,
    CatalogCategoryUSB,
    CatalogCategoryCount,
} CatalogCategory;

#define FOX_CATALOG_APP_MAX  25
#define FOX_CATALOG_NAME_MAX 40
#define FOX_CATALOG_DESC_MAX 96
#define FOX_CATALOG_ID_MAX   32
#define FOX_CATALOG_VER_MAX  16

typedef struct {
    char alias[FOX_CATALOG_ID_MAX];
    char name[FOX_CATALOG_NAME_MAX];
    char description[FOX_CATALOG_DESC_MAX];
    char version[FOX_CATALOG_VER_MAX];
    char build_id[FOX_CATALOG_ID_MAX];
} CatalogAppEntry;

#define FOX_MYAPPS_MAX      64
#define FOX_MYAPPS_PATH_MAX 160
#define FOX_MYAPPS_LABEL_MAX 48

typedef struct {
    char path[FOX_MYAPPS_PATH_MAX];
    char label[FOX_MYAPPS_LABEL_MAX];
} MyAppEntry;

#define FOX_GITHUB_FILE_MAX  32
#define FOX_GITHUB_PATH_MAX  160
#define FOX_GITHUB_OWNER_MAX 40
#define FOX_GITHUB_REPO_MAX  60

typedef struct {
    char path[FOX_GITHUB_PATH_MAX];
} GithubFileEntry;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;

    FoxSplash* splash;

    Submenu* submenu;
    TextInput* text_input;

    EspAt* esp_at;
    size_t pin_option_index;
    size_t baud_option_index;

    bool esp32_detected;
    bool launch_wifi_connection;
    FoxDownloaderView current_view;

    bool message_view_detecting;
    bool     message_view_not_detected_focus_left;
    bool     message_view_serial_busy;
    bool     message_view_serial_retrying;
    bool     message_view_serial_retry_failed;
    bool     message_view_portal_running;
    uint8_t  serial_busy_countdown;
    FuriTimer* serial_busy_timer;
    FuriTimer* serial_retry_timer;
    View* message_view;

    MenuContext menu_context;
    MenuContext menu_return_context;

    bool wifi_menu_connected;
    uint32_t main_menu_selected;

    FuriString* log;
    View* terminal_view;
    size_t terminal_scroll;

    View* network_list_view;
    FoxWifiNetwork networks[FOX_WIFI_NETWORK_MAX];
    size_t network_count;
    size_t network_selected;
    size_t network_scroll;
    bool network_list_for_connect;
    FuriString* pending_ssid;

    View* station_list_view;
    FoxStation stations[FOX_STATION_MAX];
    size_t station_count;
    size_t station_selected;
    size_t station_scroll;
    bool has_target_station;
    char target_station_mac[FOX_STATION_MAC_MAX];

    TextInputPurpose text_input_purpose;
    char text_input_buffer[FOX_TEXT_INPUT_BUFFER_MAX];

    bool expert_mode;

    View* connect_settings_view;
    uint8_t connect_settings_selected;

    View* saved_wifi_list_view;
    FoxSavedWifi saved_wifi[FOX_SAVED_WIFI_MAX];
    size_t saved_wifi_count;
    size_t saved_wifi_selected;
    size_t saved_wifi_scroll;

    DownloaderSettings download_settings;
    uint8_t download_settings_selected;
    View* download_settings_view;

    DownloadPurpose download_purpose;
    FoxDownloaderView download_return_view;
    // See the DownloadPurposeFile comment above. Plain UI-thread bools,
    // not volatile, since Flipper's ViewDispatcher processes input and
    // custom (worker-done) events on the same thread one at a time - only
    // download_connected/download_connect_attempt_tick below cross into
    // the worker thread and need volatile.
    bool download_confirmed;
    volatile bool download_connected;
    volatile uint32_t download_connect_attempt_tick;

    char download_url[FOX_TEXT_INPUT_BUFFER_MAX];
    char download_found_name[FOX_DOWNLOAD_NAME_MAX];
    char download_found_type[FOX_DOWNLOAD_TYPE_MAX];
    uint32_t download_found_size;
    bool download_found_type_suspicious;
    bool download_found_focus_left;
    View* download_found_view;

    View* download_progress_view;
    FuriTimer* download_progress_timer;
    FuriThread* download_worker;
    volatile bool download_worker_running;
    volatile bool download_cancel_requested;
    // Tick download_cancel_requested was set - lets the progress view
    // tell "cancel is instant" apart from "cancel is taking a while", so
    // it can show a "Waiting for ESP32" popup only once it's actually
    // been more than a second with no visible progress.
    volatile uint32_t download_cancel_requested_tick;
    FuriMutex* download_progress_mutex;
    volatile uint32_t download_progress_bytes;
    volatile uint32_t download_progress_total;
    // 1-based current attempt / total attempts, so the progress screen can
    // show "attempt 2/3" instead of looking frozen at 0% while a stalled
    // connection is silently being retried in the background.
    volatile uint8_t download_progress_attempt;
    volatile uint8_t download_progress_max_attempts;
    volatile bool download_progress_done;
    volatile bool download_progress_ok;
    char download_progress_error[FOX_DOWNLOAD_ERR_MAX];
    char download_path[FOX_DOWNLOAD_PATH_MAX];

    char download_resume_url[FOX_TEXT_INPUT_BUFFER_MAX];
    char download_resume_name[FOX_DOWNLOAD_NAME_MAX];
    char download_resume_path[FOX_DOWNLOAD_PATH_MAX];
    bool download_resume_focus_left;
    View* download_resume_view;

    bool catalog_disclaimer_shown;
    bool catalog_disclaimer_is_mismatch;
    char catalog_disclaimer_text[128];
    View* catalog_disclaimer_view;
    View* catalog_app_list_view;
    CatalogCategory catalog_category;
    CatalogAppEntry* catalog_apps;
    size_t catalog_app_count;
    size_t catalog_selected;
    size_t catalog_page_offset;
    bool catalog_has_more;
    bool catalog_page_nav_backward;

    View* my_apps_list_view;
    MyAppEntry* my_apps;
    size_t my_apps_count;
    size_t my_apps_selected;
    size_t my_apps_scroll;
    bool my_apps_confirm_delete;

    char github_owner[FOX_GITHUB_OWNER_MAX];
    char github_repo[FOX_GITHUB_REPO_MAX];
    View* github_file_list_view;
    GithubFileEntry* github_files;
    size_t github_file_count;
    size_t github_file_selected;
    size_t github_file_scroll;
    bool github_tree_truncated;
} App;

void app_log(App* app, const char* fmt, ...);
void app_log_raw(App* app, const char* text);
void app_render_log(App* app);
bool app_expect_line(App* app, const char* expected, uint32_t timeout_ms);
void app_switch_to_menu(App* app, MenuContext ctx);
void app_show_text_input(App* app, const char* header, TextInputPurpose purpose);
void app_show_text_input_prefill(
    App* app,
    const char* header,
    TextInputPurpose purpose,
    const char* prefill);

void app_menu_item_callback(void* context, uint32_t index);

size_t app_pin_option_count(void);
const char* app_pin_option_label(size_t index);
size_t app_baud_option_count(void);
uint32_t app_baud_option_value(size_t index);

bool app_probe_uart_selected(App* app);

void app_retry_detection(App* app);

void app_start_download(App* app);
