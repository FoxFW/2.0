#pragma once
#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RECORD_LOADER            "loader"
#define LOADER_APPLICATIONS_NAME "Apps"

typedef struct Loader Loader;

typedef enum {
    LoaderStatusOk,
    LoaderStatusErrorAppStarted,
    LoaderStatusErrorUnknownApp,
    LoaderStatusErrorInternal,
    LoaderStatusErrorApiMismatch,
    LoaderStatusErrorApiMismatchExit,
} LoaderStatus;

typedef enum {
    LoaderEventTypeApplicationBeforeLoad,
    LoaderEventTypeApplicationLoadFailed,
    LoaderEventTypeApplicationStopped,
    LoaderEventTypeNoMoreAppsInQueue,
} LoaderEventType;

typedef struct {
    LoaderEventType type;
} LoaderEvent;

typedef enum {
    LoaderDeferredLaunchFlagNone = 0,
    LoaderDeferredLaunchFlagGui = (1 << 1),
} LoaderDeferredLaunchFlag;

LoaderStatus
    loader_start(Loader* instance, const char* name, const char* args, FuriString* error_message);

LoaderStatus loader_start_with_gui_error(Loader* loader, const char* name, const char* args);

void loader_start_detached_with_gui_error(Loader* loader, const char* name, const char* args);

bool loader_lock(Loader* instance);

void loader_unlock(Loader* instance);

bool loader_is_locked(Loader* instance);

void loader_show_menu(Loader* instance);

void loader_ensure_menu_built(Loader* instance);

void loader_release_hidden_menu(Loader* instance);

FuriPubSub* loader_get_pubsub(Loader* instance);

bool loader_signal(Loader* instance, uint32_t signal, void* arg);

bool loader_get_application_name(Loader* instance, FuriString* name);

bool loader_get_application_id(Loader* instance, FuriString* appid);

bool loader_get_application_launch_path(Loader* instance, FuriString* name);

void loader_enqueue_launch(
    Loader* instance,
    const char* name,
    const char* args,
    LoaderDeferredLaunchFlag flags);

void loader_clear_launch_queue(Loader* instance);

#ifdef __cplusplus
}
#endif
