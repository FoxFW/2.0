#include <stream/stream.h>
#include "stream/file_stream.h"

#include "helper.h"
#include "nrf24tool.h"
#include "libnrf24/nrf24.h"
#include "tx/tx.h"
#include "sniff/sniff.h"
#include "badmouse/badmouse.h"
#include "rx/rx.h"
#include "settings.h"

// Main app variable
Nrf24Tool* nrf24Tool_app = NULL;

// application settings file path
static const char* FILE_PATH_SETTINGS = APP_DATA_PATH("settings.conf");

bool nrf24_test_connection(Nrf24Tool* app, uint32_t from_view, uint32_t to_view) {
    if(nrf24_check_connected()) {
        if(from_view != to_view) {
            view_dispatcher_switch_to_view(app->view_dispatcher, to_view);
            return true;
        } else {
            return true;
        }
    } else {
        app->previous_view = from_view;
        app->next_view = to_view;
        if(from_view == to_view) {
            dialog_ex_set_right_button_text(app->nrf24_disconnect, NULL);
            app->tool_running = false;
        } else {
            dialog_ex_set_right_button_text(app->nrf24_disconnect, "Retry");
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_NRF24_NOT_CONNECTED);
        return false;
    }
}

static bool load_setting(Nrf24Tool* app) {
    size_t file_size = 0;

    // set defaults settings
    app->settings = &nrf24Tool_settings;
    memcpy(app->settings->rx_settings, rx_defaults, sizeof(rx_defaults));
    memcpy(app->settings->tx_settings, tx_defaults, sizeof(tx_defaults));
    memcpy(app->settings->sniff_settings, sniff_defaults, sizeof(sniff_defaults));
    memcpy(app->settings->badmouse_settings, badmouse_defaults, sizeof(badmouse_defaults));

    app->storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(app->storage);

    if(file_stream_open(stream, FILE_PATH_SETTINGS, FSAM_READ, FSOM_OPEN_EXISTING)) {
        file_size = stream_size(stream);
        stream_seek(stream, 0, StreamOffsetFromStart);

        if(file_size > 0) {
            FuriString* line = furi_string_alloc();
            // Lire le fichier ligne par ligne
            while(stream_read_line(stream, line)) {
                // skip line if comment
                furi_string_trim(line);
                char first_char = furi_string_get_char(line, 0);
                if(first_char == '#' || first_char == ' ') continue;

                // find parameter name + value
                size_t equal_pos = furi_string_search_char(line, '=');
                if(equal_pos == FURI_STRING_FAILURE) continue;
                FuriString* key = furi_string_alloc_set(line);
                FuriString* value = furi_string_alloc_set(line);
                furi_string_left(key, equal_pos);
                furi_string_right(value, equal_pos + 1);

                // find parameter name in settings map
                for(uint8_t i = 0; i < SETTINGS_QTY; i++) {
                    if(furi_string_cmp_str(key, settings_map[i].key) == 0) {
                        // Fetch the string value
                        const char* value_str = furi_string_get_cstr(value);

                        // Handle conversion for numeric types only
                        int value_int = 0;
                        value_int = atoi(value_str);

                        switch(settings_map[i].type) {
                        case SETTING_TYPE_UINT8:
                            *((uint8_t*)settings_map[i].target) = (uint8_t)value_int;
                            break;
                        case SETTING_TYPE_UINT16:
                            *((uint16_t*)settings_map[i].target) = (uint16_t)value_int;
                            break;
                        case SETTING_TYPE_UINT32:
                            *((uint32_t*)settings_map[i].target) = (uint32_t)value_int;
                            break;
                        case SETTING_TYPE_BOOL:
                            if(value_int == 1)
                                *((bool*)settings_map[i].target) = true;
                            else
                                *((bool*)settings_map[i].target) = false;
                            break;
                        case SETTING_TYPE_DATA_RATE:
                            *((nrf24_data_rate*)settings_map[i].target) = (uint8_t)value_int;
                            break;
                        case SETTING_TYPE_TX_POWER:
                            *((nrf24_tx_power*)settings_map[i].target) = (uint8_t)value_int;
                            break;
                        case SETTING_TYPE_ADDR_WIDTH:
                            *((nrf24_addr_width*)settings_map[i].target) = (uint8_t)value_int;
                            break;
                        case SETTING_TYPE_PAYLOAD_SIZE:
                            *((nrf24_payload*)settings_map[i].target) = (uint8_t)value_int;
                            break;
                        case SETTING_TYPE_ADDR:
                            if(strlen(value_str) == HEX_MAC_LEN) {
                                unhexlify(
                                    value_str, BYTE_MAC_LEN, (uint8_t*)settings_map[i].target);
                            }
                            break;
                        case SETTING_TYPE_ADDR_1BYTE:
                            if(strlen(value_str) == HEX_MAC_LEN_1BYTE) {
                                unhexlify(
                                    value_str,
                                    BYTE_MAC_LEN_1BYTE,
                                    (uint8_t*)settings_map[i].target);
                            }
                            break;
                        case SETTING_TYPE_CRC_LENGHT:
                            *((nrf24_crc_lenght*)settings_map[i].target) = (uint8_t)value_int;
                            break;
                        case SETTING_TYPE_PIPE_NUM:
                            *((int8_t*)settings_map[i].target) = (int8_t)value_int;
                            break;
                        }
                    }
                }
                furi_string_free(key);
                furi_string_free(value);
            }
            furi_string_free(line);
        }
    }
    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);

    return true;
}

static bool save_setting(Nrf24Tool* context) {
    //size_t file_size = 0;
    bool ret = false;

    context->storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(context->storage);
    char hex_addr[HEX_MAC_LEN + 1];

    if(file_stream_open(stream, FILE_PATH_SETTINGS, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        for(uint8_t i = 0; i < SETTINGS_QTY; i++) {
            switch(settings_map[i].type) {
            case SETTING_TYPE_UINT8:
                stream_write_format(
                    stream, "%s=%u\n", settings_map[i].key, *((uint8_t*)settings_map[i].target));
                continue;
            case SETTING_TYPE_UINT16:
                stream_write_format(
                    stream, "%s=%u\n", settings_map[i].key, *((uint16_t*)settings_map[i].target));
                continue;
            case SETTING_TYPE_UINT32:
                stream_write_format(
                    stream, "%s=%lu\n", settings_map[i].key, *((uint32_t*)settings_map[i].target));
                continue;
            case SETTING_TYPE_BOOL:
                if(*(bool*)settings_map[i].target == true)
                    stream_write_format(stream, "%s=%u\n", settings_map[i].key, 1);
                else
                    stream_write_format(stream, "%s=%u\n", settings_map[i].key, 0);
                continue;
            case SETTING_TYPE_DATA_RATE:
            case SETTING_TYPE_TX_POWER:
            case SETTING_TYPE_ADDR_WIDTH:
            case SETTING_TYPE_CRC_LENGHT:
            case SETTING_TYPE_PAYLOAD_SIZE:
                stream_write_format(
                    stream, "%s=%u\n", settings_map[i].key, *((uint8_t*)settings_map[i].target));
                continue;
            case SETTING_TYPE_ADDR:
                hexlify((uint8_t*)settings_map[i].target, BYTE_MAC_LEN, hex_addr);
                stream_write_format(stream, "%s=%s\n", settings_map[i].key, hex_addr);
                continue;
            case SETTING_TYPE_ADDR_1BYTE:
                hexlify((uint8_t*)settings_map[i].target, BYTE_MAC_LEN_1BYTE, hex_addr);
                stream_write_format(stream, "%s=%s\n", settings_map[i].key, hex_addr);
                continue;
            case SETTING_TYPE_PIPE_NUM:
                stream_write_format(
                    stream, "%s=%d\n", settings_map[i].key, *((int8_t*)settings_map[i].target));
                continue;
            }
        }
        ret = true;
    } else {
        FURI_LOG_E(LOG_TAG, "Error saving settings to file");
    }

    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);

    return ret;
}

/* Allocate the memory and initialise the variables */
static Nrf24Tool* nrf24Tool_alloc(void) {
    Nrf24Tool* app = malloc(sizeof(Nrf24Tool));

    Gui* gui = furi_record_open(RECORD_GUI);
    // View dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    // Main menu
    app->app_menu = submenu_alloc();
    //submenu_set_header(app->app_menu, "Select Tool");
    submenu_add_item(app->app_menu, "Reception", MENU_RX, app_menu_enter_callback, app);
    submenu_add_item(app->app_menu, "Emission", MENU_TX, app_menu_enter_callback, app);
    submenu_add_item(app->app_menu, "Sniffer", MENU_SNIFFER, app_menu_enter_callback, app);
    submenu_add_item(app->app_menu, "Bad Mouse", MENU_BADMOUSE, app_menu_enter_callback, app);
    view_set_previous_callback(submenu_get_view(app->app_menu), app_menu_exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_MENU, submenu_get_view(app->app_menu));
    // NRF24 disconnected view
    app->nrf24_disconnect = dialog_ex_alloc();
    dialog_ex_set_context(app->nrf24_disconnect, app);
    dialog_ex_set_result_callback(app->nrf24_disconnect, no_nrf24_result_callback);
    dialog_ex_set_header(app->nrf24_disconnect, "ERROR", 64, 2, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->nrf24_disconnect, "NRF24 Disconnected", 64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->nrf24_disconnect, "Cancel");
    view_dispatcher_add_view(
        app->view_dispatcher, VIEW_NRF24_NOT_CONNECTED, dialog_ex_get_view(app->nrf24_disconnect));

    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    app->notification = furi_record_open(RECORD_NOTIFICATION);

    return app;
}

/* Release the unused resources and deallocate memory */
static void nrf24Tool_free(Nrf24Tool* app) {
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_MENU);
    submenu_free(app->app_menu);
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_NRF24_NOT_CONNECTED);
    dialog_ex_free(app->nrf24_disconnect);

    view_dispatcher_free(app->view_dispatcher);

    furi_message_queue_free(app->event_queue);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

/* Starts the reader thread and handles the input */
static void nrf24Tool_run(Nrf24Tool* app) {
    // Init NRF24 communication
    nrf24_init_bus();

    // load application settings
    if(!load_setting(app)) FURI_LOG_E(LOG_TAG, "Unable to load application settings !");

    // Alloc tools
    rx_alloc(app);
    tx_alloc(app);
    sniff_alloc(app);
    badmouse_alloc(app);

    // Test if NRF24 is connected
    nrf24_test_connection(app, VIEW_NONE, VIEW_MENU);

    // Program main
    view_dispatcher_run(app->view_dispatcher);

    // Free tools
    rx_free(app);
    tx_free(app);
    sniff_free(app);
    badmouse_free(app);

    // Deinitialize the nRF24 module
    nrf24_deinit_bus();
}

int32_t nrf24tool_app(void* p) {
    UNUSED(p);

    //nrf24_sniff(3000);
    nrf24Tool_app = nrf24Tool_alloc();

    // run program
    nrf24Tool_run(nrf24Tool_app);

    // save settings before exit
    save_setting(nrf24Tool_app);

    // free program
    nrf24Tool_free(nrf24Tool_app);

    return 0;
}
