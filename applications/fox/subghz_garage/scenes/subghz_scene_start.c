#include "../subghz_i.h"
#include "../views/subghz_view_start_grid.h"
#include "../helpers/fox_theme_compat.h"
#include "subghz_scene_start.h"
#include <loader/loader.h>
#include <storage/storage.h>
#include <furi/core/memmgr.h>

#include <lib/subghz/protocols/raw.h>

#define SUBGHZ_MOD_ANALYZER_FAP_PATH  EXT_PATH("apps/Sub-GHz/subghz_modulation_analyzer.fap")
#define SUBGHZ_FREQ_ANALYZER_FAP_PATH EXT_PATH("apps/Sub-GHz/subghz_frequency_analyzer.fap")

void subghz_blank_transition_draw_cb(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
}

void subghz_scene_start_launch_and_exit(
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
    FURI_LOG_I("SubGhzSceneStart", "on_enter");
    SubGhz* subghz = context;

    subghz_unlock_cli_sessions_after_recovery(subghz);

    subghz_txrx_release_radio(subghz->txrx);

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

    if(!subghz->shared_ram_warning_shown) {
        subghz->shared_ram_warning_shown = true;
        bool qflipper_active = rpc_gui_screen_stream_is_active();
        bool low_ram = memmgr_get_free_heap() < SUBGHZ_LOW_RAM_FREE_HEAP;
        if(qflipper_active || low_ram) {
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneSharedRamWarning);
            return;
        }
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool has_freq_analyzer = storage_file_exists(storage, SUBGHZ_FREQ_ANALYZER_FAP_PATH);
    bool has_mod_analyzer  = storage_file_exists(storage, SUBGHZ_MOD_ANALYZER_FAP_PATH);
    furi_record_close(RECORD_STORAGE);

    subghz_start_grid_set_visible(subghz->start_grid, SGRID_IDX_FREQANA, has_freq_analyzer);
    subghz_start_grid_set_visible(subghz->start_grid, SGRID_IDX_MODANA,  has_mod_analyzer);

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
        else if(focus == SubmenuIndexFrequencyAnalyzer)    grid_btn = SGRID_IDX_FREQANA;
        else if(focus == SubmenuIndexModulationAnalyzer)   grid_btn = SGRID_IDX_MODANA;
        else if(focus == SubmenuIndexProtocolList)         grid_btn = SGRID_IDX_PROTOCOLS;
        subghz_start_grid_set_selected(subghz->start_grid, grid_btn);
    }

    if(fox_theme_is_active()) {

        view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdStartGrid);
    } else {

        uint32_t focus =
            scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneStart);
        subghz_ensure_submenu(subghz);
        submenu_reset(subghz->submenu);
        submenu_add_item(subghz->submenu, "Read", SubmenuIndexRead,
            subghz_scene_start_submenu_callback, subghz);
        submenu_add_item(subghz->submenu, "Read RAW", SubmenuIndexReadRAW,
            subghz_scene_start_submenu_callback, subghz);
        submenu_add_item(subghz->submenu, "Saved", SubmenuIndexSaved,
            subghz_scene_start_submenu_callback, subghz);
        if(has_freq_analyzer)
            submenu_add_item(subghz->submenu, "Freq. Analyzer", SubmenuIndexFrequencyAnalyzer,
                subghz_scene_start_submenu_callback, subghz);
        if(has_mod_analyzer)
            submenu_add_item(subghz->submenu, "Mod. Analyzer", SubmenuIndexModulationAnalyzer,
                subghz_scene_start_submenu_callback, subghz);
        submenu_add_item(subghz->submenu, "Protocol Group", SubmenuIndexProtocolList,
            subghz_scene_start_submenu_callback, subghz);
        submenu_set_selected_item(subghz->submenu, focus);
        view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdMenu);
    }
}

bool subghz_scene_start_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    FURI_LOG_I(
        "SubGhzSceneStart", "on_event: type=%d event=%lu", (int)event.type, (unsigned long)event.event);
    if(event.type == SceneManagerEventTypeBack) {
        if(subghz->launched_from_mode_picker) {

            Storage* storage = furi_record_open(RECORD_STORAGE);
            File* f = storage_file_alloc(storage);
            if(storage_file_open(f, "/ext/subghz/.focus_menu", FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                storage_file_write(f, "menu:garage", strlen("menu:garage"));
            }
            storage_file_close(f);
            storage_file_free(f);
            furi_record_close(RECORD_STORAGE);
            subghz_scene_start_launch_and_exit(subghz, "subghz", NULL);
        } else {

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
        } else if(event.event == SubmenuIndexFrequencyAnalyzer) {

            subghz_scene_start_launch_and_exit(
                subghz, SUBGHZ_FREQ_ANALYZER_FAP_PATH, "garage:menu:freq");
            return true;
        } else if(event.event == SubmenuIndexModulationAnalyzer) {

            subghz_scene_start_launch_and_exit(
                subghz, SUBGHZ_MOD_ANALYZER_FAP_PATH, "garage:menu:mod");
            return true;
        } else if(event.event == SubmenuIndexProtocolList) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneStart, SubmenuIndexProtocolList);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneProtocolList);
        }
    }
    return false;
}

void subghz_scene_start_on_exit(void* context) {
    SubGhz* subghz = context;

    if(subghz->submenu) {
        submenu_reset(subghz->submenu);
    }
}
