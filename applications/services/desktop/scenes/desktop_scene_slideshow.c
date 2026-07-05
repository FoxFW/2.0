#include <storage/storage.h>
#include <gui/gui.h>
#include <furi_hal_power.h>
#include <flipper_format/flipper_format.h>
#include <namechanger/namechanger.h>

#include "../desktop_i.h"
#include "../views/desktop_view_slideshow.h"
#include "../views/desktop_events.h"
#include <power/power_service/power.h>

void desktop_scene_slideshow_callback(DesktopEvent event, void* context) {
    Desktop* desktop = (Desktop*)context;
    view_dispatcher_send_custom_event(desktop->view_dispatcher, event);
}

void desktop_scene_slideshow_on_enter(void* context) {
    Desktop* desktop = (Desktop*)context;
    DesktopSlideshowView* slideshow_view = desktop->slideshow_view;

    gui_set_hide_status_bar(desktop->gui, true);
    desktop_view_slideshow_set_callback(slideshow_view, desktop_scene_slideshow_callback, desktop);

    view_dispatcher_switch_to_view(desktop->view_dispatcher, DesktopViewIdSlideshow);
}

bool desktop_scene_slideshow_on_event(void* context, SceneManagerEvent event) {
    Desktop* desktop = (Desktop*)context;
    bool consumed = false;
    Power* power = NULL;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case DesktopSlideshowCompleted:
            scene_manager_previous_scene(desktop->scene_manager);
            consumed = true;
            break;
        case DesktopSlideshowPoweroff:
            power = furi_record_open(RECORD_POWER);
            power_off(power);
            furi_record_close(RECORD_POWER);
            consumed = true;
            break;

        default:
            break;
        }
    }
    return consumed;
}

void desktop_scene_slideshow_on_exit(void* context) {
    Desktop* desktop = context;
    gui_set_hide_status_bar(desktop->gui, false);
    storage_common_remove(desktop->storage, SLIDESHOW_FS_PATH);

    /* Process name.pending before rebooting so namechanger_srv can apply the
     * new name on the very next boot.
     *
     * REBOOT DECISION: based on whether name.pending EXISTED, not on whether
     * we could open it.  Old code set has_name_pending=false on open failure,
     * which suppressed the reboot and silently lost the name change.
     *
     * DOUBLE-FREE FIX: old code called storage_file_free in the else branch
     * AND again unconditionally — now freed exactly once at the end.        */
    const char* name_pending = "/ext/apps_data/fox_setup/name.pending";
    bool needs_reboot = storage_file_exists(desktop->storage, name_pending);

    if(needs_reboot) {
        File* nf = storage_file_alloc(desktop->storage);
        if(storage_file_open(nf, name_pending, FSAM_READ, FSOM_OPEN_EXISTING)) {
            char name[FURI_HAL_VERSION_ARRAY_NAME_LENGTH] = {0};
            storage_file_read(nf, name, sizeof(name) - 1);
            storage_file_close(nf);
            storage_common_remove(desktop->storage, name_pending);
            if(name[0] != '\0') {
                if(strcmp(name, "") == 0) {
                    storage_simply_remove(desktop->storage, NAMECHANGER_PATH);
                } else {
                    /* Create the parent directory first — flipper_format_file_open_always
                     * does NOT mkdir; if the NameChanger data dir has never been created
                     * the write fails silently and the name is never applied.           */
                    char nc_dir[64];
                    strlcpy(nc_dir, NAMECHANGER_PATH, sizeof(nc_dir));
                    char* sl = strrchr(nc_dir, '/');
                    if(sl) { *sl = '\0'; storage_simply_mkdir(desktop->storage, nc_dir); }
                    FlipperFormat* ff = flipper_format_file_alloc(desktop->storage);
                    if(flipper_format_file_open_always(ff, NAMECHANGER_PATH)) {
                        flipper_format_write_header_cstr(
                            ff, NAMECHANGER_HEADER, NAMECHANGER_VERSION);
                        flipper_format_write_string_cstr(ff, "Name", name);
                        flipper_format_file_close(ff);
                    }
                    flipper_format_free(ff);
                }
            }
        }
        /* If open failed: name.pending stays on disk; desktop.c startup will
         * write the namechanger file on the next boot without an extra reboot
         * — the name takes effect on the boot after that.                   */
        storage_file_free(nf);  /* single free — no double-free */
        furi_hal_power_reset();
    }
}
