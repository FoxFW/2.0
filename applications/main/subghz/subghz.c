/* Abandon hope, all ye who enter here. */

#include <furi/core/log.h>
#include <subghz/types.h>
#include <lib/toolbox/path.h>
#include <float_tools.h>
#include "subghz_i.h"
#include "scenes/subghz_scene_start.h"
#include <storage/storage.h>

#define TAG "SubGhzApp"

bool subghz_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    SubGhz* subghz = context;
    return scene_manager_handle_custom_event(subghz->scene_manager, event);
}

bool subghz_back_event_callback(void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    return scene_manager_handle_back_event(subghz->scene_manager);
}

void subghz_tick_event_callback(void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    scene_manager_handle_tick_event(subghz->scene_manager);
}

static void subghz_rpc_command_callback(const RpcAppSystemEvent* event, void* context) {
    furi_assert(context);
    SubGhz* subghz = context;

    furi_assert(subghz->rpc_ctx);

    if(event->type == RpcAppEventTypeSessionClose) {
        view_dispatcher_send_custom_event(
            subghz->view_dispatcher, SubGhzCustomEventSceneRpcSessionClose);
        rpc_system_app_set_callback(subghz->rpc_ctx, NULL, NULL);
        subghz->rpc_ctx = NULL;
    } else if(event->type == RpcAppEventTypeAppExit) {
        view_dispatcher_send_custom_event(subghz->view_dispatcher, SubGhzCustomEventSceneExit);
    } else if(event->type == RpcAppEventTypeLoadFile) {
        furi_assert(event->data.type == RpcAppSystemEventDataTypeString);
        furi_string_set(subghz->file_path, event->data.string);
        view_dispatcher_send_custom_event(subghz->view_dispatcher, SubGhzCustomEventSceneRpcLoad);
    } else if(event->type == RpcAppEventTypeButtonPress) {
        view_dispatcher_send_custom_event(
            subghz->view_dispatcher, SubGhzCustomEventSceneRpcButtonPress);
    } else if(event->type == RpcAppEventTypeButtonRelease) {
        view_dispatcher_send_custom_event(
            subghz->view_dispatcher, SubGhzCustomEventSceneRpcButtonRelease);
    } else if(event->type == RpcAppEventTypeButtonPressRelease) {
        view_dispatcher_send_custom_event(
            subghz->view_dispatcher, SubGhzCustomEventSceneRpcButtonPressRelease);
    } else {
        rpc_system_app_confirm(subghz->rpc_ctx, false);
    }
}
/* Writes all subghz settings (last_settings + protocol/mod filters) to one file. */
void subghz_save_all(SubGhz* subghz) {
    furi_assert(subghz);
    /* Copy current filter state into the last_settings struct */
    subghz_protocol_filter_get_raw(
        subghz->protocol_filter,
        subghz->last_settings->protocol_filter_data,
        sizeof(subghz->last_settings->protocol_filter_data));
    subghz->last_settings->protocol_filter_present = true;
    subghz_modulation_filter_get_raw(
        subghz->modulation_filter,
        subghz->last_settings->mod_filter_data,
        sizeof(subghz->last_settings->mod_filter_data));
    subghz->last_settings->mod_filter_present = true;
    /* Write everything in one pass */
    subghz_last_settings_save(subghz->last_settings);
}


SubGhz* subghz_alloc(bool alloc_for_tx_only) {
    SubGhz* subghz = malloc(sizeof(SubGhz));

    /* Explicitly NULL fields that subghz_free() checks but that are only
     * conditionally set during runtime. malloc() does NOT zero memory, so
     * without this an unset field containing malloc heap garbage would be
     * treated as a valid pointer and crash on free. */
    subghz->blank_transition_viewport = NULL;

    subghz->keeloq_keys_manager = NULL;

    subghz->keeloq_bf2.sig1_loaded = false;
    subghz->keeloq_bf2.sig2_loaded = false;
    subghz->keeloq_bf2.sig1_path = furi_string_alloc();
    subghz->keeloq_bf2.sig2_path = furi_string_alloc();

    subghz->file_path = furi_string_alloc();
    subghz->file_path_tmp = furi_string_alloc();
    subghz->decoded_preview_orig_path = furi_string_alloc();
    subghz->decoded_preview_active    = false;

    // GUI
    subghz->gui = furi_record_open(RECORD_GUI);

    /* Show loading wheel immediately — covers the apps menu before
     * the heavy subghz_alloc work begins, preventing the user from
     * seeing or interacting with the apps menu during startup.
     * Removed in subghz_scene_start_on_enter() once ready. */
    if(!alloc_for_tx_only) {
        subghz->startup_loading = loading_alloc();
        subghz->startup_holder  = view_holder_alloc();
        view_holder_attach_to_gui(subghz->startup_holder, subghz->gui);
        view_holder_set_view(
            subghz->startup_holder, loading_get_view(subghz->startup_loading));
    }

    // View Dispatcher
    subghz->view_dispatcher = view_dispatcher_alloc();

    subghz->scene_manager = scene_manager_alloc(&subghz_scene_handlers, subghz);
    view_dispatcher_set_event_callback_context(subghz->view_dispatcher, subghz);
    view_dispatcher_set_custom_event_callback(
        subghz->view_dispatcher, subghz_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        subghz->view_dispatcher, subghz_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        subghz->view_dispatcher, subghz_tick_event_callback, 100);

    // Open Notification record
    subghz->notifications = furi_record_open(RECORD_NOTIFICATION);
#if SUBGHZ_MEASURE_LOADING
    uint32_t load_ticks = furi_get_tick();
#endif
    subghz->txrx = subghz_txrx_alloc();

    if(!alloc_for_tx_only) {
        // SubMenu
        subghz->submenu = submenu_alloc();
        view_dispatcher_add_view(
            subghz->view_dispatcher, SubGhzViewIdMenu, submenu_get_view(subghz->submenu));

        // Receiver
        subghz->subghz_receiver = subghz_view_receiver_alloc();
        view_dispatcher_add_view(
            subghz->view_dispatcher,
            SubGhzViewIdReceiver,
            subghz_view_receiver_get_view(subghz->subghz_receiver));
    }
    // Popup
    subghz->popup = popup_alloc();
    view_dispatcher_add_view(
        subghz->view_dispatcher, SubGhzViewIdPopup, popup_get_view(subghz->popup));
    if(!alloc_for_tx_only) {
        // Text Input
        subghz->text_input = text_input_alloc();
        view_dispatcher_add_view(
            subghz->view_dispatcher,
            SubGhzViewIdTextInput,
            text_input_get_view(subghz->text_input));

        // Byte Input
        subghz->byte_input = byte_input_alloc();
        view_dispatcher_add_view(
            subghz->view_dispatcher,
            SubGhzViewIdByteInput,
            byte_input_get_view(subghz->byte_input));

        // Custom Widget
        subghz->widget = widget_alloc();
        view_dispatcher_add_view(
            subghz->view_dispatcher, SubGhzViewIdWidget, widget_get_view(subghz->widget));
    }
    //Dialog
    subghz->dialogs = furi_record_open(RECORD_DIALOGS);

    // Transmitter
    subghz->subghz_transmitter = subghz_view_transmitter_alloc();
    view_dispatcher_add_view(
        subghz->view_dispatcher,
        SubGhzViewIdTransmitter,
        subghz_view_transmitter_get_view(subghz->subghz_transmitter));
    if(!alloc_for_tx_only) {
        // Variable Item List
        subghz->variable_item_list = variable_item_list_alloc();
        view_dispatcher_add_view(
            subghz->view_dispatcher,
            SubGhzViewIdVariableItemList,
            variable_item_list_get_view(subghz->variable_item_list));

        // Signal Visualizer
        subghz->subghz_signal_visualizer = subghz_signal_visualizer_alloc(subghz->txrx);
        view_dispatcher_add_view(
            subghz->view_dispatcher,
            SubGhzViewIdSignalVisualizer,
            subghz_signal_visualizer_get_view(subghz->subghz_signal_visualizer));
    }
    // Read RAW
    subghz->subghz_read_raw = subghz_read_raw_alloc(alloc_for_tx_only);
    view_dispatcher_add_view(
        subghz->view_dispatcher,
        SubGhzViewIdReadRAW,
        subghz_read_raw_get_view(subghz->subghz_read_raw));

    /* Fox-theme start grid — always allocated, always registered */
    subghz->start_grid = subghz_start_grid_alloc();
    view_dispatcher_add_view(
        subghz->view_dispatcher,
        SubGhzViewIdStartGrid,
        subghz_start_grid_get_view(subghz->start_grid));

    subghz->subghz_psa_decrypt = subghz_view_psa_decrypt_alloc();
    view_dispatcher_add_view(
        subghz->view_dispatcher,
        SubGhzViewIdPsaDecrypt,
        subghz_view_psa_decrypt_get_view(subghz->subghz_psa_decrypt));

    subghz->subghz_keeloq_decrypt = subghz_view_keeloq_decrypt_alloc();
    view_dispatcher_add_view(
        subghz->view_dispatcher,
        SubGhzViewIdKeeloqDecrypt,
        subghz_view_keeloq_decrypt_get_view(subghz->subghz_keeloq_decrypt));

    //init threshold rssi
    subghz->threshold_rssi = subghz_threshold_rssi_alloc();

    //init TxRx & Protocol & History & KeyBoard
    subghz_unlock(subghz);

    // Load last used values for Read, Read RAW, etc. or default
    subghz->last_settings = subghz_last_settings_alloc();
    subghz->protocol_filter = subghz_protocol_filter_alloc();
    subghz->modulation_filter = subghz_modulation_filter_alloc();
    /* Load all settings (including filter data) from one file */
    subghz_last_settings_load(subghz->last_settings, 0);
    /* Apply loaded filter arrays to the runtime filter objects */
    if(subghz->last_settings->protocol_filter_present) {
        subghz_protocol_filter_set_raw(
            subghz->protocol_filter,
            subghz->last_settings->protocol_filter_data,
            sizeof(subghz->last_settings->protocol_filter_data));
    }
    if(subghz->last_settings->mod_filter_present) {
        subghz_modulation_filter_set_raw(
            subghz->modulation_filter,
            subghz->last_settings->mod_filter_data,
            sizeof(subghz->last_settings->mod_filter_data));
    }

    // Set LED and Amp GPIO control state
    furi_hal_subghz_set_ext_leds_and_amp(subghz->last_settings->leds_and_amp);

    if(!alloc_for_tx_only) {
        subghz_txrx_set_preset_internal(
            subghz->txrx,
            subghz->last_settings->frequency,
            subghz->last_settings->preset_index,
            subghz->tx_power);
        subghz->history = subghz_history_alloc();
    }

    subghz_rx_key_state_set(subghz, SubGhzRxKeyStateIDLE);

    subghz->gen_info = malloc(sizeof(GenInfo));

    if(!alloc_for_tx_only) {
        subghz->ignore_filter = subghz->last_settings->ignore_filter;
        subghz->filter = subghz->last_settings->filter;
        subghz->tx_power = subghz->last_settings->tx_power;
    } else {
        subghz->filter = SubGhzProtocolFlag_Decodable;
        subghz->ignore_filter = 0x0;
        subghz->tx_power = 0;
    }

    subghz_txrx_receiver_set_filter(subghz->txrx, subghz->filter);
    subghz_txrx_set_need_save_callback(subghz->txrx, subghz_save_to_file, subghz);

    if(!alloc_for_tx_only) {
        if(!float_is_equal(subghz->last_settings->rssi, 0)) {
            subghz_threshold_rssi_set(subghz->threshold_rssi, subghz->last_settings->rssi);
        } else {
            subghz->last_settings->rssi = SUBGHZ_LAST_SETTING_FREQUENCY_ANALYZER_TRIGGER;
        }
    }
#if SUBGHZ_MEASURE_LOADING
    load_ticks = furi_get_tick() - load_ticks;
    FURI_LOG_I(TAG, "Loaded: %ld ms.", load_ticks);
#endif
    //Init Error_str
    subghz->error_str = furi_string_alloc();

    return subghz;
}

void subghz_free(SubGhz* subghz, bool alloc_for_tx_only) {
    furi_assert(subghz);

    if(subghz->rpc_ctx) {
        rpc_system_app_set_callback(subghz->rpc_ctx, NULL, NULL);
        rpc_system_app_send_exited(subghz->rpc_ctx);
        subghz_blink_stop(subghz);
        subghz->rpc_ctx = NULL;
    }

    subghz_txrx_speaker_off(subghz->txrx);
    subghz_txrx_stop(subghz->txrx);
    subghz_txrx_sleep(subghz->txrx);

    if(!alloc_for_tx_only) {
        // Receiver
        view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdReceiver);
        subghz_view_receiver_free(subghz->subghz_receiver);

        // TextInput
        view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdTextInput);
        text_input_free(subghz->text_input);

        // ByteInput
        view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdByteInput);
        byte_input_free(subghz->byte_input);

        // Custom Widget
        view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdWidget);
        widget_free(subghz->widget);
    }
    //Dialog
    furi_record_close(RECORD_DIALOGS);

    // Transmitter
    view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdTransmitter);
    subghz_view_transmitter_free(subghz->subghz_transmitter);
    if(!alloc_for_tx_only) {
        // Variable Item List
        view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdVariableItemList);
        variable_item_list_free(subghz->variable_item_list);

        // Signal Visualizer
        view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdSignalVisualizer);
        subghz_signal_visualizer_free(subghz->subghz_signal_visualizer);
    }
    // PSA Decrypt
    view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdPsaDecrypt);
    subghz_view_psa_decrypt_free(subghz->subghz_psa_decrypt);

    // KeeLoq Decrypt
    view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdKeeloqDecrypt);
    subghz_view_keeloq_decrypt_free(subghz->subghz_keeloq_decrypt);

    // Read RAW
    view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdReadRAW);
    view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdStartGrid);
    subghz_start_grid_free(subghz->start_grid);
    subghz_read_raw_free(subghz->subghz_read_raw);
    if(!alloc_for_tx_only) {
        // Submenu
        view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdMenu);
        submenu_free(subghz->submenu);
    }
    // Popup
    view_dispatcher_remove_view(subghz->view_dispatcher, SubGhzViewIdPopup);
    popup_free(subghz->popup);

    // Scene manager
    scene_manager_free(subghz->scene_manager);

    // View Dispatcher
    view_dispatcher_free(subghz->view_dispatcher);

    // Blank transition cover — must be freed AFTER view_dispatcher_free
    // (so its registered view is no longer live) but BEFORE the GUI record
    // closes. Missing this was the cause of the white screen hang after
    // RAW Edit returns: the viewport was registered but never removed.
    if(subghz->blank_transition_viewport) {
        gui_remove_view_port(subghz->gui, subghz->blank_transition_viewport);
        view_port_free(subghz->blank_transition_viewport);
        subghz->blank_transition_viewport = NULL;
    }

    // GUI
    /* Remove startup loading wheel if still active */
    if(subghz->startup_holder) {
        view_holder_set_view(subghz->startup_holder, NULL);
        view_holder_free(subghz->startup_holder);
        subghz->startup_holder = NULL;
    }
    if(subghz->startup_loading) {
        loading_free(subghz->startup_loading);
        subghz->startup_loading = NULL;
    }
    furi_record_close(RECORD_GUI);
    subghz->gui = NULL;

    subghz_save_all(subghz);
    subghz_protocol_filter_free(subghz->protocol_filter);
    subghz_modulation_filter_free(subghz->modulation_filter);
    subghz_last_settings_free(subghz->last_settings);

    // threshold rssi
    subghz_threshold_rssi_free(subghz->threshold_rssi);

    if(!alloc_for_tx_only) {
        subghz_history_free(subghz->history);
    }

    free(subghz->gen_info);

    //TxRx
    subghz_txrx_free(subghz->txrx);

    //Error string
    furi_string_free(subghz->error_str);

    // Notifications
    furi_record_close(RECORD_NOTIFICATION);
    subghz->notifications = NULL;

    // Path strings
    furi_string_free(subghz->file_path);
    furi_string_free(subghz->file_path_tmp);
    furi_string_free(subghz->decoded_preview_orig_path);

    furi_string_free(subghz->keeloq_bf2.sig1_path);
    furi_string_free(subghz->keeloq_bf2.sig2_path);

    if(subghz->keeloq_keys_manager) {
        subghz_keeloq_keys_free(subghz->keeloq_keys_manager);
        subghz->keeloq_keys_manager = NULL;
    }

    // The rest
    free(subghz);
}

int32_t subghz_app(void* p) {
    /* Two marker files let SubGHz return precisely to where the user was
     * after closing an external sub-tool FAP, without depending on
     * chaining a SECOND args-based deferred launch (confirmed to fail —
     * see the detailed comment below). Both are only checked on a normal,
     * no-args launch, and both are deleted immediately after reading so
     * they can't incorrectly affect a later, unrelated launch.
     *
     *  .focus_menu  — written by the Frequency/Modulation Analyzer FAPs
     *                 on Back (no result selected): "menu:freq" or
     *                 "menu:mod", pre-selects that Start-menu item.
     *  .focus_file  — written by the RAW Edit FAP on exit: the file path
     *                 that was being edited. Reassigns `p` itself so all
     *                 the existing, already-proven file-load logic below
     *                 runs completely unchanged, exactly as if that path
     *                 had been the original launch argument.
     *
     * Why marker files instead of chained args: passing "menu:freq" as
     * args worked fine for SubGHz→FAP (outbound) but was confirmed to
     * fail for FAP→SubGHz (the return leg fell through to the Desktop
     * instead of relaunching SubGHz) — the earlier confirmed-working
     * test used NULL args for that exact leg. Rather than depend on an
     * unconfirmed detail of how the Loader's deferred-launch queue
     * handles two chained args-based launches back to back, this
     * sidesteps that mechanism entirely for the leg where it broke. */
    static char focus_file_buf[256];
    uint32_t menu_focus_index = 0; /* 0 = no focus override */
    bool focus_file_existed = false;
    char focus_menu_content[16] = {0};

    if(!p || strlen((const char*)p) == 0) {
        Storage* storage = furi_record_open(RECORD_STORAGE);

        if(storage_file_exists(storage, "/ext/subghz/.focus_menu")) {
            File* f = storage_file_alloc(storage);
            if(storage_file_open(f, "/ext/subghz/.focus_menu", FSAM_READ, FSOM_OPEN_EXISTING)) {
                char buf[16] = {0};
                uint16_t read = storage_file_read(f, buf, sizeof(buf) - 1);
                buf[read] = '\0';
                strncpy(focus_menu_content, buf, sizeof(focus_menu_content) - 1);
                if(strcmp(buf, "menu:freq") == 0) {
                    menu_focus_index = SubmenuIndexFrequencyAnalyzer;
                } else if(strcmp(buf, "menu:mod") == 0) {
                    menu_focus_index = SubmenuIndexModulationAnalyzer;
                } else if(strcmp(buf, "menu:gdr") == 0) {
                    menu_focus_index = SubmenuIndexGarageDoorRemote;
                } else if(strcmp(buf, "menu:jammer") == 0) {
                    menu_focus_index = SubmenuIndexRFJammer;
                } else if(strcmp(buf, "read") == 0) {
                    /* FA/MA OK result: open the Receiver directly. */
                    static const char read_arg[] = "read";
                    p = (void*)read_arg;
                } else if(strcmp(buf, "readraw") == 0) {
                    /* FA/MA OK result: open Read RAW (live capture mode). */
                    static const char readraw_arg[] = "readraw";
                    p = (void*)readraw_arg;
                }
            }
            storage_file_close(f);
            storage_file_free(f);
            storage_simply_remove(storage, "/ext/subghz/.focus_menu");
            /* Also delete any stale .focus_file so a leftover from a
             * failed RAW Edit run can't shadow a future menu return. */
            if(storage_file_exists(storage, "/ext/subghz/.focus_file")) {
                storage_simply_remove(storage, "/ext/subghz/.focus_file");
            }
        } else if(storage_file_exists(storage, "/ext/subghz/.focus_file")) {
            focus_file_existed = true;
            File* f = storage_file_alloc(storage);
            if(storage_file_open(f, "/ext/subghz/.focus_file", FSAM_READ, FSOM_OPEN_EXISTING)) {
                uint16_t read = storage_file_read(f, focus_file_buf, sizeof(focus_file_buf) - 1);
                focus_file_buf[read] = '\0';
                /* Marker format is "rawreturn:<filepath>" — strip the prefix
                 * so downstream gets a plain path, but we know NOT to set
                 * raw_send_only (which hides the "More" button and causes
                 * the white-screen hang when the viewport isn't covered). */
                if(read > 0) {
                    const char* prefix = "rawreturn:";
                    if(strncmp(focus_file_buf, prefix, strlen(prefix)) == 0) {
                        /* Strip the prefix — p points at the plain path.
                         * Set is_rawreturn via a dedicated flag rather than
                         * pointer arithmetic (simpler, no void* cast issues). */
                        p = focus_file_buf + strlen(prefix);
                        focus_file_existed = true; /* already true, reaffirm */
                    } else {
                        p = focus_file_buf;
                    }
                }
            }
            storage_file_close(f);
            storage_file_free(f);
            storage_simply_remove(storage, "/ext/subghz/.focus_file");
        }

        furi_record_close(RECORD_STORAGE);
    }

    bool open_receiver = (p && strcmp((const char*)p, "read") == 0);
    bool open_readraw  = (p && strcmp((const char*)p, "readraw") == 0);
    bool open_menu_focused = (menu_focus_index != 0);

    /* When returning from RAW Edit the pointer was stripped of the
     * "rawreturn:" prefix — detect by checking if it points into
     * focus_file_buf past offset 10 (length of "rawreturn:"). We
     * want full app allocation so the normal Saved-file UI appears
     * with the "More" button, not the raw-TX-only stripped mode. */
    /* Set during focus_file parsing above when "rawreturn:" prefix found. */
    bool is_rawreturn = (focus_file_existed &&
                         p != NULL &&
                         p != (void*)focus_file_buf &&
                         (const char*)p == focus_file_buf + 10);

    bool alloc_for_tx;
    if(p && strlen((const char*)p) && !open_receiver && !open_readraw && !open_menu_focused && !is_rawreturn) {
        alloc_for_tx = true;
    } else {
        alloc_for_tx = false;
    }

    SubGhz* subghz = subghz_alloc(alloc_for_tx);

    if(alloc_for_tx) {
        subghz->raw_send_only = true;
    } else {
        subghz->raw_send_only = false;
    }

    // Check argument and run corresponding scene
    if(open_menu_focused) {
        view_dispatcher_attach_to_gui(
            subghz->view_dispatcher, subghz->gui, ViewDispatcherTypeFullscreen);
        scene_manager_set_scene_state(subghz->scene_manager, SubGhzSceneStart, menu_focus_index);
        scene_manager_next_scene(subghz->scene_manager, SubGhzSceneStart);
    } else if(open_receiver) {
        view_dispatcher_attach_to_gui(
            subghz->view_dispatcher, subghz->gui, ViewDispatcherTypeFullscreen);
        furi_string_set(subghz->file_path, SUBGHZ_APP_FOLDER);
        if(subghz_txrx_is_database_loaded(subghz->txrx)) {
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneStart);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneReceiver);
        } else {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneShowError, SubGhzCustomEventManagerSet);
            furi_string_set(
                subghz->error_str,
                "No SD card or\ndatabase found.\nSome app function\nmay be reduced.");
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneShowError);
        }
    } else if(open_readraw) {
        /* Launched from FA/MA OK — open Read RAW live-capture mode,
         * already tuned to the frequency written into last_subghz.settings
         * by the FAP. Back from Read RAW returns to the Start menu. */
        view_dispatcher_attach_to_gui(
            subghz->view_dispatcher, subghz->gui, ViewDispatcherTypeFullscreen);
        furi_string_set(subghz->file_path, SUBGHZ_APP_FOLDER);
        if(subghz_txrx_is_database_loaded(subghz->txrx)) {
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneStart);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneReadRAW);
        } else {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneShowError, SubGhzCustomEventManagerSet);
            furi_string_set(
                subghz->error_str,
                "No SD card or\ndatabase found.\nSome app function\nmay be reduced.");
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneShowError);
        }
    } else if(p && strlen((const char*)p)) {
        uint32_t rpc_ctx = 0;

        if(sscanf(p, "RPC %lX", &rpc_ctx) == 1) {
            subghz->rpc_ctx = (void*)rpc_ctx;
            rpc_system_app_set_callback(subghz->rpc_ctx, subghz_rpc_command_callback, subghz);
            rpc_system_app_send_started(subghz->rpc_ctx);
            view_dispatcher_attach_to_gui(
                subghz->view_dispatcher, subghz->gui, ViewDispatcherTypeDesktop);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneRpc);
        } else {
            view_dispatcher_attach_to_gui(
                subghz->view_dispatcher, subghz->gui, ViewDispatcherTypeFullscreen);
            if(subghz_key_load(subghz, p, true)) {
                furi_string_set(subghz->file_path, (const char*)p);

                if(subghz_get_load_type_file(subghz) == SubGhzLoadTypeFileRaw) {
                    //Load Raw TX
                    subghz_rx_key_state_set(subghz, SubGhzRxKeyStateRAWLoad);
                    if(is_rawreturn) {
                        /* Returning from RAW Edit — build a proper scene stack
                         * so Back from ReadRAW returns to the Start menu with
                         * "Read RAW" highlighted, rather than exiting SubGHz. */
                        scene_manager_set_scene_state(
                            subghz->scene_manager,
                            SubGhzSceneStart,
                            SubmenuIndexReadRAW);
                        scene_manager_next_scene(subghz->scene_manager, SubGhzSceneStart);
                    }
                    scene_manager_next_scene(subghz->scene_manager, SubGhzSceneReadRAW);
                } else {
                    //Load transmitter TX
                    scene_manager_next_scene(subghz->scene_manager, SubGhzSceneTransmitter);
                }
            } else {
                //exit app
                scene_manager_stop(subghz->scene_manager);
                view_dispatcher_stop(subghz->view_dispatcher);
            }
        }
    } else {
        view_dispatcher_attach_to_gui(
            subghz->view_dispatcher, subghz->gui, ViewDispatcherTypeFullscreen);
        furi_string_set(subghz->file_path, SUBGHZ_APP_FOLDER);
        if(subghz_txrx_is_database_loaded(subghz->txrx)) {
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneStart);
        } else {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneShowError, SubGhzCustomEventManagerSet);
            furi_string_set(
                subghz->error_str,
                "No SD card or\ndatabase found.\nSome app function\nmay be reduced.");
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneShowError);
        }
    }

    furi_hal_power_suppress_charge_enter();

    view_dispatcher_run(subghz->view_dispatcher);

    furi_hal_power_suppress_charge_exit();

    subghz_free(subghz, alloc_for_tx);

    return 0;
}
