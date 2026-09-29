#include "../subghz_i.h"
#include <lib/subghz/protocols/base.h>
#include <lib/subghz/protocols/fiat_v1.h>

enum SubmenuIndex {
    SubmenuIndexEmulate,
    SubmenuIndexDetails,
    SubmenuIndexPsaDecrypt,
    SubmenuIndexEdit,
    SubmenuIndexDelete,
    SubmenuIndexSignalSettings,
    SubmenuIndexCounterBf,
    SubmenuIndexSeedBf
};

void subghz_scene_saved_menu_submenu_callback(void* context, uint32_t index) {
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, index);
}

void subghz_scene_saved_menu_on_enter(void* context) {
    SubGhz* subghz = context;

    FlipperFormat* fff = subghz_txrx_get_fff_data(subghz->txrx);
    bool is_psa_encrypted = false;
    bool has_counter = false;
    bool is_renault_seed_candidate = false;
    if(fff) {
        FuriString* proto = furi_string_alloc();
        flipper_format_rewind(fff);
        if(flipper_format_read_string(fff, "Protocol", proto)) {
            if(furi_string_equal_str(proto, "PSA GROUP")) {
                FuriString* type_str = furi_string_alloc();
                flipper_format_rewind(fff);
                if(!flipper_format_read_string(fff, "Type", type_str) ||
                   furi_string_equal_str(type_str, "00")) {
                    is_psa_encrypted = true;
                }
                furi_string_free(type_str);
            } else if(furi_string_equal_str(proto, "Renault V1")) {

                uint8_t recovered_hex = 0;
                flipper_format_rewind(fff);
                if(!flipper_format_read_hex(fff, "Recovered", &recovered_hex, 1) ||
                   recovered_hex != 1U) {
                    is_renault_seed_candidate = true;
                }
            }
        }
        furi_string_free(proto);
    }

    if(fff) {
        uint32_t cnt_tmp = 0;
        flipper_format_rewind(fff);
        if(flipper_format_read_uint32(fff, "Cnt", &cnt_tmp, 1)) {
            has_counter = true;
        }
    }

    if(!is_psa_encrypted) {
        submenu_add_item(
            subghz->submenu,
            "Emulate",
            SubmenuIndexEmulate,
            subghz_scene_saved_menu_submenu_callback,
            subghz);
    }

    if(is_psa_encrypted) {
        submenu_add_item(
            subghz->submenu,
            "PSA Decrypt",
            SubmenuIndexPsaDecrypt,
            subghz_scene_saved_menu_submenu_callback,
            subghz);
    }

    submenu_add_item(
        subghz->submenu,
        "Details",
        SubmenuIndexDetails,
        subghz_scene_saved_menu_submenu_callback,
        subghz);

    submenu_add_item(
        subghz->submenu,
        "Rename",
        SubmenuIndexEdit,
        subghz_scene_saved_menu_submenu_callback,
        subghz);

    submenu_add_item(
        subghz->submenu,
        "Delete",
        SubmenuIndexDelete,
        subghz_scene_saved_menu_submenu_callback,
        subghz);

    if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug)) {
        submenu_add_item(
            subghz->submenu,
            "Signal Settings",
            SubmenuIndexSignalSettings,
            subghz_scene_saved_menu_submenu_callback,
            subghz);
    }

    if(has_counter) {
        submenu_add_item(
            subghz->submenu,
            "Counter BruteForce",
            SubmenuIndexCounterBf,
            subghz_scene_saved_menu_submenu_callback,
            subghz);
    }

    if(is_renault_seed_candidate) {
        submenu_add_item(
            subghz->submenu,
            "Seed BF",
            SubmenuIndexSeedBf,
            subghz_scene_saved_menu_submenu_callback,
            subghz);
    }

    submenu_set_selected_item(
        subghz->submenu,
        scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneSavedMenu));

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdMenu);
}

bool subghz_scene_saved_menu_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexEmulate) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneSavedMenu, SubmenuIndexEmulate);

            FlipperFormat* fff = subghz_txrx_get_fff_data(subghz->txrx);
            bool needs_fiat_v1_key = false;
            if(fff) {
                FuriString* proto = furi_string_alloc();
                flipper_format_rewind(fff);
                if(flipper_format_read_string(fff, "Protocol", proto) &&
                   furi_string_equal_str(proto, FIAT_V1_PROTOCOL_NAME)) {
                    uint8_t key[6];
                    flipper_format_rewind(fff);
                    needs_fiat_v1_key =
                        !flipper_format_read_hex(fff, FIAT_V1_HITAG2_KEY_FIELD, key, sizeof(key));
                }
                furi_string_free(proto);
            }
            scene_manager_next_scene(
                subghz->scene_manager,
                needs_fiat_v1_key ? SubGhzSceneFiatV1KeyMethod : SubGhzSceneTransmitter);
            return true;
        } else if(event.event == SubmenuIndexDetails) {

            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneSavedMenu, SubmenuIndexDetails);
            SubGhzProtocolDecoderBase* dec = subghz_txrx_get_decoder(subghz->txrx);
            if(dec) {
                FuriString* kstr = furi_string_alloc();
                if(subghz_protocol_decoder_base_deserialize(
                       dec, subghz_txrx_get_fff_data(subghz->txrx)) == SubGhzProtocolStatusOk) {
                    subghz_protocol_decoder_base_get_string(dec, kstr);
                    furi_string_set(subghz->error_str, kstr);
                } else {
                    furi_string_set(subghz->error_str, "Could not decode signal.");
                }
                furi_string_free(kstr);
            } else {
                furi_string_set(subghz->error_str, "No decoder available.");
            }
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneDetails);
            return true;
        } else if(event.event == SubmenuIndexPsaDecrypt) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneSavedMenu, SubmenuIndexPsaDecrypt);
            scene_manager_next_scene(subghz->scene_manager, SubGhzScenePsaDecrypt);
            return true;
        } else if(event.event == SubmenuIndexDelete) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneSavedMenu, SubmenuIndexDelete);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneDelete);
            return true;
        } else if(event.event == SubmenuIndexEdit) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneSavedMenu, SubmenuIndexEdit);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneSaveName);
            return true;
        } else if(event.event == SubmenuIndexSignalSettings) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneSavedMenu, SubmenuIndexSignalSettings);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneSignalSettings);
            return true;
        } else if(event.event == SubmenuIndexCounterBf) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneSavedMenu, SubmenuIndexCounterBf);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneCounterBf);
            return true;
        } else if(event.event == SubmenuIndexSeedBf) {

            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneSavedMenu, SubmenuIndexSeedBf);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneSeedBf);
            return true;
        }
    }
    return false;
}

void subghz_scene_saved_menu_on_exit(void* context) {
    SubGhz* subghz = context;
    submenu_reset(subghz->submenu);
}
