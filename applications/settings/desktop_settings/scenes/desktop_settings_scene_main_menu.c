#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
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
#define MAIN_MENU_PINS_FILE_NAME ".main_menu.pins"

typedef struct {
    char paths[MAIN_MENU_PINS_MAX][MAIN_MENU_PINS_PATH_LEN];
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
                    strlcpy(pins->paths[pins->count], line, sizeof(pins->paths[pins->count]));
                    pins->count++;
                    line_len = 0;
                }
            } else if(line_len < sizeof(line) - 1) {
                line[line_len++] = (char)byte;
            }
        }

        if(line_len > 0 && pins->count < MAIN_MENU_PINS_MAX) {
            line[line_len] = '\0';
            strlcpy(pins->paths[pins->count], line, sizeof(pins->paths[pins->count]));
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
            storage_file_write(file, "\n", 1);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void main_menu_pin_label(const char* path, char* out, size_t out_size) {
    const char* slash = strrchr(path, '/');
    const char* base = slash ? slash + 1 : path;
    strlcpy(out, base, out_size);
    size_t len = strlen(out);
    if(len > 4 && strcmp(out + len - 4, ".fap") == 0) {
        out[len - 4] = '\0';
    }
}

static bool main_menu_selector_item_callback(
    FuriString* file_path,
    void* context,
    uint8_t** icon_ptr,
    FuriString* item_name) {
    UNUSED(file_path);
    UNUSED(context);
    UNUSED(icon_ptr);
    UNUSED(item_name);
    return false;
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

    for(uint8_t i = 0; i < s_pins.count; i++) {
        char label[MAIN_MENU_PINS_PATH_LEN];
        main_menu_pin_label(s_pins.paths[i], label, sizeof(label));
        submenu_add_item(
            submenu, label, i, desktop_settings_scene_main_menu_submenu_callback, app);
    }

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
                const DialogsFileBrowserOptions browser_options = {
                    .extension = ".fap",
                    .icon = NULL,
                    .skip_assets = true,
                    .hide_ext = true,
                    .item_loader_callback = main_menu_selector_item_callback,
                    .item_loader_context = app,
                    .base_path = EXT_PATH("apps"),
                };

                if(dialog_file_browser_show(app->dialogs, temp_path, temp_path, &browser_options)) {
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
