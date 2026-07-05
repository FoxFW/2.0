#include <furi.h>
#include <gui/scene_manager.h>
#include "../desktop_i.h"
#include "desktop_scene.h"

enum {
    DesktopSceneClockLockEventExit,
};

static void desktop_scene_clock_lock_exit_callback(void* context) {
    Desktop* desktop = context;
    view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopSceneClockLockEventExit);
}

void desktop_scene_clock_lock_on_enter(void* context) {
    Desktop* desktop = context;
    
    // Listen for the exit trigger we wrote in the view
    desktop_clock_lock_set_callback(desktop->clock_lock_view, desktop_scene_clock_lock_exit_callback, desktop);
    
    // Switch the screen to our clock
    view_dispatcher_switch_to_view(desktop->view_dispatcher, DesktopViewIdClockLock);
}

bool desktop_scene_clock_lock_on_event(void* context, SceneManagerEvent event) {
    Desktop* desktop = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DesktopSceneClockLockEventExit) {
            // Drops us back to the main desktop
            scene_manager_previous_scene(desktop->scene_manager);
            consumed = true;
        }
    }
    return consumed;
}

void desktop_scene_clock_lock_on_exit(void* context) {
    Desktop* desktop = context;
    desktop_clock_lock_set_callback(desktop->clock_lock_view, NULL, NULL);
}