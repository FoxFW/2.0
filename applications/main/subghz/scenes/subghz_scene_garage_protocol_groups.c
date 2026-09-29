#include "../subghz_i.h"

static void subghz_scene_garage_protocol_groups_toggled_cb(
    void* context, uint8_t group_index, bool enabled) {
    SubGhz* subghz = context;
    if(group_index >= SUBGHZ_GARAGE_PROTOCOL_GROUP_COUNT) return;
    subghz->last_settings->protocol_groups_enabled[group_index] = enabled ? 1 : 0;
}

void subghz_scene_garage_protocol_groups_on_enter(void* context) {
    SubGhz* subghz = context;
    subghz_protocol_groups_set_enabled_all(
        subghz->garage_protocol_groups, subghz->last_settings->protocol_groups_enabled);
    subghz_protocol_groups_set_callback(
        subghz->garage_protocol_groups, subghz_scene_garage_protocol_groups_toggled_cb, subghz);
    subghz_protocol_groups_resume_scroll(subghz->garage_protocol_groups);
    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdGarageProtocolGroups);
}

bool subghz_scene_garage_protocol_groups_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    if(event.type == SceneManagerEventTypeBack) {
        subghz_save_all(subghz);
        return false;
    } else if(event.type == SceneManagerEventTypeCustom) {
        return true;
    }
    return false;
}

void subghz_scene_garage_protocol_groups_on_exit(void* context) {
    SubGhz* subghz = context;
    subghz_protocol_groups_pause_scroll(subghz->garage_protocol_groups);
    subghz_save_all(subghz);
}
