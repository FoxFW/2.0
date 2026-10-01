#include <gui/gui.h>
#include <core/kernel.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/menu.h>
#include <gui/modules/submenu.h>
#include <assets_icons.h>
#include <applications.h>
#include <archive/helpers/archive_favorites.h>
#include <storage/storage.h>
#include <flipper_application/flipper_application.h>
#include <string.h>
#include <toolbox/api_lock.h>

#include "loader.h"
#include "loader_menu.h"
#include "loader_main_menu_pins.h"
#include "../desktop/desktop_settings.h"

#define TAG "LoaderMenu"

typedef enum {
    LoaderMenuThreadFlagShow = (1 << 0),
    LoaderMenuThreadFlagExit = (1 << 1),
} LoaderMenuThreadFlag;

#define LoaderMenuThreadFlagAny (LoaderMenuThreadFlagShow | LoaderMenuThreadFlagExit)

struct LoaderMenu {
    FuriThread* thread;
    void (*closed_cb)(void*);
    void* context;
    FuriApiLock built_lock;
    ViewDispatcher* view_dispatcher;
    const char* settings_item;
};

static const char* loader_menu_settings_return_item = NULL;

static const char loader_menu_fox_settings_marker = 0;

static int32_t loader_menu_thread(void* p);

LoaderMenu* loader_menu_alloc(void (*closed_cb)(void*), void* context) {
    LoaderMenu* loader_menu = malloc(sizeof(LoaderMenu));
    loader_menu->closed_cb = closed_cb;
    loader_menu->context = context;
    loader_menu->built_lock = api_lock_alloc_locked();
    loader_menu->thread = furi_thread_alloc_ex(TAG, 1024, loader_menu_thread, loader_menu);
    furi_thread_start(loader_menu->thread);
    api_lock_wait_unlock(loader_menu->built_lock);
    return loader_menu;
}

void loader_menu_free(LoaderMenu* loader_menu) {
    furi_assert(loader_menu);
    furi_thread_flags_set(furi_thread_get_id(loader_menu->thread), LoaderMenuThreadFlagExit);
    furi_thread_join(loader_menu->thread);
    furi_kernel_lock();
    furi_thread_free(loader_menu->thread);
    api_lock_free(loader_menu->built_lock);
    free(loader_menu);
    furi_kernel_unlock();
}

void loader_menu_show(LoaderMenu* loader_menu) {
    furi_assert(loader_menu);
    furi_thread_flags_set(furi_thread_get_id(loader_menu->thread), LoaderMenuThreadFlagShow);
}

void loader_menu_show_settings(LoaderMenu* loader_menu, const char* settings_item) {
    furi_assert(loader_menu);
    loader_menu->settings_item = settings_item;
    furi_thread_flags_set(furi_thread_get_id(loader_menu->thread), LoaderMenuThreadFlagShow);
}

const char* loader_menu_take_settings_return(void) {
    const char* settings_item = loader_menu_settings_return_item;
    loader_menu_settings_return_item = NULL;
    return settings_item;
}

bool loader_menu_settings_return_pending(void) {
    return loader_menu_settings_return_item != NULL;
}

typedef enum {
    LoaderMenuViewPrimary,
    LoaderMenuViewSettings,
} LoaderMenuView;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Menu* primary_menu;
    Submenu* settings_menu;
    MainMenuPins pins;

    char pin_labels[MAIN_MENU_PINS_MAX][MAIN_MENU_PINS_PATH_LEN];

    LoaderMenu* loader_menu;
    Loader* loader;
    FuriPubSubSubscription* loader_sub;
} LoaderMenuApp;

#define LoaderMenuCustomEventRefreshPins 0

static bool loader_menu_start(LoaderMenu* menu, const char* name) {
    Loader* loader = furi_record_open(RECORD_LOADER);
    LoaderStatus status = loader_start_with_gui_error(loader, name, NULL);
    furi_record_close(RECORD_LOADER);
    if(status == LoaderStatusOk && menu->view_dispatcher) {
        view_dispatcher_stop(menu->view_dispatcher);
    }
    return status == LoaderStatusOk;
}

static void loader_menu_apps_callback(void* context, uint32_t index) {
    LoaderMenu* menu = context;
    const char* name = FLIPPER_APPS[index].name;
    loader_menu_start(menu, name);
}

static void loader_menu_external_apps_callback(void* context, uint32_t index) {
    LoaderMenu* menu = context;
    const char* path = FLIPPER_EXTERNAL_APPS[index].name;
    loader_menu_start(menu, path);
}

static void loader_menu_pinned_callback(void* context, uint32_t index) {
    LoaderMenu* menu = context;
    const char* path = (const char*)index;
    loader_menu_start(menu, path);
}

static void loader_menu_pin_label_from_filename(const char* path, char* out, size_t out_size) {
    const char* slash = strrchr(path, '/');
    const char* base = slash ? slash + 1 : path;
    strlcpy(out, base, out_size);
    size_t len = strlen(out);
    if(len > 4 && strcmp(out + len - 4, ".fap") == 0) {
        out[len - 4] = '\0';
    }
}

static void loader_menu_pin_label(
    Storage* storage,
    const char* path,
    const char* custom_name,
    char* out,
    size_t out_size) {
    if(custom_name && custom_name[0] != '\0') {
        strlcpy(out, custom_name, out_size);
        return;
    }

    FuriString* path_str = furi_string_alloc_set_str(path);
    FuriString* name_str = furi_string_alloc();
    uint8_t icon_buf[FAP_MANIFEST_MAX_ICON_SIZE];
    uint8_t* icon_ptr = icon_buf;

    bool loaded = flipper_application_load_name_and_icon(path_str, storage, &icon_ptr, name_str);
    if(loaded && !furi_string_empty(name_str)) {
        strlcpy(out, furi_string_get_cstr(name_str), out_size);
    } else {
        loader_menu_pin_label_from_filename(path, out, out_size);
    }

    furi_string_free(path_str);
    furi_string_free(name_str);
}

static void loader_menu_applications_callback(void* context, uint32_t index) {
    UNUSED(index);
    LoaderMenu* menu = context;
    const char* name = LOADER_APPLICATIONS_NAME;
    loader_menu_start(menu, name);
}

static void
    loader_menu_settings_menu_callback(void* context, InputType input_type, uint32_t index) {
    LoaderMenu* menu = context;
    if(input_type == InputTypeShort) {
        loader_menu_settings_return_item = (const char*)index;
        if(!loader_menu_start(menu, (const char*)index)) {
            loader_menu_settings_return_item = NULL;
        }
    } else if(input_type == InputTypeLong) {
        archive_favorites_handle_setting_pin_unpin((const char*)index, NULL);
    }
}

static void loader_menu_fox_settings_callback(void* context, uint32_t index) {
    UNUSED(index);
    LoaderMenu* menu = context;
    loader_menu_settings_return_item = &loader_menu_fox_settings_marker;
    if(!loader_menu_start(menu, "/ext/apps/Fox/desktop_settings.fap")) {
        loader_menu_settings_return_item = NULL;
    }
}

static void loader_menu_switch_to_settings(void* context, uint32_t index) {
    UNUSED(index);
    LoaderMenuApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, LoaderMenuViewSettings);
}

static uint32_t loader_menu_switch_to_primary(void* context) {
    UNUSED(context);
    return LoaderMenuViewPrimary;
}

static uint32_t loader_menu_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static void loader_menu_build_menu(LoaderMenuApp* app, LoaderMenu* menu) {
    size_t i = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    for(size_t p = 0; p < app->pins.count; p++) {
        loader_menu_pin_label(
            storage,
            app->pins.paths[p],
            app->pins.names[p],
            app->pin_labels[p],
            sizeof(app->pin_labels[p]));
        menu_add_item(
            app->primary_menu,
            app->pin_labels[p],
            &A_Star_14,
            (uint32_t)app->pins.paths[p],
            loader_menu_pinned_callback,
            (void*)menu);
    }
    furi_record_close(RECORD_STORAGE);

    menu_add_item(
        app->primary_menu,
        LOADER_APPLICATIONS_NAME,
        &A_Plugins_14,
        i++,
        loader_menu_applications_callback,
        (void*)menu);

    for(i = 0; i < FLIPPER_APPS_COUNT; i++) {
        menu_add_item(
            app->primary_menu,
            FLIPPER_APPS[i].name,
            FLIPPER_APPS[i].icon,
            i,
            loader_menu_apps_callback,
            (void*)menu);
    }

    for(i = 0; i < FLIPPER_EXTERNAL_APPS_COUNT; i++) {
        menu_add_item(
            app->primary_menu,
            FLIPPER_EXTERNAL_APPS[i].name,
            FLIPPER_EXTERNAL_APPS[i].icon,
            i,
            loader_menu_external_apps_callback,
            (void*)menu);
    }

    menu_add_item(
        app->primary_menu,
        "Fox Settings",
        &A_Settings_14,
        i++,
        loader_menu_fox_settings_callback,
        (void*)menu);

    menu_add_item(
        app->primary_menu, "Settings", &A_Settings_14, i++, loader_menu_switch_to_settings, app);
}

static void loader_menu_build_submenu(LoaderMenuApp* app, LoaderMenu* loader_menu) {
    for(size_t i = 0; i < FLIPPER_EXTSETTINGS_APPS_COUNT; i++) {
        submenu_add_item_ex(
            app->settings_menu,
            FLIPPER_EXTSETTINGS_APPS[i].name,
            (uint32_t)FLIPPER_EXTSETTINGS_APPS[i].name,
            loader_menu_settings_menu_callback,
            loader_menu);
    }
    for(size_t i = 0; i < FLIPPER_SETTINGS_APPS_COUNT; i++) {
        submenu_add_item_ex(
            app->settings_menu,
            FLIPPER_SETTINGS_APPS[i].name,
            (uint32_t)FLIPPER_SETTINGS_APPS[i].name,
            loader_menu_settings_menu_callback,
            loader_menu);
    }
}

static bool loader_menu_pins_equal(const MainMenuPins* a, const MainMenuPins* b) {
    if(a->count != b->count) return false;
    for(uint8_t i = 0; i < a->count; i++) {
        if(strcmp(a->paths[i], b->paths[i]) != 0) return false;
        if(strcmp(a->names[i], b->names[i]) != 0) return false;
    }
    return true;
}

static void loader_menu_refresh_pins_if_changed(LoaderMenuApp* app) {
    MainMenuPins* new_pins = malloc(sizeof(MainMenuPins));
    if(new_pins) {
        main_menu_pins_load(new_pins);

        if(!loader_menu_pins_equal(&app->pins, new_pins)) {
            app->pins = *new_pins;
            menu_reset(app->primary_menu);
            loader_menu_build_menu(app, app->loader_menu);
        }
        free(new_pins);
    }
}

static bool loader_menu_custom_event_callback(void* context, uint32_t event) {
    LoaderMenuApp* app = context;
    if(event == LoaderMenuCustomEventRefreshPins) {
        loader_menu_refresh_pins_if_changed(app);
    }
    return true;
}

static void loader_menu_loader_pubsub_callback(const void* message, void* context) {
    LoaderMenuApp* app = context;
    const LoaderEvent* event = message;
    if(event->type == LoaderEventTypeApplicationStopped) {
        view_dispatcher_send_custom_event(app->view_dispatcher, LoaderMenuCustomEventRefreshPins);
    }
}

static LoaderMenuApp* loader_menu_app_alloc(LoaderMenu* loader_menu) {
    LoaderMenuApp* app = malloc(sizeof(LoaderMenuApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->primary_menu = menu_alloc();
    app->settings_menu = submenu_alloc();
    app->loader_menu = loader_menu;
    main_menu_pins_load(&app->pins);

    app->loader = furi_record_open(RECORD_LOADER);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, loader_menu_custom_event_callback);
    app->loader_sub = furi_pubsub_subscribe(
        loader_get_pubsub(app->loader), loader_menu_loader_pubsub_callback, app);

    DesktopSettings* settings = malloc(sizeof(DesktopSettings));
    if(settings) {
        desktop_settings_load(settings);
        menu_set_theme(app->primary_menu, settings->menu_theme);
        free(settings);
    }

    loader_menu_build_menu(app, loader_menu);
    loader_menu_build_submenu(app, loader_menu);

    View* primary_view = menu_get_view(app->primary_menu);
    view_set_context(primary_view, app->primary_menu);
    view_set_previous_callback(primary_view, loader_menu_exit);
    view_dispatcher_add_view(app->view_dispatcher, LoaderMenuViewPrimary, primary_view);

    View* settings_view = submenu_get_view(app->settings_menu);
    view_set_context(settings_view, app->settings_menu);
    view_set_previous_callback(settings_view, loader_menu_switch_to_primary);
    view_dispatcher_add_view(app->view_dispatcher, LoaderMenuViewSettings, settings_view);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoaderMenuViewPrimary);

    return app;
}

static void loader_menu_app_free(LoaderMenuApp* app) {
    furi_pubsub_unsubscribe(loader_get_pubsub(app->loader), app->loader_sub);
    furi_record_close(RECORD_LOADER);

    view_dispatcher_remove_view(app->view_dispatcher, LoaderMenuViewPrimary);
    view_dispatcher_remove_view(app->view_dispatcher, LoaderMenuViewSettings);
    view_dispatcher_free(app->view_dispatcher);


    menu_free(app->primary_menu);
    submenu_free(app->settings_menu);
    furi_record_close(RECORD_GUI);
    free(app);
}

static int32_t loader_menu_thread(void* p) {
    LoaderMenu* loader_menu = p;
    furi_assert(loader_menu);


    LoaderMenuApp* app = loader_menu_app_alloc(loader_menu);
    loader_menu->view_dispatcher = app->view_dispatcher;


    api_lock_unlock(loader_menu->built_lock);

    while(true) {
        uint32_t flags =
            furi_thread_flags_wait(LoaderMenuThreadFlagAny, FuriFlagWaitAny, FuriWaitForever);
        if(flags & LoaderMenuThreadFlagExit) {
            break;
        }


        loader_menu_refresh_pins_if_changed(app);
        const char* settings_item = loader_menu->settings_item;
        loader_menu->settings_item = NULL;
        const uint32_t fox_settings_position =
            app->pins.count + FLIPPER_APPS_COUNT + FLIPPER_EXTERNAL_APPS_COUNT + 1;
        if(settings_item == &loader_menu_fox_settings_marker) {
            menu_set_selected_item(app->primary_menu, fox_settings_position);
            view_dispatcher_switch_to_view(app->view_dispatcher, LoaderMenuViewPrimary);
        } else if(settings_item) {
            menu_set_selected_item(app->primary_menu, fox_settings_position + 1);
            submenu_set_selected_item(app->settings_menu, (uint32_t)settings_item);
            view_dispatcher_switch_to_view(app->view_dispatcher, LoaderMenuViewSettings);
        } else {
            view_dispatcher_switch_to_view(app->view_dispatcher, LoaderMenuViewPrimary);
        }
        view_dispatcher_attach_to_gui(
            app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);


        view_dispatcher_run(app->view_dispatcher);


        view_dispatcher_detach_from_gui(app->view_dispatcher);


        if(loader_menu->closed_cb) {
            loader_menu->closed_cb(loader_menu->context);
        }
    }

    loader_menu->view_dispatcher = NULL;
    loader_menu_app_free(app);


    return 0;
}
