#include "apps_manager.h"

#include <storage/storage.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static bool ends_with_fap(const char* name) {
    size_t len = strlen(name);
    if(len < 4) return false;
    const char* tail = name + (len - 4);
    return (tail[0] == '.' && (tail[1] == 'f' || tail[1] == 'F') &&
            (tail[2] == 'a' || tail[2] == 'A') && (tail[3] == 'p' || tail[3] == 'P'));
}

static void join_path(char* out, size_t out_size, const char* dir, const char* name) {
    size_t dlen = strlen(dir);
    if(dlen && dir[dlen - 1] == '/') dlen--;
    snprintf(out, out_size, "%.*s/%s", (int)dlen, dir, name);
}

static char* fox_strdup(const char* s) {
    size_t n = strlen(s) + 1;
    char* d = malloc(n);
    if(d) memcpy(d, s, n);
    return d;
}

void my_apps_free_buffers(App* app) {
    if(app->my_apps) {
        free(app->my_apps);
        app->my_apps = NULL;
    }
}

void my_apps_open(App* app) {
    app->my_apps_count = 0;
    if(!app->my_apps) app->my_apps = malloc(FOX_MYAPPS_MAX * sizeof(MyAppEntry));
    if(!app->my_apps) {
        app_log(app, "Out of memory.");
        app_render_log(app);
        return;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);

    char** dstack = malloc(16 * sizeof(char*));
    size_t dcount = 0, dcap = 16;
    dstack[dcount++] = fox_strdup(FOX_APPS_DIR);

    File* dir = storage_file_alloc(storage);
    FileInfo info;
    char name[128];

    while(dcount > 0 && app->my_apps_count < FOX_MYAPPS_MAX) {
        char* cur = dstack[--dcount];
        if(storage_dir_open(dir, cur)) {
            while(app->my_apps_count < FOX_MYAPPS_MAX &&
                  storage_dir_read(dir, &info, name, sizeof(name))) {
                char full[FOX_MYAPPS_PATH_MAX];
                join_path(full, sizeof(full), cur, name);
                if(info.flags & FSF_DIRECTORY) {
                    // "assets" folders hold each app's bundled resource
                    // files (icons, dictionaries, etc.), not installed
                    // apps themselves, and aren't meant to be deleted on
                    // their own - don't descend into them.
                    if(strcmp(name, "assets") != 0) {
                        if(dcount == dcap) {
                            dcap *= 2;
                            dstack = realloc(dstack, dcap * sizeof(char*));
                        }
                        dstack[dcount++] = fox_strdup(full);
                    }
                } else if(ends_with_fap(name)) {
                    MyAppEntry* entry = &app->my_apps[app->my_apps_count];
                    snprintf(entry->path, sizeof(entry->path), "%s", full);
                    const char* rel = full + strlen(FOX_APPS_DIR);
                    while(*rel == '/') rel++;
                    size_t rel_len = strlen(rel);
                    if(rel_len >= sizeof(entry->label)) rel_len = sizeof(entry->label) - 1;
                    memcpy(entry->label, rel, rel_len);
                    entry->label[rel_len] = '\0';
                    app->my_apps_count++;
                }
            }
        }
        storage_dir_close(dir);
        free(cur);
    }
    storage_file_free(dir);

    while(dcount > 0) free(dstack[--dcount]);
    free(dstack);

    furi_record_close(RECORD_STORAGE);

    app->my_apps_selected = 0;
    app->my_apps_scroll = 0;
    app->my_apps_confirm_delete = false;

    if(app->my_apps_count == 0) {
        app_log(app, "No installed apps found under %s.", FOX_APPS_DIR);
        app_render_log(app);
        return;
    }

    app->current_view = FoxDownloaderViewMyAppsList;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewMyAppsList);
    with_view_model(app->my_apps_list_view, uint8_t * m, { UNUSED(m); }, true);
}

static App* s_my_apps_app = NULL;

#define MYAPPS_ROW_H     11
#define MYAPPS_VISIBLE   4

static void my_apps_draw(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_my_apps_app;
    if(!app) return;

    canvas_clear(canvas);

    if(app->my_apps_confirm_delete && app->my_apps_count > 0) {
        const MyAppEntry* entry = &app->my_apps[app->my_apps_selected];
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignTop, "Delete this app?");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, entry->label);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "OK = Delete, Back = Cancel");
        return;
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignCenter, "Manage Apps - OK to delete");
    canvas_set_color(canvas, ColorBlack);

    if(app->my_apps_count == 0) return;

    if(app->my_apps_selected < app->my_apps_scroll) {
        app->my_apps_scroll = app->my_apps_selected;
    } else if(app->my_apps_selected >= app->my_apps_scroll + MYAPPS_VISIBLE) {
        app->my_apps_scroll = app->my_apps_selected - MYAPPS_VISIBLE + 1;
    }

    for(size_t row = 0; row < MYAPPS_VISIBLE; row++) {
        size_t idx = app->my_apps_scroll + row;
        if(idx >= app->my_apps_count) break;
        int32_t y = 11 + (int32_t)row * MYAPPS_ROW_H;
        bool selected = (idx == app->my_apps_selected);
        if(selected) {
            canvas_draw_box(canvas, 0, y, 128, MYAPPS_ROW_H);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 3, y + MYAPPS_ROW_H - 2, app->my_apps[idx].label);
        if(selected) canvas_set_color(canvas, ColorBlack);
    }
}

static bool my_apps_input(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    if(app->my_apps_confirm_delete) {
        if(event->type != InputTypeShort) return false;
        if(event->key == InputKeyOk) {
            Storage* storage = furi_record_open(RECORD_STORAGE);
            storage_common_remove(storage, app->my_apps[app->my_apps_selected].path);
            furi_record_close(RECORD_STORAGE);
            app_log(app, "Deleted %s", app->my_apps[app->my_apps_selected].label);
            my_apps_open(app);
            return true;
        }
        if(event->key == InputKeyBack) {
            app->my_apps_confirm_delete = false;
            with_view_model(app->my_apps_list_view, uint8_t * m, { UNUSED(m); }, true);
            return true;
        }
        return true;
    }

    if(app->my_apps_count == 0) return false;

    switch(event->key) {
    case InputKeyUp:
        if(app->my_apps_selected > 0) app->my_apps_selected--;
        with_view_model(app->my_apps_list_view, uint8_t * m, { UNUSED(m); }, true);
        return true;
    case InputKeyDown:
        if(app->my_apps_selected + 1 < app->my_apps_count) app->my_apps_selected++;
        with_view_model(app->my_apps_list_view, uint8_t * m, { UNUSED(m); }, true);
        return true;
    case InputKeyOk:
        if(event->type == InputTypeShort) {
            app->my_apps_confirm_delete = true;
            with_view_model(app->my_apps_list_view, uint8_t * m, { UNUSED(m); }, true);
        }
        return true;
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

View* my_apps_list_view_alloc(App* app) {
    s_my_apps_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, my_apps_draw);
    view_set_input_callback(v, my_apps_input);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(uint8_t));
    return v;
}

void my_apps_list_view_free(View* v) {
    s_my_apps_app = NULL;
    view_free(v);
}
