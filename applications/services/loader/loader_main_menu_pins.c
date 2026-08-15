#include "loader_main_menu_pins.h"

#include <furi.h>
#include <storage/storage.h>

void main_menu_pins_load(MainMenuPins* pins) {
    furi_assert(pins);
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
                        strlcpy(
                            pins->paths[pins->count], line, sizeof(pins->paths[pins->count]));
                        strlcpy(
                            pins->names[pins->count], sep + 1, sizeof(pins->names[pins->count]));
                    } else {
                        strlcpy(
                            pins->paths[pins->count], line, sizeof(pins->paths[pins->count]));
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
