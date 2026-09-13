#include "../subghz_i.h"
#include "../views/subghz_view_mode_picker.h"
#include <loader/loader.h>
#include <storage/storage.h>

/* Defined in subghz_scene_start.c - reused here as-is, same pattern. */
void subghz_blank_transition_draw_cb(Canvas* canvas, void* ctx);

/* Launched instead of Garage directly - probes for an external CC1101
 * module (~25KB plugin-load cost, so never done inside Garage itself) and
 * caches the answer, then relaunches Garage via CC1101_PROBE_RELAUNCH_PATH
 * (below) once it's done. See applications/fox/SubGhz_Garage_cc1101_check/
 * SubGhz_Garage_cc1101_check.c - it also runs periodically in the
 * background via Desktop as a freshness safety net, but this is the path
 * that guarantees a check right before the single most common entry point
 * into Garage/Gate/Other. */
#define CC1101_EXT_PROBE_FAP_PATH EXT_PATH("apps/Sub-GHz/subghz_garage_cc1101_check.fap")
#define CC1101_PROBE_RELAUNCH_PATH EXT_PATH("subghz/.cc1101_probe_relaunch")
#define SUBGHZ_GARAGE_FAP_PATH EXT_PATH("apps/Sub-GHz/subghz_garage.fap")
#define SUBGHZ_TPMS_FAP_PATH EXT_PATH("apps/Fox/fox_tpms.fap")
#define SUBGHZ_RF_JAMMER_FAP_PATH EXT_PATH("apps/Fox/fox_rf_jammer.fap")

static void subghz_scene_mode_picker_callback(void* context, uint32_t index) {
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, index);
}

void subghz_scene_mode_picker_on_enter(void* context) {
    SubGhz* subghz = context;

    if(subghz->startup_holder) {
        view_holder_set_view(subghz->startup_holder, NULL);
        view_holder_free(subghz->startup_holder);
        subghz->startup_holder = NULL;
    }
    if(subghz->startup_loading) {
        loading_free(subghz->startup_loading);
        subghz->startup_loading = NULL;
    }
    if(subghz->state_notifications == SubGhzNotificationStateStarting) {
        subghz->state_notifications = SubGhzNotificationStateIDLE;
    }

    uint32_t last_selected =
        scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneModePicker);
    subghz_mode_picker_set_selected(subghz->mode_picker, (uint8_t)last_selected);

    subghz_mode_picker_set_callback(
        subghz->mode_picker, subghz_scene_mode_picker_callback, subghz);

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdModePicker);
}

static void subghz_scene_mode_picker_launch_garage_and_exit(SubGhz* subghz) {
    /* Cover the screen before tearing down - same reasoning and mechanism
     * as subghz_scene_start_launch_and_exit() (subghz_scene_start.c):
     * scene_manager_stop()/view_dispatcher_stop() briefly reveal the Apps
     * menu underneath before the next app attaches. A ViewHolder loading
     * wheel here didn't survive that gap (this app's own process teardown
     * frees it before the headless CC1101 probe or Garage itself ever
     * draws anything), so this uses the same standalone, later-freed
     * ViewPort the F.A./M.A. launch path already relies on - blank is
     * fine here too, since the probe app has no GUI of its own and
     * Garage's own boot spinner takes over the instant it starts. */
    if(!subghz->blank_transition_viewport) {
        subghz->blank_transition_viewport = view_port_alloc();
        view_port_draw_callback_set(
            subghz->blank_transition_viewport, subghz_blank_transition_draw_cb, NULL);
        gui_add_view_port(subghz->gui, subghz->blank_transition_viewport, GuiLayerFullscreen);
        view_port_update(subghz->blank_transition_viewport);
    }

    /* Route through the CC1101 external-module probe first instead of
     * launching Garage directly - it caches a fresh answer, then relaunches
     * Garage itself with these same args once done (see
     * SubGhz_Garage_cc1101_check.c). Marker file (not chained args) carries
     * "frommode" through the extra hop - this codebase already found
     * chaining two args-based deferred launches back to back unreliable
     * for the FAP->SubGHz return leg (see subghz_scene_start_launch_and_
     * exit's comment in the Garage app), so this reuses the same
     * marker-file idiom that fix landed on instead of risking the same
     * failure mode here.
     *
     * Falls back to launching Garage directly if the probe .fap isn't on
     * the SD card (e.g. not yet built/copied after a firmware update) -
     * loader_enqueue_launch() itself has no return status, so an unnoticed
     * missing probe would otherwise fail completely silently and strand
     * the user back at the Apps list with no explanation. */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool probe_installed = storage_file_exists(storage, CC1101_EXT_PROBE_FAP_PATH);

    Loader* loader = furi_record_open(RECORD_LOADER);
    if(probe_installed) {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, CC1101_PROBE_RELAUNCH_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_write(f, "frommode", strlen("frommode"));
        }
        storage_file_close(f);
        storage_file_free(f);
        loader_enqueue_launch(
            loader, CC1101_EXT_PROBE_FAP_PATH, NULL, LoaderDeferredLaunchFlagNone);
    } else {
        FURI_LOG_W(
            "SubGhzSceneModePicker",
            "CC1101 probe .fap not found, launching Garage directly");
        loader_enqueue_launch(
            loader, SUBGHZ_GARAGE_FAP_PATH, "frommode", LoaderDeferredLaunchFlagNone);
    }
    furi_record_close(RECORD_LOADER);
    furi_record_close(RECORD_STORAGE);

    scene_manager_stop(subghz->scene_manager);
    view_dispatcher_stop(subghz->view_dispatcher);
}

static void subghz_scene_mode_picker_launch_jammer_and_exit(SubGhz* subghz) {
    /* Same blank-transition-cover + deferred-launch pattern as Garage/TPMS
     * above - fox_rf_jammer.fap already has its own working "menu:jammer"
     * marker protocol (no "core:" prefix, unlike TPMS/FA/MA - it hardcodes
     * that exact string when writing .focus_menu on exit), so this reuses
     * it completely unchanged, just launching it from a different screen. */
    if(!subghz->blank_transition_viewport) {
        subghz->blank_transition_viewport = view_port_alloc();
        view_port_draw_callback_set(
            subghz->blank_transition_viewport, subghz_blank_transition_draw_cb, NULL);
        gui_add_view_port(subghz->gui, subghz->blank_transition_viewport, GuiLayerFullscreen);
        view_port_update(subghz->blank_transition_viewport);
    }

    Loader* loader = furi_record_open(RECORD_LOADER);
    loader_enqueue_launch(
        loader, SUBGHZ_RF_JAMMER_FAP_PATH, "menu:jammer", LoaderDeferredLaunchFlagNone);
    furi_record_close(RECORD_LOADER);

    scene_manager_stop(subghz->scene_manager);
    view_dispatcher_stop(subghz->view_dispatcher);
}

static void subghz_scene_mode_picker_launch_tpms_and_exit(SubGhz* subghz) {
    /* Same blank-transition-cover + deferred-launch pattern as Garage above
     * and as subghz_scene_start_launch_and_exit() (subghz_scene_start.c) -
     * no CC1101 probe needed here, Fox_TPMS doesn't use the external
     * module. "core:menu:tpms" tells Fox_TPMS which marker to write on
     * Back so we reopen with this same row focused - see subghz.c. */
    if(!subghz->blank_transition_viewport) {
        subghz->blank_transition_viewport = view_port_alloc();
        view_port_draw_callback_set(
            subghz->blank_transition_viewport, subghz_blank_transition_draw_cb, NULL);
        gui_add_view_port(subghz->gui, subghz->blank_transition_viewport, GuiLayerFullscreen);
        view_port_update(subghz->blank_transition_viewport);
    }

    Loader* loader = furi_record_open(RECORD_LOADER);
    loader_enqueue_launch(
        loader, SUBGHZ_TPMS_FAP_PATH, "core:menu:tpms", LoaderDeferredLaunchFlagNone);
    furi_record_close(RECORD_LOADER);

    scene_manager_stop(subghz->scene_manager);
    view_dispatcher_stop(subghz->view_dispatcher);
}

bool subghz_scene_mode_picker_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    if(event.type == SceneManagerEventTypeBack) {
        // exit app
        scene_manager_stop(subghz->scene_manager);
        view_dispatcher_stop(subghz->view_dispatcher);
        return true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SUBGHZ_MODE_PICKER_AUTOMOTIVE) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneModePicker, event.event);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneStart);
            return true;
        } else if(event.event == SUBGHZ_MODE_PICKER_GARAGE) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneModePicker, event.event);
            subghz_scene_mode_picker_launch_garage_and_exit(subghz);
            return true;
        } else if(event.event == SUBGHZ_MODE_PICKER_JAMMER) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneModePicker, event.event);
            subghz_scene_mode_picker_launch_jammer_and_exit(subghz);
            return true;
        } else if(event.event == SUBGHZ_MODE_PICKER_TPMS) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneModePicker, event.event);
            subghz_scene_mode_picker_launch_tpms_and_exit(subghz);
            return true;
        } else if(event.event == SUBGHZ_MODE_PICKER_RADIO_SETTINGS) {
            /* Radio Settings is just a row like Automotive/Garage/TPMS, so
             * it's remembered the same way per FoxFW's usual menu-selection-
             * memory convention. Same running app instance
             * already has a live txrx, so no relaunch is needed here unlike
             * Garage - the single shared subghz_scene_radio_settings.c
             * reads/writes the same last_subghz.settings file Garage
             * does. */
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneModePicker, event.event);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneExtModuleSettings);
            return true;
        }
    }
    return false;
}

void subghz_scene_mode_picker_on_exit(void* context) {
    UNUSED(context);
}
