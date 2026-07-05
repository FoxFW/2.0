#include "../subghz_i.h"
#include <loader/loader.h>
#include <storage/storage.h>

/* Forward declaration — defined in subghz_scene_start.c. Used here to
 * register the same blank transition cover for the RAW Edit launch. */
void subghz_blank_transition_draw_cb(Canvas* canvas, void* ctx);

/* fap_category="Sub-GHz" in the manifest places the built .fap at
 * /ext/apps/Sub-GHz/<appid>.fap, the standard FBT convention. Bare-appid
 * by-name resolution doesn't work for external FAPs — see the detailed
 * comment in subghz_scene_start.c for why ("App Not Found": by-name
 * resolution only checks a hardcoded catalog of "officially known" apps,
 * which custom FAPs aren't part of). */
#define SUBGHZ_RAW_EDIT_FAP_PATH EXT_PATH("apps/Sub-GHz/subghz_raw_edit.fap")

enum SubmenuIndex {
    SubmenuIndexDecode,
    SubmenuIndexRawEdit,   /* launches the external SubGHz RAW Edit FAP */
    SubmenuIndexEdit,
    SubmenuIndexDelete,
};

void subghz_scene_more_raw_submenu_callback(void* context, uint32_t index) {
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, index);
}

void subghz_scene_more_raw_on_enter(void* context) {
    SubGhz* subghz = context;

    /* Reset FIRST — if SubGHz was relaunched fresh (e.g. via rawreturn
     * from RAW Edit), on_exit may never have run to clear the submenu,
     * leaving stale items that cause a furi_check crash when we try to
     * add items while the list is non-empty. */
    submenu_reset(subghz->submenu);

    submenu_add_item(
        subghz->submenu,
        "Decode",
        SubmenuIndexDecode,
        subghz_scene_more_raw_submenu_callback,
        subghz);

    /* Only show Edit RAW if the external FAP is installed */
    {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        bool has_raw_edit = storage_file_exists(storage, SUBGHZ_RAW_EDIT_FAP_PATH);
        furi_record_close(RECORD_STORAGE);
        if(has_raw_edit) {
            submenu_add_item(
                subghz->submenu,
                "Edit RAW",
                SubmenuIndexRawEdit,
                subghz_scene_more_raw_submenu_callback,
                subghz);
        }
    }

    submenu_add_item(
        subghz->submenu,
        "Rename",
        SubmenuIndexEdit,
        subghz_scene_more_raw_submenu_callback,
        subghz);

    submenu_add_item(
        subghz->submenu,
        "Delete",
        SubmenuIndexDelete,
        subghz_scene_more_raw_submenu_callback,
        subghz);

    submenu_set_selected_item(
        subghz->submenu, scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneMoreRAW));

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdMenu);
}

bool subghz_scene_more_raw_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexRawEdit) {
            if(subghz_file_available(subghz)) {
                /* Launch the external SubGHz RAW Edit FAP with the current
                 * file path.
                 *
                 * Verified directly against the real loader.c:
                 * loader_start() can NEVER succeed while called from
                 * inside an already-running app (loader_do_is_locked()
                 * simply checks loader->app.thread != NULL, which is true
                 * for the entire time SubGHz itself is running — not a
                 * transient condition, permanently true until SubGHz
                 * exits). The correct mechanism is loader_enqueue_launch(),
                 * which queues the FAP to start the instant SubGHz's own
                 * thread stops (confirmed: loader_do_next_deferred_launch
                 * _if_available() fires right after app cleanup). The args
                 * string is strdup'd internally by the Loader immediately,
                 * so it's safe to pass file_path even though SubGhz (and
                 * its FuriString) is freed shortly after this call.
                 *
                 * Loading hourglass intentionally not used
                 * (LoaderDeferredLaunchFlagNone) — was only showing as a
                 * brief, not-needed flash. */

                /* Write the "saved" return marker FIRST — the file_path is
                 * what subghz_app() uses to reload the file directly, landing
                 * on the Send screen with "More >" visible, which is the
                 * closest we can get to "back where you were" since SubGHz
                 * has to fully exit and restart. The "saved" key tells
                 * subghz_app() to also pre-navigate the scene stack so Back
                 * from that screen returns to the Saved list (handled in the
                 * subghz.c .focus_file branch). */
                {
                    Storage* raw_storage = furi_record_open(RECORD_STORAGE);
                    File* raw_f = storage_file_alloc(raw_storage);
                    if(storage_file_open(raw_f, "/ext/subghz/.focus_file",
                                         FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                        /* Write "rawreturn:<path>" so subghz_app() knows to
                     * use full allocation (not raw_send_only) on return,
                     * which gives the normal Saved-file UI with "More". */
                    const char* fp = furi_string_get_cstr(subghz->file_path);
                    const char* prefix = "rawreturn:";
                    storage_file_write(raw_f, prefix, strlen(prefix));
                    storage_file_write(raw_f, fp, strlen(fp));
                    }
                    storage_file_close(raw_f);
                    storage_file_free(raw_f);
                    furi_record_close(RECORD_STORAGE);
                }

                /* Blank transition cover (same pattern as analyzer launches) */
                if(!subghz->blank_transition_viewport) {
                    subghz->blank_transition_viewport = view_port_alloc();
                    view_port_draw_callback_set(
                        subghz->blank_transition_viewport,
                        subghz_blank_transition_draw_cb, NULL);
                    gui_add_view_port(
                        subghz->gui,
                        subghz->blank_transition_viewport,
                        GuiLayerFullscreen);
                    view_port_update(subghz->blank_transition_viewport);
                }

                Loader* loader = furi_record_open(RECORD_LOADER);
                loader_enqueue_launch(
                    loader,
                    SUBGHZ_RAW_EDIT_FAP_PATH,
                    furi_string_get_cstr(subghz->file_path),
                    LoaderDeferredLaunchFlagNone);
                furi_record_close(RECORD_LOADER);

                scene_manager_stop(subghz->scene_manager);
                view_dispatcher_stop(subghz->view_dispatcher);
                return true;
            } else {
                if(!scene_manager_search_and_switch_to_previous_scene(
                       subghz->scene_manager, SubGhzSceneStart)) {
                    scene_manager_stop(subghz->scene_manager);
                    view_dispatcher_stop(subghz->view_dispatcher);
                }
            }
        } else if(event.event == SubmenuIndexDelete) {
            if(subghz_file_available(subghz)) {
                scene_manager_set_scene_state(
                    subghz->scene_manager, SubGhzSceneReadRAW, SubGhzCustomEventManagerNoSet);
                scene_manager_set_scene_state(
                    subghz->scene_manager, SubGhzSceneMoreRAW, SubmenuIndexDelete);
                scene_manager_next_scene(subghz->scene_manager, SubGhzSceneDeleteRAW);
                return true;
            } else {
                if(!scene_manager_search_and_switch_to_previous_scene(
                       subghz->scene_manager, SubGhzSceneStart)) {
                    scene_manager_stop(subghz->scene_manager);
                    view_dispatcher_stop(subghz->view_dispatcher);
                }
            }
        } else if(event.event == SubmenuIndexEdit) {
            if(subghz_file_available(subghz)) {
                furi_string_reset(subghz->file_path_tmp);
                scene_manager_set_scene_state(
                    subghz->scene_manager, SubGhzSceneMoreRAW, SubmenuIndexEdit);
                scene_manager_next_scene(subghz->scene_manager, SubGhzSceneSaveName);
                return true;
            } else {
                if(!scene_manager_search_and_switch_to_previous_scene(
                       subghz->scene_manager, SubGhzSceneStart)) {
                    scene_manager_stop(subghz->scene_manager);
                    view_dispatcher_stop(subghz->view_dispatcher);
                }
            }
        } else if(event.event == SubmenuIndexDecode) {
            if(subghz_file_available(subghz)) {
                scene_manager_set_scene_state(
                    subghz->scene_manager, SubGhzSceneMoreRAW, SubmenuIndexDecode);
                scene_manager_next_scene(subghz->scene_manager, SubGhzSceneDecodeRAW);
                return true;
            } else {
                if(!scene_manager_search_and_switch_to_previous_scene(
                       subghz->scene_manager, SubGhzSceneStart)) {
                    scene_manager_stop(subghz->scene_manager);
                    view_dispatcher_stop(subghz->view_dispatcher);
                }
            }
        }
    }
    return false;
}

void subghz_scene_more_raw_on_exit(void* context) {
    SubGhz* subghz = context;
    submenu_reset(subghz->submenu);
}
