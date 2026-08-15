#include <gui/scene_manager.h>
#include <gui/modules/text_input.h>
#include <storage/storage.h>
#include <flipper_application/flipper_application.h>
#include <string.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"
#include "desktop_settings_scene_i.h"

// Kept in sync with desktop_settings_scene_main_menu.c,
// desktop_settings_scene_main_menu_actions.c and
// applications/services/loader/loader_main_menu_pins.c.
#define MAIN_MENU_PINS_MAX       12
#define MAIN_MENU_PINS_PATH_LEN  128
#define MAIN_MENU_PINS_NAME_LEN  7 // 6-char custom label + NUL
#define MAIN_MENU_PINS_FILE_NAME ".main_menu.pins"

typedef struct {
    char paths[MAIN_MENU_PINS_MAX][MAIN_MENU_PINS_PATH_LEN];
    char names[MAIN_MENU_PINS_MAX][MAIN_MENU_PINS_NAME_LEN];
    uint8_t count;
} MainMenuPinsUI;

typedef enum {
    RenameResultOk,
} RenameResult;

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

static void
    main_menu_pin_label(Storage* storage, const char* path, char* out, size_t out_size) {
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

static void desktop_settings_scene_main_menu_rename_text_input_callback(void* context) {
    DesktopSettingsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, RenameResultOk);
}

static bool desktop_settings_scene_main_menu_rename_validator(
    const char* text,
    FuriString* error,
    void* context) {
    UNUSED(context);
    for(; *text; ++text) {
        if(*text == '|') {
            furi_string_printf(error, "Can't use\nthe '|'\ncharacter!");
            return false;
        }
    }
    return true;
}

void desktop_settings_scene_main_menu_rename_on_enter(void* context) {
    DesktopSettingsApp* app = context;
    TextInput* text_input = app->text_input;

    uint32_t pin_index = scene_manager_get_scene_state(
        app->scene_manager, DesktopSettingsAppSceneMainMenuRename);

    MainMenuPinsUI* pins = malloc(sizeof(MainMenuPinsUI)); // too big for a stack local here
    main_menu_pins_load(pins);

    app->main_menu_rename_buffer[0] = '\0';
    if(pin_index < pins->count) {
        if(pins->names[pin_index][0] != '\0') {
            strlcpy(
                app->main_menu_rename_buffer,
                pins->names[pin_index],
                sizeof(app->main_menu_rename_buffer));
        } else {
            Storage* storage = furi_record_open(RECORD_STORAGE);
            char* full_label = malloc(MAIN_MENU_PINS_PATH_LEN);
            main_menu_pin_label(storage, pins->paths[pin_index], full_label, MAIN_MENU_PINS_PATH_LEN);
            furi_record_close(RECORD_STORAGE);
            strlcpy(app->main_menu_rename_buffer, full_label, sizeof(app->main_menu_rename_buffer));
            free(full_label);
        }
    }
    free(pins);

    text_input_set_header_text(text_input, "Rename (6 chars, empty=default)");
    text_input_set_validator(text_input, desktop_settings_scene_main_menu_rename_validator, NULL);
    text_input_set_minimum_length(text_input, 0);
    text_input_set_result_callback(
        text_input,
        desktop_settings_scene_main_menu_rename_text_input_callback,
        app,
        app->main_menu_rename_buffer,
        sizeof(app->main_menu_rename_buffer),
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewTextInput);
}

bool desktop_settings_scene_main_menu_rename_on_event(void* context, SceneManagerEvent event) {
    DesktopSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == RenameResultOk) {
        uint32_t pin_index = scene_manager_get_scene_state(
            app->scene_manager, DesktopSettingsAppSceneMainMenuRename);

        MainMenuPinsUI* pins = malloc(sizeof(MainMenuPinsUI));
        main_menu_pins_load(pins);
        if(pin_index < pins->count) {
            strlcpy(
                pins->names[pin_index], app->main_menu_rename_buffer, sizeof(pins->names[pin_index]));
            main_menu_pins_save(pins);
        }
        free(pins);

        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, DesktopSettingsAppSceneMainMenu);
        consumed = true;
    }

    return consumed;
}

void desktop_settings_scene_main_menu_rename_on_exit(void* context) {
    DesktopSettingsApp* app = context;
    text_input_reset(app->text_input);
}
