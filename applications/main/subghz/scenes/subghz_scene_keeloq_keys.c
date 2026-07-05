#include "../subghz_i.h"

#define KEELOQ_KEY_LIST_ADD   0u
#define KEELOQ_KEY_OPT_EDIT   100u
#define KEELOQ_KEY_OPT_DELETE 101u

// File-scope state
static bool kl_opts_mode = false;
static bool kl_info_mode = false;
static char kl_info_text[48];
// Set true just before navigating forward to KeeloqKeyEdit so on_exit doesn't free the manager
static bool kl_going_to_edit = false;

static void keeloq_keys_submenu_callback(void* context, uint32_t index) {
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, index);
}

static void keeloq_keys_show_list(SubGhz* subghz) {
    submenu_reset(subghz->submenu);
    size_t n_user = subghz_keeloq_keys_user_count(subghz->keeloq_keys_manager);
    size_t n_total = subghz_keeloq_keys_count(subghz->keeloq_keys_manager);
    size_t n_sys = n_total - n_user;
    char header[32];
    snprintf(header, sizeof(header), "KL Keys U:%zu S:%zu", n_user, n_sys);
    submenu_set_header(subghz->submenu, header);
    submenu_add_item(
        subghz->submenu, "+ Add Key", KEELOQ_KEY_LIST_ADD, keeloq_keys_submenu_callback, subghz);
    for(size_t i = 0; i < n_total; i++) {
        SubGhzKey* k = subghz_keeloq_keys_get(subghz->keeloq_keys_manager, i);
        submenu_add_item(
            subghz->submenu,
            furi_string_get_cstr(k->name),
            (uint32_t)(i + 1),
            keeloq_keys_submenu_callback,
            subghz);
    }
    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdMenu);
}

static void keeloq_keys_show_options(SubGhz* subghz) {
    SubGhzKey* k =
        subghz_keeloq_keys_get(subghz->keeloq_keys_manager, subghz->keeloq_edit.edit_index);
    submenu_reset(subghz->submenu);
    submenu_set_header(subghz->submenu, furi_string_get_cstr(k->name));
    submenu_add_item(
        subghz->submenu, "Edit", KEELOQ_KEY_OPT_EDIT, keeloq_keys_submenu_callback, subghz);
    submenu_add_item(
        subghz->submenu, "Delete", KEELOQ_KEY_OPT_DELETE, keeloq_keys_submenu_callback, subghz);
    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdMenu);
}

static void keeloq_keys_show_info(SubGhz* subghz, size_t idx) {
    SubGhzKey* k = subghz_keeloq_keys_get(subghz->keeloq_keys_manager, idx);
    popup_reset(subghz->popup);
    popup_set_header(
        subghz->popup, furi_string_get_cstr(k->name), 64, 5, AlignCenter, AlignTop);
    snprintf(
        kl_info_text,
        sizeof(kl_info_text),
        "%016llX\nType: %hu",
        (unsigned long long)k->key,
        k->type);
    popup_set_text(subghz->popup, kl_info_text, 64, 28, AlignCenter, AlignTop);
    popup_disable_timeout(subghz->popup);
    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdPopup);
    kl_info_mode = true;
}

void subghz_scene_keeloq_keys_on_enter(void* context) {
    SubGhz* subghz = context;
    kl_opts_mode = false;
    kl_info_mode = false;
    kl_going_to_edit = false;
    if(!subghz->keeloq_keys_manager) {
        subghz->keeloq_keys_manager = subghz_keeloq_keys_alloc();
    }
    keeloq_keys_show_list(subghz);
}

bool subghz_scene_keeloq_keys_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;

    if(event.type == SceneManagerEventTypeBack) {
        if(kl_info_mode) {
            kl_info_mode = false;
            keeloq_keys_show_list(subghz);
            return true;
        }
        if(kl_opts_mode) {
            kl_opts_mode = false;
            keeloq_keys_show_list(subghz);
            return true;
        }
        return false;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == KEELOQ_KEY_LIST_ADD) {
            subghz->keeloq_edit.is_new = true;
            subghz->keeloq_edit.edit_step = 0;
            memset(subghz->keeloq_edit.key_bytes, 0, sizeof(subghz->keeloq_edit.key_bytes));
            memset(subghz->keeloq_edit.name, 0, sizeof(subghz->keeloq_edit.name));
            subghz->keeloq_edit.type = 1;
            kl_going_to_edit = true;
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneKeeloqKeyEdit);
            return true;
        } else if(event.event >= 1 && event.event < KEELOQ_KEY_OPT_EDIT) {
            size_t idx = (size_t)(event.event - 1);
            size_t n_user = subghz_keeloq_keys_user_count(subghz->keeloq_keys_manager);
            if(idx >= n_user) {
                // System key: read-only, show info popup
                keeloq_keys_show_info(subghz, idx);
                return true;
            }
            subghz->keeloq_edit.edit_index = idx;
            subghz->keeloq_edit.is_new = false;
            kl_opts_mode = true;
            keeloq_keys_show_options(subghz);
            return true;
        } else if(event.event == KEELOQ_KEY_OPT_EDIT) {
            kl_opts_mode = false;
            subghz->keeloq_edit.edit_step = 0;
            SubGhzKey* k = subghz_keeloq_keys_get(
                subghz->keeloq_keys_manager, subghz->keeloq_edit.edit_index);
            // Pre-fill key bytes (big-endian)
            uint64_t kv = k->key;
            for(int b = 7; b >= 0; b--) {
                subghz->keeloq_edit.key_bytes[b] = (uint8_t)(kv & 0xFF);
                kv >>= 8;
            }
            strncpy(
                subghz->keeloq_edit.name,
                furi_string_get_cstr(k->name),
                sizeof(subghz->keeloq_edit.name) - 1);
            subghz->keeloq_edit.name[sizeof(subghz->keeloq_edit.name) - 1] = '\0';
            subghz->keeloq_edit.type = k->type;
            kl_going_to_edit = true;
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneKeeloqKeyEdit);
            return true;
        } else if(event.event == KEELOQ_KEY_OPT_DELETE) {
            kl_opts_mode = false;
            subghz_keeloq_keys_delete(
                subghz->keeloq_keys_manager, subghz->keeloq_edit.edit_index);
            subghz_keeloq_keys_save(subghz->keeloq_keys_manager);
            keeloq_keys_show_list(subghz);
            return true;
        }
    }
    return false;
}

void subghz_scene_keeloq_keys_on_exit(void* context) {
    SubGhz* subghz = context;
    kl_opts_mode = false;
    kl_info_mode = false;
    submenu_reset(subghz->submenu);
    if(!kl_going_to_edit) {
        if(subghz->keeloq_keys_manager) {
            subghz_keeloq_keys_free(subghz->keeloq_keys_manager);
            subghz->keeloq_keys_manager = NULL;
        }
    }
    kl_going_to_edit = false;
}
