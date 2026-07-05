#pragma once
/* fox_theme.h — Fox firmware menu-theme API.
 *
 * The implementation lives in fox_theme.c (compiled as part of the GUI
 * service).  A REAL global is used (not a static-inline cache) so that
 * all compilation units share one value and fox_theme_set() takes effect
 * immediately without a firmware reboot.
 *
 * Theme values:
 *   Classic   (0): plain vertical list menus, no border boxes
 *   Fox Theme (1): rounded border boxes in submenus / variable lists,
 *                  grid start-menu in the SubGHz app
 *
 * The setting is persisted to /int/Fox.cfg (1 byte) and mirrored
 * from desktop_settings_scene_menu_style.c whenever the user saves. */

#include <stdbool.h>

#define FOX_THEME_FILE  "/int/Fox.cfg"

#ifdef __cplusplus
extern "C" {
#endif

/* Returns true when Fox Theme is the active menu style. */
bool fox_theme_is_active(void);

/* Sets the active theme AND persists it to FOX_THEME_FILE.
 * Call this from desktop_settings_scene_menu_style_on_exit(). */
void fox_theme_set(bool active);

#ifdef __cplusplus
}
#endif
