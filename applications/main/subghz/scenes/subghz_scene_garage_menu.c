#include "../subghz_i.h"
#include "../views/subghz_view_garage_grid.h"
#include <gui/modules/fox_theme.h>
#include "subghz_scene_garage_menu.h"
#include <loader/loader.h>
#include <storage/storage.h>

#define SUBGHZ_GMENU_MOD_ANALYZER_FAP_PATH  EXT_PATH("apps/Sub-GHz/subghz_modulation_analyzer.fap")
#define SUBGHZ_GMENU_FREQ_ANALYZER_FAP_PATH EXT_PATH("apps/Sub-GHz/subghz_frequency_analyzer.fap")

void subghz_blank_transition_draw_cb(Canvas* canvas, void* ctx);

static void subghz_scene_garage_menu_launch_and_exit(
    SubGhz* subghz, const char* fap_path, const char* args) {
    Loader* loader = furi_record_open(RECORD_LOADER);
    loader_enqueue_launch(loader, fap_path, args, LoaderDeferredLaunchFlagNone);
    furi_record_close(RECORD_LOADER);

    if(!subghz->blank_transition_viewport) {
        subghz->blank_transition_viewport = view_port_alloc();
        view_port_draw_callback_set(
            subghz->blank_transition_viewport, subghz_blank_transition_draw_cb, NULL);
        gui_add_view_port(subghz->gui, subghz->blank_transition_viewport, GuiLayerFullscreen);
        view_port_update(subghz->blank_transition_viewport);
    }

    scene_manager_stop(subghz->scene_manager);
    view_dispatcher_stop(subghz->view_dispatcher);
}

static void subghz_scene_garage_menu_submenu_callback(void* context, uint32_t index) {
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, index);
}

void subghz_scene_garage_menu_on_enter(void* context) {
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
    bool has_freq_analyzer = storage_file_exists(storage, SUBGHZ_GMENU_FREQ_ANALYZER_FAP_PATH);
    bool has_mod_analyzer  = storage_file_exists(storage, SUBGHZ_GMENU_MOD_ANALYZER_FAP_PATH);
    furi_record_close(RECORD_STORAGE);

    subghz_garage_grid_set_visible(subghz->garage_grid, GGRID_IDX_FREQANA, has_freq_analyzer);
    subghz_garage_grid_set_visible(subghz->garage_grid, GGRID_IDX_MODANA, has_mod_analyzer);

    subghz_garage_grid_set_callback(
        subghz->garage_grid, subghz_scene_garage_menu_submenu_callback, subghz);

    {
        uint32_t focus =
            scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneGarageMenu);
        uint8_t grid_btn = 0;
        if(focus == GarageMenuIndexSaved)                 grid_btn = GGRID_IDX_SAVED;
        else if(focus == GarageMenuIndexReadRAW)          grid_btn = GGRID_IDX_READRAW;
        else if(focus == GarageMenuIndexFrequencyAnalyzer) grid_btn = GGRID_IDX_FREQANA;
        else if(focus == GarageMenuIndexModulationAnalyzer) grid_btn = GGRID_IDX_MODANA;
        else if(focus == GarageMenuIndexProtocolGroups)   grid_btn = GGRID_IDX_PROTOCOLS;
        subghz_garage_grid_set_selected(subghz->garage_grid, grid_btn);
    }

    if(fox_theme_is_active()) {
        view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdGarageGrid);
    } else {
        uint32_t focus =
            scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneGarageMenu);
        submenu_reset(subghz->submenu);
        submenu_add_item(
            subghz->submenu, "Read", GarageMenuIndexRead,
            subghz_scene_garage_menu_submenu_callback, subghz);
        submenu_add_item(
            subghz->submenu, "Read RAW", GarageMenuIndexReadRAW,
            subghz_scene_garage_menu_submenu_callback, subghz);
        submenu_add_item(
            subghz->submenu, "Saved", GarageMenuIndexSaved,
            subghz_scene_garage_menu_submenu_callback, subghz);
        if(has_freq_analyzer)
            submenu_add_item(
                subghz->submenu, "Freq. Analyzer", GarageMenuIndexFrequencyAnalyzer,
                subghz_scene_garage_menu_submenu_callback, subghz);
        if(has_mod_analyzer)
            submenu_add_item(
                subghz->submenu, "Mod. Analyzer", GarageMenuIndexModulationAnalyzer,
                subghz_scene_garage_menu_submenu_callback, subghz);
        submenu_add_item(
            subghz->submenu, "Protocol Group", GarageMenuIndexProtocolGroups,
            subghz_scene_garage_menu_submenu_callback, subghz);
        submenu_set_selected_item(subghz->submenu, focus);
        view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdMenu);
    }
}

bool subghz_scene_garage_menu_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    if(event.type == SceneManagerEventTypeBack) {
        if(!scene_manager_previous_scene(subghz->scene_manager)) {
            scene_manager_stop(subghz->scene_manager);
            view_dispatcher_stop(subghz->view_dispatcher);
        }
        return true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(subghz->scene_manager, SubGhzSceneGarageMenu, event.event);
        if(event.event == GarageMenuIndexRead) {
            subghz_launch_garage_via_probe(subghz, "read");
            return true;
        } else if(event.event == GarageMenuIndexReadRAW) {
            subghz_launch_garage_via_probe(subghz, "readraw");
            return true;
        } else if(event.event == GarageMenuIndexSaved) {
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneSaved);
            return true;
        } else if(event.event == GarageMenuIndexFrequencyAnalyzer) {
            subghz_scene_garage_menu_launch_and_exit(
                subghz, SUBGHZ_GMENU_FREQ_ANALYZER_FAP_PATH, "gmenu:menu:freq");
            return true;
        } else if(event.event == GarageMenuIndexModulationAnalyzer) {
            subghz_scene_garage_menu_launch_and_exit(
                subghz, SUBGHZ_GMENU_MOD_ANALYZER_FAP_PATH, "gmenu:menu:mod");
            return true;
        } else if(event.event == GarageMenuIndexProtocolGroups) {
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneGarageProtocolGroups);
            return true;
        }
    }
    return false;
}

void subghz_scene_garage_menu_on_exit(void* context) {
    SubGhz* subghz = context;
    submenu_reset(subghz->submenu);
}
