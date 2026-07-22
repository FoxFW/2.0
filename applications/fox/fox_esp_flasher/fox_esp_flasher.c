#include "fox_esp_flasher.h"
#include <string.h>
#include <furi_hal_serial_control.h>

#define TAG "FoxESPFlasher"

static int32_t probe_thread_fn(void* context) {
    FlasherApp* app = context;
    bool found = flasher_uart_probe(app);
    view_detect_set_found(app->detect_view, found);
    FlasherEvent ev = found ? FlasherEventDetectOk : FlasherEventDetectFail;
    view_dispatcher_send_custom_event(app->view_dispatcher, ev);
    return 0;
}

static void start_probe(FlasherApp* app) {
    if(app->probe_thread) {
        furi_thread_join(app->probe_thread);
        furi_thread_free(app->probe_thread);
    }
    view_detect_set_probing(app->detect_view, true);
    app->probe_thread = furi_thread_alloc_ex("FlasherProbe", 1024, probe_thread_fn, app);
    furi_thread_start(app->probe_thread);
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

static bool navigation_cb(void* context) {
    FlasherApp* app = context;
    switch(app->current_view) {
    case FlasherViewConnect:
        switch_view(app, FlasherViewDetect);
        return true;
    case FlasherViewBoard:
        switch_view(app, FlasherViewMenu);
        return true;
    case FlasherViewFiles:
        switch_view(app, FlasherViewBoard);
        return true;
    case FlasherViewResult:
        switch_view(app, FlasherViewMenu);
        return true;
    case FlasherViewProgress:
        return true; /* blocked during flash */
    case FlasherViewTerminal:
        switch_view(app, app->flashing_active ? FlasherViewProgress : FlasherViewMenu);
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
    case FlasherEventDetectOk:
        switch_view(app, FlasherViewMenu);
        return true;

    case FlasherEventDetectFail:
        return true;

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
        view_terminal_refresh(app->terminal_view);
        switch_view(app, FlasherViewTerminal);
        return true;

    case FlasherEventBoardGo:
        if(app->board_custom) {
            view_files_refresh(app->files_view);
            switch_view(app, FlasherViewFiles);
        } else {
            view_progress_refresh(app->progress_view);
            app->flashing_active = true;
            switch_view(app, FlasherViewProgress);
            flasher_worker_start(app);
        }
        return true;

    case FlasherEventFilesGo:
        view_progress_refresh(app->progress_view);
        app->flashing_active = true;
        switch_view(app, FlasherViewProgress);
        flasher_worker_start(app);
        return true;

    case FlasherEventFlashProgress:
        view_progress_refresh(app->progress_view);
        return true;

    case FlasherEventFlashDone:
        app->flashing_active = false;
        flasher_worker_stop(app);
        view_result_set(app->result_view, true, app->board_index);
        switch_view(app, FlasherViewResult);
        return true;

    case FlasherEventFlashFail:
        app->flashing_active = false;
        flasher_worker_stop(app);
        view_result_set(app->result_view, false, app->board_index);
        switch_view(app, FlasherViewResult);
        return true;

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

    default:
        if(event == 99) start_probe(app); /* retry sentinel from detect/connect views */
        return true;
    }
}

static FlasherApp* app_alloc(void) {
    FlasherApp* app = malloc(sizeof(FlasherApp));
    furi_check(app);
    memset(app, 0, sizeof(FlasherApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->storage       = furi_record_open(RECORD_STORAGE);
    app->dialogs       = furi_record_open(RECORD_DIALOGS);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->worker_state.mutex = furi_mutex_alloc(FuriMutexTypeNormal);

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
    app->progress_view = view_progress_alloc(app);
    app->terminal_view = view_terminal_alloc(app);
    app->result_view   = view_result_alloc(app);
    app->text_input    = text_input_alloc();

    view_dispatcher_add_view(app->view_dispatcher, FlasherViewDetect,   app->detect_view);
    view_dispatcher_add_view(app->view_dispatcher, FlasherViewConnect,  app->connect_view);
    view_dispatcher_add_view(app->view_dispatcher, FlasherViewMenu,     app->menu_view);
    view_dispatcher_add_view(app->view_dispatcher, FlasherViewBoard,    app->board_view);
    view_dispatcher_add_view(app->view_dispatcher, FlasherViewFiles,    app->files_view);
    view_dispatcher_add_view(app->view_dispatcher, FlasherViewProgress, app->progress_view);
    view_dispatcher_add_view(app->view_dispatcher, FlasherViewTerminal, app->terminal_view);
    view_dispatcher_add_view(app->view_dispatcher, FlasherViewResult,   app->result_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FlasherViewInput, text_input_get_view(app->text_input));

    return app;
}

static void app_free(FlasherApp* app) {
    if(app->flash_thread) flasher_worker_stop(app);

    if(app->probe_thread) {
        furi_thread_join(app->probe_thread);
        furi_thread_free(app->probe_thread);
    }

    flasher_uart_close(app);

    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewDetect);
    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewConnect);
    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewBoard);
    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewFiles);
    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewProgress);
    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewTerminal);
    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewResult);
    view_dispatcher_remove_view(app->view_dispatcher, FlasherViewInput);

    view_detect_free(app->detect_view);
    view_connect_free(app->connect_view);
    view_menu_free(app->menu_view);
    view_board_free(app->board_view);
    view_files_free(app->files_view);
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

int32_t fox_esp_flasher_app(void* p) {
    UNUSED(p);
    FlasherApp* app = app_alloc();
    flasher_uart_open(app);
    switch_view(app, FlasherViewDetect);
    start_probe(app);
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0;
}
