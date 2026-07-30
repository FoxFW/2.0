#include "foxportal_menu.h"
#include "message_view.h"

#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FOXPORTAL_DIR "/ext/apps_data/fox_portal"
#define FOXPORTAL_START_PATH FOXPORTAL_DIR "/start.html"
#define FOXPORTAL_FINISH_PATH FOXPORTAL_DIR "/finish.html"

#define FOXPORTAL_CONFIG_PATH FOXPORTAL_DIR "/portal.cfg"

#define FOXPORTAL_DEFAULT_SSID "Fox Portal Demo"

#define FOXPORTAL_CONFIG_FILE_MAX 512

#define FOXPORTAL_RESULTS_PATH FOXPORTAL_DIR "/portal.txt"

#define FOXPORTAL_STAGE_PATH FOXPORTAL_DIR "/portal_pending.tmp"

#define FOXPORTAL_EXPORT_MAX_LINES 4000

#define FOXPORTAL_RESULTS_VIEW_MAX 3800

typedef enum {
    MenuPortalStart,
    MenuPortalStop,
    MenuPortalViewResults,
    MenuPortalViewTerminal,
    MenuPortalShowQr,
} MenuFoxPortalIndex;

static void foxportal_query_running_state(App* app);
static void foxportal_seed_default_pages(App* app);

void foxportal_render_menu(App* app) {
    foxportal_query_running_state(app);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Fox Portal");
    if(app->portal_running) {
        submenu_add_item(app->submenu, "Stop Portal", MenuPortalStop, app_menu_item_callback, app);
    } else {
        submenu_add_item(app->submenu, "Start Portal", MenuPortalStart, app_menu_item_callback, app);
    }
    submenu_add_item(
        app->submenu, "View Saved Results", MenuPortalViewResults, app_menu_item_callback, app);
    submenu_add_item(
        app->submenu, "View Terminal", MenuPortalViewTerminal, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "Show QR", MenuPortalShowQr, app_menu_item_callback, app);

    foxportal_sync_saved_results(app, false);
    foxportal_seed_default_pages(app);
}

static void build_wifi_qr_text(App* app, char* out, size_t out_size) {
    const char* ssid =
        (app->portal_ssid != NULL && furi_string_size(app->portal_ssid) > 0) ?
            furi_string_get_cstr(app->portal_ssid) :
            FOXPORTAL_DEFAULT_SSID;

    snprintf(out, out_size, "WIFI:T:nopass;S:%s;;", ssid);
}

static void show_qr(App* app) {
    char text[FOX_WIFI_SSID_MAX + 32];
    build_wifi_qr_text(app, text, sizeof(text));

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

static bool foxportal_path_exists(Storage* storage, const char* path) {
    File* file = storage_file_alloc(storage);
    bool exists = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    storage_file_close(file);
    storage_file_free(file);
    return exists;
}

static bool foxportal_wifi_connected(App* app) {
    esp_at_send(app->esp_at, "[WIFI/STATUS]");
    EspAtMsg msg;
    if(!esp_at_receive(app->esp_at, &msg, 1500)) return false;
    return strcmp(msg.line, "[WIFI/STATUS/SUCCESS]true") == 0;
}

static void foxportal_query_running_state(App* app) {
    esp_at_send(app->esp_at, "WIFIFOXPORTAL:STATUS");

    EspAtMsg msg;
    if(!esp_at_receive(app->esp_at, &msg, 1500)) return;

    static const char* running_prefix = "FOXPORTAL:RUNNING:";
    size_t running_prefix_len = strlen(running_prefix);
    if(strncmp(msg.line, running_prefix, running_prefix_len) == 0) {
        app->portal_running = true;
        furi_string_set(app->portal_ssid, msg.line + running_prefix_len);
    } else if(strcmp(msg.line, "FOXPORTAL:STOPPED") == 0) {
        app->portal_running = false;
    }
}

static bool foxportal_fetch_chunked_page(App* app, const char* prefix, char* out, size_t out_size) {
    out[0] = '\0';

    char begin_tag[48];
    char chunk_tag[48];
    char end_tag[48];
    snprintf(begin_tag, sizeof(begin_tag), "%s:BEGIN", prefix);
    snprintf(chunk_tag, sizeof(chunk_tag), "%s:CHUNK:", prefix);
    snprintf(end_tag, sizeof(end_tag), "%s:END", prefix);
    size_t chunk_tag_len = strlen(chunk_tag);

    EspAtMsg msg;
    if(!esp_at_receive(app->esp_at, &msg, 3000)) return false;
    if(strcmp(msg.line, begin_tag) != 0) return false;

    size_t pos = 0;
    for(int guard = 0; guard < 64; guard++) {
        if(!esp_at_receive(app->esp_at, &msg, 3000)) return false;
        if(strcmp(msg.line, end_tag) == 0) return true;
        if(strncmp(msg.line, chunk_tag, chunk_tag_len) == 0) {
            const char* content = msg.line + chunk_tag_len;
            size_t n = strlen(content);
            if(pos + n < out_size) {
                memcpy(out + pos, content, n);
                pos += n;
                out[pos] = '\0';
            }
        }
    }
    return false;
}

static void foxportal_seed_default_pages(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    foxportal_ensure_dir(storage);
    bool need_start = !foxportal_path_exists(storage, FOXPORTAL_START_PATH);
    bool need_finish = !foxportal_path_exists(storage, FOXPORTAL_FINISH_PATH);
    furi_record_close(RECORD_STORAGE);

    if(!need_start && !need_finish) return;

    static char page_buf[FOX_PORTAL_HTML_TRANSFER_MAX + 1];

    if(need_start) {
        esp_at_send(app->esp_at, "WIFIFOXPORTAL:GETDEFAULTSTART");
        if(foxportal_fetch_chunked_page(app, "FOXPORTAL:DEFAULTSTART", page_buf, sizeof(page_buf)) &&
           page_buf[0] != '\0') {
            Storage* s = furi_record_open(RECORD_STORAGE);
            foxportal_write_file(s, FOXPORTAL_START_PATH, page_buf);
            furi_record_close(RECORD_STORAGE);
            app_log(app, "Copied default start.html to SD card.");
        }
    }

    if(need_finish) {
        esp_at_send(app->esp_at, "WIFIFOXPORTAL:GETDEFAULTTHANKS");
        if(foxportal_fetch_chunked_page(app, "FOXPORTAL:DEFAULTTHANKS", page_buf, sizeof(page_buf)) &&
           page_buf[0] != '\0') {
            Storage* s = furi_record_open(RECORD_STORAGE);
            foxportal_write_file(s, FOXPORTAL_FINISH_PATH, page_buf);
            furi_record_close(RECORD_STORAGE);
            app_log(app, "Copied default finish.html to SD card.");
        }
    }
}

static void foxportal_config_parse(App* app, const char* raw) {
    size_t len = strlen(raw);
    size_t i = 0;

    size_t ssid_start = i;
    while(i < len && raw[i] != '\n') i++;
    size_t ssid_len = i - ssid_start;
    while(ssid_len > 0 &&
          (raw[ssid_start + ssid_len - 1] == '\r' || raw[ssid_start + ssid_len - 1] == ' '))
        ssid_len--;
    if(ssid_len > FOX_WIFI_SSID_MAX - 1) ssid_len = FOX_WIFI_SSID_MAX - 1;
    furi_string_set_strn(app->portal_ssid, raw + ssid_start, ssid_len);
}

static void foxportal_config_save(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    foxportal_write_file(storage, FOXPORTAL_CONFIG_PATH, furi_string_get_cstr(app->portal_ssid));
    furi_record_close(RECORD_STORAGE);
}

static void foxportal_config_load(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    foxportal_ensure_dir(storage);

    static char raw[FOXPORTAL_CONFIG_FILE_MAX];
    foxportal_read_file(storage, FOXPORTAL_CONFIG_PATH, raw, sizeof(raw));
    foxportal_config_parse(app, raw);

    furi_record_close(RECORD_STORAGE);

    if(furi_string_size(app->portal_ssid) == 0) {
        furi_string_set(app->portal_ssid, FOXPORTAL_DEFAULT_SSID);
        foxportal_config_save(app);
    }
}

static bool foxportal_copy_file_contents(File* src, File* dst) {
    char buf[128];
    size_t n;
    while((n = storage_file_read(src, buf, sizeof(buf))) > 0) {
        if(storage_file_write(dst, buf, n) != n) return false;
    }
    return true;
}

void foxportal_sync_saved_results(App* app, bool background) {
    esp_at_send(app->esp_at, "WIFIFOXPORTAL:EXPORTLOG");

    EspAtMsg msg;
    if(!esp_at_receive(app->esp_at, &msg, 1500)) {
        if(!background) {
            app_log(app, "No response checking for saved results - will retry next visit.");
        }
        return;
    }
    if(strcmp(msg.line, "LOGEMPTY") == 0) return;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    foxportal_ensure_dir(storage);

    FileInfo results_info;
    bool results_has_content =
        storage_common_stat(storage, FOXPORTAL_RESULTS_PATH, &results_info) == FSE_OK &&
        results_info.size > 0;

    File* stage = storage_file_alloc(storage);
    bool stage_open =
        storage_file_open(stage, FOXPORTAL_STAGE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);

    static const char* file_hdr_prefix = "LOGFILE:";
    static const char* file_end_prefix = "LOGFILEEND:";
    static const char* line_prefix = "LOGLINE:";
    static const char* done_prefix = "LOGDONE:";
    size_t file_hdr_prefix_len = strlen(file_hdr_prefix);
    size_t file_end_prefix_len = strlen(file_end_prefix);
    size_t line_prefix_len = strlen(line_prefix);
    size_t done_prefix_len = strlen(done_prefix);

    bool verified = false;
    bool aborted = false;
    size_t total_lines = 0;
    size_t seen_files = 0;

    size_t file_expected_lines = 0;
    bool file_in_progress = false;
    size_t file_seen_lines = 0;
    unsigned next_seq = 1;

    for(size_t guard = 0; !aborted && !verified && guard < FOXPORTAL_EXPORT_MAX_LINES; guard++) {
        if(strncmp(msg.line, done_prefix, done_prefix_len) == 0) {
            unsigned files = 0, lines = 0;
            sscanf(msg.line + done_prefix_len, "%u:%u", &files, &lines);
            verified = (seen_files == files) && (total_lines == lines) && !file_in_progress;
            if(!verified) aborted = true;
        } else if(strncmp(msg.line, file_hdr_prefix, file_hdr_prefix_len) == 0) {
            const char* rest = msg.line + file_hdr_prefix_len;
            const char* sep = strrchr(rest, ':');
            if(sep == NULL) {
                aborted = true;
            } else {
                file_expected_lines = (size_t)atoi(sep + 1);
                file_in_progress = true;
                file_seen_lines = 0;
                next_seq = 1;
                if(stage_open && file_expected_lines > 0) {
                    char header[80];
                    int name_len = (int)(sep - rest);
                    const char* header_fmt =
                        results_has_content ? "\n=== %.*s ===\n" : "=== %.*s ===\n";
                    int n = snprintf(header, sizeof(header), header_fmt, name_len, rest);
                    if(n > 0) {
                        size_t wlen = (size_t)n < sizeof(header) ? (size_t)n : sizeof(header) - 1;
                        storage_file_write(stage, header, wlen);
                    }
                    results_has_content = true;
                }
            }
        } else if(strncmp(msg.line, line_prefix, line_prefix_len) == 0) {
            const char* rest = msg.line + line_prefix_len;
            const char* sep = strchr(rest, ':');
            unsigned seq = sep ? (unsigned)atoi(rest) : 0;
            if(sep == NULL || !file_in_progress || seq != next_seq) {
                aborted = true;
            } else {
                const char* content = sep + 1;
                if(stage_open) {
                    const char* seg_start = content;
                    const char* p = content;
                    while(true) {
                        if(*p == ';' || *p == '\0') {
                            size_t seg_len = (size_t)(p - seg_start);
                            if(seg_len > 0) {
                                storage_file_write(stage, seg_start, seg_len);
                                storage_file_write(stage, "\n", 1);
                            }
                            if(*p == '\0') break;
                            seg_start = p + 1;
                        }
                        p++;
                    }
                }
                next_seq++;
                file_seen_lines++;
                total_lines++;
            }
        } else if(strncmp(msg.line, file_end_prefix, file_end_prefix_len) == 0) {
            const char* rest = msg.line + file_end_prefix_len;
            const char* sep = strrchr(rest, ':');
            size_t count = sep ? (size_t)atoi(sep + 1) : (size_t)-1;
            if(!file_in_progress || file_seen_lines != file_expected_lines ||
               file_seen_lines != count) {
                aborted = true;
            } else {
                seen_files++;
                file_in_progress = false;
            }
        }

        if(!aborted && !verified) {
            if(!esp_at_receive(app->esp_at, &msg, 1500)) {
                aborted = true;
            }
        }
    }

    if(stage_open) storage_file_close(stage);
    storage_file_free(stage);

    if(!verified) {
        storage_simply_remove(storage, FOXPORTAL_STAGE_PATH);
        furi_record_close(RECORD_STORAGE);
        if(!background) {
            app_log(
                app,
                "Saved-results check didn't verify (possible transmission hiccup) - nothing written or cleared, will retry next visit.");
        }
        return;
    }

    bool promoted = false;
    File* stage_r = storage_file_alloc(storage);
    File* dst = storage_file_alloc(storage);
    if(storage_file_open(stage_r, FOXPORTAL_STAGE_PATH, FSAM_READ, FSOM_OPEN_EXISTING) &&
       storage_file_open(dst, FOXPORTAL_RESULTS_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        promoted = foxportal_copy_file_contents(stage_r, dst);
    }
    storage_file_close(dst);
    storage_file_free(dst);
    storage_file_close(stage_r);
    storage_file_free(stage_r);
    storage_simply_remove(storage, FOXPORTAL_STAGE_PATH);
    furi_record_close(RECORD_STORAGE);

    if(!promoted) {
        if(!background) {
            app_log(
                app,
                "Verified saved results but couldn't write them to portal.txt - will retry next visit.");
        }
        return;
    }

    bool confirmed = false;
    for(int attempt = 0; !confirmed && attempt < 3; attempt++) {
        esp_at_send(app->esp_at, "WIFIFOXPORTAL:EXPORTCONFIRM");
        EspAtMsg ack;
        if(esp_at_receive(app->esp_at, &ack, 1500) && strcmp(ack.line, "OK") == 0) {
            confirmed = true;
        }
    }

    if(confirmed) {
        app_log(
            app,
            "Exported %u saved result line(s) to portal.txt and cleared them from the ESP32.",
            (unsigned)total_lines);
    } else {
        app_log(
            app,
            "Exported %u saved result line(s) to portal.txt, but couldn't confirm clearing the ESP32's copy - it may resend the same rows next sync (harmless duplicate, never lost data).",
            (unsigned)total_lines);
    }
}

void foxportal_show_saved_results(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    foxportal_ensure_dir(storage);

    furi_string_reset(app->results_text);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, FOXPORTAL_RESULTS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file);
        uint64_t to_read = size > FOXPORTAL_RESULTS_VIEW_MAX ? FOXPORTAL_RESULTS_VIEW_MAX : size;
        uint64_t skip = size - to_read;
        if(skip > 0) storage_file_seek(file, (uint32_t)skip, true);

        static char buf[FOXPORTAL_RESULTS_VIEW_MAX + 1];
        size_t read = storage_file_read(file, buf, (size_t)to_read);
        buf[read] = '\0';

        if(skip > 0) {
            furi_string_cat(
                app->results_text,
                "(earlier results trimmed - see portal.txt on the SD card)\n\n");
        }
        furi_string_cat(app->results_text, buf);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(furi_string_size(app->results_text) == 0) {
        furi_string_set(app->results_text, "No saved results yet.");
    }

    app->showing_results = true;
    app->menu_return_context = app->menu_context;
    app->terminal_scroll = 0;
    app->current_view = FoxCommanderViewTerminal;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxCommanderViewTerminal);
}

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
        if(foxportal_wifi_connected(app)) {
            message_view_show_wifi_disconnect_required(app);
            break;
        }

        foxportal_config_load(app);
        app_show_text_input(
            app,
            "SSID (edit or Save)",
            TextInputPurposeFoxPortalSsid,
            furi_string_get_cstr(app->portal_ssid));
        break;
    case MenuPortalStop:
        app_log(app, "Stopping Fox Portal...");
        app_render_log(app);
        esp_at_send(app->esp_at, "WIFIFOXPORTAL:STOP");
        if(app_expect_line(app, "FOXPORTAL:STOPPED", 5000)) {
            app->portal_running = false;
        } else {
            app_log(app, "No response.");
        }
        app_render_log(app);
        break;
    case MenuPortalViewResults:
        foxportal_show_saved_results(app);
        break;
    case MenuPortalViewTerminal:
        app_log(app, "Terminal - watching for live sync activity...");
        app_render_log(app);
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
    char date[32];
    snprintf(
        date,
        sizeof(date),
        "%04u-%02u-%02u_%02u-%02u-%02u",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);

    app_log(app, "Syncing pages from SD card...");
    app_render_log(app);

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

    char cmd[FOX_TEXT_INPUT_BUFFER_MAX + 64];
    snprintf(cmd, sizeof(cmd), "WIFIFOXPORTAL:START:%s:%s", ssid, date);
    esp_at_send(app->esp_at, cmd);

    app_log(app, "Starting Fox Portal...");
    app_render_log(app);
    EspAtMsg msg;
    if(esp_at_receive(app->esp_at, &msg, 5000)) {
        app_log(app, "%s", msg.line);

        static const char* prefix = "FOXPORTAL:STARTED:";
        size_t prefix_len = strlen(prefix);
        if(strncmp(msg.line, prefix, prefix_len) == 0) {
            furi_string_set(app->portal_ssid, msg.line + prefix_len);
            app->portal_running = true;

            foxportal_config_save(app);
        }
    } else {
        app_log(app, "No response.");
    }
    app_render_log(app);
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
