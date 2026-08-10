#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <storage/storage.h>
#include <string.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"
#include "desktop_settings_scene_i.h"

// Kept in sync with desktop_settings_scene_main_menu.c and
// applications/services/loader/loader_main_menu_pins.c — see the comment
// there for why this is duplicated rather than shared via a header.
#define MAIN_MENU_PINS_MAX       12
#define MAIN_MENU_PINS_PATH_LEN  128
#define MAIN_MENU_PINS_FILE_NAME ".main_menu.pins"

typedef struct {
    char paths[MAIN_MENU_PINS_MAX][MAIN_MENU_PINS_PATH_LEN];
    uint8_t count;
} MainMenuPinsUI;

typedef enum {
    MainMenuActionMoveUp,
    MainMenuActionMoveDown,
    MainMenuActionRemove,
    MainMenuActionCancel,
} MainMenuAction;

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

static void desktop_settings_scene_main_menu_actions_submenu_callback(
    void* context,
    uint32_t index) {
    DesktopSettingsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void desktop_settings_scene_main_menu_actions_on_enter(void* context) {
    DesktopSettingsApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);

    uint32_t pin_index = scene_manager_get_scene_state(
        app->scene_manager, DesktopSettingsAppSceneMainMenuActions);

    MainMenuPinsUI pins;
    main_menu_pins_load(&pins);

    char header[MAIN_MENU_PINS_PATH_LEN + 8] = "App";
    if(pin_index < pins.count) {
        char label[MAIN_MENU_PINS_PATH_LEN];
        main_menu_pin_label(pins.paths[pin_index], label, sizeof(label));
        strlcpy(header, label, sizeof(header));
    }

    submenu_add_item(
        submenu,
        "Move Up",
        MainMenuActionMoveUp,
        desktop_settings_scene_main_menu_actions_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Move Down",
        MainMenuActionMoveDown,
        desktop_settings_scene_main_menu_actions_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Remove",
        MainMenuActionRemove,
        desktop_settings_scene_main_menu_actions_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Cancel",
        MainMenuActionCancel,
        desktop_settings_scene_main_menu_actions_submenu_callback,
        app);

    submenu_set_header(submenu, header);
    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewMenu);
}

bool desktop_settings_scene_main_menu_actions_on_event(void* context, SceneManagerEvent event) {
    DesktopSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t pin_index = scene_manager_get_scene_state(
            app->scene_manager, DesktopSettingsAppSceneMainMenuActions);

        MainMenuPinsUI pins;
        main_menu_pins_load(&pins);

        if(pin_index < pins.count) {
            if(event.event == MainMenuActionMoveUp) {
                if(pin_index > 0) {
                    char tmp[MAIN_MENU_PINS_PATH_LEN];
                    strlcpy(tmp, pins.paths[pin_index], sizeof(tmp));
                    strlcpy(
                        pins.paths[pin_index],
                        pins.paths[pin_index - 1],
                        sizeof(pins.paths[pin_index]));
                    strlcpy(pins.paths[pin_index - 1], tmp, sizeof(pins.paths[pin_index - 1]));
                    main_menu_pins_save(&pins);
                }
            } else if(event.event == MainMenuActionMoveDown) {
                if(pin_index + 1 < pins.count) {
                    char tmp[MAIN_MENU_PINS_PATH_LEN];
                    strlcpy(tmp, pins.paths[pin_index], sizeof(tmp));
                    strlcpy(
                        pins.paths[pin_index],
                        pins.paths[pin_index + 1],
                        sizeof(pins.paths[pin_index]));
                    strlcpy(pins.paths[pin_index + 1], tmp, sizeof(pins.paths[pin_index + 1]));
                    main_menu_pins_save(&pins);
                }
            } else if(event.event == MainMenuActionRemove) {
                for(uint8_t i = pin_index; i + 1 < pins.count; i++) {
                    strlcpy(pins.paths[i], pins.paths[i + 1], sizeof(pins.paths[i]));
                }
                pins.count--;
                main_menu_pins_save(&pins);
            }
        }

        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void desktop_settings_scene_main_menu_actions_on_exit(void* context) {
    DesktopSettingsApp* app = context;
    submenu_reset(app->submenu);
}
