#include "app.h"
#include "downloader.h"
#include "update_meta.h"
#include "installer.h"
#include "strutil.h"

#include "fox_update_downloader_icons.h"
#include <gui/icon_i.h>

#include <string.h>
#include <stdio.h>

typedef struct {
    FuriHalSerialId serial_id;
    const char* label;
} PinOption;

static const PinOption pin_options[] = {
    {FuriHalSerialIdUsart, "13/14 (USART)"},
    {FuriHalSerialIdLpuart, "15/16 (LPUART)"},
};
#define PIN_OPTION_COUNT (sizeof(pin_options) / sizeof(pin_options[0]))

static const uint32_t baud_options[] = {UPDATER_BAUD};
#define BAUD_OPTION_DEFAULT_INDEX 0

static void start_check(UpdaterApp* app);
static void begin_check_for_flow(UpdaterApp* app, UpdaterFlow flow);

void updater_draw_ok_button(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t w,
    uint8_t h,
    uint8_t radius,
    const char* label) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, x, y, w, h, radius);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);

    const Icon* icon = &I_ButtonCenter_7x7;
    int32_t icon_gap = 3;
    int32_t group_w = icon->width + icon_gap + (int32_t)canvas_string_width(canvas, label);
    int32_t gx = x + ((int32_t)w - group_w) / 2;
    int32_t gy_icon = y + ((int32_t)h - icon->height) / 2;

    canvas_draw_icon(canvas, gx, gy_icon, icon);
    canvas_draw_str_aligned(
        canvas, gx + icon->width + icon_gap, y + h / 2, AlignLeft, AlignCenter, label);

    canvas_set_color(canvas, ColorBlack);
}

void updater_draw_ok_button_centered(
    Canvas* canvas,
    uint8_t y,
    uint8_t h,
    uint8_t radius,
    const char* label) {
    canvas_set_font(canvas, FontSecondary);
    const Icon* icon = &I_ButtonCenter_7x7;
    int32_t icon_gap = 3;
    int32_t pad = 8;
    int32_t w = icon->width + icon_gap + (int32_t)canvas_string_width(canvas, label) + pad * 2;
    if(w > 124) w = 124;
    int32_t x = (128 - w) / 2;
    updater_draw_ok_button(canvas, (uint8_t)x, y, (uint8_t)w, h, radius, label);
}

void updater_switch_to_status(
    UpdaterApp* app,
    const char* title,
    const char* line1,
    const char* line2,
    const char* btn_left,
    const char* btn_right) {
    snprintf(app->status_title, sizeof(app->status_title), "%s", title ? title : "");
    snprintf(app->status_line1, sizeof(app->status_line1), "%s", line1 ? line1 : "");
    snprintf(app->status_line2, sizeof(app->status_line2), "%s", line2 ? line2 : "");
    app->status_has_left = btn_left != NULL;
    app->status_has_right = btn_right != NULL;
    app->status_selected = 1;
    app->status_cycle_mode = false;
    snprintf(app->status_btn_left, sizeof(app->status_btn_left), "%s", btn_left ? btn_left : "");
    snprintf(
        app->status_btn_right, sizeof(app->status_btn_right), "%s", btn_right ? btn_right : "");
    app->current_view = UpdaterViewStatus;
    view_status_refresh(app->status_view);
    view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewStatus);
}

void updater_switch_to_status_cycle(
    UpdaterApp* app,
    const char* title,
    const char* line1,
    const char* line2,
    const char* const* options,
    uint8_t option_count) {
    snprintf(app->status_title, sizeof(app->status_title), "%s", title ? title : "");
    snprintf(app->status_line1, sizeof(app->status_line1), "%s", line1 ? line1 : "");
    snprintf(app->status_line2, sizeof(app->status_line2), "%s", line2 ? line2 : "");
    app->status_has_left = false;
    app->status_has_right = false;
    app->status_cycle_mode = true;
    if(option_count > 5) option_count = 5;
    app->status_cycle_count = option_count;
    app->status_cycle_selected = 0;
    for(uint8_t i = 0; i < option_count; i++) {
        app->status_cycle_options[i] = options[i];
    }
    app->current_view = UpdaterViewStatus;
    view_status_refresh(app->status_view);
    view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewStatus);
}

void updater_start_worker(UpdaterApp* app, UpdaterStage stage) {
    if(app->worker) {
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
    app->stage = stage;
    app->cancel_requested = false;
    app->worker_running = true;
    app->worker = furi_thread_alloc_ex("UpdaterWorker", 8192, updater_worker_thread, app);
    furi_thread_start(app->worker);
}

static void show_install_ready_prompt(UpdaterApp* app) {
    app->pending_action =
        (app->flow == UpdaterFlowEsp32) ? UpdaterActionConfirmEsp32Install : UpdaterActionInstall;
    updater_switch_to_status(app, "Downloaded", app->release.tag, "Install now?", "Later", "Install");
}

typedef enum {
    StatusCycleKindCachedComplete,
    StatusCycleKindPartial,
    StatusCycleKindDownloadFailed,
} StatusCycleKind;

static const char* const k_cached_options[5] = {
    "Install", "Later", "Re-Check", "Re-Download", "Delete"};
static const char* const k_partial_options[4] = {"Resume", "Later", "Re-Check", "Delete"};
static const char* const k_failed_options[3] = {"Retry", "Back", "Delete"};

static void show_cached_install_prompt(UpdaterApp* app) {
    app->status_cycle_kind = StatusCycleKindCachedComplete;
    app->pending_action =
        (app->flow == UpdaterFlowEsp32) ? UpdaterActionConfirmEsp32Install : UpdaterActionInstall;
    updater_switch_to_status_cycle(
        app, "Already Downloaded", app->release.tag, "", k_cached_options, 5);
}

static void show_partial_download_prompt(UpdaterApp* app) {
    app->status_cycle_kind = StatusCycleKindPartial;
    app->pending_action = UpdaterActionStartDownload;
    updater_switch_to_status_cycle(app, "Partial Download", app->release.tag, "", k_partial_options, 4);
}

static void show_download_failed_prompt(UpdaterApp* app, const char* error) {
    app->status_cycle_kind = StatusCycleKindDownloadFailed;
    app->pending_action = UpdaterActionStartDownload;
    updater_switch_to_status_cycle(app, "Download Failed", error, "", k_failed_options, 3);
}

void updater_handle_cached_option(UpdaterApp* app) {
    if(app->status_cycle_kind == StatusCycleKindPartial) {
        switch(app->status_cycle_selected) {
        case 0:
            updater_handle_status_confirm(app);
            return;
        case 1:
            app->pending_action = UpdaterActionNone;
            app->current_view = UpdaterViewMenu;
            view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewMenu);
            return;
        case 2:
            app->pending_action = UpdaterActionNone;
            begin_check_for_flow(app, app->flow);
            return;
        case 3:
            update_meta_delete(app->storage, app->cached_asset_path);
            app->pending_action = UpdaterActionNone;
            app->current_view = UpdaterViewMenu;
            view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewMenu);
            return;
        default:
            return;
        }
    }

    if(app->status_cycle_kind == StatusCycleKindDownloadFailed) {
        switch(app->status_cycle_selected) {
        case 0:
            updater_handle_status_confirm(app);
            return;
        case 1:
            app->pending_action = UpdaterActionNone;
            app->current_view = UpdaterViewMenu;
            view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewMenu);
            return;
        case 2:
            update_meta_delete(app->storage, app->download_path);
            app->pending_action = UpdaterActionNone;
            app->current_view = UpdaterViewMenu;
            view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewMenu);
            return;
        default:
            return;
        }
    }

    switch(app->status_cycle_selected) {
    case 0:
        updater_handle_status_confirm(app);
        return;
    case 1:
        app->pending_action = UpdaterActionNone;
        app->current_view = UpdaterViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewMenu);
        return;
    case 2:
        app->pending_action = UpdaterActionNone;
        begin_check_for_flow(app, app->flow);
        return;
    case 3:
        update_meta_delete(app->storage, app->cached_asset_path);
        app->pending_action = UpdaterActionNone;
        app->verifying_cached = false;
        start_check(app);
        return;
    case 4:
        update_meta_delete(app->storage, app->cached_asset_path);
        app->pending_action = UpdaterActionNone;
        app->current_view = UpdaterViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewMenu);
        return;
    default:
        return;
    }
}

void updater_handle_worker_done(UpdaterApp* app) {
    bool ok;
    char error[UPDATER_STR_LEN];
    furi_mutex_acquire(app->progress_mutex, FuriWaitForever);
    ok = app->progress_ok;
    snprintf(error, sizeof(error), "%s", app->progress_error);
    furi_mutex_release(app->progress_mutex);

    if(app->stage == UpdaterStageCheck) {
        bool was_verifying = app->verifying_cached;
        bool was_verifying_partial = app->verifying_partial;
        app->verifying_cached = false;
        app->verifying_partial = false;

        if(!ok) {
            if(was_verifying) {
                str_copy(app->download_path, sizeof(app->download_path), app->cached_asset_path);
                str_copy(app->release.tag, sizeof(app->release.tag), app->cached_tag);
                str_copy(app->release.commit, sizeof(app->release.commit), app->cached_commit);
                show_cached_install_prompt(app);
                return;
            }
            if(was_verifying_partial) {
                str_copy(app->download_path, sizeof(app->download_path), app->cached_asset_path);
                str_copy(app->release.tag, sizeof(app->release.tag), app->cached_tag);
                show_partial_download_prompt(app);
                return;
            }
            app->pending_action = UpdaterActionNone;
            updater_switch_to_status(app, "Check failed", error, "", NULL, "OK");
            return;
        }

        if(was_verifying) {
            if(strcmp(app->release.tag, app->cached_tag) == 0) {
                str_copy(app->download_path, sizeof(app->download_path), app->cached_asset_path);
                show_cached_install_prompt(app);
                return;
            }
            update_meta_delete(app->storage, app->cached_asset_path);
        } else if(was_verifying_partial) {
            if(strcmp(app->release.tag, app->cached_tag) == 0) {
                str_copy(app->download_path, sizeof(app->download_path), app->cached_asset_path);
                show_partial_download_prompt(app);
                return;
            }
            update_meta_delete(app->storage, app->cached_asset_path);
        }

        if(app->result == UpdaterResultUpToDate) {
            char line1[UPDATER_STR_LEN];
            snprintf(line1, sizeof(line1), "Current: %s", app->compare_current);
            app->pending_action = UpdaterActionStartDownload;
            updater_switch_to_status(app, "Up to date", line1, "", NULL, "Download Anyway");
            return;
        }
        char line1[UPDATER_STR_LEN];
        char line2[UPDATER_STR_LEN];
        str_join2(line1, sizeof(line1), "New: ", app->release.tag);
        snprintf(line2, sizeof(line2), "Current: %s", app->compare_current);
        app->pending_action = UpdaterActionStartDownload;
        updater_switch_to_status(app, "Update available", line1, line2, "Not now", "Download");
        return;
    }

    furi_timer_stop(app->progress_timer);

    if(app->stage == UpdaterStageInstall) {
        if(!ok) {
            app->pending_action = UpdaterActionNone;
            updater_switch_to_status(app, "Install failed", error, "", NULL, "OK");
        }
        return;
    }

    if(!ok) {
        show_download_failed_prompt(app, error);
        return;
    }

    show_install_ready_prompt(app);
}

void updater_handle_status_confirm(UpdaterApp* app) {
    switch(app->pending_action) {
    case UpdaterActionStartDownload:
        app->current_view = UpdaterViewProgress;
        view_progress_reset(app->progress_view);
        view_progress_refresh(app->progress_view);
        view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewProgress);
        furi_timer_start(app->progress_timer, furi_ms_to_ticks(200));
        updater_start_worker(app, UpdaterStageDownload);
        return;
    case UpdaterActionInstall:
        app->current_view = UpdaterViewProgress;
        view_progress_reset(app->progress_view);
        app->progress_phase = ProgressPhaseInstall;
        view_progress_refresh(app->progress_view);
        view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewProgress);
        furi_timer_start(app->progress_timer, furi_ms_to_ticks(200));
        updater_start_worker(app, UpdaterStageInstall);
        return;
    case UpdaterActionConfirmEsp32Install:
        app->pending_action = UpdaterActionEsp32ResetAndInstall;
        updater_switch_to_status(
            app,
            "Ready to Install",
            "This will disconnect WiFi",
            "and reboot your ESP32",
            "< Cancel",
            "Start >");
        return;
    case UpdaterActionEsp32ResetAndInstall:
        if(!app->esp_at) {
            app->esp_at = esp_at_alloc(
                pin_options[app->pin_option_index].serial_id,
                baud_options[app->baud_option_index]);
        }
        if(app->esp_at) {
            esp_at_send(app->esp_at, "[WIFI/DISCONNECT]");
            furi_delay_ms(50);
            esp_at_send(app->esp_at, "[REBOOT]");
            furi_delay_ms(50);
        }
        installer_install_esp32(app);
        return;
    default:
        app->current_view = UpdaterViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewMenu);
        return;
    }
}

void updater_handle_status_back(UpdaterApp* app) {
    if(app->pending_action == UpdaterActionEsp32ResetAndInstall) {
        show_install_ready_prompt(app);
        return;
    }
    app->pending_action = UpdaterActionNone;
    app->current_view = UpdaterViewMenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewMenu);
}

static void fox_splash_done_cb(void* context) {
    UpdaterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, UpdaterEventSplashDone);
}

static bool navigation_callback(void* context) {
    UpdaterApp* app = context;
    switch(app->current_view) {
    case UpdaterViewConnectSettings:
        app->current_view = UpdaterViewMessage;
        view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewMessage);
        return true;
    case UpdaterViewBoard:
        app->current_view = UpdaterViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewMenu);
        return true;
    case UpdaterViewStatus:
        app->current_view = UpdaterViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewMenu);
        return true;
    case UpdaterViewCheckProgress:
        furi_timer_stop(app->check_progress_timer);
        app->current_view = UpdaterViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewMenu);
        return true;
    case UpdaterViewDownloadSettings:
        app->current_view = UpdaterViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewMenu);
        return true;
    default:
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }
}

static void start_check(UpdaterApp* app) {

    if(app->esp_at) {
        esp_at_free(app->esp_at);
        app->esp_at = NULL;
    }
    app->esp_at = esp_at_alloc(
        pin_options[app->pin_option_index].serial_id, baud_options[app->baud_option_index]);
    if(!app->esp_at) {
        updater_switch_to_status(app, "Error", "Could not open UART", "", NULL, "OK");
        return;
    }
    app->check_stage_await_next = false;
    updater_set_check_stage(app, "Connecting...", 0);
    view_check_progress_reset(app->check_progress_view);
    app->current_view = UpdaterViewCheckProgress;
    view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewCheckProgress);
    furi_timer_start(app->check_progress_timer, furi_ms_to_ticks(80));
    updater_start_worker(app, UpdaterStageCheck);
}

static void begin_check_for_flow(UpdaterApp* app, UpdaterFlow flow) {
    DownloadedMeta meta;
    const char* board_folder =
        (flow == UpdaterFlowEsp32) ? k_updater_boards[app->board_index].folder : NULL;
    update_meta_find(app->storage, flow, board_folder, &meta);
    bool matches = meta.found;
    app->verifying_cached = false;
    app->verifying_partial = false;
    if(matches) {
        str_copy(app->cached_asset_path, sizeof(app->cached_asset_path), meta.asset_path);
        str_copy(app->cached_tag, sizeof(app->cached_tag), meta.tag);
        str_copy(app->cached_commit, sizeof(app->cached_commit), meta.commit);
        str_copy(app->cached_board_folder, sizeof(app->cached_board_folder), meta.board_folder);
        app->verifying_cached = true;
    } else if(updater_has_partial_download(
                  app,
                  flow,
                  app->cached_tag,
                  sizeof(app->cached_tag),
                  app->cached_asset_path,
                  sizeof(app->cached_asset_path))) {
        app->verifying_partial = true;
    }
    start_check(app);
}

static bool probe_esp32(UpdaterApp* app, size_t pin_index, size_t baud_index) {
    if(app->esp_at) {
        esp_at_free(app->esp_at);
        app->esp_at = NULL;
    }
    app->esp_at = esp_at_alloc(pin_options[pin_index].serial_id, baud_options[baud_index]);
    if(!app->esp_at) return false;

    esp_at_send(app->esp_at, "info");
    EspAtMsg msg;
    uint32_t deadline = furi_get_tick() + 1500;
    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;
        if(strcmp(msg.line, "Fox ESP32 Firmware") == 0) {
            app->pin_option_index = pin_index;
            app->baud_option_index = baud_index;
            return true;
        }
    }
    esp_at_free(app->esp_at);
    app->esp_at = NULL;
    return false;
}

static void proceed_after_detection(UpdaterApp* app) {

    app->current_view = UpdaterViewMenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewMenu);
}

static void run_detection(UpdaterApp* app) {
    app->message_view_detecting = true;
    app->message_view_not_detected_focus_left = false;
    app->current_view = UpdaterViewMessage;
    view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewMessage);
    with_view_model(app->message_view, uint8_t * _m, { UNUSED(_m); }, true);

    if(probe_esp32(app, app->pin_option_index, app->baud_option_index)) {
        proceed_after_detection(app);
    } else {
        app->message_view_detecting = false;
        with_view_model(app->message_view, uint8_t * _m, { UNUSED(_m); }, true);
    }
}

void updater_retry_detection(UpdaterApp* app) {
    run_detection(app);
}

size_t app_pin_option_count(void) {
    return PIN_OPTION_COUNT;
}

const char* app_pin_option_label(size_t index) {
    if(index >= PIN_OPTION_COUNT) index = 0;
    return pin_options[index].label;
}

size_t app_baud_option_count(void) {
    return sizeof(baud_options) / sizeof(baud_options[0]);
}

uint32_t app_baud_option_value(size_t index) {
    size_t count = app_baud_option_count();
    if(index >= count) index = 0;
    return baud_options[index];
}

bool app_probe_uart_selected(UpdaterApp* app) {
    app->message_view_detecting = true;
    app->current_view = UpdaterViewMessage;
    view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewMessage);
    with_view_model(app->message_view, uint8_t * _m, { UNUSED(_m); }, true);

    if(probe_esp32(app, app->pin_option_index, app->baud_option_index)) {
        proceed_after_detection(app);
        return true;
    }

    app->message_view_detecting = false;
    app->message_view_not_detected_focus_left = false;
    with_view_model(app->message_view, uint8_t * _m, { UNUSED(_m); }, true);
    return false;
}

static bool custom_event_callback(void* context, uint32_t event) {
    UpdaterApp* app = context;

    switch((UpdaterEvent)event) {
    case UpdaterEventSplashDone:
        run_detection(app);
        return true;
    case UpdaterEventMenuFw:
        app->flow = UpdaterFlowFirmware;
        begin_check_for_flow(app, UpdaterFlowFirmware);
        return true;
    case UpdaterEventMenuEsp32:
        app->flow = UpdaterFlowEsp32;
        app->current_view = UpdaterViewBoard;
        view_board_refresh(app->board_view);
        view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewBoard);
        return true;
    case UpdaterEventBoardGo:
        begin_check_for_flow(app, UpdaterFlowEsp32);
        return true;
    case UpdaterEventWorkerDone:
        if(app->stage == UpdaterStageCheck) {
            bool ok;
            furi_mutex_acquire(app->progress_mutex, FuriWaitForever);
            ok = app->progress_ok;
            furi_mutex_release(app->progress_mutex);
            if(ok) {
                updater_set_check_stage(app, "Done", 100);
                app->check_stage_await_next = true;
                view_check_progress_refresh(app->check_progress_view);
            } else {
                furi_timer_stop(app->check_progress_timer);
                updater_handle_worker_done(app);
            }
        } else {
            updater_handle_worker_done(app);
        }
        return true;
    case UpdaterEventCheckProgressNext:
        furi_timer_stop(app->check_progress_timer);
        app->check_stage_await_next = false;
        updater_handle_worker_done(app);
        return true;
    case UpdaterEventCachedOptionConfirm:
        updater_handle_cached_option(app);
        return true;
    case UpdaterEventMenuDownloadSettings:
        download_settings_view_reset(app);
        app->current_view = UpdaterViewDownloadSettings;
        view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewDownloadSettings);
        return true;
    case UpdaterEventStatusConfirm:
        updater_handle_status_confirm(app);
        return true;
    case UpdaterEventStatusBack:
        updater_handle_status_back(app);
        return true;
    default:
        return true;
    }
}

static UpdaterApp* app_alloc(bool skip_splash) {
    UpdaterApp* app = malloc(sizeof(UpdaterApp));
    memset(app, 0, sizeof(UpdaterApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->loader = furi_record_open(RECORD_LOADER);

    storage_simply_mkdir(app->storage, UPDATER_DATA_DIR);
    storage_simply_mkdir(app->storage, UPDATER_DATA_DIR "/downloads");
    updater_settings_load(app);

    app->progress_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->matched_asset = -1;
    app->matched_boot = -1;
    app->matched_part = -1;
    app->matched_fw = -1;

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_callback);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);

    app->baud_option_index = BAUD_OPTION_DEFAULT_INDEX;

    app->splash = fox_splash_alloc(&I_fox_64x64, 2000, 666, fox_splash_done_cb, app);
    app->message_view = view_message_alloc(app);
    app->connect_settings_view = connect_settings_view_alloc(app);
    app->download_settings_view = download_settings_view_alloc(app);
    app->menu_view = view_menu_alloc(app);
    app->board_view = view_board_alloc(app);
    app->status_view = view_status_alloc(app);
    app->progress_view = view_progress_alloc(app);
    app->check_progress_view = view_check_progress_alloc(app);

    view_dispatcher_add_view(
        app->view_dispatcher, UpdaterViewSplash, fox_splash_get_view(app->splash));
    view_dispatcher_add_view(app->view_dispatcher, UpdaterViewMessage, app->message_view);
    view_dispatcher_add_view(
        app->view_dispatcher, UpdaterViewConnectSettings, app->connect_settings_view);
    view_dispatcher_add_view(
        app->view_dispatcher, UpdaterViewDownloadSettings, app->download_settings_view);
    view_dispatcher_add_view(app->view_dispatcher, UpdaterViewMenu, app->menu_view);
    view_dispatcher_add_view(app->view_dispatcher, UpdaterViewBoard, app->board_view);
    view_dispatcher_add_view(app->view_dispatcher, UpdaterViewStatus, app->status_view);
    view_dispatcher_add_view(app->view_dispatcher, UpdaterViewProgress, app->progress_view);
    view_dispatcher_add_view(
        app->view_dispatcher, UpdaterViewCheckProgress, app->check_progress_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    if(skip_splash) {
        run_detection(app);
    } else {
        app->current_view = UpdaterViewSplash;
        view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewSplash);
        fox_splash_start(app->splash);
    }

    return app;
}

static void app_free(UpdaterApp* app) {
    if(app->worker) {
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
    }
    if(app->esp_at) esp_at_free(app->esp_at);

    view_dispatcher_remove_view(app->view_dispatcher, UpdaterViewSplash);
    view_dispatcher_remove_view(app->view_dispatcher, UpdaterViewMessage);
    view_dispatcher_remove_view(app->view_dispatcher, UpdaterViewConnectSettings);
    view_dispatcher_remove_view(app->view_dispatcher, UpdaterViewDownloadSettings);
    view_dispatcher_remove_view(app->view_dispatcher, UpdaterViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, UpdaterViewBoard);
    view_dispatcher_remove_view(app->view_dispatcher, UpdaterViewStatus);
    view_dispatcher_remove_view(app->view_dispatcher, UpdaterViewProgress);
    view_dispatcher_remove_view(app->view_dispatcher, UpdaterViewCheckProgress);

    fox_splash_free(app->splash);
    view_message_free(app->message_view);
    connect_settings_view_free(app->connect_settings_view);
    download_settings_view_free(app->download_settings_view);
    view_menu_free(app->menu_view);
    view_board_free(app->board_view);
    view_status_free(app->status_view);
    view_progress_free(app->progress_view);
    view_check_progress_free(app->check_progress_view);

    view_dispatcher_free(app->view_dispatcher);
    furi_mutex_free(app->progress_mutex);

    furi_record_close(RECORD_LOADER);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t fox_update_downloader_main(void* p) {
    bool skip_splash = (p != NULL && strcmp((const char*)p, "SKIPSPLASH") == 0);
    UpdaterApp* app = app_alloc(skip_splash);
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0;
}
