#include "desktop_settings_view_wallpaper.h"
#include <gui/view.h>
#include <gui/canvas.h>
#include <storage/storage.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define WALLPAPER_DIR       EXT_PATH("wallpapers")
#define WALLPAPER_NAME_MAX  64 /* matches DesktopSettings.wallpaper_filename */
#define WALLPAPER_LIST_MAX  64

#define ROW_X 4
#define ROW_W 120
#define ROW_R 3
#define ROW_TOP 12
#define ROW_H 22
#define ROW_GAP 3

typedef enum {
    WallpaperRowName,
    WallpaperRowToggle,
    WallpaperRowCount,
} WallpaperRow;

typedef struct {
    char names[WALLPAPER_LIST_MAX][WALLPAPER_NAME_MAX];
    uint8_t count;
    uint8_t selected_index;
    bool enabled;
    uint8_t focus_row;
} WallpaperSettingsModel;

struct DesktopSettingsViewWallpaper {
    View* view;
};

static bool ends_with_xbm(const char* name) {
    size_t len = strlen(name);
    if(len < 4) return false;
    const char* tail = name + (len - 4);
    return (tail[0] == '.' && (tail[1] == 'x' || tail[1] == 'X') &&
            (tail[2] == 'b' || tail[2] == 'B') && (tail[3] == 'm' || tail[3] == 'M'));
}

static bool xbm_file_is_128x64(Storage* storage, const char* path) {
    File* f = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char header[220];
        uint16_t n = storage_file_read(f, header, sizeof(header) - 1);
        header[n] = '\0';
        storage_file_close(f);

        char* w = strstr(header, "_width ");
        char* h = strstr(header, "_height ");
        if(w && h) {
            ok = (atoi(w + 7) == 128) && (atoi(h + 8) == 64);
        }
    }
    storage_file_free(f);
    return ok;
}

static void desktop_settings_view_wallpaper_draw_row(
    Canvas* canvas, int32_t y, bool selected, const char* line1, const char* line2) {
    canvas_set_color(canvas, ColorBlack);
    if(selected) {
        canvas_draw_rbox(canvas, ROW_X, y, ROW_W, ROW_H, ROW_R);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, ROW_X, y, ROW_W, ROW_H, ROW_R);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, y + 6, AlignCenter, AlignCenter, line1);
    canvas_draw_str_aligned(canvas, 64, y + 16, AlignCenter, AlignCenter, line2);
    canvas_draw_str_aligned(canvas, ROW_X + 6, y + ROW_H / 2, AlignLeft, AlignCenter, "<");
    canvas_draw_str_aligned(canvas, ROW_X + ROW_W - 6, y + ROW_H / 2, AlignRight, AlignCenter, ">");

    canvas_set_color(canvas, ColorBlack);
}

static void desktop_settings_view_wallpaper_draw_callback(Canvas* canvas, void* _model) {
    WallpaperSettingsModel* model = _model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Custom Wallpaper");

    int32_t y0 = ROW_TOP;
    int32_t y1 = y0 + ROW_H + ROW_GAP;

    if(model->count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 34, AlignCenter, AlignCenter, "No 128x64 .xbm files found");
        canvas_draw_str_aligned(
            canvas, 64, 46, AlignCenter, AlignCenter, "Add some to /wallpapers");
        return;
    }

    char name_line[WALLPAPER_NAME_MAX];
    snprintf(
        name_line, sizeof(name_line), "%s", model->names[model->selected_index]);
    desktop_settings_view_wallpaper_draw_row(
        canvas, y0, model->focus_row == WallpaperRowName, "Wallpaper Name", name_line);

    desktop_settings_view_wallpaper_draw_row(
        canvas, y1, model->focus_row == WallpaperRowToggle, "Custom Wallpaper",
        model->enabled ? "ON" : "OFF");
}

static bool desktop_settings_view_wallpaper_input_callback(InputEvent* event, void* context) {
    View* view = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool consumed = false;
    with_view_model(
        view,
        WallpaperSettingsModel* model,
        {
            switch(event->key) {
            case InputKeyUp:
                model->focus_row = (model->focus_row == 0) ?
                                        (uint8_t)(WallpaperRowCount - 1) :
                                        (uint8_t)(model->focus_row - 1);
                consumed = true;
                break;
            case InputKeyDown:
                model->focus_row = (uint8_t)((model->focus_row + 1) % WallpaperRowCount);
                consumed = true;
                break;
            case InputKeyLeft:
            case InputKeyRight: {
                int delta = (event->key == InputKeyRight) ? 1 : -1;
                if(model->focus_row == WallpaperRowName && model->count > 0) {
                    int idx = (int)model->selected_index + delta;
                    if(idx < 0) idx = model->count - 1;
                    if(idx >= model->count) idx = 0;
                    model->selected_index = (uint8_t)idx;
                } else if(model->focus_row == WallpaperRowToggle) {
                    model->enabled = !model->enabled;
                }
                consumed = true;
                break;
            }
            default:
                break;
            }
        },
        consumed);
    return consumed;
}

DesktopSettingsViewWallpaper* desktop_settings_view_wallpaper_alloc(void) {
    DesktopSettingsViewWallpaper* instance = malloc(sizeof(DesktopSettingsViewWallpaper));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(WallpaperSettingsModel));
    view_set_context(instance->view, instance->view);
    view_set_draw_callback(instance->view, desktop_settings_view_wallpaper_draw_callback);
    view_set_input_callback(instance->view, desktop_settings_view_wallpaper_input_callback);
    return instance;
}

void desktop_settings_view_wallpaper_free(DesktopSettingsViewWallpaper* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* desktop_settings_view_wallpaper_get_view(DesktopSettingsViewWallpaper* instance) {
    furi_assert(instance);
    return instance->view;
}

void desktop_settings_view_wallpaper_load(
    DesktopSettingsViewWallpaper* instance, const char* current_filename, bool enabled) {
    furi_assert(instance);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, WALLPAPER_DIR);

    // names[][] is WALLPAPER_LIST_MAX * WALLPAPER_NAME_MAX = 4096 bytes -
    // this app's thread only has a 2KB stack (see application.fam), so a
    // local array this size overflows it outright. Heap-allocate instead.
    char(*names)[WALLPAPER_NAME_MAX] = malloc(WALLPAPER_LIST_MAX * WALLPAPER_NAME_MAX);
    uint8_t count = 0;

    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, WALLPAPER_DIR)) {
        FileInfo info;
        char name[128];
        while(count < WALLPAPER_LIST_MAX && storage_dir_read(dir, &info, name, sizeof(name))) {
            if(info.flags & FSF_DIRECTORY) continue;
            if(name[0] == '.') continue; // skip the .activate marker and any other dotfiles
            if(!ends_with_xbm(name)) continue;

            char full[160];
            snprintf(full, sizeof(full), "%s/%s", WALLPAPER_DIR, name);
            if(!xbm_file_is_128x64(storage, full)) continue;

            strlcpy(names[count], name, WALLPAPER_NAME_MAX);
            count++;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);

    // Simple alphabetical (case-insensitive) selection sort - list sizes here
    // are small enough that this is plenty fast.
    for(uint8_t i = 0; i + 1 < count; i++) {
        uint8_t smallest = i;
        for(uint8_t j = i + 1; j < count; j++) {
            if(strcasecmp(names[j], names[smallest]) < 0) smallest = j;
        }
        if(smallest != i) {
            char tmp[WALLPAPER_NAME_MAX];
            memcpy(tmp, names[i], WALLPAPER_NAME_MAX);
            memcpy(names[i], names[smallest], WALLPAPER_NAME_MAX);
            memcpy(names[smallest], tmp, WALLPAPER_NAME_MAX);
        }
    }

    with_view_model(
        instance->view,
        WallpaperSettingsModel* model,
        {
            memcpy(model->names, names, sizeof(model->names));
            model->count = count;
            model->enabled = enabled;
            model->focus_row = WallpaperRowName;
            model->selected_index = 0;
            for(uint8_t i = 0; i < count; i++) {
                if(strcmp(model->names[i], current_filename) == 0) {
                    model->selected_index = i;
                    break;
                }
            }
        },
        true);

    free(names);
}

void desktop_settings_view_wallpaper_get(
    DesktopSettingsViewWallpaper* instance,
    char* filename_out,
    size_t filename_out_size,
    bool* enabled_out) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        WallpaperSettingsModel* model,
        {
            if(filename_out) {
                if(model->count > 0) {
                    snprintf(filename_out, filename_out_size, "%s", model->names[model->selected_index]);
                } else {
                    filename_out[0] = '\0';
                }
            }
            if(enabled_out) *enabled_out = model->enabled;
        },
        false);
}

bool desktop_settings_view_wallpaper_has_files(DesktopSettingsViewWallpaper* instance) {
    furi_assert(instance);
    bool has = false;
    with_view_model(
        instance->view, WallpaperSettingsModel* model, { has = model->count > 0; }, false);
    return has;
}
