#include "saved_wifi.h"

#include <stdio.h>
#include <string.h>
#include <storage/storage.h>
#include <gui/elements.h>

#define WIFI_TXT_PATH     "/ext/wifi.txt"
#define WIFI_TXT_BAK_PATH "/ext/wifi.txt.bak"
#define WIFI_SYNC_DIR     "/ext/apps_data/fox_esp32"
#define WIFI_SYNC_PATH    "/ext/apps_data/fox_esp32/wifi_synced.txt"

#define WIFI_SYNC_ENTRY_MAX 16
#define WIFI_SYNC_FILE_BUF  2048
#define WIFI_LINE_BUF       (FOX_WIFI_SSID_MAX + FOX_WIFI_PASS_MAX + 4)

static char s_sync_buf[WIFI_SYNC_FILE_BUF];

static void json_escape(const char* in, char* out, size_t out_size) {
    size_t o = 0;
    for(size_t i = 0; in[i] != '\0' && o + 2 < out_size; i++) {
        char c = in[i];
        if(c == '"' || c == '\\') out[o++] = '\\';
        out[o++] = c;
    }
    out[o] = '\0';
}

static bool json_extract_field(const char* json, const char* key, char* out, size_t out_size) {
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char* p = strstr(json, pattern);
    if(p == NULL) {
        out[0] = '\0';
        return false;
    }
    p += strlen(pattern);
    size_t o = 0;
    while(*p != '\0' && *p != '"' && o + 1 < out_size) {
        out[o++] = *p++;
    }
    out[o] = '\0';
    return true;
}

static bool read_file_to_buf(Storage* storage, const char* path, char* buf, size_t buf_size) {
    File* f = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t got = storage_file_read(f, buf, buf_size - 1);
        buf[got] = '\0';
        ok = true;
    }
    storage_file_close(f);
    storage_file_free(f);
    return ok;
}

static size_t parse_saved_lines(const char* buf, FoxSavedWifi* out, size_t out_max) {
    size_t count = 0;
    size_t len = strlen(buf);
    size_t pos = 0;
    while(pos < len && count < out_max) {
        size_t line_end = pos;
        while(line_end < len && buf[line_end] != '\n') line_end++;

        char line[WIFI_LINE_BUF];
        size_t line_len = line_end - pos;
        if(line_len >= sizeof(line)) line_len = sizeof(line) - 1;
        memcpy(line, buf + pos, line_len);
        line[line_len] = '\0';
        while(line_len > 0 && (line[line_len - 1] == '\r' || line[line_len - 1] == ' ')) {
            line[--line_len] = '\0';
        }

        size_t start = 0;
        while(line[start] == ' ') start++;

        if(line[start] != '\0' && line[start] != '#') {
            char* colon = strchr(line + start, ':');
            if(colon != NULL) {
                *colon = '\0';
                const char* ssid = line + start;
                const char* pass = colon + 1;
                if(ssid[0] != '\0') {
                    snprintf(
                        out[count].ssid, sizeof(out[count].ssid), "%.*s", FOX_WIFI_SSID_MAX - 1, ssid);
                    snprintf(
                        out[count].password,
                        sizeof(out[count].password),
                        "%.*s",
                        FOX_WIFI_PASS_MAX - 1,
                        pass);
                    count++;
                }
            }
        }
        pos = line_end + 1;
    }
    return count;
}

static void write_entries_to_file(
    Storage* storage,
    const char* path,
    const FoxSavedWifi* entries,
    size_t count) {
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        for(size_t i = 0; i < count; i++) {
            char line[WIFI_LINE_BUF];
            snprintf(line, sizeof(line), "%s:%s\n", entries[i].ssid, entries[i].password);
            storage_file_write(f, line, strlen(line));
        }
    }
    storage_file_close(f);
    storage_file_free(f);
}

static void ensure_wifi_txt(Storage* storage) {
    FileInfo info;
    if(storage_common_stat(storage, WIFI_TXT_PATH, &info) == FSE_OK) return;
    static const char* header = "# ONE SSID PER LINE ENTERED AS SSID:PASSWORD\n"
                                 "# E.g:\n"
                                 "# TestNetwork:TestPassword123\n";
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, WIFI_TXT_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, header, strlen(header));
    }
    storage_file_close(f);
    storage_file_free(f);
}

static void ensure_wifi_txt_bak(Storage* storage) {
    FileInfo info;
    if(storage_common_stat(storage, WIFI_TXT_BAK_PATH, &info) == FSE_OK) return;
    static const char* header = "# List of Deleted saved Wifi Networks from wifi.txt\n";
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, WIFI_TXT_BAK_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, header, strlen(header));
    }
    storage_file_close(f);
    storage_file_free(f);
}

static void ensure_sync_file(Storage* storage) {
    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, WIFI_SYNC_DIR);
    FileInfo info;
    if(storage_common_stat(storage, WIFI_SYNC_PATH, &info) == FSE_OK) return;
    File* f = storage_file_alloc(storage);
    storage_file_open(f, WIFI_SYNC_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    storage_file_close(f);
    storage_file_free(f);
}

static bool mirror_find(const FoxSavedWifi* arr, size_t count, const char* ssid, size_t* idx) {
    for(size_t i = 0; i < count; i++) {
        if(strcmp(arr[i].ssid, ssid) == 0) {
            *idx = i;
            return true;
        }
    }
    return false;
}

void wifi_saved_sync(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    ensure_wifi_txt(storage);
    ensure_sync_file(storage);

    FoxSavedWifi wtxt[WIFI_SYNC_ENTRY_MAX];
    size_t wtxt_count = 0;
    if(read_file_to_buf(storage, WIFI_TXT_PATH, s_sync_buf, sizeof(s_sync_buf))) {
        wtxt_count = parse_saved_lines(s_sync_buf, wtxt, WIFI_SYNC_ENTRY_MAX);
    }

    FoxSavedWifi mirror[WIFI_SYNC_ENTRY_MAX];
    size_t mirror_count = 0;
    if(read_file_to_buf(storage, WIFI_SYNC_PATH, s_sync_buf, sizeof(s_sync_buf))) {
        mirror_count = parse_saved_lines(s_sync_buf, mirror, WIFI_SYNC_ENTRY_MAX);
    }

    bool changed = false;
    bool list_full = false;
    for(size_t i = 0; i < wtxt_count; i++) {
        size_t idx = 0;
        bool found = mirror_find(mirror, mirror_count, wtxt[i].ssid, &idx);
        if(found && strcmp(mirror[idx].password, wtxt[i].password) == 0) continue;

        char ssid_esc[FOX_WIFI_SSID_MAX * 2];
        char pass_esc[FOX_WIFI_PASS_MAX * 2];
        json_escape(wtxt[i].ssid, ssid_esc, sizeof(ssid_esc));
        json_escape(wtxt[i].password, pass_esc, sizeof(pass_esc));
        char cmd[sizeof(ssid_esc) + sizeof(pass_esc) + 40];
        snprintf(
            cmd,
            sizeof(cmd),
            "[WIFI/SAVE]{\"ssid\":\"%s\",\"password\":\"%s\"}",
            ssid_esc,
            pass_esc);
        esp_at_send(app->esp_at, cmd);

        EspAtMsg msg;
        bool ok = false;
        if(esp_at_receive(app->esp_at, &msg, 4000)) {
            ok = strncmp(msg.line, "[WIFI/SAVE/SUCCESS]", 19) == 0;
            if(!ok && strstr(msg.line, "list full") != NULL) list_full = true;
        }
        if(!ok) continue;

        if(found) {
            snprintf(
                mirror[idx].password,
                sizeof(mirror[idx].password),
                "%.*s",
                FOX_WIFI_PASS_MAX - 1,
                wtxt[i].password);
        } else if(mirror_count < WIFI_SYNC_ENTRY_MAX) {
            snprintf(
                mirror[mirror_count].ssid,
                sizeof(mirror[mirror_count].ssid),
                "%.*s",
                FOX_WIFI_SSID_MAX - 1,
                wtxt[i].ssid);
            snprintf(
                mirror[mirror_count].password,
                sizeof(mirror[mirror_count].password),
                "%.*s",
                FOX_WIFI_PASS_MAX - 1,
                wtxt[i].password);
            mirror_count++;
        }
        changed = true;
    }

    if(changed) {
        write_entries_to_file(storage, WIFI_SYNC_PATH, mirror, mirror_count);
    }
    if(list_full) {
        app_log(app, "wifi.txt has more networks than the ESP32 can store.");
    }

    furi_record_close(RECORD_STORAGE);
}

static void remove_ssid_from_wifi_txt(Storage* storage, const char* ssid) {
    if(!read_file_to_buf(storage, WIFI_TXT_PATH, s_sync_buf, sizeof(s_sync_buf))) return;

    File* f = storage_file_alloc(storage);
    if(!storage_file_open(f, WIFI_TXT_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(f);
        return;
    }

    ensure_wifi_txt_bak(storage);
    File* bak = storage_file_alloc(storage);
    bool bak_open = storage_file_open(bak, WIFI_TXT_BAK_PATH, FSAM_WRITE, FSOM_OPEN_APPEND);

    size_t len = strlen(s_sync_buf);
    size_t pos = 0;
    while(pos < len) {
        size_t line_end = pos;
        while(line_end < len && s_sync_buf[line_end] != '\n') line_end++;

        char line[WIFI_LINE_BUF];
        size_t copy_len = line_end - pos;
        if(copy_len >= sizeof(line)) copy_len = sizeof(line) - 1;
        memcpy(line, s_sync_buf + pos, copy_len);
        line[copy_len] = '\0';
        while(copy_len > 0 && line[copy_len - 1] == '\r') line[--copy_len] = '\0';

        size_t start = 0;
        while(line[start] == ' ') start++;

        bool is_match = false;
        if(line[start] != '\0' && line[start] != '#') {
            char* colon = strchr(line + start, ':');
            if(colon != NULL) {
                *colon = '\0';
                if(strcmp(line + start, ssid) == 0) is_match = true;
                *colon = ':';
            }
        }

        if(!is_match) {
            storage_file_write(f, s_sync_buf + pos, line_end - pos);
            if(line_end < len) storage_file_write(f, "\n", 1);
        } else if(bak_open) {
            storage_file_write(bak, line + start, strlen(line + start));
            storage_file_write(bak, "\n", 1);
        }

        pos = line_end + 1;
    }

    if(bak_open) storage_file_close(bak);
    storage_file_free(bak);

    storage_file_close(f);
    storage_file_free(f);
}

static void remove_ssid_from_mirror(Storage* storage, const char* ssid) {
    if(!read_file_to_buf(storage, WIFI_SYNC_PATH, s_sync_buf, sizeof(s_sync_buf))) return;
    FoxSavedWifi entries[WIFI_SYNC_ENTRY_MAX];
    size_t count = parse_saved_lines(s_sync_buf, entries, WIFI_SYNC_ENTRY_MAX);
    size_t write = 0;
    for(size_t i = 0; i < count; i++) {
        if(strcmp(entries[i].ssid, ssid) == 0) continue;
        if(write != i) entries[write] = entries[i];
        write++;
    }
    write_entries_to_file(storage, WIFI_SYNC_PATH, entries, write);
}

void wifi_saved_remove_from_files(const char* ssid) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    remove_ssid_from_wifi_txt(storage, ssid);
    remove_ssid_from_mirror(storage, ssid);
    furi_record_close(RECORD_STORAGE);
}

static size_t saved_list_refresh(App* app) {
    esp_at_send(app->esp_at, "[WIFI/SAVED/LIST]");
    EspAtMsg msg;
    app->saved_wifi_count = 0;
    uint32_t deadline = furi_get_tick() + 4000;
    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;
        if(strcmp(msg.line, "[WIFI/SAVED/LIST/SUCCESS]") == 0) break;
        if(strncmp(msg.line, "[WIFI/SAVED/LIST]", 17) != 0) continue;
        if(app->saved_wifi_count >= FOX_SAVED_WIFI_MAX) continue;

        char ssid[FOX_WIFI_SSID_MAX] = {0};
        char pass[FOX_WIFI_PASS_MAX] = {0};
        json_extract_field(msg.line + 17, "ssid", ssid, sizeof(ssid));
        json_extract_field(msg.line + 17, "password", pass, sizeof(pass));
        if(ssid[0] == '\0') continue;

        FoxSavedWifi* n = &app->saved_wifi[app->saved_wifi_count];
        snprintf(n->ssid, sizeof(n->ssid), "%.*s", FOX_WIFI_SSID_MAX - 1, ssid);
        snprintf(n->password, sizeof(n->password), "%.*s", FOX_WIFI_PASS_MAX - 1, pass);
        app->saved_wifi_count++;
    }
    return app->saved_wifi_count;
}

static void show_saved_list_or_menu(App* app, size_t count) {
    if(count == 0) {
        app_switch_to_menu(app, MenuContextWifiConnection);
        return;
    }
    if(app->saved_wifi_selected >= count) app->saved_wifi_selected = count - 1;
    app->current_view = FoxCommanderViewSavedWifiList;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxCommanderViewSavedWifiList);
}

void wifi_saved_list_show(App* app) {
    app->menu_return_context = app->menu_context;
    app_log(app, "Loading saved networks...");
    app_render_log(app);

    size_t n = saved_list_refresh(app);
    if(n == 0) {
        app_log(app, "No saved networks.");
        app_render_log(app);
        return;
    }
    app->saved_wifi_selected = 0;
    app->saved_wifi_scroll = 0;
    app->current_view = FoxCommanderViewSavedWifiList;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxCommanderViewSavedWifiList);
}

void wifi_saved_action_render_menu(App* app) {
    submenu_reset(app->submenu);
    if(app->saved_wifi_selected < app->saved_wifi_count) {
        submenu_set_header(app->submenu, app->saved_wifi[app->saved_wifi_selected].ssid);
    } else {
        submenu_set_header(app->submenu, "Saved Network");
    }
    submenu_add_item(app->submenu, "Edit Password", 0, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "Delete", 1, app_menu_item_callback, app);
}

void wifi_saved_action_select(App* app, uint32_t index) {
    if(app->saved_wifi_selected >= app->saved_wifi_count) {
        app_switch_to_menu(app, MenuContextWifiConnection);
        return;
    }

    if(index == 0) {
        app_show_text_input(app, "New Password", TextInputPurposeSavedWifiEditPassword);
        return;
    }

    char ssid[FOX_WIFI_SSID_MAX];
    snprintf(
        ssid,
        sizeof(ssid),
        "%.*s",
        FOX_WIFI_SSID_MAX - 1,
        app->saved_wifi[app->saved_wifi_selected].ssid);

    char ssid_esc[FOX_WIFI_SSID_MAX * 2];
    json_escape(ssid, ssid_esc, sizeof(ssid_esc));
    char cmd[sizeof(ssid_esc) + 32];
    snprintf(cmd, sizeof(cmd), "[WIFI/FORGET]{\"ssid\":\"%s\"}", ssid_esc);
    esp_at_send(app->esp_at, cmd);
    EspAtMsg msg;
    esp_at_receive(app->esp_at, &msg, 4000);

    wifi_saved_remove_from_files(ssid);

    show_saved_list_or_menu(app, saved_list_refresh(app));
}

void wifi_saved_edit_password_submitted(App* app) {
    if(app->saved_wifi_selected >= app->saved_wifi_count) {
        app_switch_to_menu(app, MenuContextWifiConnection);
        return;
    }

    char ssid[FOX_WIFI_SSID_MAX];
    snprintf(
        ssid,
        sizeof(ssid),
        "%.*s",
        FOX_WIFI_SSID_MAX - 1,
        app->saved_wifi[app->saved_wifi_selected].ssid);
    const char* pass = app->text_input_buffer;

    char ssid_esc[FOX_WIFI_SSID_MAX * 2];
    char pass_esc[FOX_TEXT_INPUT_BUFFER_MAX * 2];
    json_escape(ssid, ssid_esc, sizeof(ssid_esc));
    json_escape(pass, pass_esc, sizeof(pass_esc));

    char cmd[sizeof(ssid_esc) + sizeof(pass_esc) + 40];
    snprintf(
        cmd, sizeof(cmd), "[WIFI/SAVE]{\"ssid\":\"%s\",\"password\":\"%s\"}", ssid_esc, pass_esc);
    esp_at_send(app->esp_at, cmd);

    app_log(app, "Updating password for %s...", ssid);
    app_render_log(app);

    EspAtMsg msg;
    if(esp_at_receive(app->esp_at, &msg, 4000)) {
        app_log(app, "%s", msg.line);
    } else {
        app_log(app, "No response.");
    }
    app_render_log(app);

    show_saved_list_or_menu(app, saved_list_refresh(app));
}

static App* s_saved_list_app = NULL;

#define SAVED_ROW_HEADER_H 14
#define SAVED_ROW_H        22
#define SAVED_ROW_VIS      2

static void saved_list_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_saved_list_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Saved Networks");

    if(app->saved_wifi_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "None found");
        return;
    }

    for(size_t i = app->saved_wifi_scroll;
        i < app->saved_wifi_count && (i - app->saved_wifi_scroll) < SAVED_ROW_VIS;
        i++) {
        int row = (int)(i - app->saved_wifi_scroll);
        int ry = SAVED_ROW_HEADER_H + row * SAVED_ROW_H;
        int by = ry + 1;
        int bh = SAVED_ROW_H - 2;
        bool selected = (i == app->saved_wifi_selected);
        const FoxSavedWifi* n = &app->saved_wifi[i];

        if(selected) {
            canvas_draw_rbox(canvas, 2, by, 124, bh, 3);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, 2, by, 124, bh, 3);
        }

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, by + 5, AlignCenter, AlignCenter, n->ssid);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, by + 15, AlignCenter, AlignCenter, app->expert_mode ? n->password : "********");

        canvas_set_color(canvas, ColorBlack);
    }

    if(app->saved_wifi_count > SAVED_ROW_VIS) {
        // Dotted track + solid position block, matching FOX_CHILL's
        // scrollbar style instead of a plain solid bar with no track.
        int available_h = 64 - SAVED_ROW_HEADER_H;
        elements_scrollbar_pos(
            canvas,
            128,
            SAVED_ROW_HEADER_H,
            (size_t)available_h,
            (size_t)app->saved_wifi_scroll,
            (size_t)app->saved_wifi_count);
    }
}

static bool saved_list_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(app->saved_wifi_count == 0) return false;

    switch(event->key) {
    case InputKeyUp:
        if(app->saved_wifi_selected > 0) {
            app->saved_wifi_selected--;
            if(app->saved_wifi_selected < app->saved_wifi_scroll) {
                app->saved_wifi_scroll = app->saved_wifi_selected;
            }
        } else {
            app->saved_wifi_selected = app->saved_wifi_count - 1;
            app->saved_wifi_scroll = (app->saved_wifi_count > SAVED_ROW_VIS) ?
                                          app->saved_wifi_count - SAVED_ROW_VIS :
                                          0;
        }
        with_view_model(app->saved_wifi_list_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyDown:
        if(app->saved_wifi_selected + 1 < app->saved_wifi_count) {
            app->saved_wifi_selected++;
            if(app->saved_wifi_selected >= app->saved_wifi_scroll + SAVED_ROW_VIS) {
                app->saved_wifi_scroll = app->saved_wifi_selected - SAVED_ROW_VIS + 1;
            }
        } else {
            app->saved_wifi_selected = 0;
            app->saved_wifi_scroll = 0;
        }
        with_view_model(app->saved_wifi_list_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyOk:
    case InputKeyRight:
        app_switch_to_menu(app, MenuContextWifiSavedAction);
        return true;
    case InputKeyBack:
    case InputKeyLeft:
        return false;
    default:
        return false;
    }
}

View* wifi_saved_list_view_alloc(App* app) {
    s_saved_list_app = app;
    View* view = view_alloc();
    view_set_draw_callback(view, saved_list_draw_cb);
    view_set_input_callback(view, saved_list_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void wifi_saved_list_view_free(View* view) {
    s_saved_list_app = NULL;
    view_free(view);
}
