#include "../mass_storage_app_i.h"
#include "../helpers/fox_usb_builder.h"
#include <gui/modules/widget.h>

void mass_storage_scene_start_on_enter(void* context) {
    MassStorageApp* app = context;

    mass_storage_app_show_loading_popup(app, true);

#ifdef FOX_USB_ENSURE_MEDIA_DIRS
    static const char* const ensure_dirs[] = {"MUSIC", "VIDEO", "PICTURE"};
    const char* const* ensure_ptr = ensure_dirs;
    size_t ensure_count = 3;
#else
    const char* const* ensure_ptr = NULL;
    size_t ensure_count = 0;
#endif

    FoxUsbBuildConfig cfg = {
        .storage = app->fs_api,
        .source_dir = FOX_USB_SOURCE_DIR,
        .meta_path = FOX_USB_META_PATH,
        .label = FOX_USB_VOLUME_LABEL,
        .recursive = FOX_USB_RECURSIVE,
        .ensure_dirs = ensure_ptr,
        .ensure_dir_count = ensure_count,
    };

    FoxUsbBuildResult result = fox_usb_builder_run(&cfg, &app->vfat, NULL, NULL);

    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        (result == FoxUsbBuildResultError) ? MassStorageCustomEventBuildFailed :
                                             MassStorageCustomEventBuildDone);
}

bool mass_storage_scene_start_on_event(void* context, SceneManagerEvent event) {
    MassStorageApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MassStorageCustomEventBuildDone) {
            scene_manager_next_scene(app->scene_manager, MassStorageSceneWork);
            consumed = true;
        } else if(event.event == MassStorageCustomEventBuildFailed) {
            widget_reset(app->widget);
            widget_add_string_multiline_element(
                app->widget,
                64,
                28,
                AlignCenter,
                AlignCenter,
                FontSecondary,
                "Drive build failed.\nCheck SD card space.");
            view_dispatcher_switch_to_view(app->view_dispatcher, MassStorageAppViewWidget);
            consumed = true;
        }
    }
    return consumed;
}

void mass_storage_scene_start_on_exit(void* context) {
    UNUSED(context);
}
