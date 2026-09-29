#pragma once

#include <gui/view.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct DesktopSettingsViewWallpaper DesktopSettingsViewWallpaper;

DesktopSettingsViewWallpaper* desktop_settings_view_wallpaper_alloc(void);

void desktop_settings_view_wallpaper_free(DesktopSettingsViewWallpaper* instance);

View* desktop_settings_view_wallpaper_get_view(DesktopSettingsViewWallpaper* instance);

void desktop_settings_view_wallpaper_load(
    DesktopSettingsViewWallpaper* instance,
    const char* current_filename,
    bool enabled);

void desktop_settings_view_wallpaper_get(
    DesktopSettingsViewWallpaper* instance,
    char* filename_out,
    size_t filename_out_size,
    bool* enabled_out);

bool desktop_settings_view_wallpaper_has_files(DesktopSettingsViewWallpaper* instance);
