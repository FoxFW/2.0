#include "loader.h"
#include "loader_applications.h"
#include <dialogs/dialogs.h>
#include <flipper_application/flipper_application.h>
#include <assets_icons.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_holder.h>
#include <gui/modules/loading.h>
#include <dolphin/dolphin.h>
#include <lib/toolbox/path.h>

#define TAG "LoaderApplications"

/* See loader.c for why this only clears the stale spinner and logs, rather
 * than killing/retrying a load: the blocking storage read has no safe
 * cancellation point from another thread. */
#define LOADER_APPLICATIONS_WATCHDOG_TIMEOUT_MS 5000

struct LoaderApplications {
    FuriThread* thread;
    void (*closed_cb)(void*);
    void* context;
};

static int32_t loader_applications_thread(void* p);

LoaderApplications* loader_applications_alloc(void (*closed_cb)(void*), void* context) {
    LoaderApplications* loader_applications = malloc(sizeof(LoaderApplications));
    loader_applications->thread =
        furi_thread_alloc_ex(TAG, 768, loader_applications_thread, (void*)loader_applications);
    loader_applications->closed_cb = closed_cb;
    loader_applications->context = context;
    furi_thread_start(loader_applications->thread);
    return loader_applications;
}

void loader_applications_free(LoaderApplications* loader_applications) {
    furi_assert(loader_applications);
    furi_thread_join(loader_applications->thread);
    furi_thread_free(loader_applications->thread);
    free(loader_applications);
}

typedef struct {
    FuriString* file_path;
    DialogsApp* dialogs;
    Storage* storage;
    Loader* loader;

    Gui* gui;
    ViewHolder* view_holder;
    Loading* loading;

    FuriTimer* load_watchdog;
    uint32_t load_watchdog_started_tick;
    FuriString* load_watchdog_app_name;
    View* still_loading_view;
} LoaderApplicationsApp;

// See loader.c for the full rationale: this covers only the window of the
// blocking loader_start_with_gui_error() call, and cannot cancel it - long-
// press Back here just hands the screen back to whatever was showing
// before (typically the file browser dialog will reappear once its own
// blocking call underneath finally returns); it does not reset the device.
static void loader_applications_still_loading_draw_callback(Canvas* canvas, void* model) {
    UNUSED(model);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignTop, "App is Still Loading");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, "Please Wait OR");
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignTop, "Press and Hold Back");
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, "To Try Again");
}

static bool
    loader_applications_still_loading_input_callback(InputEvent* event, void* context) {
    LoaderApplicationsApp* app = context;
    if(event->key == InputKeyBack && event->type == InputTypeLong) {
        // Just walk away - the blocked loader_start_with_gui_error() call
        // can't be cancelled and keeps running in the background until it
        // resolves on its own; no device reset.
        FURI_LOG_W(TAG, "User exited still-loading screen (load continues in background)");
        view_holder_set_view(app->view_holder, NULL);
    }
    return true;
}

static void loader_applications_watchdog_callback(void* context) {
    LoaderApplicationsApp* app = context;
    uint32_t elapsed = furi_get_tick() - app->load_watchdog_started_tick;
    FURI_LOG_E(
        TAG,
        "Loading spinner still visible after %lums for \"%s\" - switching to still-loading "
        "screen",
        (unsigned long)elapsed,
        furi_string_get_cstr(app->load_watchdog_app_name));
    view_holder_set_view(app->view_holder, app->still_loading_view);
}

static void loader_applications_watchdog_arm(LoaderApplicationsApp* app, const char* name) {
    furi_string_set(app->load_watchdog_app_name, name ? name : "?");
    app->load_watchdog_started_tick = furi_get_tick();
    furi_timer_start(
        app->load_watchdog, furi_ms_to_ticks(LOADER_APPLICATIONS_WATCHDOG_TIMEOUT_MS));
}

static void loader_applications_watchdog_disarm(LoaderApplicationsApp* app) {
    furi_timer_stop(app->load_watchdog);
}

static LoaderApplicationsApp* loader_applications_app_alloc(void) {
    LoaderApplicationsApp* app = malloc(sizeof(LoaderApplicationsApp)); //-V799
    app->file_path = furi_string_alloc_set(EXT_PATH("apps"));
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->loader = furi_record_open(RECORD_LOADER);

    app->gui = furi_record_open(RECORD_GUI);
    app->view_holder = view_holder_alloc();
    app->loading = loading_alloc();

    view_holder_attach_to_gui(app->view_holder, app->gui);

    app->load_watchdog_app_name = furi_string_alloc();
    app->load_watchdog = furi_timer_alloc(
        loader_applications_watchdog_callback, FuriTimerTypeOnce, app);
    app->still_loading_view = view_alloc();
    view_set_draw_callback(
        app->still_loading_view, loader_applications_still_loading_draw_callback);
    view_set_input_callback(
        app->still_loading_view, loader_applications_still_loading_input_callback);
    view_set_context(app->still_loading_view, app);

    return app;
} //-V773

static void loader_applications_app_free(LoaderApplicationsApp* app) {
    furi_assert(app);

    furi_timer_free(app->load_watchdog);
    furi_string_free(app->load_watchdog_app_name);

    view_holder_set_view(app->view_holder, NULL);
    view_free(app->still_loading_view);
    view_holder_free(app->view_holder);
    loading_free(app->loading);
    furi_record_close(RECORD_GUI);

    furi_record_close(RECORD_LOADER);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(app->file_path);
    free(app);
}

static bool loader_applications_item_callback(
    FuriString* path,
    void* context,
    uint8_t** icon_ptr,
    FuriString* item_name) {
    LoaderApplicationsApp* loader_applications_app = context;
    furi_assert(loader_applications_app);
    return flipper_application_load_name_and_icon(
        path, loader_applications_app->storage, icon_ptr, item_name);
}

static bool loader_applications_select_app(LoaderApplicationsApp* loader_applications_app) {
    const DialogsFileBrowserOptions browser_options = {
        .extension = ".fap",
        .skip_assets = true,
        .icon = &I_unknown_10px,
        .hide_ext = true,
        .item_loader_callback = loader_applications_item_callback,
        .item_loader_context = loader_applications_app,
        .base_path = EXT_PATH("apps"),
    };

    return dialog_file_browser_show(
        loader_applications_app->dialogs,
        loader_applications_app->file_path,
        loader_applications_app->file_path,
        &browser_options);
}

#define APPLICATION_STOP_EVENT 1

static void loader_pubsub_callback(const void* message, void* context) {
    const LoaderEvent* event = message;
    const FuriThreadId thread_id = (FuriThreadId)context;

    if(event->type == LoaderEventTypeNoMoreAppsInQueue) {
        furi_thread_flags_set(thread_id, APPLICATION_STOP_EVENT);
    }
}

static void
    loader_applications_start_app(LoaderApplicationsApp* app, const char* name, const char* args) {
    dolphin_deed(DolphinDeedPluginStart);

    // load app
    FuriThreadId thread_id = furi_thread_get_current_id();
    FuriPubSubSubscription* subscription =
        furi_pubsub_subscribe(loader_get_pubsub(app->loader), loader_pubsub_callback, thread_id);

    loader_applications_watchdog_arm(app, name);
    LoaderStatus status = loader_start_with_gui_error(app->loader, name, args);
    loader_applications_watchdog_disarm(app);

    if(status == LoaderStatusOk) {
        furi_thread_flags_wait(APPLICATION_STOP_EVENT, FuriFlagWaitAny, FuriWaitForever);
    }

    furi_pubsub_unsubscribe(loader_get_pubsub(app->loader), subscription);
    furi_thread_flags_clear(APPLICATION_STOP_EVENT);
}

static int32_t loader_applications_thread(void* p) {
    LoaderApplications* loader_applications = p;
    LoaderApplicationsApp* app = loader_applications_app_alloc();

    // start loading animation
    view_holder_set_view(app->view_holder, loading_get_view(app->loading));

    while(loader_applications_select_app(app)) {
        loader_applications_start_app(app, furi_string_get_cstr(app->file_path), NULL);
    }

    // stop loading animation
    view_holder_set_view(app->view_holder, NULL);

    loader_applications_app_free(app);

    if(loader_applications->closed_cb) {
        loader_applications->closed_cb(loader_applications->context);
    }

    return 0;
}
