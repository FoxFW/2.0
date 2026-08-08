#pragma once

#include <gui/view.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct DesktopSettingsViewWallpaper DesktopSettingsViewWallpaper;

DesktopSettingsViewWallpaper* desktop_settings_view_wallpaper_alloc(void);

void desktop_settings_view_wallpaper_free(DesktopSettingsViewWallpaper* instance);

View* desktop_settings_view_wallpaper_get_view(DesktopSettingsViewWallpaper* instance);

// Scans /ext/wallpapers for valid 128x64 *.xbm files (alphabetical, dotfiles
// skipped) and selects `current_filename` in the list if present.
void desktop_settings_view_wallpaper_load(
    DesktopSettingsViewWallpaper* instance,
    const char* current_filename,
    bool enabled);

// Returns the filename currently selected in the list (empty string if none
// were found) and whether the ON/OFF row is set to ON.
void desktop_settings_view_wallpaper_get(
    DesktopSettingsViewWallpaper* instance,
    char* filename_out,
    size_t filename_out_size,
    bool* enabled_out);

bool desktop_settings_view_wallpaper_has_files(DesktopSettingsViewWallpaper* instance);
