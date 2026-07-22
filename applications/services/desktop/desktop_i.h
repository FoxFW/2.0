#pragma once

#include "desktop.h"
#include "desktop_settings.h"

#include "animations/animation_manager.h"
#include "views/desktop_view_pin_timeout.h"
#include "views/desktop_view_pin_input.h"
#include "views/desktop_view_locked.h"
#include "views/desktop_view_main.h"
#include "views/desktop_view_lock_menu.h"
#include "views/desktop_view_debug.h"
#include "views/desktop_view_slideshow.h"
#include "views/desktop_view_clock_lock.h"

#include <gui/gui.h>
#include <gui/view_stack.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/popup.h>
#include <gui/scene_manager.h>

#include <loader/loader.h>
#include <notification/notification_app.h>

#define STATUS_BAR_Y_SHIFT 13

typedef enum {
    DesktopViewIdMain,
    DesktopViewIdLockMenu,
    DesktopViewIdLocked,
    DesktopViewIdDebug,
    DesktopViewIdPopup,
    DesktopViewIdPinInput,
    DesktopViewIdPinTimeout,
    DesktopViewIdSlideshow,
    DesktopViewIdClockLock,
    DesktopViewIdTotal,
} DesktopViewId;

typedef struct {
    uint8_t hour;
    uint8_t minute;
    bool format_12;
} DesktopClock;

struct Desktop {
    FuriThread* scene_thread;

    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    Popup* popup;
    DesktopLockMenuView* lock_menu;
    DesktopDebugView* debug_view;
    DesktopViewLocked* locked_view;
    DesktopMainView* main_view;
    DesktopViewPinTimeout* pin_timeout_view;
    DesktopSlideshowView* slideshow_view;
    DesktopViewPinInput* pin_input_view;
    DesktopClockLockView* clock_lock_view;

    ViewStack* main_view_stack;
    ViewStack* locked_view_stack;

    ViewPort* lock_icon_viewport;
    ViewPort* clock_viewport;
    ViewPort* stealth_mode_icon_viewport;
    ViewPort* no_sd_viewport;  // Shown when SD card is ejected mid-session

    // Fox ESP32 WiFi status icon - always visible (unlike lock_icon_viewport,
    // which toggles on/off), just draws differently connected vs not. The
    // icon itself always just reads FOX_WIFI_STATUS_PATH on the SD card
    // every update_wifi_timer tick (cheap, frequent, never contends for the
    // UART). That file is normally kept fresh by whichever Fox app the user
    // has open explicitly writing it on connect/disconnect - but nothing
    // used to notice if it went stale (e.g. the ESP32 got reflashed or
    // unplugged while no Fox app was open to report it). wifi_recheck_thread
    // is the fix for that: roughly once a minute, if nothing currently holds
    // the UART, it briefly borrows it for a real liveness probe and
    // rewrites the flag file itself. See desktop_wifi_icon_draw_callback()'s
    // and desktop_wifi_recheck_thread()'s header comments in desktop.c for
    // the full reasoning on both halves of this.
    ViewPort* wifi_icon_viewport;
    FuriTimer* update_wifi_timer;
    FuriThread* wifi_recheck_thread;
    bool wifi_connected;
    bool pending_slideshow;  // Set at boot when fox_setup needs to run before the
                              // slideshow; consumed in DesktopGlobalAfterAppFinished
                              // once fox_setup exits — no timer guessing involved.

    View* wallpaper_view;
    uint8_t* wallpaper_data;

    Loader* loader;
    Storage* storage;
    NotificationApp* notification;

    FuriPubSub* status_pubsub;
    FuriPubSub* input_events_pubsub;
    FuriPubSubSubscription* input_events_subscription;

    FuriTimer* auto_lock_timer;
    FuriTimer* update_clock_timer;

    AnimationManager* animation_manager;
    FuriSemaphore* animation_semaphore;

    DesktopClock clock;
    DesktopSettings settings;

    bool in_transition;
    bool app_running;
    bool locked;
};

void desktop_lock(Desktop* desktop);
void desktop_unlock(Desktop* desktop);
void desktop_set_stealth_mode_state(Desktop* desktop, bool enabled);
