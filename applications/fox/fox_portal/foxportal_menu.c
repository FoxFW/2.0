#include "foxportal_menu.h"

#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

#define FOXPORTAL_DIR "/ext/apps_data/fox_portal"
#define FOXPORTAL_START_PATH FOXPORTAL_DIR "/start.html"
#define FOXPORTAL_FINISH_PATH FOXPORTAL_DIR "/finish.html"
#define FOXPORTAL_FIELDS_PATH FOXPORTAL_DIR "/inputnames.txt"

#define FOXPORTAL_FIELDS_FILE_MAX 512

typedef enum {
    MenuPortalStart,
    MenuPortalStop,
    MenuPortalEditFields,
    MenuPortalShowQr,
} MenuFoxPortalIndex;

void foxportal_render_menu(App* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Fox Portal");
    submenu_add_item(app->submenu, "Start", MenuPortalStart, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "Stop", MenuPortalStop, app_menu_item_callback, app);
    submenu_add_item(
        app->submenu, "Edit Input Names", MenuPortalEditFields, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "Show QR", MenuPortalShowQr, app_menu_item_callback, app);
}

static void build_wifi_qr_text(App* app, char* out, size_t out_size) {
    const char* ssid =
        (app->portal_ssid != NULL && furi_string_size(app->portal_ssid) > 0) ?
            furi_string_get_cstr(app->portal_ssid) :
            "Fox Portal Demo";
    /* Open AP - fox_portal.cpp's startPortal() never sets a softAP
       password, so "nopass" is always correct here, not just a
       fallback for the not-yet-started case. */
    snprintf(out, out_size, "WIFI:T:nopass;S:%s;;", ssid);
}

static void show_qr(App* app) {
    char text[FOX_WIFI_SSID_MAX + 32];
    build_wifi_qr_text(app, text, sizeof(text));

    /* Function-static, not a local array - FOX_QR_BUFFER_LEN is a few
       hundred bytes, too much to put on this app's task stack
       alongside everything else already several calls deep by the
       time a menu selection handler runs. */
    static uint8_t tempBuf[FOX_QR_BUFFER_LEN];
    bool ok = qrcodegen_encodeText(
        text,
        tempBuf,
        app->qr_buf,
        qrcodegen_Ecc_LOW,
        qrcodegen_VERSION_MIN,
        FOX_QR_MAX_VERSION,
        qrcodegen_Mask_AUTO,
        true);
    app->qr_size = ok ? qrcodegen_getSize(app->qr_buf) : 0;

    app->current_view = FoxCommanderViewFoxPortalQr;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxCommanderViewFoxPortalQr);
}

static void foxportal_ensure_dir(Storage* storage) {
    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, FOXPORTAL_DIR);
}

/* Reads up to buf_size-1 bytes of path into buf, NUL-terminated. buf is
   left as an empty string if the file doesn't exist. Returns the
   number of bytes read. */
static size_t foxportal_read_file(Storage* storage, const char* path, char* buf, size_t buf_size) {
    buf[0] = '\0';
    File* file = storage_file_alloc(storage);
    size_t read = 0;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        read = storage_file_read(file, buf, buf_size - 1);
        buf[read] = '\0';
    }
    storage_file_close(file);
    storage_file_free(file);
    return read;
}

static bool foxportal_write_file(Storage* storage, const char* path, const char* content) {
    foxportal_ensure_dir(storage);
    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(ok) {
        size_t len = strlen(content);
        ok = storage_file_write(file, content, len) == len;
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static void foxportal_fields_parse(App* app, const char* raw) {
    app->portal_field_count = 0;
    size_t len = strlen(raw);
    size_t i = 0;
    while(i < len && app->portal_field_count < FOX_PORTAL_MAX_FIELDS) {
        size_t start = i;
        while(i < len && raw[i] != '\n') i++;
        size_t line_len = i - start;
        /* Trim a trailing \r (CRLF-saved files) and surrounding
           spaces. */
        while(line_len > 0 &&
              (raw[start + line_len - 1] == '\r' || raw[start + line_len - 1] == ' '))
            line_len--;
        size_t s = start;
        while(line_len > 0 && raw[s] == ' ') {
            s++;
            line_len--;
        }
        if(line_len > 0) {
            if(line_len > FOX_PORTAL_FIELD_KEY_MAX) line_len = FOX_PORTAL_FIELD_KEY_MAX;
            memcpy(app->portal_fields[app->portal_field_count], raw + s, line_len);
            app->portal_fields[app->portal_field_count][line_len] = '\0';
            app->portal_field_count++;
        }
        if(i < len) i++; /* skip the newline itself */
    }
}

static void foxportal_fields_load(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    foxportal_ensure_dir(storage);

    static char raw[FOXPORTAL_FIELDS_FILE_MAX];
    foxportal_read_file(storage, FOXPORTAL_FIELDS_PATH, raw, sizeof(raw));
    foxportal_fields_parse(app, raw);

    if(app->portal_field_count == 0) {
        strncpy(app->portal_fields[0], "name", FOX_PORTAL_FIELD_KEY_MAX);
        app->portal_fields[0][FOX_PORTAL_FIELD_KEY_MAX] = '\0';
        strncpy(app->portal_fields[1], "phone", FOX_PORTAL_FIELD_KEY_MAX);
        app->portal_fields[1][FOX_PORTAL_FIELD_KEY_MAX] = '\0';
        app->portal_field_count = 2;
        foxportal_write_file(storage, FOXPORTAL_FIELDS_PATH, "name\nphone\n");
    }

    furi_record_close(RECORD_STORAGE);
}

static void foxportal_fields_save(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    static char raw[FOXPORTAL_FIELDS_FILE_MAX];
    size_t pos = 0;
    for(size_t i = 0; i < app->portal_field_count; i++) {
        size_t n = strlen(app->portal_fields[i]);
        if(pos + n + 1 >= sizeof(raw)) break;
        memcpy(raw + pos, app->portal_fields[i], n);
        pos += n;
        raw[pos++] = '\n';
    }
    raw[pos] = '\0';

    foxportal_write_file(storage, FOXPORTAL_FIELDS_PATH, raw);
    furi_record_close(RECORD_STORAGE);
}

/* In-place: replaces \n with space — WIFIFOXPORTAL:SETPAGE:* is a single-line command. */
static void foxportal_strip_newlines(char* buf) {
    char* w = buf;
    for(char* r = buf; *r != '\0'; r++) {
        if(*r == '\r') continue;
        *w++ = (*r == '\n') ? ' ' : *r;
    }
    *w = '\0';
}

void foxportal_menu_select(App* app, uint32_t index) {
    switch((MenuFoxPortalIndex)index) {
    case MenuPortalStart:
        app_show_text_input(app, "SSID (blank=default)", TextInputPurposeFoxPortalSsid);
        break;
    case MenuPortalStop:
        app_log(app, "Stopping Fox Portal...");
        app_render_log(app);
        esp_at_send(app->esp_at, "WIFIFOXPORTAL:STOP");
        {
            EspAtMsg msg;
            if(esp_at_receive(app->esp_at, &msg, 5000)) {
                app_log(app, "%s", msg.line);
            } else {
                app_log(app, "No response.");
            }
        }
        app_render_log(app);
        break;
    case MenuPortalEditFields:
        app_switch_to_menu(app, MenuContextFoxPortalFields);
        break;
    case MenuPortalShowQr:
        show_qr(app);
        break;
    }
}

void foxportal_ssid_submitted(App* app) {
    const char* ssid = app->text_input_buffer;

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    char date[16];
    snprintf(date, sizeof(date), "%04u-%02u-%02u", dt.year, dt.month, dt.day);

    app_log(app, "Syncing input names + pages from SD card...");
    app_render_log(app);

    foxportal_fields_load(app);
    {
        static char cmd[32 + FOX_PORTAL_MAX_FIELDS * (FOX_PORTAL_FIELD_KEY_MAX + 1)];
        int pos = snprintf(cmd, sizeof(cmd), "WIFIFOXPORTAL:FIELDS:");
        for(size_t i = 0; i < app->portal_field_count && pos > 0; i++) {
            pos += snprintf(
                cmd + pos,
                sizeof(cmd) - (size_t)pos,
                "%s%s",
                (i > 0) ? "," : "",
                app->portal_fields[i]);
        }
        esp_at_send(app->esp_at, cmd);
        EspAtMsg msg;
        if(esp_at_receive(app->esp_at, &msg, 5000)) app_log(app, "Fields: %s", msg.line);
    }

    {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        static char start_html[FOX_PORTAL_HTML_TRANSFER_MAX + 1];
        static char thanks_html[FOX_PORTAL_HTML_TRANSFER_MAX + 1];
        foxportal_read_file(storage, FOXPORTAL_START_PATH, start_html, sizeof(start_html));
        foxportal_read_file(storage, FOXPORTAL_FINISH_PATH, thanks_html, sizeof(thanks_html));
        furi_record_close(RECORD_STORAGE);

        foxportal_strip_newlines(start_html);
        foxportal_strip_newlines(thanks_html);

        if(start_html[0] != '\0') {
            static char cmd[FOX_PORTAL_HTML_TRANSFER_MAX + 32];
            snprintf(cmd, sizeof(cmd), "WIFIFOXPORTAL:SETPAGE:START:%s", start_html);
            esp_at_send(app->esp_at, cmd);
            EspAtMsg msg;
            if(esp_at_receive(app->esp_at, &msg, 5000)) app_log(app, "Start page: %s", msg.line);
        }
        if(thanks_html[0] != '\0') {
            static char cmd[FOX_PORTAL_HTML_TRANSFER_MAX + 32];
            snprintf(cmd, sizeof(cmd), "WIFIFOXPORTAL:SETPAGE:THANKS:%s", thanks_html);
            esp_at_send(app->esp_at, cmd);
            EspAtMsg msg;
            if(esp_at_receive(app->esp_at, &msg, 5000)) app_log(app, "Thanks page: %s", msg.line);
        }
    }
    app_render_log(app);

    char cmd[FOX_TEXT_INPUT_BUFFER_MAX + 40];
    snprintf(cmd, sizeof(cmd), "WIFIFOXPORTAL:START:%s:%s", ssid, date);
    esp_at_send(app->esp_at, cmd);

    app_log(app, "Starting Fox Portal...");
    app_render_log(app);
    EspAtMsg msg;
    if(esp_at_receive(app->esp_at, &msg, 5000)) {
        app_log(app, "%s", msg.line);
        /* Echoes back whichever SSID the firmware actually used - the
           one place this app learns the real name when the field was
           left blank and FOX_PORTAL_DEFAULT_SSID applied instead. */
        static const char* prefix = "FOXPORTAL:STARTED:";
        size_t prefix_len = strlen(prefix);
        if(strncmp(msg.line, prefix, prefix_len) == 0) {
            furi_string_set(app->portal_ssid, msg.line + prefix_len);
        }
    } else {
        app_log(app, "No response.");
    }
    app_render_log(app);
}

void foxportal_fields_render_menu(App* app) {
    foxportal_fields_load(app);
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Edit Input Names");
    submenu_add_item(app->submenu, "+ Add Input Name", 0, app_menu_item_callback, app);
    for(size_t i = 0; i < app->portal_field_count; i++) {
        submenu_add_item(
            app->submenu, app->portal_fields[i], (uint32_t)(i + 1), app_menu_item_callback, app);
    }
}

void foxportal_fields_menu_select(App* app, uint32_t index) {
    if(index == 0) {
        app_show_text_input(app, "New input name", TextInputPurposeFoxPortalNewFieldName);
        return;
    }
    size_t field_index = (size_t)(index - 1);
    if(field_index >= app->portal_field_count) return;
    app->portal_field_pending_delete = field_index;
    app_switch_to_menu(app, MenuContextFoxPortalFieldDelete);
}

void foxportal_new_field_submitted(App* app) {
    const char* raw = app->text_input_buffer;
    size_t len = strlen(raw);

    /* Mirrors fox_portal.cpp's own FIELDS key validation
       (parseFieldsCommand) - a name accepted here is guaranteed to be
       accepted when this app pushes it over serial later. */
    bool ok = len > 0 && len <= FOX_PORTAL_FIELD_KEY_MAX;
    for(size_t i = 0; ok && i < len; i++) {
        char c = raw[i];
        ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
             c == '_';
    }
    if(!ok) {
        app_log(
            app,
            "Invalid name - letters/digits/underscore only, max %d chars.",
            FOX_PORTAL_FIELD_KEY_MAX);
        app_render_log(app);
        app_switch_to_menu(app, MenuContextFoxPortalFields);
        return;
    }

    foxportal_fields_load(app); /* fresh, in case the card changed elsewhere */

    for(size_t i = 0; i < app->portal_field_count; i++) {
        if(strcmp(app->portal_fields[i], raw) == 0) {
            app_log(app, "\"%s\" is already in the list.", raw);
            app_render_log(app);
            app_switch_to_menu(app, MenuContextFoxPortalFields);
            return;
        }
    }

    if(app->portal_field_count >= FOX_PORTAL_MAX_FIELDS) {
        app_log(app, "Limit of %d input names reached.", FOX_PORTAL_MAX_FIELDS);
        app_render_log(app);
        app_switch_to_menu(app, MenuContextFoxPortalFields);
        return;
    }

    strncpy(app->portal_fields[app->portal_field_count], raw, FOX_PORTAL_FIELD_KEY_MAX);
    app->portal_fields[app->portal_field_count][FOX_PORTAL_FIELD_KEY_MAX] = '\0';
    app->portal_field_count++;
    foxportal_fields_save(app);
    app_switch_to_menu(app, MenuContextFoxPortalFields);
}

void foxportal_field_delete_render_menu(App* app) {
    submenu_reset(app->submenu);
    char header[40];
    const char* name = (app->portal_field_pending_delete < app->portal_field_count) ?
                            app->portal_fields[app->portal_field_pending_delete] :
                            "?";
    snprintf(header, sizeof(header), "Delete \"%s\"?", name);
    submenu_set_header(app->submenu, header);
    submenu_add_item(app->submenu, "Yes, Delete", 0, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "Cancel", 1, app_menu_item_callback, app);
}

void foxportal_field_delete_menu_select(App* app, uint32_t index) {
    if(index == 0) {
        if(app->portal_field_count <= 1) {
            /* inputnames.txt must never end up with 0 lines - see
               foxportal_fields_load()'s own comment. */
            app_log(app, "At least one input name is required.");
            app_render_log(app);
            app_switch_to_menu(app, MenuContextFoxPortalFields);
            return;
        }
        for(size_t i = app->portal_field_pending_delete; i + 1 < app->portal_field_count; i++) {
            strncpy(app->portal_fields[i], app->portal_fields[i + 1], FOX_PORTAL_FIELD_KEY_MAX + 1);
        }
        app->portal_field_count--;
        foxportal_fields_save(app);
    }
    app_switch_to_menu(app, MenuContextFoxPortalFields);
}

static App* s_qr_view_app = NULL;

static void qr_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_qr_view_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    if(app->qr_size <= 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 32, AlignCenter, AlignCenter, "No QR yet - run Start first.");
        return;
    }

    int scale = 64 / app->qr_size;
    if(scale < 1) scale = 1;
    int rendered = app->qr_size * scale;
    int ox = (128 - rendered) / 2;
    int oy = (64 - rendered) / 2;

    for(int y = 0; y < app->qr_size; y++) {
        for(int x = 0; x < app->qr_size; x++) {
            if(qrcodegen_getModule(app->qr_buf, x, y)) {
                canvas_draw_box(canvas, ox + x * scale, oy + y * scale, scale, scale);
            }
        }
    }
}

static bool qr_input_cb(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false;
}

View* foxportal_qr_view_alloc(App* app) {
    View* view = view_alloc();
    view_set_draw_callback(view, qr_draw_cb);
    view_set_input_callback(view, qr_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    s_qr_view_app = app;
    return view;
}
