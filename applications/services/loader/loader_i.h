#pragma once
#include <furi.h>
#include <toolbox/api_lock.h>
#include <flipper_application/flipper_application.h>

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_holder.h>
#include <gui/modules/loading.h>
#include <gui/modules/empty_screen.h>

#include <m-array.h>

#include "loader.h"
#include "loader_menu.h"
#include "loader_applications.h"
#include "loader_queue.h"

typedef struct {
    FuriString* launch_path;
    char* args;
    FuriThread* thread;
    bool insomniac;
    FlipperApplication* fap;
} LoaderAppData;

struct Loader {
    FuriPubSub* pubsub;
    FuriMessageQueue* queue;
    LoaderMenu* loader_menu;
    LoaderApplications* loader_applications;
    LoaderAppData app;

    LoaderLaunchQueue launch_queue;

    Gui* gui;
    ViewHolder* view_holder;
    Loading* loading;
    EmptyScreen* empty_screen; /* blank white cover for inbound FAP transitions */

    /* Failsafe watchdog: if the loading spinner stays on screen longer than
     * LOADER_LOAD_WATCHDOG_TIMEOUT_MS, something has gone wrong with the
     * *visibility* of the launch (e.g. the new app's viewport never took
     * over) or the load is genuinely stuck in a blocking storage call.
     * Runs on the FreeRTOS timer service thread, so it still fires even
     * while loader_srv itself is blocked inside a slow storage call.
     * It cannot cancel that blocked call - see loader.c for why - instead
     * it switches to still_loading_view, which offers a fast, controlled
     * long-press-Back reset in place of the plain spinner (which has no
     * Back handling at all) and in place of waiting on the Flipper's
     * uncontrolled hardware reset-on-long-hold fallback. */
    FuriTimer* load_watchdog;
    uint32_t load_watchdog_started_tick;
    char load_watchdog_app_name[40];
    View* still_loading_view;
};

typedef enum {
    LoaderMessageTypeStartByName,
    LoaderMessageTypeAppClosed,
    LoaderMessageTypeShowMenu,
    LoaderMessageTypeMenuClosed,
    LoaderMessageTypeApplicationsClosed,
    LoaderMessageTypeLock,
    LoaderMessageTypeUnlock,
    LoaderMessageTypeIsLocked,
    LoaderMessageTypeStartByNameDetachedWithGuiError,
    LoaderMessageTypeSignal,
    LoaderMessageTypeGetApplicationName,
    LoaderMessageTypeGetApplicationLaunchPath,
    LoaderMessageTypeEnqueueLaunch,
    LoaderMessageTypeClearLaunchQueue,
} LoaderMessageType;

typedef struct {
    const char* name;
    const char* args;
    FuriString* error_message;
} LoaderMessageStartByName;

typedef struct {
    uint32_t signal;
    void* arg;
} LoaderMessageSignal;

typedef enum {
    LoaderStatusErrorUnknown,
    LoaderStatusErrorInvalidFile,
    LoaderStatusErrorInvalidManifest,
    LoaderStatusErrorMissingImports,
    LoaderStatusErrorHWMismatch,
    LoaderStatusErrorOutdatedApp,
    LoaderStatusErrorOutOfMemory,
    LoaderStatusErrorOutdatedFirmware,
} LoaderStatusError;

typedef struct {
    LoaderStatus value;
    LoaderStatusError error;
} LoaderMessageLoaderStatusResult;

typedef struct {
    bool value;
} LoaderMessageBoolResult;

typedef struct {
    FuriApiLock api_lock;
    LoaderMessageType type;

    union {
        LoaderMessageStartByName start;
        LoaderDeferredLaunchRecord defer_start;
        LoaderMessageSignal signal;
        FuriString* application_name;
    };

    union {
        LoaderMessageLoaderStatusResult* status_value;
        LoaderMessageBoolResult* bool_value;
    };
} LoaderMessage;
