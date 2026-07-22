#include "../subghz_i.h"
#include "../views/subghz_view_start_grid.h"
#include <gui/modules/fox_theme.h>
#include "subghz_scene_start.h"
#include <dolphin/dolphin.h>
#include <loader/loader.h>
#include <storage/storage.h>

#include <lib/subghz/protocols/raw.h>

/* Uses loader_enqueue_launch() (not loader_start() — locked while any app runs).
 * Passes full FAP path — custom FAPs are not in the loader name catalog. */

#define SUBGHZ_MOD_ANALYZER_FAP_PATH  EXT_PATH("apps/Sub-GHz/subghz_modulation_analyzer.fap")
#define SUBGHZ_FREQ_ANALYZER_FAP_PATH EXT_PATH("apps/Sub-GHz/subghz_frequency_analyzer.fap")
#define SUBGHZ_GDR_FAP_PATH           EXT_PATH("apps/Sub-GHz/garage_door_remote.fap")
#define SUBGHZ_RF_JAMMER_FAP_PATH     EXT_PATH("apps/Fox/fox_rf_jammer.fap")

void subghz_blank_transition_draw_cb(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
}

static void subghz_scene_start_launch_and_exit(
    SubGhz* subghz, const char* fap_path, const char* args) {
    Loader* loader = furi_record_open(RECORD_LOADER);
    loader_enqueue_launch(loader, fap_path, args, LoaderDeferredLaunchFlagNone);
    furi_record_close(RECORD_LOADER);

    /* Cover the screen with a blank page BEFORE tearing down SubGHz's own
     * GUI presence. view_dispatcher_stop() fully detaches SubGHz's view
     * from the GUI, which briefly reveals the Desktop/Apps menu
     * underneath before the next app's own view attaches — this
     * standalone viewport (independent of the ViewDispatcher being torn
     * down) gives the screen something of ours to show during that exact
     * gap instead. Freed in subghz_free(), right before the app's thread
     * truly ends — see the struct field comment in subghz_i.h. */
    subghz->blank_transition_viewport = view_port_alloc();
    view_port_draw_callback_set(
        subghz->blank_transition_viewport, subghz_blank_transition_draw_cb, NULL);
    gui_add_view_port(subghz->gui, subghz->blank_transition_viewport, GuiLayerFullscreen);
    view_port_update(subghz->blank_transition_viewport);

    scene_manager_stop(subghz->scene_manager);
    view_dispatcher_stop(subghz->view_dispatcher);
}

void subghz_scene_start_submenu_callback(void* context, uint32_t index) {
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, index);
}

void subghz_scene_start_on_enter(void* context) {
    SubGhz* subghz = context;

    /* Dismiss the startup loading wheel now that the start grid is
     * ready to display.  SubGhz's own viewport takes over immediately. */
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

    /* Check which FAPs are installed — FA/MA/GDR are conditional */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool has_freq_analyzer = storage_file_exists(storage, SUBGHZ_FREQ_ANALYZER_FAP_PATH);
    bool has_mod_analyzer  = storage_file_exists(storage, SUBGHZ_MOD_ANALYZER_FAP_PATH);
    bool has_gdr           = storage_file_exists(storage, SUBGHZ_GDR_FAP_PATH);
    bool has_rf_jammer     = storage_file_exists(storage, SUBGHZ_RF_JAMMER_FAP_PATH);
    furi_record_close(RECORD_STORAGE);

    /* Configure the Fox-theme grid — show/hide conditional buttons */
    subghz_start_grid_set_visible(subghz->start_grid, SGRID_IDX_FREQANA, has_freq_analyzer);
    subghz_start_grid_set_visible(subghz->start_grid, SGRID_IDX_MODANA,  has_mod_analyzer);
    subghz_start_grid_set_visible(subghz->start_grid, SGRID_IDX_GDR,     has_gdr);
    subghz_start_grid_set_visible(subghz->start_grid, SGRID_IDX_JAMMER,  has_rf_jammer);

    /* Wire callback so grid button presses fire scene custom events */
    subghz_start_grid_set_callback(
        subghz->start_grid,
        subghz_scene_start_submenu_callback,
        subghz);

    /* Restore focus when returning from FA/MA or other scenes.
     * scene_manager_get_scene_state returns the SubmenuIndex value that
     * was last active — map it to the grid button index. */
    {
        uint32_t focus =
            scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneStart);
        uint8_t grid_btn = 0; /* default: Read */
        if     (focus == SubmenuIndexSaved)                grid_btn = SGRID_IDX_SAVED;
        else if(focus == SubmenuIndexReadRAW)              grid_btn = SGRID_IDX_READRAW;
        else if(focus == SubmenuIndexAddManuallyAdvanced)  grid_btn = SGRID_IDX_ADDMAN;
        else if(focus == SubmenuIndexFrequencyAnalyzer)    grid_btn = SGRID_IDX_FREQANA;
        else if(focus == SubmenuIndexModulationAnalyzer)   grid_btn = SGRID_IDX_MODANA;
        else if(focus == SubmenuIndexProtocolList)         grid_btn = SGRID_IDX_PROTOCOLS;
        else if(focus == SubmenuIndexModulationList)       grid_btn = SGRID_IDX_MODLIST;
        else if(focus == SubmenuIndexGarageDoorRemote)     grid_btn = SGRID_IDX_GDR;
        else if(focus == SubmenuIndexRFJammer)             grid_btn = SGRID_IDX_JAMMER;
        else if(focus == SubmenuIndexExtSettings)          grid_btn = SGRID_IDX_RADIOSETTINGS;
        else if(focus == SubmenuIndexKeeloqKeys)           grid_btn = SGRID_IDX_KEELOQ;
        else if(focus == SubmenuIndexKeeloqBf2)            grid_btn = SGRID_IDX_KEELOQBF;
        subghz_start_grid_set_selected(subghz->start_grid, grid_btn);
    }

    if(fox_theme_is_active()) {
        /* Fox Theme: show the custom button grid */
        view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdStartGrid);
    } else {
        /* Classic Theme: fall back to the standard vertical submenu.
         * Build the classic menu items matching the grid's SubmenuIndex values. */
        uint32_t focus =
            scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneStart);
        submenu_reset(subghz->submenu);
        submenu_add_item(subghz->submenu, "Read", SubmenuIndexRead,
            subghz_scene_start_submenu_callback, subghz);
        submenu_add_item(subghz->submenu, "Read RAW", SubmenuIndexReadRAW,
            subghz_scene_start_submenu_callback, subghz);
        submenu_add_item(subghz->submenu, "Saved", SubmenuIndexSaved,
            subghz_scene_start_submenu_callback, subghz);
        submenu_add_item(subghz->submenu, "Add Manually", SubmenuIndexAddManuallyAdvanced,
            subghz_scene_start_submenu_callback, subghz);
        if(has_freq_analyzer)
            submenu_add_item(subghz->submenu, "Freq. Analyzer", SubmenuIndexFrequencyAnalyzer,
                subghz_scene_start_submenu_callback, subghz);
        if(has_mod_analyzer)
            submenu_add_item(subghz->submenu, "Mod. Analyzer", SubmenuIndexModulationAnalyzer,
                subghz_scene_start_submenu_callback, subghz);
        submenu_add_item(subghz->submenu, "Protocols", SubmenuIndexProtocolList,
            subghz_scene_start_submenu_callback, subghz);
        submenu_add_item(subghz->submenu, "Modulations", SubmenuIndexModulationList,
            subghz_scene_start_submenu_callback, subghz);
        if(has_gdr)
            submenu_add_item(subghz->submenu, "Garage Remote", SubmenuIndexGarageDoorRemote,
                subghz_scene_start_submenu_callback, subghz);
        if(has_rf_jammer)
            submenu_add_item(subghz->submenu, "RF Jammer", SubmenuIndexRFJammer,
                subghz_scene_start_submenu_callback, subghz);
        submenu_add_item(subghz->submenu, "Radio Settings", SubmenuIndexExtSettings,
            subghz_scene_start_submenu_callback, subghz);
        submenu_set_selected_item(subghz->submenu, focus);
        view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdMenu);
    }
}

bool subghz_scene_start_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    if(event.type == SceneManagerEventTypeBack) {
        //exit app
        scene_manager_stop(subghz->scene_manager);
        view_dispatcher_stop(subghz->view_dispatcher);
        return true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexReadRAW) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneStart, SubmenuIndexReadRAW);
            subghz_rx_key_state_set(subghz, SubGhzRxKeyStateIDLE);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneReadRAW);
            return true;
        } else if(event.event == SubmenuIndexRead) {
            subghz_rx_key_state_set(subghz, SubGhzRxKeyStateIDLE);
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneStart, SubmenuIndexRead);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneReceiver);
            return true;
        } else if(event.event == SubmenuIndexSaved) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneStart, SubmenuIndexSaved);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneSaved);
            return true;
        } else if(event.event == SubmenuIndexAddManuallyAdvanced) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneStart, SubmenuIndexAddManuallyAdvanced);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneSetType);
            return true;
        } else if(event.event == SubmenuIndexFrequencyAnalyzer) {
            /* Moved to an external FAP to save firmware .text space — see
             * applications/main/subghz_frequency_analyzer/. SubGHz exits
             * cleanly and the Loader launches the FAP right after — see
             * the comment above subghz_scene_start_launch_and_exit(). The
             * "menu:freq" argument is what lets the FAP send us back to
             * this exact menu item on Back, instead of the Desktop. */
            dolphin_deed(DolphinDeedSubGhzFrequencyAnalyzer);
            subghz_scene_start_launch_and_exit(subghz, SUBGHZ_FREQ_ANALYZER_FAP_PATH, "menu:freq");
            return true;
        } else if(event.event == SubmenuIndexModulationAnalyzer) {
            /* Moved to an external FAP to save firmware .text space — see
             * applications/main/subghz_modulation_analyzer/. */
            subghz_scene_start_launch_and_exit(subghz, SUBGHZ_MOD_ANALYZER_FAP_PATH, "menu:mod");
            return true;
        } else if(event.event == SubmenuIndexProtocolList) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneStart, SubmenuIndexProtocolList);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneProtocolList);
        } else if(event.event == SubmenuIndexModulationList) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneStart, SubmenuIndexModulationList);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneModulationList);
            return true;
        } else if(event.event == SubmenuIndexGarageDoorRemote) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneStart, SubmenuIndexGarageDoorRemote);
            subghz_scene_start_launch_and_exit(subghz, SUBGHZ_GDR_FAP_PATH, NULL);
            return true;
        } else if(event.event == SubmenuIndexRFJammer) {
            /* Same pattern as FA/MA: pass "menu:jammer" so the FAP writes
             * .focus_menu on Back and enqueues SubGHz — SubGHz re-opens with
             * this item selected. Launched from Apps list (no arg) exits normally. */
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneStart, SubmenuIndexRFJammer);
            subghz_scene_start_launch_and_exit(subghz, SUBGHZ_RF_JAMMER_FAP_PATH, "menu:jammer");
            return true;
        } else if(event.event == SubmenuIndexExtSettings) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneStart, SubmenuIndexExtSettings);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneExtModuleSettings);
            return true;
        } else if(event.event == SubmenuIndexKeeloqKeys) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneStart, SubmenuIndexKeeloqKeys);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneKeeloqKeys);
            return true;
        } else if(event.event == SubmenuIndexKeeloqBf2) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneStart, SubmenuIndexKeeloqBf2);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneKeeloqBf2);
            return true;
        }
    }
    return false;
}

void subghz_scene_start_on_exit(void* context) {
    SubGhz* subghz = context;
    /* Always reset the shared submenu widget.
     * Fox Theme uses the grid view (not submenu), but any stale items from
     * a prior Classic-mode run would otherwise bleed into scenes like
     * SubGhzSceneSavedMenu that append to the same widget without resetting. */
    submenu_reset(subghz->submenu);
}
