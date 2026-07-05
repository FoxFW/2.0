#include "../subghz_i.h"
#include <string.h>

// Internal step events used only within this scene
#define KL_EDIT_EV_KEY_DONE  200u
#define KL_EDIT_EV_NAME_DONE 201u
#define KL_EDIT_EV_TYPE_BASE 210u // type 1..8 mapped to events 211..218

static void kl_edit_byte_input_cb(void* context) {
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, KL_EDIT_EV_KEY_DONE);
}

static void kl_edit_text_input_cb(void* context) {
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, KL_EDIT_EV_NAME_DONE);
}

static void kl_edit_type_submenu_cb(void* context, uint32_t index) {
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(
        subghz->view_dispatcher, (uint32_t)(KL_EDIT_EV_TYPE_BASE + 1 + index));
}

static const char* const kl_type_names[8] = {
    "1-Simple",
    "2-Normal",
    "3-Secure",
    "4-Magic XOR",
    "5-FAAC SLH",
    "6-Magic Ser1",
    "7-Magic Ser2",
    "8-Magic Ser3",
};

static void kl_edit_show_step(SubGhz* subghz) {
    switch(subghz->keeloq_edit.edit_step) {
    case 0:
        byte_input_set_header_text(subghz->byte_input, "Enter 64-bit key (hex)");
        byte_input_set_result_callback(
            subghz->byte_input,
            kl_edit_byte_input_cb,
            NULL,
            subghz,
            subghz->keeloq_edit.key_bytes,
            sizeof(subghz->keeloq_edit.key_bytes));
        view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdByteInput);
        break;
    case 1:
        text_input_set_header_text(subghz->text_input, "Manufacturer name");
        text_input_set_result_callback(
            subghz->text_input,
            kl_edit_text_input_cb,
            subghz,
            subghz->keeloq_edit.name,
            sizeof(subghz->keeloq_edit.name),
            false);
        view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdTextInput);
        break;
    case 2:
        submenu_reset(subghz->submenu);
        submenu_set_header(subghz->submenu, "Learning type");
        for(size_t i = 0; i < 8; i++) {
            submenu_add_item(
                subghz->submenu,
                kl_type_names[i],
                (uint32_t)i,
                kl_edit_type_submenu_cb,
                subghz);
        }
        if(subghz->keeloq_edit.type >= 1 && subghz->keeloq_edit.type <= 8) {
            submenu_set_selected_item(subghz->submenu, (uint32_t)(subghz->keeloq_edit.type - 1));
        }
        view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdMenu);
        break;
    default:
        break;
    }
}

void subghz_scene_keeloq_key_edit_on_enter(void* context) {
    SubGhz* subghz = context;
    // edit_step, key_bytes, name, type, is_new, edit_index are pre-set by the caller scene
    kl_edit_show_step(subghz);
}

bool subghz_scene_keeloq_key_edit_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    if(event.event == KL_EDIT_EV_KEY_DONE) {
        subghz->keeloq_edit.edit_step = 1;
        kl_edit_show_step(subghz);
        return true;
    }

    if(event.event == KL_EDIT_EV_NAME_DONE) {
        subghz->keeloq_edit.edit_step = 2;
        kl_edit_show_step(subghz);
        return true;
    }

    if(event.event > KL_EDIT_EV_TYPE_BASE && event.event <= KL_EDIT_EV_TYPE_BASE + 8) {
        subghz->keeloq_edit.type = (uint16_t)(event.event - KL_EDIT_EV_TYPE_BASE);

        // Reconstruct 64-bit key from byte array (big-endian, same as ByteInput display order)
        uint64_t kval = 0;
        for(int b = 0; b < 8; b++) {
            kval = (kval << 8) | (uint64_t)subghz->keeloq_edit.key_bytes[b];
        }

        if(subghz->keeloq_edit.is_new) {
            subghz_keeloq_keys_add(
                subghz->keeloq_keys_manager,
                kval,
                subghz->keeloq_edit.type,
                subghz->keeloq_edit.name);
        } else {
            subghz_keeloq_keys_set(
                subghz->keeloq_keys_manager,
                subghz->keeloq_edit.edit_index,
                kval,
                subghz->keeloq_edit.type,
                subghz->keeloq_edit.name);
        }

        if(!subghz_keeloq_keys_save(subghz->keeloq_keys_manager)) {
            furi_string_set(subghz->error_str, "Cannot save\nkeystore file.");
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneShowErrorSub);
        } else {
            scene_manager_previous_scene(subghz->scene_manager);
        }
        return true;
    }

    return false;
}

void subghz_scene_keeloq_key_edit_on_exit(void* context) {
    SubGhz* subghz = context;
    byte_input_set_result_callback(subghz->byte_input, NULL, NULL, NULL, NULL, 0);
    byte_input_set_header_text(subghz->byte_input, "");
    text_input_reset(subghz->text_input);
    submenu_reset(subghz->submenu);
}
