/**
 * @file subghz_scene_signal_visualizer.c
 * @brief Scene that hosts the SubGHz signal visualizer view.
 *
 * Entry point: "Signal Visualizer" item in the SubGHz main menu.
 * Pressing Back from the view returns to the previous scene (Start).
 */

#include "../subghz_i.h"
#include "../views/subghz_signal_visualizer.h"

#define TAG "SubGhzSceneSignalVisualizer"

/* ── scene callback forwarded from the view ──────────────────────────────── */

static void subghz_scene_signal_visualizer_view_callback(SubGhzCustomEvent event, void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, event);
}

/* ── scene lifecycle ─────────────────────────────────────────────────────── */

void subghz_scene_signal_visualizer_on_enter(void* context) {
    SubGhz* subghz = context;

    subghz_signal_visualizer_set_callback(
        subghz->subghz_signal_visualizer,
        subghz_scene_signal_visualizer_view_callback,
        subghz);

    view_dispatcher_switch_to_view(
        subghz->view_dispatcher, SubGhzViewIdSignalVisualizer);

    /* Apply the persisted display mode from Radio Settings. */
    subghz_signal_visualizer_set_mode(
        subghz->subghz_signal_visualizer,
        subghz->last_settings->visualizer_display_mode);

    subghz_signal_visualizer_start(subghz->subghz_signal_visualizer);
}

bool subghz_scene_signal_visualizer_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubGhzCustomEventViewSignalVisualizerBack) {
            scene_manager_previous_scene(subghz->scene_manager);
            return true;
        }
    }

    return false;
}

void subghz_scene_signal_visualizer_on_exit(void* context) {
    SubGhz* subghz = context;
    subghz_signal_visualizer_stop(subghz->subghz_signal_visualizer);

    /* Persist whatever display mode the user may have toggled with OK.
     * Use the public getter — SubGhzSignalVisualizerModel is private to
     * the view's translation unit and must not be accessed directly here. */
    subghz->last_settings->visualizer_display_mode =
        subghz_signal_visualizer_get_mode(subghz->subghz_signal_visualizer);
    subghz_last_settings_save(subghz->last_settings);
}
