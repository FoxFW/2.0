// scenes/gdr_scene_start.c
#include "../gdr_app_i.h"
#include "../helpers/gdr_storage.h"

#include "gdr_icons.h"

#define TAG "GDRSceneStart"

typedef enum {
    SubmenuIndexGDRReceiver,
#ifdef ENABLE_DUAL_RX_SCENE
    SubmenuIndexGDRDualReceiver,
#endif
#ifdef ENABLE_SHIELD_RX_SCENE
    SubmenuIndexGDRShieldReceiver,
#endif
    SubmenuIndexGDRSaved,
    SubmenuIndexGDRReceiverConfig,
#ifdef ENABLE_SUB_DECODE_SCENE
    SubmenuIndexGDRSubDecode,
#endif
#ifdef ENABLE_TIMING_TUNER_SCENE
    SubmenuIndexGDRTimingTuner,
#endif
} SubmenuIndex;

// Forward declaration
static void gdr_scene_start_open_saved_captures(GDRApp* app);

static void gdr_scene_start_submenu_callback(void* context, uint32_t index) {
    furi_check(context);
    GDRApp* app = context;

    // Handle "Saved Captures" directly here, not via custom event
    if(index == SubmenuIndexGDRSaved) {
        gdr_scene_start_open_saved_captures(app);
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, index);
    }
}

static void gdr_scene_start_open_saved_captures(GDRApp* app) {
    FURI_LOG_I(TAG, "[1] Opening saved captures browser");
    FURI_LOG_I(TAG, "[1a] GDR_APP_FOLDER = %s", GDR_APP_FOLDER);

    // Check and create folder
    FURI_LOG_D(TAG, "[2] Opening storage");
    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(!storage) {
        FURI_LOG_E(TAG, "[2a] Failed to open storage!");
        return;
    }

    FURI_LOG_D(TAG, "[3] Checking folder exists");

    if(!storage_dir_exists(storage, GDR_APP_FOLDER)) {
        FURI_LOG_I(TAG, "[4] Creating folder");
        storage_simply_mkdir(storage, GDR_APP_FOLDER);
    }

#ifndef REMOVE_LOGS
    bool folder_ok = storage_dir_exists(storage, GDR_APP_FOLDER);
    FURI_LOG_D(TAG, "[5] Folder exists: %s", folder_ok ? "yes" : "no");
#endif

    furi_record_close(RECORD_STORAGE);
    FURI_LOG_D(TAG, "[6] Storage closed");

    // Check file_path
    FURI_LOG_D(TAG, "[7] Checking app->file_path");
    if(!app->file_path) {
        FURI_LOG_E(TAG, "[7a] app->file_path is NULL!");
        return;
    }

    // Set starting path
    FURI_LOG_D(TAG, "[8] Setting file_path");
    furi_string_set(app->file_path, GDR_APP_FOLDER);
    FURI_LOG_D(TAG, "[9] file_path set to: %s", furi_string_get_cstr(app->file_path));

    // Configure file browser
    FURI_LOG_D(TAG, "[10] Creating browser_options");
    DialogsFileBrowserOptions browser_options;

    FURI_LOG_D(TAG, "[11] Calling dialog_file_browser_set_basic_options");
    dialog_file_browser_set_basic_options(&browser_options, ".psf", &I_subghz_10px);

    FURI_LOG_D(TAG, "[12] Setting browser_options fields");
    browser_options.base_path = GDR_APP_FOLDER;
    browser_options.skip_assets = true;
    browser_options.hide_dot_files = true;

    FURI_LOG_D(TAG, "[13] Checking app->dialogs");
    FURI_LOG_D(TAG, "[13a] app->dialogs = %p", (void*)app->dialogs);

    if(!app->dialogs) {
        FURI_LOG_E(TAG, "[13b] dialogs is NULL! Trying to open...");
        app->dialogs = furi_record_open(RECORD_DIALOGS);
        if(!app->dialogs) {
            FURI_LOG_E(TAG, "[13c] Still NULL after open attempt!");
            return;
        }
        FURI_LOG_I(TAG, "[13d] dialogs opened successfully");
    }

    FURI_LOG_I(TAG, "[14] === CALLING dialog_file_browser_show ===");
    FURI_LOG_D(TAG, "[14a] dialogs=%p, file_path=%p", (void*)app->dialogs, (void*)app->file_path);

    bool file_selected =
        dialog_file_browser_show(app->dialogs, app->file_path, app->file_path, &browser_options);

    FURI_LOG_I(TAG, "[15] === RETURNED from dialog_file_browser_show ===");
    FURI_LOG_D(TAG, "[15a] file_selected = %d", file_selected);

    if(file_selected) {
        FURI_LOG_I(TAG, "[16] File selected: %s", furi_string_get_cstr(app->file_path));

        if(app->loaded_file_path) {
            FURI_LOG_D(TAG, "[17] Freeing old loaded_file_path");
            furi_string_free(app->loaded_file_path);
        }

        FURI_LOG_D(TAG, "[18] Allocating new loaded_file_path");
        app->loaded_file_path = furi_string_alloc_set(app->file_path);

        FURI_LOG_D(TAG, "[19] Navigating to SavedInfo scene");
        scene_manager_next_scene(app->scene_manager, GDRSceneSavedInfo);
    } else {
        FURI_LOG_I(TAG, "[16] File browser cancelled or empty");
    }

    FURI_LOG_I(TAG, "[20] open_saved_captures complete");
}

void gdr_scene_start_on_enter(void* context) {
    furi_check(context);
    GDRApp* app = context;

    /* Dismiss the startup loading wheel now that we're ready to show the
     * menu. This completes the seamless FAP-load → loading-wheel → menu
     * transition with no blank-screen gap. */
    if(app->startup_holder) {
        view_holder_set_view(app->startup_holder, NULL);
        view_holder_free(app->startup_holder);
        app->startup_holder = NULL;
    }
    if(app->startup_loading) {
        loading_free(app->startup_loading);
        app->startup_loading = NULL;
    }

    gdr_release_shared_radio_state(app);

    submenu_add_item(
        app->submenu,
        "Receive",
        SubmenuIndexGDRReceiver,
        gdr_scene_start_submenu_callback,
        app);

#ifdef ENABLE_DUAL_RX_SCENE
    submenu_add_item(
        app->submenu,
        "Dual RX",
        SubmenuIndexGDRDualReceiver,
        gdr_scene_start_submenu_callback,
        app);
#endif

#ifdef ENABLE_SHIELD_RX_SCENE
    submenu_add_item(
        app->submenu,
        "RollJam!",
        SubmenuIndexGDRShieldReceiver,
        gdr_scene_start_submenu_callback,
        app);
#endif

    submenu_add_item(
        app->submenu,
        "Saved Captures",
        SubmenuIndexGDRSaved,
        gdr_scene_start_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Configuration",
        SubmenuIndexGDRReceiverConfig,
        gdr_scene_start_submenu_callback,
        app);
#ifdef ENABLE_SUB_DECODE_SCENE
    submenu_add_item(
        app->submenu,
        "Sub Decode",
        SubmenuIndexGDRSubDecode,
        gdr_scene_start_submenu_callback,
        app);
#endif
#ifdef ENABLE_TIMING_TUNER_SCENE
    submenu_add_item(
        app->submenu,
        "Timing Tuner",
        SubmenuIndexGDRTimingTuner,
        gdr_scene_start_submenu_callback,
        app);
#endif

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, GDRSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, GDRViewSubmenu);
}

bool gdr_scene_start_on_event(void* context, SceneManagerEvent event) {
    furi_check(context);
    GDRApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexGDRReceiver) {
            scene_manager_next_scene(app->scene_manager, GDRSceneReceiver);
            consumed = true;
        }
#ifdef ENABLE_DUAL_RX_SCENE
        else if(event.event == SubmenuIndexGDRDualReceiver) {
            scene_manager_next_scene(app->scene_manager, GDRSceneDualReceiver);
            consumed = true;
        }
#endif
#ifdef ENABLE_SHIELD_RX_SCENE
        else if(event.event == SubmenuIndexGDRShieldReceiver) {
            scene_manager_next_scene(app->scene_manager, GDRSceneShieldReceiver);
            consumed = true;
        }
#endif
        else if(event.event == SubmenuIndexGDRReceiverConfig) {
            scene_manager_next_scene(app->scene_manager, GDRSceneReceiverConfig);
            consumed = true;
        }
#ifdef ENABLE_SUB_DECODE_SCENE
        else if(event.event == SubmenuIndexGDRSubDecode) {
            scene_manager_next_scene(app->scene_manager, GDRSceneSubDecode);
            consumed = true;
        }
#endif
#ifdef ENABLE_TIMING_TUNER_SCENE
        else if(event.event == SubmenuIndexGDRTimingTuner) {
            scene_manager_next_scene(app->scene_manager, GDRSceneTimingTuner);
            consumed = true;
        }
#endif
        scene_manager_set_scene_state(app->scene_manager, GDRSceneStart, event.event);
    }

    return consumed;
}

void gdr_scene_start_on_exit(void* context) {
    furi_check(context);
    GDRApp* app = context;
    submenu_reset(app->submenu);
}
