#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/file_browser.h>
#include <storage/storage.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "xbm_bmp_convert.h"
#include "fox_xbm_converter_icons.h"

#define SCREENSHOTS_DIR EXT_PATH("Screenshots")
#define RESULT_AUTO_RETURN_MS 1800

typedef enum {
    ViewIdMenu = 0,
    ViewIdBrowser,
    ViewIdConverting,
    ViewIdResult,
} AppViewId;

typedef enum {
    ModeXbmToBmp = 0,
    ModeBmpToXbm,
} ConvertMode;

typedef struct {
    Gui* gui;
    Storage* storage;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;

    View* menu_view;
    View* converting_view;
    View* result_view;
    FileBrowser* browser;
    FuriString* picked_path;
    FuriString* browse_path;
    FuriTimer* result_timer;

    AppViewId current_view;
    bool browser_started;
    bool single_file_mode;

    uint8_t menu_selected;
    ConvertMode pending_mode;

    char convert_title[32];
    uint8_t convert_percent;

    bool result_success;
    char result_line1[32];
    char result_line2[48];
} FoxXbmApp;

static FoxXbmApp* s_app = NULL;

static void begin_browse(FoxXbmApp* app, ConvertMode mode);
static void do_conversion(FoxXbmApp* app, ConvertMode mode, const char* src_path);
static void return_to_start(FoxXbmApp* app);

static bool has_extension_ci(const char* path, const char* ext) {
    size_t path_len = strlen(path);
    size_t ext_len = strlen(ext);
    if(ext_len > path_len) return false;
    const char* suffix = path + (path_len - ext_len);
    for(size_t i = 0; i < ext_len; i++) {
        char a = suffix[i];
        char b = ext[i];
        if(a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if(b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if(a != b) return false;
    }
    return true;
}

static void build_dest_path(const char* src_path, ConvertMode mode, char* out, size_t out_size) {
    const char* new_ext = (mode == ModeXbmToBmp) ? ".bmp" : ".xbm";
    size_t len = strlen(src_path);
    const char* dot = strrchr(src_path, '.');
    const char* slash = strrchr(src_path, '/');
    size_t base_len = (dot && (!slash || dot > slash)) ? (size_t)(dot - src_path) : len;
    if(base_len >= out_size) base_len = out_size - 1;
    memcpy(out, src_path, base_len);
    out[base_len] = '\0';
    strlcat(out, new_ext, out_size);
}

static void draw_menu_button(
    Canvas* canvas,
    int32_t y,
    int32_t height,
    const char* title,
    const char* subtitle,
    bool selected) {
    int32_t x = 2;
    int32_t width = 128 - 4;

    canvas_set_color(canvas, ColorBlack);
    if(selected) {
        canvas_draw_rbox(canvas, x, y, width, height, 4);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, width, height, 4);
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, y + height / 2 - 6, AlignCenter, AlignCenter, title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, y + height / 2 + 8, AlignCenter, AlignCenter, subtitle);
    canvas_set_color(canvas, ColorBlack);
}

static void menu_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    FoxXbmApp* app = s_app;
    if(!app) return;
    canvas_clear(canvas);
    draw_menu_button(canvas, 1, 30, "XBM -> BMP", "Convert xbm to bmp", app->menu_selected == 0);
    draw_menu_button(canvas, 33, 30, "BMP -> XBM", "Convert bmp to xbm", app->menu_selected == 1);
}

static bool menu_input_cb(InputEvent* event, void* context) {
    FoxXbmApp* app = context;
    if(event->type != InputTypeShort) return false;
    switch(event->key) {
    case InputKeyUp:
    case InputKeyDown:
        app->menu_selected = app->menu_selected ? 0 : 1;
        with_view_model(app->menu_view, uint8_t * _model, { UNUSED(_model); }, true);
        return true;
    case InputKeyOk:
        begin_browse(app, app->menu_selected == 0 ? ModeXbmToBmp : ModeBmpToXbm);
        return true;
    case InputKeyBack:
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    default:
        return false;
    }
}

static void begin_browse(FoxXbmApp* app, ConvertMode mode) {
    app->pending_mode = mode;
    storage_common_mkdir(app->storage, SCREENSHOTS_DIR);

    const char* ext = (mode == ModeXbmToBmp) ? ".xbm" : ".bmp";
    if(app->browser_started) file_browser_stop(app->browser);
    file_browser_configure(app->browser, ext, SCREENSHOTS_DIR, false, true, &I_image_10px, false);
    furi_string_set(app->browse_path, SCREENSHOTS_DIR);
    file_browser_start(app->browser, app->browse_path);
    app->browser_started = true;

    app->current_view = ViewIdBrowser;
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdBrowser);
}

static void browser_selected_callback(void* context) {
    FoxXbmApp* app = context;
    do_conversion(app, app->pending_mode, furi_string_get_cstr(app->picked_path));
}

static bool nav_callback(void* context) {
    FoxXbmApp* app = context;
    if(app->current_view == ViewIdBrowser) {
        app->current_view = ViewIdMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMenu);
        return true;
    }
    return false;
}

static void converting_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    FoxXbmApp* app = s_app;
    if(!app) return;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, app->convert_title);

    elements_progress_bar(canvas, 14, 27, 100, app->convert_percent / 100.0f);

    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", app->convert_percent);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, pct);
}

static bool converting_input_cb(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);

    return false;
}

static void ui_progress_cb(uint8_t percent, void* context) {
    FoxXbmApp* app = context;
    app->convert_percent = percent;
    with_view_model(app->converting_view, uint8_t * _model, { UNUSED(_model); }, true);
    furi_delay_ms(2);
}

static void result_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    FoxXbmApp* app = s_app;
    if(!app) return;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignCenter, app->result_line1);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, app->result_line2);
}

static bool result_input_cb(InputEvent* event, void* context) {
    FoxXbmApp* app = context;
    if(event->type == InputTypeShort && (event->key == InputKeyOk || event->key == InputKeyBack)) {
        return_to_start(app);
        return true;
    }
    return false;
}

static void result_timer_cb(void* context) {
    return_to_start((FoxXbmApp*)context);
}

static void do_conversion(FoxXbmApp* app, ConvertMode mode, const char* src_path) {
    app->pending_mode = mode;
    strlcpy(
        app->convert_title,
        mode == ModeXbmToBmp ? "Converting XBM->BMP" : "Converting BMP->XBM",
        sizeof(app->convert_title));
    app->convert_percent = 0;
    app->current_view = ViewIdConverting;
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdConverting);
    with_view_model(app->converting_view, uint8_t * _model, { UNUSED(_model); }, true);

    char dest_path[160];
    build_dest_path(src_path, mode, dest_path, sizeof(dest_path));

    XbmBmpResult res = (mode == ModeXbmToBmp) ?
                            xbm_to_bmp_convert(app->storage, src_path, dest_path, ui_progress_cb, app) :
                            bmp_to_xbm_convert(app->storage, src_path, dest_path, ui_progress_cb, app);

    app->result_success = (res == XbmBmpOk);
    if(app->result_success) {
        notification_message(app->notifications, &sequence_success);
        strlcpy(app->result_line1, "Image Saved!", sizeof(app->result_line1));
        const char* base = strrchr(dest_path, '/');
        strlcpy(app->result_line2, base ? base + 1 : dest_path, sizeof(app->result_line2));
    } else {
        notification_message(app->notifications, &sequence_error);
        strlcpy(app->result_line1, "Conversion Failed", sizeof(app->result_line1));
        strlcpy(app->result_line2, xbm_bmp_result_text(res), sizeof(app->result_line2));
    }

    app->current_view = ViewIdResult;
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdResult);
    with_view_model(app->result_view, uint8_t * _model, { UNUSED(_model); }, true);

    furi_timer_start(app->result_timer, furi_ms_to_ticks(RESULT_AUTO_RETURN_MS));
}

static void return_to_start(FoxXbmApp* app) {
    furi_timer_stop(app->result_timer);
    if(app->single_file_mode) {
        view_dispatcher_stop(app->view_dispatcher);
    } else {
        app->current_view = ViewIdMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMenu);
    }
}

static FoxXbmApp* app_alloc(void) {
    FoxXbmApp* app = malloc(sizeof(FoxXbmApp));
    memset(app, 0, sizeof(FoxXbmApp));
    s_app = app;

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->picked_path = furi_string_alloc();
    app->browse_path = furi_string_alloc();

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, nav_callback);

    app->menu_view = view_alloc();
    view_set_context(app->menu_view, app);
    view_set_draw_callback(app->menu_view, menu_draw_cb);
    view_set_input_callback(app->menu_view, menu_input_cb);
    view_allocate_model(app->menu_view, ViewModelTypeLocking, sizeof(uint8_t));
    view_dispatcher_add_view(app->view_dispatcher, ViewIdMenu, app->menu_view);

    app->converting_view = view_alloc();
    view_set_context(app->converting_view, app);
    view_set_draw_callback(app->converting_view, converting_draw_cb);
    view_set_input_callback(app->converting_view, converting_input_cb);
    view_allocate_model(app->converting_view, ViewModelTypeLocking, sizeof(uint8_t));
    view_dispatcher_add_view(app->view_dispatcher, ViewIdConverting, app->converting_view);

    app->result_view = view_alloc();
    view_set_context(app->result_view, app);
    view_set_draw_callback(app->result_view, result_draw_cb);
    view_set_input_callback(app->result_view, result_input_cb);
    view_allocate_model(app->result_view, ViewModelTypeLocking, sizeof(uint8_t));
    view_dispatcher_add_view(app->view_dispatcher, ViewIdResult, app->result_view);

    app->browser = file_browser_alloc(app->picked_path);
    file_browser_set_callback(app->browser, browser_selected_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, ViewIdBrowser, file_browser_get_view(app->browser));

    app->result_timer = furi_timer_alloc(result_timer_cb, FuriTimerTypeOnce, app);

    app->current_view = ViewIdMenu;
    return app;
}

static void app_free(FoxXbmApp* app) {
    s_app = NULL;

    furi_timer_stop(app->result_timer);
    furi_timer_free(app->result_timer);

    if(app->browser_started) file_browser_stop(app->browser);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdBrowser);
    file_browser_free(app->browser);

    view_dispatcher_remove_view(app->view_dispatcher, ViewIdMenu);
    view_free(app->menu_view);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdConverting);
    view_free(app->converting_view);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdResult);
    view_free(app->result_view);

    view_dispatcher_free(app->view_dispatcher);

    furi_string_free(app->picked_path);
    furi_string_free(app->browse_path);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t fox_xbm_converter_app(void* p) {
    const char* args = (const char*)p;
    FoxXbmApp* app = app_alloc();

    if(args && args[0] != '\0') {

        app->single_file_mode = true;
        if(has_extension_ci(args, ".xbm")) {
            do_conversion(app, ModeXbmToBmp, args);
        } else if(has_extension_ci(args, ".bmp")) {
            do_conversion(app, ModeBmpToXbm, args);
        } else {
            app->result_success = false;
            strlcpy(app->result_line1, "Unsupported File", sizeof(app->result_line1));
            strlcpy(app->result_line2, "Need .xbm or .bmp", sizeof(app->result_line2));
            app->current_view = ViewIdResult;
            view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdResult);
            furi_timer_start(app->result_timer, furi_ms_to_ticks(RESULT_AUTO_RETURN_MS));
        }
    } else {
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMenu);
    }

    view_dispatcher_run(app->view_dispatcher);

    app_free(app);
    return 0;
}
