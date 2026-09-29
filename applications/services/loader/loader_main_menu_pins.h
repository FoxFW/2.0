#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAIN_MENU_PINS_MAX       12
#define MAIN_MENU_PINS_PATH_LEN  128
#define MAIN_MENU_PINS_NAME_LEN  7
#define MAIN_MENU_PINS_FILE_NAME ".main_menu.pins"

typedef struct {
    char paths[MAIN_MENU_PINS_MAX][MAIN_MENU_PINS_PATH_LEN];
    char names[MAIN_MENU_PINS_MAX][MAIN_MENU_PINS_NAME_LEN];
    uint8_t count;
} MainMenuPins;

void main_menu_pins_load(MainMenuPins* pins);

#ifdef __cplusplus
}
#endif
