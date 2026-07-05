// scenes/gdr_scene_saved.c
#include "../gdr_app_i.h"

#define TAG "GDRSceneSaved"

void gdr_scene_saved_on_enter(void* context) {
    GDRApp* app = context;
    scene_manager_previous_scene(app->scene_manager);
}

bool gdr_scene_saved_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void gdr_scene_saved_on_exit(void* context) {
    UNUSED(context);
}
