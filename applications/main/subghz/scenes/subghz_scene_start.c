#include "../subghz_i.h"
#include "../views/subghz_view_start_grid.h"
#include <gui/modules/fox_theme.h>
#include "subghz_scene_start.h"
#include <loader/loader.h>
#include <storage/storage.h>

#include <lib/subghz/protocols/raw.h>

#define SUBGHZ_MOD_ANALYZER_FAP_PATH  EXT_PATH("apps/Sub-GHz/subghz_modulation_analyzer.fap")
#define SUBGHZ_FREQ_ANALYZER_FAP_PATH EXT_PATH("apps/Sub-GHz/subghz_frequency_analyzer.fap")
#define SUBGHZ_GDR_FAP_PATH           EXT_PATH("apps/Sub-GHz/garage_door_remote.fap")

void subghz_blank_transition_draw_cb(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
}

static void subghz_scene_start_launch_and_exit(
    SubGhz* subghz, const char* fap_path, const char* args) {
    Loader* loader = furi_record_open(RECORD_LOADER);
    loader_enqueue_launch(loader, fap_path, args, LoaderDeferredLaunchFlagNone);
    furi_record_close(RECORD_LOADER);

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

    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool has_freq_analyzer = storage_file_exists(storage, SUBGHZ_FREQ_ANALYZER_FAP_PATH);
    bool has_mod_analyzer  = storage_file_exists(storage, SUBGHZ_MOD_ANALYZER_FAP_PATH);
    bool has_gdr           = storage_file_exists(storage, SUBGHZ_GDR_FAP_PATH);
    furi_record_close(RECORD_STORAGE);

    subghz_start_grid_set_visible(subghz->start_grid, SGRID_IDX_FREQANA, has_freq_analyzer);
    subghz_start_grid_set_visible(subghz->start_grid, SGRID_IDX_MODANA,  has_mod_analyzer);
    subghz_start_grid_set_visible(subghz->start_grid, SGRID_IDX_GDR,     has_gdr);

    subghz_start_grid_set_visible(subghz->start_grid, SGRID_IDX_RADIOSETTINGS, false);
    subghz_start_grid_set_visible(subghz->start_grid, SGRID_IDX_JAMMER, false);
    subghz_start_grid_set_visible(subghz->start_grid, SGRID_IDX_TPMS, false);

    subghz_start_grid_set_callback(
        subghz->start_grid,
        subghz_scene_start_submenu_callback,
        subghz);

    {
        uint32_t focus =
            scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneStart);
        uint8_t grid_btn = 0;
        if     (focus == SubmenuIndexSaved)                grid_btn = SGRID_IDX_SAVED;
        else if(focus == SubmenuIndexReadRAW)              grid_btn = SGRID_IDX_READRAW;
        else if(focus == SubmenuIndexAddManuallyAdvanced)  grid_btn = SGRID_IDX_ADDMAN;
        else if(focus == SubmenuIndexFrequencyAnalyzer)    grid_btn = SGRID_IDX_FREQANA;
        else if(focus == SubmenuIndexModulationAnalyzer)   grid_btn = SGRID_IDX_MODANA;
        else if(focus == SubmenuIndexProtocolList)         grid_btn = SGRID_IDX_PROTOCOLS;
        else if(focus == SubmenuIndexModulationList)       grid_btn = SGRID_IDX_MODLIST;
        else if(focus == SubmenuIndexGarageDoorRemote)     grid_btn = SGRID_IDX_GDR;
        else if(focus == SubmenuIndexKeeloqKeys)           grid_btn = SGRID_IDX_KEELOQ;
        else if(focus == SubmenuIndexKeeloqBf2)            grid_btn = SGRID_IDX_KEELOQBF;
        subghz_start_grid_set_selected(subghz->start_grid, grid_btn);
    }

    if(fox_theme_is_active()) {

        view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdStartGrid);
    } else {

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
        submenu_set_selected_item(subghz->submenu, focus);
        view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdMenu);
    }
}

bool subghz_scene_start_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    if(event.type == SceneManagerEventTypeBack) {
        if(!scene_manager_previous_scene(subghz->scene_manager)) {

            scene_manager_stop(subghz->scene_manager);
            view_dispatcher_stop(subghz->view_dispatcher);
        }
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

            subghz_scene_start_launch_and_exit(
                subghz, SUBGHZ_FREQ_ANALYZER_FAP_PATH, "core:menu:freq");
            return true;
        } else if(event.event == SubmenuIndexModulationAnalyzer) {

            subghz_scene_start_launch_and_exit(
                subghz, SUBGHZ_MOD_ANALYZER_FAP_PATH, "core:menu:mod");
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

    submenu_reset(subghz->submenu);
}
