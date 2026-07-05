// scenes/gdr_scene_receiver_info.c
#include "../gdr_app_i.h"
#include "../helpers/gdr_storage.h"
#include "../helpers/gdr_psa_bf_host.h"
#include "gdr_icons.h"

#define TAG "GDRReceiverInfo"

static void gdr_scene_receiver_info_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context);

static void gdr_scene_receiver_info_text_input_callback(void* context) {
    GDRApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, GDRCustomEventReceiverInfoSaveConfirm);
}

static void gdr_receiver_info_build_normal_widget(GDRApp* app) {
    widget_reset(app->widget);

    GDRHistory* history = gdr_selected_capture_get_history(app);
    uint16_t selected_index = gdr_selected_capture_get_index(app);
    if(!history) {
        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Capture unavailable");
        return;
    }

    FuriString* text = furi_string_alloc();
    gdr_history_get_text_item_menu(history, text, selected_index);
    widget_add_string_element(
        app->widget, 64, 0, AlignCenter, AlignTop, FontPrimary, furi_string_get_cstr(text));

    furi_string_reset(text);
    gdr_history_get_text_item_detail(
        history, selected_index, text, app->txrx->environment);

    bool is_psa = false;
    FlipperFormat* ff = gdr_selected_capture_get_raw_data(app);
    if(ff) {
        FuriString* protocol = furi_string_alloc();
        flipper_format_rewind(ff);
        if(flipper_format_read_string(ff, FF_PROTOCOL, protocol)) {
            if(furi_string_cmp_str(protocol, "PSA") == 0) is_psa = true;
            app->emulate_disabled_for_loaded = (furi_string_cmp_str(protocol, "Scher-Khan") == 0);
        }
        furi_string_free(protocol);
    }

    const char* text_str = furi_string_get_cstr(text);
    const char* first_newline = strchr(text_str, '\r');
    if(first_newline) {
        text_str = first_newline + 1;
        if(*text_str == '\n') text_str++;
    } else {
        first_newline = strchr(text_str, '\n');
        if(first_newline) text_str = first_newline + 1;
    }

    FuriString* display_text = furi_string_alloc();
    GDRHistorySource source = gdr_selected_capture_get_source(app);
    if(source != GDRHistorySourceUnknown) {
        furi_string_cat_printf(
            display_text, "Receiver: %s\r\n", gdr_history_source_name(source));
    }
    furi_string_cat_str(display_text, text_str);
    text_str = furi_string_get_cstr(display_text);

    if(is_psa) {
        FuriString* reformatted = furi_string_alloc();
        const char* current = text_str;
        while(*current) {
            const char* line_end = strchr(current, '\r');
            if(!line_end) line_end = strchr(current, '\n');
            if(!line_end) line_end = current + strlen(current);

            if(strncmp(current, "Ser:", 4) == 0) {
                size_t ser_len = line_end - current;
                furi_string_cat_printf(reformatted, "%.*s", (int)ser_len, current);
                const char* next_line = line_end;
                if(*next_line == '\r') next_line++;
                if(*next_line == '\n') next_line++;
                if(strncmp(next_line, "Cnt:", 4) == 0) {
                    const char* cnt_end = strchr(next_line, '\r');
                    if(!cnt_end) cnt_end = strchr(next_line, '\n');
                    if(!cnt_end) cnt_end = next_line + strlen(next_line);
                    furi_string_cat_printf(
                        reformatted, " %.*s\r\n", (int)(cnt_end - next_line), next_line);
                    current = cnt_end;
                } else {
                    furi_string_cat_printf(reformatted, "\r\n");
                    current = line_end;
                }
                if(*current == '\r') current++;
                if(*current == '\n') current++;
            } else {
                size_t line_len = line_end - current;
                furi_string_cat_printf(reformatted, "%.*s\r\n", (int)line_len, current);
                current = line_end;
                if(*current == '\r') current++;
                if(*current == '\n') current++;
            }
            if(*current == '\0') break;
        }
        widget_add_string_multiline_element(
            app->widget,
            0,
            11,
            AlignLeft,
            AlignTop,
            FontSecondary,
            furi_string_get_cstr(reformatted));
        furi_string_free(reformatted);
    } else {
        widget_add_string_multiline_element(
            app->widget, 0, 11, AlignLeft, AlignTop, FontSecondary, text_str);
    }

    bool psa_needs_bf = false;
    if(is_psa && gdr_psa_bf_plugin_ensure_loaded(app) && app->psa_bf_plugin) {
        psa_needs_bf = app->psa_bf_plugin->widget_left_should_bruteforce(
            app, GDRPsaBfContextReceiverInfo);
    }
    if(psa_needs_bf) {
        widget_add_button_element(
            app->widget,
            GuiButtonTypeLeft,
            "Brute force",
            gdr_scene_receiver_info_widget_callback,
            app);
    } else
#ifdef ENABLE_EMULATE_FEATURE
    if(app->emulate_feature_enabled && !app->emulate_disabled_for_loaded) {
        widget_add_button_element(
            app->widget,
            GuiButtonTypeLeft,
            "Emulate",
            gdr_scene_receiver_info_widget_callback,
            app);
    }
#endif

    widget_add_button_element(
        app->widget,
        GuiButtonTypeRight,
        "Save",
        gdr_scene_receiver_info_widget_callback,
        app);

    furi_string_free(display_text);
    furi_string_free(text);
}

void gdr_receiver_info_rebuild_normal_widget(void* app) {
    gdr_receiver_info_build_normal_widget((GDRApp*)app);
}

static void gdr_scene_receiver_info_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    GDRApp* app = context;
    if(type == InputTypeShort || type == InputTypeLong) {
        if(result == GuiButtonTypeRight) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, GDRCustomEventReceiverInfoSave);
        } else if(result == GuiButtonTypeLeft) {
            if(gdr_psa_bf_plugin_ensure_loaded(app) && app->psa_bf_plugin &&
               app->psa_bf_plugin->widget_left_should_bruteforce(
                   app, GDRPsaBfContextReceiverInfo)) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, GDRCustomEventReceiverInfoBruteforceStart);
            } else
#ifdef ENABLE_EMULATE_FEATURE
            if(app->emulate_feature_enabled && !app->emulate_disabled_for_loaded) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, GDRCustomEventReceiverInfoEmulate);
            }
#endif
        } else if(result == GuiButtonTypeCenter) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, GDRCustomEventReceiverInfoBruteforceCancel);
        }
    }
}

void gdr_scene_receiver_info_on_enter(void* context) {
    furi_check(context);
    GDRApp* app = context;

    if(!gdr_ensure_widget(app) || !gdr_ensure_text_input(app)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    app->emulate_disabled_for_loaded = false;

    if(!gdr_selected_capture_is_valid(app) && app->txrx && app->txrx->history &&
       app->txrx->idx_menu_chosen < gdr_history_get_item(app->txrx->history)) {
        gdr_selected_capture_set(
            app,
            app->txrx->history,
            NULL,
            app->txrx->idx_menu_chosen,
            GDRCaptureOwnerReceiver);
    }

    if(gdr_psa_bf_plugin_ensure_loaded(app) && app->psa_bf_plugin) {
        if(app->psa_bf_plugin->is_running(app)) {
            app->psa_bf_plugin->on_scene_enter(app, GDRPsaBfContextReceiverInfo);
            view_dispatcher_switch_to_view(app->view_dispatcher, GDRViewWidget);
            return;
        }
    }

    gdr_receiver_info_build_normal_widget(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, GDRViewWidget);
}

bool gdr_scene_receiver_info_on_event(void* context, SceneManagerEvent event) {
    GDRApp* app = context;
    bool consumed = false;

    if(gdr_psa_bf_plugin_ensure_loaded(app) && app->psa_bf_plugin) {
        if(app->psa_bf_plugin->is_running(app) ||
           event.event == GDRCustomEventPsaBruteforceComplete ||
           event.event == GDRCustomEventReceiverInfoBruteforceStart ||
           event.event == GDRCustomEventReceiverInfoBruteforceCancel) {
            consumed = app->psa_bf_plugin->on_scene_event(
                app, GDRPsaBfContextReceiverInfo, event);
            if(consumed) return true;
        }
        if(event.type == SceneManagerEventTypeBack &&
           app->psa_bf_plugin->on_scene_event(
               app, GDRPsaBfContextReceiverInfo, event)) {
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GDRCustomEventReceiverInfoSave) {
            FlipperFormat* ff = gdr_selected_capture_get_raw_data(app);
            if(ff) {
                FuriString* protocol = furi_string_alloc();
                flipper_format_rewind(ff);
                if(!flipper_format_read_string(ff, FF_PROTOCOL, protocol)) {
                    furi_string_set_str(protocol, "Unknown");
                }

                furi_string_replace_all(protocol, "/", "_");
                furi_string_replace_all(protocol, " ", "_");

                FuriString* auto_path = furi_string_alloc();
                if(gdr_storage_get_next_filename(
                       furi_string_get_cstr(protocol), auto_path)) {
                    const char* full = furi_string_get_cstr(auto_path);
                    const char* slash = strrchr(full, '/');
                    const char* name_start = slash ? slash + 1 : full;

                    size_t name_len = strlen(name_start);
                    const char* dot = strrchr(name_start, '.');
                    if(dot) name_len = dot - name_start;
                    if(name_len >= sizeof(app->save_filename))
                        name_len = sizeof(app->save_filename) - 1;

                    memcpy(app->save_filename, name_start, name_len);
                    app->save_filename[name_len] = '\0';
                } else {
                    snprintf(app->save_filename, sizeof(app->save_filename), "capture");
                }
                furi_string_free(auto_path);

                if(app->save_protocol) furi_string_free(app->save_protocol);
                app->save_protocol = protocol;
                app->save_history_idx = gdr_selected_capture_get_index(app);
                app->save_from_saved_info = false;

                text_input_reset(app->text_input);
                text_input_set_header_text(app->text_input, "Save filename:");
                text_input_set_result_callback(
                    app->text_input,
                    gdr_scene_receiver_info_text_input_callback,
                    app,
                    app->save_filename,
                    sizeof(app->save_filename),
                    false);

                view_dispatcher_switch_to_view(app->view_dispatcher, GDRViewTextInput);
            }
            consumed = true;
        }

        if(event.event == GDRCustomEventReceiverInfoSaveConfirm) {
            GDRHistory* history = gdr_selected_capture_get_history(app);
            FlipperFormat* ff =
                history ? gdr_history_get_raw_data(history, app->save_history_idx) : NULL;
            if(ff) {
                FuriString* save_path = furi_string_alloc_printf(
                    "%s/%s%s",
                    GDR_APP_FOLDER,
                    app->save_filename,
                    GDR_APP_EXTENSION);

                if(gdr_storage_save_capture_to_path(ff, furi_string_get_cstr(save_path))) {
                    notification_message(app->notifications, &sequence_success);
                    FURI_LOG_I(TAG, "Saved to: %s", furi_string_get_cstr(save_path));
                } else {
                    notification_message(app->notifications, &sequence_error);
                    FURI_LOG_E(TAG, "Save failed");
                }
                furi_string_free(save_path);
            }

            if(app->save_protocol) {
                furi_string_free(app->save_protocol);
                app->save_protocol = NULL;
            }

            view_dispatcher_switch_to_view(app->view_dispatcher, GDRViewWidget);
            consumed = true;
        }

#ifdef ENABLE_EMULATE_FEATURE
        if(event.event == GDRCustomEventReceiverInfoEmulate &&
           app->emulate_feature_enabled && !app->emulate_disabled_for_loaded) {
            FuriString* hist_path = furi_string_alloc();
            if(gdr_selected_capture_get_path(app, hist_path)) {
                gdr_selected_capture_release_scratch(app);
                if(app->loaded_file_path) furi_string_free(app->loaded_file_path);
                app->loaded_file_path = furi_string_alloc_set(hist_path);
                furi_string_free(hist_path);
                FURI_LOG_I(
                    TAG,
                    "Emulate from history file: %s",
                    furi_string_get_cstr(app->loaded_file_path));
                scene_manager_next_scene(app->scene_manager, GDRSceneEmulate);
            } else {
                furi_string_free(hist_path);
                FURI_LOG_E(
                    TAG,
                    "No capture path for index %d",
                    gdr_selected_capture_get_index(app));
                notification_message(app->notifications, &sequence_error);
            }
            consumed = true;
        }
#endif
    }

    return consumed;
}

void gdr_scene_receiver_info_on_exit(void* context) {
    furi_check(context);
    GDRApp* app = context;
    widget_reset(app->widget);
    gdr_selected_capture_release_scratch(app);
    gdr_psa_bf_plugin_unload_if_idle(app);
}
