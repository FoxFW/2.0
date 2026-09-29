#pragma once

#include "view.h"
#include "gui.h"
#include "scene_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ViewDispatcherTypeDesktop,
    ViewDispatcherTypeWindow,
    ViewDispatcherTypeFullscreen
} ViewDispatcherType;

typedef struct ViewDispatcher ViewDispatcher;

typedef bool (*ViewDispatcherCustomEventCallback)(void* context, uint32_t event);

typedef bool (*ViewDispatcherNavigationEventCallback)(void* context);

typedef void (*ViewDispatcherTickEventCallback)(void* context);

ViewDispatcher* view_dispatcher_alloc(void);

ViewDispatcher* view_dispatcher_alloc_ex(FuriEventLoop* loop);

void view_dispatcher_free(ViewDispatcher* view_dispatcher);

FURI_DEPRECATED void view_dispatcher_enable_queue(ViewDispatcher* view_dispatcher);

void view_dispatcher_send_custom_event(ViewDispatcher* view_dispatcher, uint32_t event);

void view_dispatcher_set_custom_event_callback(
    ViewDispatcher* view_dispatcher,
    ViewDispatcherCustomEventCallback callback);

void view_dispatcher_set_navigation_event_callback(
    ViewDispatcher* view_dispatcher,
    ViewDispatcherNavigationEventCallback callback);

void view_dispatcher_set_tick_event_callback(
    ViewDispatcher* view_dispatcher,
    ViewDispatcherTickEventCallback callback,
    uint32_t tick_period);

void view_dispatcher_set_event_callback_context(ViewDispatcher* view_dispatcher, void* context);

FuriEventLoop* view_dispatcher_get_event_loop(ViewDispatcher* view_dispatcher);

void view_dispatcher_run(ViewDispatcher* view_dispatcher);

void view_dispatcher_stop(ViewDispatcher* view_dispatcher);

void view_dispatcher_add_view(ViewDispatcher* view_dispatcher, uint32_t view_id, View* view);

void view_dispatcher_remove_view(ViewDispatcher* view_dispatcher, uint32_t view_id);

void view_dispatcher_switch_to_view(ViewDispatcher* view_dispatcher, uint32_t view_id);

void view_dispatcher_show_loading(ViewDispatcher* view_dispatcher);

void view_dispatcher_send_to_front(ViewDispatcher* view_dispatcher);

void view_dispatcher_send_to_back(ViewDispatcher* view_dispatcher);

void view_dispatcher_attach_to_gui(
    ViewDispatcher* view_dispatcher,
    Gui* gui,
    ViewDispatcherType type);

void view_dispatcher_detach_from_gui(ViewDispatcher* view_dispatcher);

#ifdef __cplusplus
}
#endif
