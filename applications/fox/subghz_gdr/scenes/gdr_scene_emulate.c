// scenes/gdr_scene_emulate.c
#include "../gdr_app_i.h"

#ifdef ENABLE_EMULATE_FEATURE

#include "plugins/gdr_emulate_plugin.h"
#include "../helpers/gdr_storage.h"
#include "../gdr_history.h"

#include <loader/firmware_api/firmware_api.h>
#include <lib/flipper_application/plugins/plugin_manager.h>
#include <lib/flipper_application/plugins/composite_resolver.h>

#define TAG "GDRSceneEmulate"

#define EMULATE_PLUGIN_PATH APP_ASSETS_PATH("plugins/gdr_emulate_plugin.fal")


static bool host_radio_init(void* app) {
    return gdr_radio_init((GDRApp*)app);
}

static bool host_apply_protocol_registry_for_preset_data(
    void* app,
    const uint8_t* preset_data,
    size_t preset_data_size) {
    return gdr_apply_protocol_registry_for_preset_data(
        (GDRApp*)app, preset_data, preset_data_size);
}

static void host_rx_stack_suspend_for_tx(void* app) {
    gdr_rx_stack_suspend_for_tx((GDRApp*)app);
}

static bool host_ensure_view_about(void* app) {
    return gdr_ensure_view_about((GDRApp*)app);
}

static void host_idle(void* app) {
    gdr_idle((GDRApp*)app);
}

static void host_history_release_scratch(void* app) {
    GDRApp* a = (GDRApp*)app;
    if(a) gdr_selected_capture_release_scratch(a);
}

static void host_storage_delete_temp(void) {
    gdr_storage_delete_temp();
}

static void gdr_emulate_apply_pending_nav(GDRApp* app) {
    furi_check(app);

    const uint8_t nav = app->emulate_nav_pending;
    if(nav == EMULATE_NAV_NONE) {
        return;
    }

    app->emulate_nav_pending = EMULATE_NAV_NONE;

    if(nav == EMULATE_NAV_POP) {
        scene_manager_previous_scene(app->scene_manager);
    } else if(nav == EMULATE_NAV_STOP_APP) {
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
    }
}

static const GDREmulateHostApi gdr_emulate_host_api = {
    .radio_init = host_radio_init,
    .apply_protocol_registry_for_preset_data = host_apply_protocol_registry_for_preset_data,
    .rx_stack_suspend_for_tx = host_rx_stack_suspend_for_tx,
    .ensure_view_about = host_ensure_view_about,
    .idle = host_idle,
    .history_release_scratch = host_history_release_scratch,
    .storage_delete_temp = host_storage_delete_temp,
};

// -----------------------------------------------------------------------------
// Plugin load / unload
// -----------------------------------------------------------------------------
static void emulate_plugin_unload(GDRApp* app) {
    furi_check(app);

    app->emulate_plugin = NULL;

    if(app->emulate_plugin_manager) {
        plugin_manager_free(app->emulate_plugin_manager);
        app->emulate_plugin_manager = NULL;
    }

    if(app->emulate_plugin_resolver) {
        composite_api_resolver_free(app->emulate_plugin_resolver);
        app->emulate_plugin_resolver = NULL;
    }
}

static bool emulate_plugin_load(GDRApp* app) {
    furi_check(app);

    if(app->emulate_plugin) return true;

    if(app->emulate_plugin_manager || app->emulate_plugin_resolver) {
        emulate_plugin_unload(app);
    }

    CompositeApiResolver* resolver = composite_api_resolver_alloc();
    if(!resolver) {
        FURI_LOG_E(TAG, "Failed to allocate emulate plugin resolver");
        return false;
    }
    composite_api_resolver_add(resolver, firmware_api_interface);

    PluginManager* manager = plugin_manager_alloc(
        GDR_EMULATE_PLUGIN_APP_ID,
        GDR_EMULATE_PLUGIN_API_VERSION,
        composite_api_resolver_get(resolver));
    if(!manager) {
        FURI_LOG_E(TAG, "Failed to allocate emulate plugin manager");
        composite_api_resolver_free(resolver);
        return false;
    }

    PluginManagerError error = plugin_manager_load_single(manager, EMULATE_PLUGIN_PATH);
    if(error != PluginManagerErrorNone) {
        FURI_LOG_E(TAG, "Failed to load emulate plugin %s: %d", EMULATE_PLUGIN_PATH, (int)error);
        plugin_manager_free(manager);
        composite_api_resolver_free(resolver);
        return false;
    }

    const GDREmulatePlugin* plugin = plugin_manager_get_ep(manager, 0U);
    if(!plugin || !plugin->on_enter || !plugin->on_event || !plugin->on_exit ||
       !plugin->set_host_api) {
        FURI_LOG_E(TAG, "Emulate plugin entry point is invalid");
        plugin_manager_free(manager);
        composite_api_resolver_free(resolver);
        return false;
    }

    app->emulate_plugin_resolver = resolver;
    app->emulate_plugin_manager = manager;
    app->emulate_plugin = plugin;

    plugin->set_host_api(&gdr_emulate_host_api);
    return true;
}

void gdr_emulate_context_release(GDRApp* app) {
    if(!app) return;
    if(app->emulate_plugin && app->emulate_plugin->context_release) {
        app->emulate_plugin->context_release(app);
    }
    emulate_plugin_unload(app);
}

void gdr_scene_emulate_on_enter(void* context) {
    GDRApp* app = context;

    app->emulate_nav_pending = EMULATE_NAV_NONE;

    if(!emulate_plugin_load(app)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    app->emulate_plugin->on_enter(app);
    gdr_emulate_apply_pending_nav(app);
}

bool gdr_scene_emulate_on_event(void* context, SceneManagerEvent event) {
    GDRApp* app = context;

    if(!app->emulate_plugin || !app->emulate_plugin->on_event) {
        return false;
    }

    const bool consumed = app->emulate_plugin->on_event(app, event);
    gdr_emulate_apply_pending_nav(app);
    return consumed;
}

void gdr_scene_emulate_on_exit(void* context) {
    GDRApp* app = context;

    if(app->emulate_plugin && app->emulate_plugin->on_exit) {
        app->emulate_plugin->on_exit(app);
    }
    emulate_plugin_unload(app);
}

#endif // ENABLE_EMULATE_FEATURE
