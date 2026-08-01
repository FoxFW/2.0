#include "app.h"
#include "fox_chill_icons.h"
#include "fox_chill_events.h"
#include "menu_view.h"
#include "content_view.h"
#include "scroll_counter_view.h"
#include "mindful_view.h"
#include "long_wait_view.h"
#include "stats_view.h"

#include <string.h>

#define MINDFUL_TIMER_PERIOD_MS 250
#define LONG_WAIT_TIMER_PERIOD_MS 1000

static void fox_splash_done_cb(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FoxChillEventSplashDone);
}

static void mindful_timer_cb(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FoxChillEventMindfulTick);
}

static void long_wait_timer_cb(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FoxChillEventLongWaitTick);
}

void fox_chill_goto_menu(App* app) {
    furi_timer_stop(app->mindful_timer);
    furi_timer_stop(app->long_wait_timer);
    notification_message(app->notifications, &sequence_reset_rgb);
    app->current_view = FoxChillViewMenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxChillViewMenu);
}

static bool custom_event_callback(void* context, uint32_t event) {
    App* app = context;

    if(event == FoxChillEventSplashDone) {
        fox_chill_goto_menu(app);
        return true;
    }

    if(event == FoxChillEventMindfulTick) {
        if(mindful_view_tick(app)) {
            fox_chill_goto_menu(app);
        }
        return true;
    }

    if(event == FoxChillEventLongWaitTick) {
        if(long_wait_view_tick(app)) {
            fox_chill_goto_menu(app);
        }
        return true;
    }

    if(event >= FoxChillEventMenuSelectBase && event < FoxChillEventMenuSelectBase + 9) {
        uint32_t index = event - FoxChillEventMenuSelectBase;
        switch(index) {
        case 0:
            content_view_show(app, ContentKindJoke);
            break;
        case 1:
            content_view_show(app, ContentKindRiddle);
            break;
        case 2:
            content_view_show(app, ContentKindFact);
            break;
        case 3:
            content_view_show(app, ContentKindStatistic);
            break;
        case 4:
            content_view_show(app, ContentKindYoMama);
            break;
        case 5:
            scroll_counter_view_show(app);
            break;
        case 6:
            furi_timer_start(app->mindful_timer, furi_ms_to_ticks(MINDFUL_TIMER_PERIOD_MS));
            mindful_view_show(app);
            break;
        case 7:
            furi_timer_start(
                app->long_wait_timer, furi_ms_to_ticks(LONG_WAIT_TIMER_PERIOD_MS));
            long_wait_view_show(app);
            break;
        case 8:
            app->current_view = FoxChillViewStats;
            view_dispatcher_switch_to_view(app->view_dispatcher, FoxChillViewStats);
            break;
        }
        return true;
    }

    return false;
}

static bool navigation_callback(void* context) {
    App* app = context;

    if(app->current_view == FoxChillViewMenu) {
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }

    if(app->current_view == FoxChillViewContent || app->current_view == FoxChillViewScrollCounter ||
       app->current_view == FoxChillViewBeMindful || app->current_view == FoxChillViewLongWait ||
       app->current_view == FoxChillViewStats) {
        fox_chill_goto_menu(app);
        return true;
    }

    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

static App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    fox_chill_save_load(app);

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_callback);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);

    app->mindful_timer = furi_timer_alloc(mindful_timer_cb, FuriTimerTypePeriodic, app);
    app->long_wait_timer = furi_timer_alloc(long_wait_timer_cb, FuriTimerTypePeriodic, app);

    app->splash = fox_splash_alloc(&I_fox_64x64, 1500, 666, fox_splash_done_cb, app);

    app->menu_view = menu_view_alloc(app);
    app->content_view = content_view_alloc(app);
    app->scroll_counter_view = scroll_counter_view_alloc(app);
    app->be_mindful_view = mindful_view_alloc(app);
    app->long_wait_view = long_wait_view_alloc(app);
    app->stats_view = stats_view_alloc(app);

    view_dispatcher_add_view(
        app->view_dispatcher, FoxChillViewSplash, fox_splash_get_view(app->splash));
    view_dispatcher_add_view(app->view_dispatcher, FoxChillViewMenu, app->menu_view);
    view_dispatcher_add_view(app->view_dispatcher, FoxChillViewContent, app->content_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FoxChillViewScrollCounter, app->scroll_counter_view);
    view_dispatcher_add_view(app->view_dispatcher, FoxChillViewBeMindful, app->be_mindful_view);
    view_dispatcher_add_view(app->view_dispatcher, FoxChillViewLongWait, app->long_wait_view);
    view_dispatcher_add_view(app->view_dispatcher, FoxChillViewStats, app->stats_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->current_view = FoxChillViewSplash;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxChillViewSplash);
    fox_splash_start(app->splash);

    return app;
}

static void app_free(App* app) {
    fox_chill_save_write(app);

    view_dispatcher_remove_view(app->view_dispatcher, FoxChillViewSplash);
    view_dispatcher_remove_view(app->view_dispatcher, FoxChillViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FoxChillViewContent);
    view_dispatcher_remove_view(app->view_dispatcher, FoxChillViewScrollCounter);
    view_dispatcher_remove_view(app->view_dispatcher, FoxChillViewBeMindful);
    view_dispatcher_remove_view(app->view_dispatcher, FoxChillViewLongWait);
    view_dispatcher_remove_view(app->view_dispatcher, FoxChillViewStats);

    fox_splash_free(app->splash);
    menu_view_free(app->menu_view);
    content_view_free(app->content_view);
    scroll_counter_view_free(app->scroll_counter_view);
    mindful_view_free(app->be_mindful_view);
    long_wait_view_free(app->long_wait_view);
    stats_view_free(app->stats_view);

    furi_timer_free(app->mindful_timer);
    furi_timer_free(app->long_wait_timer);

    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t fox_chill_main(void* p) {
    UNUSED(p);
    App* app = app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0;
}
