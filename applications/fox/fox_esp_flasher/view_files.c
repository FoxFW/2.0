#include "fox_esp_flasher.h"
#include <string.h>

static FlasherApp* s_app = NULL;

typedef struct {
    uint8_t selected; /* 0-3 (0-2 = files, 3 = Install) */
} FilesModel;

typedef enum {
    FileRowBoot = 0,
    FileRowPart,
    FileRowFW,
    FileRowInstall,
    FileRowCount,
} FileRow;

#define BOX_X 4
#define BOX_W 120
#define BOX_H 14
#define BOX_R 3

static const uint8_t k_box_y[FileRowCount] = {10, 26, 42, 52};
static const char* k_row_label[3] = {"Bootloader", "Partitions", "Firmware"};

static const char* fname(const char* path) {
    if(!path || path[0] == '\0') return "(not set)";
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void files_draw(Canvas* canvas, void* model_ptr) {
    FilesModel* m = model_ptr;
    FlasherApp* app = s_app;
    if(!app) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Select .bin Files");

    const char* paths[3] = {app->file_bootloader, app->file_partitions, app->file_firmware};
    bool all_set = (app->files_selected & 0x07) == 0x07;

    for(uint8_t row = 0; row < (uint8_t)FileRowCount; row++) {
        bool sel = (m->selected == row);
        uint8_t y = k_box_y[row];
        uint8_t h = (row == FileRowInstall) ? BOX_H + 2 : BOX_H;

        canvas_set_color(canvas, ColorBlack);
        if(sel) {
            canvas_draw_rbox(canvas, BOX_X, y, BOX_W, h, BOX_R);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, BOX_X, y, BOX_W, h, BOX_R);
            canvas_draw_rframe(canvas, BOX_X + 2, y + 2, BOX_W - 4, h - 4, BOX_R - 1);
        }

        uint8_t ty = y + h / 2;
        canvas_set_font(canvas, FontSecondary);

        if(row < 3) {
            char text[40];
            const char* fn = fname(paths[row]);
            char fn_short[17];
            snprintf(fn_short, sizeof(fn_short), "%s", fn);
            snprintf(text, sizeof(text), "%s: %s", k_row_label[row], fn_short);
            canvas_draw_str_aligned(canvas, 64, ty, AlignCenter, AlignCenter, text);
        } else {
            const char* label = all_set ? "Install" : "Install (select all files)";
            canvas_draw_str_aligned(canvas, 64, ty, AlignCenter, AlignCenter, label);
        }
        canvas_set_color(canvas, ColorBlack);
    }
}

static bool files_input(InputEvent* event, void* context) {
    FlasherApp* app = context;
    if(event->type != InputTypeShort) return false;

    switch(event->key) {
    case InputKeyUp:
        with_view_model(app->files_view, FilesModel* m, {
            m->selected = (m->selected == 0) ? (uint8_t)(FileRowCount - 1)
                                              : m->selected - 1;
        }, true);
        return true;
    case InputKeyDown:
        with_view_model(app->files_view, FilesModel* m, {
            m->selected = (m->selected + 1) % FileRowCount;
        }, true);
        return true;
    case InputKeyOk: {
        uint8_t sel = 0;
        with_view_model(app->files_view, FilesModel* m, { sel = m->selected; }, false);

        if(sel == FileRowInstall) {
            if((app->files_selected & 0x07) == 0x07) {
                view_dispatcher_send_custom_event(app->view_dispatcher, FlasherEventFilesGo);
            }
            return true;
        }

        DialogsFileBrowserOptions opts;
        dialog_file_browser_set_basic_options(&opts, ".bin", NULL);
        opts.hide_ext = false;

        char* dest = (sel == FileRowBoot) ? app->file_bootloader
                   : (sel == FileRowPart) ? app->file_partitions
                                          : app->file_firmware;
        FuriString* path_str = furi_string_alloc_set(EXT_PATH(""));

        if(dialog_file_browser_show(app->dialogs, path_str, path_str, &opts)) {
            snprintf(dest, FLASHER_PATH_LEN, "%s", furi_string_get_cstr(path_str));
            app->files_selected |= (1 << sel);
        }
        furi_string_free(path_str);
        with_view_model(app->files_view, FilesModel* m, { UNUSED(m); }, true);
        return true;
    }
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

View* view_files_alloc(FlasherApp* app) {
    s_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, files_draw);
    view_set_input_callback(v, files_input);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(FilesModel));
    with_view_model(v, FilesModel* m, { m->selected = 0; }, false);
    return v;
}

void view_files_free(View* v) {
    s_app = NULL;
    view_free(v);
}

void view_files_refresh(View* v) {
    with_view_model(v, FilesModel* m, { UNUSED(m); }, true);
}
