#include "fox_esp_flasher.h"
#include <string.h>

static FlasherApp* s_app = NULL;

typedef struct {
    uint8_t selected;
    uint8_t offset;
} FilesModel;

typedef enum {
    FileRowBoot = 0,
    FileRowPart,
    FileRowFW,
    FileRowInstall,
    FileRowCount,
} FileRow;

#define BOX_X  4
#define BOX_W  120
#define BOX_H  22
#define BOX_R  4

static const uint8_t k_slot_y[2] = {15, 40};
static const char* const k_row_label[3] = {"Bootloader", "Partitions", "Firmware"};

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
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Select .bin Files");

    const char* paths[3] = {app->file_bootloader, app->file_partitions, app->file_firmware};

    for(uint8_t slot = 0; slot < 2; slot++) {
        uint8_t row = m->offset + slot;
        if(row >= (uint8_t)FileRowCount) break;

        bool sel = (m->selected == row);
        uint8_t y  = k_slot_y[slot];

        if(row == FileRowInstall) {
            flasher_draw_ok_button(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R, "INSTALL");
            continue;
        }

        canvas_set_color(canvas, ColorBlack);
        if(sel) {
            canvas_draw_rbox(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
        }

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, y + 8, AlignCenter, AlignCenter, k_row_label[row]);
        canvas_set_font(canvas, FontSecondary);
        char fn_short[19];
        snprintf(fn_short, sizeof(fn_short), "%s", fname(paths[row]));
        canvas_draw_str_aligned(canvas, 64, y + 17, AlignCenter, AlignCenter, fn_short);
        canvas_set_color(canvas, ColorBlack);
    }
}

static bool files_input(InputEvent* event, void* context) {
    FlasherApp* app = context;
    if(event->type != InputTypeShort) return false;

    switch(event->key) {
    case InputKeyUp:
        with_view_model(app->files_view, FilesModel* m, {
            if(m->selected > 0) {
                m->selected--;
                if(m->selected < m->offset) m->offset = m->selected;
            }
        }, true);
        return true;
    case InputKeyDown:
        with_view_model(app->files_view, FilesModel* m, {
            if(m->selected < (uint8_t)(FileRowCount - 1)) {
                m->selected++;
                if(m->selected >= m->offset + 2) m->offset = m->selected - 1;
            }
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
        FuriString* path_str = furi_string_alloc_set(FLASHER_DATA_DIR);

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
    with_view_model(v, FilesModel* m, { m->selected = 0; m->offset = 0; }, false);
    return v;
}

void view_files_free(View* v) {
    s_app = NULL;
    view_free(v);
}

void view_files_refresh(View* v) {
    with_view_model(v, FilesModel* m, { UNUSED(m); }, true);
}

void view_files_select_install(View* v) {
    with_view_model(v, FilesModel* m, {
        m->selected = FileRowInstall;
        m->offset = 2;
    }, true);
}
