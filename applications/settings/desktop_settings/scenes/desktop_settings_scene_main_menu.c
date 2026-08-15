#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <flipper_application/flipper_application.h>
#include <string.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"
#include "desktop_settings_scene_i.h"

// Format shared with applications/services/loader/loader_main_menu_pins.c —
// keep these three in sync with that file. Duplicated (not a shared header)
// because external .fap apps can't reliably include another app's private
// headers; the two sides only agree via this plain-text file on disk.
#define MAIN_MENU_PINS_MAX       12
#define MAIN_MENU_PINS_PATH_LEN  128
#define MAIN_MENU_PINS_NAME_LEN  7 // 6-char custom label + NUL
#define MAIN_MENU_PINS_FILE_NAME ".main_menu.pins"

typedef struct {
    char paths[MAIN_MENU_PINS_MAX][MAIN_MENU_PINS_PATH_LEN];
    char names[MAIN_MENU_PINS_MAX][MAIN_MENU_PINS_NAME_LEN];
    uint8_t count;
} MainMenuPinsUI;

static MainMenuPinsUI s_pins;

#define ADD_APP_INDEX (0xFFFF)

static void main_menu_pins_load(MainMenuPinsUI* pins) {
    pins->count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, INT_PATH(MAIN_MENU_PINS_FILE_NAME), FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[MAIN_MENU_PINS_PATH_LEN];
        size_t line_len = 0;
        uint8_t byte;

        while(pins->count < MAIN_MENU_PINS_MAX && storage_file_read(file, &byte, 1) == 1) {
            if(byte == '\n' || byte == '\r') {
                if(line_len > 0) {
                    line[line_len] = '\0';
                    char* sep = strchr(line, '|');
                    if(sep) {
                        *sep = '\0';
                        strlcpy(pins->paths[pins->count], line, sizeof(pins->paths[pins->count]));
                        strlcpy(
                            pins->names[pins->count], sep + 1, sizeof(pins->names[pins->count]));
                    } else {
                        strlcpy(pins->paths[pins->count], line, sizeof(pins->paths[pins->count]));
                        pins->names[pins->count][0] = '\0';
                    }
                    pins->count++;
                    line_len = 0;
                }
            } else if(line_len < sizeof(line) - 1) {
                line[line_len++] = (char)byte;
            }
        }

        if(line_len > 0 && pins->count < MAIN_MENU_PINS_MAX) {
            line[line_len] = '\0';
            char* sep = strchr(line, '|');
            if(sep) {
                *sep = '\0';
                strlcpy(pins->paths[pins->count], line, sizeof(pins->paths[pins->count]));
                strlcpy(pins->names[pins->count], sep + 1, sizeof(pins->names[pins->count]));
            } else {
                strlcpy(pins->paths[pins->count], line, sizeof(pins->paths[pins->count]));
                pins->names[pins->count][0] = '\0';
            }
            pins->count++;
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void main_menu_pins_save(const MainMenuPinsUI* pins) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(
           file, INT_PATH(MAIN_MENU_PINS_FILE_NAME), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        for(uint8_t i = 0; i < pins->count; i++) {
            storage_file_write(file, pins->paths[i], strlen(pins->paths[i]));
            if(pins->names[i][0] != '\0') {
                storage_file_write(file, "|", 1);
                storage_file_write(file, pins->names[i], strlen(pins->names[i]));
            }
            storage_file_write(file, "\n", 1);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void main_menu_pin_label_from_filename(const char* path, char* out, size_t out_size) {
    const char* slash = strrchr(path, '/');
    const char* base = slash ? slash + 1 : path;
    strlcpy(out, base, out_size);
    size_t len = strlen(out);
    if(len > 4 && strcmp(out + len - 4, ".fap") == 0) {
        out[len - 4] = '\0';
    }
}

static void main_menu_pin_label(
    Storage* storage,
    const char* path,
    const char* custom_name,
    char* out,
    size_t out_size) {
    if(custom_name && custom_name[0] != '\0') {
        strlcpy(out, custom_name, out_size);
        return;
    }

    FuriString* path_str = furi_string_alloc_set_str(path);
    FuriString* name_str = furi_string_alloc();
    uint8_t icon_buf[FAP_MANIFEST_MAX_ICON_SIZE];
    uint8_t* icon_ptr = icon_buf;

    bool loaded = flipper_application_load_name_and_icon(path_str, storage, &icon_ptr, name_str);
    if(loaded && !furi_string_empty(name_str)) {
        strlcpy(out, furi_string_get_cstr(name_str), out_size);
    } else {
        main_menu_pin_label_from_filename(path, out, out_size);
    }

    furi_string_free(path_str);
    furi_string_free(name_str);
}

static bool main_menu_selector_item_callback(
    FuriString* file_path,
    void* context,
    uint8_t** icon_ptr,
    FuriString* item_name) {
    Storage* storage = context;
    return flipper_application_load_name_and_icon(file_path, storage, icon_ptr, item_name);
}

static void desktop_settings_scene_main_menu_submenu_callback(void* context, uint32_t index) {
    DesktopSettingsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void desktop_settings_scene_main_menu_on_enter(void* context) {
    DesktopSettingsApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);

    main_menu_pins_load(&s_pins);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    for(uint8_t i = 0; i < s_pins.count; i++) {
        char label[MAIN_MENU_PINS_PATH_LEN];
        main_menu_pin_label(storage, s_pins.paths[i], s_pins.names[i], label, sizeof(label));
        submenu_add_item(
            submenu, label, i, desktop_settings_scene_main_menu_submenu_callback, app);
    }
    furi_record_close(RECORD_STORAGE);

    submenu_add_item(
        submenu,
        "+ Add App",
        ADD_APP_INDEX,
        desktop_settings_scene_main_menu_submenu_callback,
        app);

    submenu_set_header(submenu, "Main Menu Apps");
    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewMenu);
}

bool desktop_settings_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    DesktopSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ADD_APP_INDEX) {
            if(s_pins.count < MAIN_MENU_PINS_MAX) {
                FuriString* temp_path = furi_string_alloc_set_str(EXT_PATH("apps"));
                Storage* storage = furi_record_open(RECORD_STORAGE);
                const DialogsFileBrowserOptions browser_options = {
                    .extension = ".fap",
                    .icon = NULL,
                    .skip_assets = true,
                    .hide_ext = true,
                    .item_loader_callback = main_menu_selector_item_callback,
                    .item_loader_context = storage,
                    .base_path = EXT_PATH("apps"),
                };

                bool picked_one =
                    dialog_file_browser_show(app->dialogs, temp_path, temp_path, &browser_options);
                furi_record_close(RECORD_STORAGE);
                if(picked_one) {
                    const char* picked = furi_string_get_cstr(temp_path);
                    bool duplicate = false;
                    for(uint8_t i = 0; i < s_pins.count; i++) {
                        if(strcmp(s_pins.paths[i], picked) == 0) {
                            duplicate = true;
                            break;
                        }
                    }
                    if(!duplicate) {
                        strlcpy(
                            s_pins.paths[s_pins.count],
                            picked,
                            sizeof(s_pins.paths[s_pins.count]));

                        Storage* name_storage = furi_record_open(RECORD_STORAGE);
                        char full_name[MAIN_MENU_PINS_PATH_LEN];
                        main_menu_pin_label(
                            name_storage, picked, NULL, full_name, sizeof(full_name));
                        furi_record_close(RECORD_STORAGE);
                        strlcpy(
                            s_pins.names[s_pins.count],
                            full_name,
                            sizeof(s_pins.names[s_pins.count]));

                        s_pins.count++;
                        main_menu_pins_save(&s_pins);
                    }
                }
                furi_string_free(temp_path);
            }
            submenu_reset(app->submenu);
            desktop_settings_scene_main_menu_on_enter(app);
            consumed = true;
        } else {
            scene_manager_set_scene_state(
                app->scene_manager, DesktopSettingsAppSceneMainMenuActions, event.event);
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppSceneMainMenuActions);
            consumed = true;
        }
    }

    return consumed;
}

void desktop_settings_scene_main_menu_on_exit(void* context) {
    DesktopSettingsApp* app = context;
    submenu_reset(app->submenu);
}
