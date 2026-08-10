#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAIN_MENU_PINS_MAX       12
#define MAIN_MENU_PINS_PATH_LEN  128
#define MAIN_MENU_PINS_FILE_NAME ".main_menu.pins"

typedef struct {
    char paths[MAIN_MENU_PINS_MAX][MAIN_MENU_PINS_PATH_LEN];
    uint8_t count;
} MainMenuPins;

/* Loads the user-pinned Main Menu app list (see desktop_settings' Main Menu
 * editor for the writer side). Newline-delimited plain text file on internal
 * storage, one .fap path per line. Missing file or read error just yields
 * count = 0 - the pinned section simply doesn't appear, no error surfaced. */
void main_menu_pins_load(MainMenuPins* pins);

#ifdef __cplusplus
}
#endif
