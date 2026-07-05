#include "../predator_i.h"

// Access Control Menu - Entry point for Wiegand + EM4305
// Pattern: Same as Transit Cards Menu

typedef enum {
    AccessControlMenuWiegand,
    AccessControlMenuEM4305,
    AccessControlMenuISO15693,
    AccessControlMenuBack,
} AccessControlMenuItem;

void predator_scene_access_control_menu_submenu_callback(void* context, uint32_t index) {
    PredatorApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void predator_scene_access_control_menu_on_enter(void* context) {
    PredatorApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Access Control");

    submenu_add_item(
        submenu,
        "🔑 Wiegand/HID",
        AccessControlMenuWiegand,
        predator_scene_access_control_menu_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "📡 EM4305 Cloner",
        AccessControlMenuEM4305,
        predator_scene_access_control_menu_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "📚 ISO 15693",
        AccessControlMenuISO15693,
        predator_scene_access_control_menu_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Back",
        AccessControlMenuBack,
        predator_scene_access_control_menu_submenu_callback,
        app);

    submenu_set_selected_item(
        submenu,
        scene_manager_get_scene_state(app->scene_manager, PredatorSceneAccessControlMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
}

bool predator_scene_access_control_menu_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, PredatorSceneAccessControlMenu, event.event);
        
        switch(event.event) {
        case AccessControlMenuWiegand:
            scene_manager_next_scene(app->scene_manager, PredatorSceneWiegandReader);
            consumed = true;
            break;
        case AccessControlMenuEM4305:
            scene_manager_next_scene(app->scene_manager, PredatorSceneEM4305Clone);
            consumed = true;
            break;
        case AccessControlMenuISO15693:
            scene_manager_next_scene(app->scene_manager, PredatorSceneISO15693Scanner);
            consumed = true;
            break;
        case AccessControlMenuBack:
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void predator_scene_access_control_menu_on_exit(void* context) {
    PredatorApp* app = context;
    submenu_reset(app->submenu);
}
