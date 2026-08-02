#include "fox_esp32_flasher.h"
#include <string.h>
#include <stdlib.h>
#include <furi_hal_serial_control.h>

#include "fox_esp32_flasher_icons.h"
#include <gui/icon_i.h>

#define TAG "FoxESP32Flasher"

void flasher_draw_ok_button(
    Canvas* canvas, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t radius, const char* label) {
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

static void cmd_result_cb(void* context) {
    FlasherApp* app = context;
    if(app->cmd_buf[0]) {
        snprintf(app->last_cmd, sizeof(app->last_cmd), "%s", app->cmd_buf);
        size_t len = strlen(app->cmd_buf);
        flasher_uart_tx(app, (uint8_t*)app->cmd_buf, len);
        flasher_uart_tx(app, (uint8_t*)"\r\n", 2);
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, FlasherEventCmdSent);
}

static void switch_view(FlasherApp* app, FlasherView v) {
    app->current_view = v;
    view_dispatcher_switch_to_view(app->view_dispatcher, v);
}

void flasher_switch_view(FlasherApp* app, FlasherView v) {
    switch_view(app, v);
}

static void terminal_back(FlasherApp* app) {
    if(app->flash_done_pending) {
        app->flash_done_pending = false;
        view_result_set(app->result_view, app->last_result_success, app->board_index);
        switch_view(app, FlasherViewResult);
        furi_timer_start(app->result_dwell_timer, furi_ms_to_ticks(2000));
    } else {
        switch_view(app, app->flashing_active ? FlasherViewProgress : FlasherViewMenu);
    }
}

void flasher_terminal_back(FlasherApp* app) {
    terminal_back(app);
}

static bool navigation_cb(void* context) {
    FlasherApp* app = context;
    switch(app->current_view) {
    case FlasherViewBoard:
        switch_view(app, FlasherViewMenu);
        return true;
    case FlasherViewFiles:
        switch_view(app, FlasherViewBoard);
        return true;
    case FlasherViewPrepare:
        if(app->prepare_is_startup) {
            view_dispatcher_stop(app->view_dispatcher);
        } else {
            flasher_uart_resume_rx(app);
            switch_view(app, app->board_custom ? FlasherViewFiles : FlasherViewBoard);
        }
        return true;
    case FlasherViewResult:
        switch_view(app, FlasherViewMenu);
        return true;
    case FlasherViewProgress:
        if(app->flashing_active) {
            return true;
        }

        switch_view(app, FlasherViewMenu);
        return true;
    case FlasherViewTerminal:
        terminal_back(app);
        return true;
    case FlasherViewInput:
        switch_view(app, FlasherViewTerminal);
        return true;
    default:
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }
}

static bool custom_event_cb(void* context, uint32_t event) {
    FlasherApp* app = context;

    switch((FlasherEvent)event) {
    case FlasherEventMenuFirmware:
        app->board_custom = false;
        view_board_refresh(app->board_view);
        switch_view(app, FlasherViewBoard);
        return true;

    case FlasherEventMenuCustom:
        app->board_custom = true;
        app->files_selected = 0;
        view_board_refresh(app->board_view);
        switch_view(app, FlasherViewBoard);
        return true;

    case FlasherEventMenuTerminal:
        view_terminal_reset_scroll(app->terminal_view);
        view_terminal_refresh(app->terminal_view);
        switch_view(app, FlasherViewTerminal);
        return true;

    case FlasherEventBoardGo:
        if(app->board_custom) {
            view_files_refresh(app->files_view);
            switch_view(app, FlasherViewFiles);
        } else {
            flasher_uart_pause_rx(app);
            view_progress_refresh(app->progress_view);
            app->flashing_active = true;
            switch_view(app, FlasherViewProgress);
            flasher_worker_start(app);
        }
        return true;

    case FlasherEventFilesGo:
        flasher_uart_pause_rx(app);
        view_progress_refresh(app->progress_view);
        app->flashing_active = true;
        switch_view(app, FlasherViewProgress);
        flasher_worker_start(app);
        return true;

    case FlasherEventPrepareContinue:

        flasher_prepare_poll_stop(app);
        app->esp32_in_bootloader = true;
        app->prepare_is_startup  = false;
        if(app->auto_install_pending) {
            app->auto_install_pending = false;
            view_files_refresh(app->files_view);
            view_files_select_install(app->files_view);
            switch_view(app, FlasherViewFiles);
        } else {
            switch_view(app, FlasherViewMenu);
        }
        return true;

    case FlasherEventPrepareAutoDetected:

        flasher_prepare_poll_stop(app);
        app->esp32_in_bootloader = true;
        app->prepare_is_startup  = false;
        if(app->auto_install_pending) {
            app->auto_install_pending = false;
            view_files_refresh(app->files_view);
            view_files_select_install(app->files_view);
            switch_view(app, FlasherViewFiles);
        } else {
            switch_view(app, FlasherViewMenu);
        }
        return true;

    case FlasherEventPrepareGo:

        app->esp32_in_bootloader = true;
        view_progress_refresh(app->progress_view);
        app->flashing_active = true;
        switch_view(app, FlasherViewProgress);
        flasher_worker_start(app);
        return true;

    case FlasherEventPrepareCancel:

        flasher_uart_resume_rx(app);
        switch_view(app, app->board_custom ? FlasherViewFiles : FlasherViewBoard);
        return true;

    case FlasherEventBootNotDetected:

        app->flashing_active    = false;
        app->prepare_is_startup = false;
        flasher_worker_stop(app);
        flasher_uart_pause_rx(app);
        app->esp32_in_bootloader = false;
        view_prepare_set_error(app->prepare_view);
        switch_view(app, FlasherViewPrepare);
        return true;

    case FlasherEventFlashProgress:
        view_progress_refresh(app->progress_view);
        return true;

    case FlasherEventFlashDone:
    case FlasherEventFlashFail: {
        bool success = ((FlasherEvent)event == FlasherEventFlashDone);
        app->flashing_active = false;
        flasher_worker_stop(app);
        if(success) app->esp32_in_bootloader = false;

        if(app->current_view == FlasherViewTerminal) {
            static const char done_msg[]   = "\n=== Flash Complete! ===\n";
            static const char failed_msg[] = "\n=== Flash Failed ===\n";
            const char* msg = success ? done_msg : failed_msg;
            view_terminal_append(app, msg, strlen(msg));
            view_terminal_refresh(app->terminal_view);
            flasher_worker_log(msg);
            app->flash_done_pending    = true;
            app->last_result_success   = success;
        } else {
            app->last_result_success = success;
            view_result_set(app->result_view, success, app->board_index);
            switch_view(app, FlasherViewResult);
            furi_timer_start(app->result_dwell_timer, furi_ms_to_ticks(2000));
        }
        return true;
    }

    case FlasherEventTerminalUpdate:
        view_terminal_refresh(app->terminal_view);
        return true;

    case FlasherEventTerminalCmd:
        snprintf(app->cmd_buf, sizeof(app->cmd_buf), "%s", app->last_cmd);
        text_input_set_header_text(app->text_input, "Send AT command");
        text_input_set_result_callback(
            app->text_input, cmd_result_cb, app, app->cmd_buf, sizeof(app->cmd_buf), false);
        switch_view(app, FlasherViewInput);
        return true;

    case FlasherEventCmdSent:
        switch_view(app, FlasherViewTerminal);
        return true;

    case FlasherEventResultDwellDone:

        view_progress_refresh(app->progress_view);
        switch_view(app, FlasherViewProgress);
        return true;

    default:
        return true;
    }
}

static void result_dwell_timer_cb(void* context) {
    FlasherApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FlasherEventResultDwellDone);
}

static FlasherApp* app_alloc(void) {
    FlasherApp* app = malloc(sizeof(FlasherApp));
    furi_check(app);
    memset(app, 0, sizeof(FlasherApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->storage       = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(app->storage, FLASHER_DATA_DIR);
    app->dialogs       = furi_record_open(RECORD_DIALOGS);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->worker_state.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->result_dwell_timer =
        furi_timer_alloc(result_dwell_timer_cb, FuriTimerTypeOnce, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_cb);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_cb);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->detect_view   = view_detect_alloc(app);
    app->connect_view  = view_connect_alloc(app);
    app->menu_view     = view_menu_alloc(app);
    app->board_view    = view_board_alloc(app);
    app->files_view    = view_files_alloc(app);
    app->prepare_view  = view_prepare_alloc(app);
    app->progress_view = view_progress_alloc(app);
    app->terminal_view = view_terminal_alloc(app);
    app->result_view   = view_result_alloc(app);
    app->text_input    = text_input_alloc();

    view_dispatcher_add_view(app->view_dispatcher, FlasherViewDetect,   app->detect_view);
    view_dispatcher_add_view(app->view_dispatcher, FlasherViewConnect,  app->connect_view);
    view_dispatcher_add_view(app->view_dispatcher, FlasherViewMenu,     app->menu_view);
    view_dispatcher_add_view(app->view_dispatcher, FlasherViewBoard,    app->board_view);
    view_dispatcher_add_view(app->view_dispatcher, FlasherViewFiles,    app->files_view);
    view_dispatcher_add_view(app->view_dispatcher, FlasherViewPrepare,  app->prepare_view);
    view_dispatcher_add_view(app->view_dispatcher, FlasherViewProgress, app->progress_view);
    view_dispatcher_add_view(app->view_dispatcher, FlasherViewTerminal, app->terminal_view);
    view_dispatcher_add_view(app->view_dispatcher, FlasherViewResult,   app->result_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FlasherViewInput, text_input_get_view(app->text_input));

    return app;
}

static void app_free(FlasherApp* app) {
    if(app->flash_thread) flasher_worker_stop(app);
    flasher_prepare_poll_stop(app);
    furi_timer_stop(app->result_dwell_timer);
    furi_timer_free(app->result_dwell_timer);

    flasher_uart_close(app);

    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewDetect);
    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewConnect);
    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewBoard);
    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewFiles);
    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewPrepare);
    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewProgress);
    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewTerminal);
    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewResult);
    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewInput);

    view_detect_free(app->detect_view);
    view_connect_free(app->connect_view);
    view_menu_free(app->menu_view);
    view_board_free(app->board_view);
    view_files_free(app->files_view);
    view_prepare_free(app->prepare_view);
    view_progress_free(app->progress_view);
    view_terminal_free(app->terminal_view);
    view_result_free(app->result_view);
    text_input_free(app->text_input);

    view_dispatcher_free(app->view_dispatcher);
    furi_mutex_free(app->worker_state.mutex);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
}

static bool parse_auto_install_args(
    const char* args,
    uint8_t* board_index,
    char* boot_path,
    size_t boot_size,
    char* part_path,
    size_t part_size,
    char* fw_path,
    size_t fw_size) {
    if(!args) return false;
    if(strncmp(args, "AUTOINSTALL|", 12) != 0) return false;
    const char* p = args + 12;

    char* end = NULL;
    long idx = strtol(p, &end, 10);
    if(end == p || *end != '|') return false;
    if(idx < 0 || idx >= FLASHER_BOARD_COUNT) return false;
    *board_index = (uint8_t)idx;
    p = end + 1;

    const char* sep1 = strchr(p, '|');
    if(!sep1) return false;
    size_t len1 = (size_t)(sep1 - p);
    if(len1 >= boot_size) return false;
    memcpy(boot_path, p, len1);
    boot_path[len1] = '\0';
    p = sep1 + 1;

    const char* sep2 = strchr(p, '|');
    if(!sep2) return false;
    size_t len2 = (size_t)(sep2 - p);
    if(len2 >= part_size) return false;
    memcpy(part_path, p, len2);
    part_path[len2] = '\0';
    p = sep2 + 1;

    size_t len3 = strlen(p);
    if(len3 >= fw_size) return false;
    memcpy(fw_path, p, len3);
    fw_path[len3] = '\0';

    return true;
}

int32_t fox_esp32_flasher_app(void* p) {
    FlasherApp* app = app_alloc();
    flasher_uart_open(app);

    uint8_t auto_board_index = 0;
    char auto_boot[FLASHER_PATH_LEN];
    char auto_part[FLASHER_PATH_LEN];
    char auto_fw[FLASHER_PATH_LEN];

    if(parse_auto_install_args(
           (const char*)p,
           &auto_board_index,
           auto_boot,
           sizeof(auto_boot),
           auto_part,
           sizeof(auto_part),
           auto_fw,
           sizeof(auto_fw))) {
        app->board_index = auto_board_index;
        app->board_custom = true;
        snprintf(app->file_bootloader, sizeof(app->file_bootloader), "%s", auto_boot);
        snprintf(app->file_partitions, sizeof(app->file_partitions), "%s", auto_part);
        snprintf(app->file_firmware, sizeof(app->file_firmware), "%s", auto_fw);
        app->files_selected = 0x07;

        if(flasher_uart_check_bootloader(app)) {
            app->esp32_in_bootloader = true;
            app->prepare_is_startup = false;
            app->auto_install_pending = false;
            view_files_refresh(app->files_view);
            view_files_select_install(app->files_view);
            switch_view(app, FlasherViewFiles);
        } else {
            app->esp32_in_bootloader = false;
            app->prepare_is_startup = false;
            app->auto_install_pending = true;
            view_prepare_set_startup(app->prepare_view);
            switch_view(app, FlasherViewPrepare);
            flasher_prepare_poll_start(app);
        }
    } else if(flasher_uart_check_bootloader(app)) {
        app->esp32_in_bootloader = true;
        app->prepare_is_startup  = false;
        switch_view(app, FlasherViewMenu);
    } else {
        app->esp32_in_bootloader = false;
        app->prepare_is_startup  = true;
        view_prepare_set_startup(app->prepare_view);
        switch_view(app, FlasherViewPrepare);

        flasher_prepare_poll_start(app);
    }

    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0;
}
