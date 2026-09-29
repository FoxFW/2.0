#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FOX_THEME_FILE  "/int/Fox.cfg"

#ifdef __cplusplus
extern "C" {
#endif

bool fox_theme_is_active(void);
uint8_t fox_theme_get_style(void);
void fox_theme_set(bool active);
void fox_theme_set_style(uint8_t style);

#ifdef __cplusplus
}
#endif
