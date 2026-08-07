#include "download.h"
#include "download_progress_view.h"
#include "download_queue.h"
#include "url_download.h"
#include "json_mini.h"
#include "fox_file_downloader_icons.h"

#include <gui/icon.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define FOX_LAST_URL_PATH FOX_DOWNLOAD_DATA_DIR "/last_download_url.txt"

static bool ends_with_ci(const char* s, const char* suffix) {
    size_t s_len = strlen(s);
    size_t suf_len = strlen(suffix);
    if(suf_len > s_len) return false;
    const char* tail = s + (s_len - suf_len);
    for(size_t i = 0; i < suf_len; i++) {
        if(tolower((unsigned char)tail[i]) != tolower((unsigned char)suffix[i])) return false;
    }
    return true;
}

void url_derive_filename(const char* url, char* out, size_t out_size) {
    const char* p = strstr(url, "://");
    p = p ? p + 3 : url;
    const char* path_start = strchr(p, '/');

    const char* name = "";
    if(path_start) {
        const char* last_slash = path_start;
        for(const char* q = path_start; *q; q++) {
            if(*q == '/') last_slash = q;
        }
        name = last_slash + 1;
    }

    char trimmed[FOX_DOWNLOAD_NAME_MAX * 2];
    size_t n = 0;
    for(; name[n] != '\0' && name[n] != '?' && name[n] != '#' && n + 1 < sizeof(trimmed); n++) {
        trimmed[n] = name[n];
    }
    trimmed[n] = '\0';

    char decoded[FOX_DOWNLOAD_NAME_MAX * 2];
    size_t d = 0;
    for(size_t i = 0; trimmed[i] != '\0' && d + 1 < sizeof(decoded); i++) {
        if(trimmed[i] == '%' && isxdigit((unsigned char)trimmed[i + 1]) &&
           isxdigit((unsigned char)trimmed[i + 2])) {
            char hex[3] = {trimmed[i + 1], trimmed[i + 2], '\0'};
            decoded[d++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else {
            decoded[d++] = trimmed[i];
        }
    }
    decoded[d] = '\0';

    size_t o = 0;
    for(size_t i = 0; decoded[i] != '\0' && o + 1 < out_size; i++) {
        char c = decoded[i];
        if(c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' ||
           c == '|') {
            c = '_';
        }
        out[o++] = c;
    }
    out[o] = '\0';

    if(out[0] == '\0') {
        snprintf(out, out_size, "download.bin");
    }
}

static bool file_exists(Storage* storage, const char* path) {
    FileInfo info;
    return storage_common_stat(storage, path, &info) == FSE_OK;
}

static void unique_download_path(Storage* storage, const char* name, char* out, size_t out_size) {
    snprintf(out, out_size, "%s/%s", FOX_DOWNLOAD_DIR, name);
    if(!file_exists(storage, out)) return;

    char base[FOX_DOWNLOAD_NAME_MAX];
    char ext[FOX_DOWNLOAD_NAME_MAX] = "";
    snprintf(base, sizeof(base), "%.63s", name);
    char* dot = strrchr(base, '.');
    if(dot && dot != base) {
        snprintf(ext, sizeof(ext), "%.63s", dot);
        *dot = '\0';
    }

    for(int i = 2; i < 100; i++) {
        snprintf(out, out_size, "%s/%s (%d)%s", FOX_DOWNLOAD_DIR, base, i, ext);
        if(!file_exists(storage, out)) return;
    }
}

void http_download_url_submitted(App* app) {
    if(app->text_input_buffer[0] == '\0') {
        app_log(app, "No URL entered.");
        app_render_log(app);
        return;
    }

    snprintf(app->download_url, sizeof(app->download_url), "%s", app->text_input_buffer);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, FOX_DOWNLOAD_DATA_DIR);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, FOX_LAST_URL_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, app->download_url, strlen(app->download_url));
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);

    // This used to run a whole separate "is this URL real?" probe first -
    // connect, read the headers, then deliberately hang up - before ever
    // starting the actual download, which meant a full second connection
    // (and its own chance to fail) right after the first one succeeded.
    // Now there's exactly one connection: it starts streaming into the
    // .download file the moment [DOWNLOAD/START/SUCCESS] comes back
    // (see download_derive_found_info/download_attempt in url_download.c),
    // in the background, before the user has done anything. download_confirmed
    // stays false until they press Install on the Connecting/Downloading
    // progress screen; until then, Cancel/Back just deletes whatever came
    // down so far (download_unconfirmed_finished, download_pending_cancel)
    // instead of leaving it as a resumable interrupted download.
    app->download_purpose = DownloadPurposeFile;
    app->download_confirmed = false;
    app->download_connected = false;
    app->download_found_name[0] = '\0';
    app->download_path[0] = '\0';
    app->download_found_focus_left = false;
    app->download_return_view = FoxDownloaderViewMenu;
    app->menu_return_context = app->menu_context;
    app_start_download(app);
}

// Called from the worker thread (url_download.c's download_attempt) the
// first time a download actually connects - derives the destination
// filename/path from the URL now that we know it's real, so the
// Connecting screen has something to show and a real download_path exists
// for the streaming loop to write to.
void download_derive_found_info(App* app, uint32_t size, const char* type) {
    url_derive_filename(
        app->download_url, app->download_found_name, sizeof(app->download_found_name));
    app->download_found_size = size;
    snprintf(app->download_found_type, sizeof(app->download_found_type), "%s", type);

    bool looks_like_html = (strstr(type, "text/html") != NULL) ||
                           (strstr(type, "application/xhtml") != NULL);
    bool url_wants_html =
        ends_with_ci(app->download_found_name, ".html") ||
        ends_with_ci(app->download_found_name, ".htm");
    app->download_found_type_suspicious = looks_like_html && !url_wants_html;

    Storage* path_storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(path_storage, FOX_DOWNLOAD_DIR);
    unique_download_path(
        path_storage, app->download_found_name, app->download_path, sizeof(app->download_path));
    furi_record_close(RECORD_STORAGE);
}

// Sets the worker's cancel flag - same mechanism as a normal confirmed
// download's Back-to-cancel, just without the confirm dialog, since
// nothing here has been confirmed as something the user wants to keep
// yet. The actual cleanup (delete vs. just stopping) happens once the
// worker has actually unwound, in download_unconfirmed_finished below -
// it can't happen here, since the worker thread may still be mid-write.
void download_pending_cancel(App* app) {
    app->download_cancel_requested = true;
    app->download_cancel_requested_tick = furi_get_tick();
}

// The user pressed Install - from this point on it behaves exactly like a
// download they started normally: mark it resumable if interrupted, and
// let the progress view switch from "Connecting" to "Downloading".
void download_install_confirm(App* app) {
    app->download_confirmed = true;

    bool already_done = false;
    bool already_ok = false;
    furi_mutex_acquire(app->download_progress_mutex, FuriWaitForever);
    already_done = app->download_progress_done;
    already_ok = app->download_progress_ok;
    furi_mutex_release(app->download_progress_mutex);

    if(already_done && already_ok) {
        // It finished in the background before the user got here - the
        // worker already renamed the file into place (download_worker_thread
        // doesn't wait on confirmation to do that). Nothing left to mark as
        // resumable for a download that's already done; just report it.
        uint32_t bytes;
        furi_mutex_acquire(app->download_progress_mutex, FuriWaitForever);
        bytes = app->download_progress_bytes;
        furi_mutex_release(app->download_progress_mutex);
        app_log(
            app,
            "Downloaded %s (%lu KB) to %s",
            app->download_found_name,
            (unsigned long)(bytes / 1024),
            app->download_path);
        app_render_log(app);
        return;
    }

    download_state_mark_started(app->download_url, app->download_path);
}

// Called from main.c's FOX_DOWNLOAD_EVENT_WORKER_DONE handler when a plain
// URL download's worker finishes before the user has pressed Install.
// Nothing here was ever something the user asked to keep: a cancel or a
// genuine failure cleans up and heads back; finishing successfully in the
// background just leaves the merged Connecting/Install screen up (it's
// already showing it, pinned at 100%) so the user still gets to decide.
void download_unconfirmed_finished(App* app, bool ok, const char* error) {
    if(app->download_cancel_requested) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        if(app->download_path[0] != '\0') {
            char work_path[FOX_DOWNLOAD_PATH_MAX + 10];
            download_work_path(app->download_path, work_path, sizeof(work_path));
            storage_common_remove(storage, work_path);
            storage_common_remove(storage, app->download_path);
        }
        furi_record_close(RECORD_STORAGE);
        app_switch_to_menu(app, app->menu_return_context);
        return;
    }
    if(!ok) {
        app_log(app, "Download failed: %s", error);
        app_render_log(app);
        return;
    }
    // Finished cleanly but unconfirmed - stay put, the Connecting/Install
    // screen is already showing.
}

static void load_last_download_url(App* app) {
    app->download_url[0] = '\0';
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, FOX_LAST_URL_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint16_t got =
            storage_file_read(file, app->download_url, sizeof(app->download_url) - 1);
        app->download_url[got] = '\0';
        char* nl = strchr(app->download_url, '\n');
        if(nl) *nl = '\0';
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void file_downloader_check_queue_or_keyboard(App* app) {
    char queued_url[FOX_TEXT_INPUT_BUFFER_MAX];
    if(download_queue_take_next(queued_url, sizeof(queued_url))) {
        snprintf(app->text_input_buffer, sizeof(app->text_input_buffer), "%s", queued_url);
        http_download_url_submitted(app);
        return;
    }

    load_last_download_url(app);
    app_show_text_input_prefill(app, "File URL", TextInputPurposeHttpDownloadUrl, app->download_url);
}

void file_downloader_open(App* app) {
    char resume_url[FOX_TEXT_INPUT_BUFFER_MAX];
    char resume_path[FOX_DOWNLOAD_PATH_MAX];
    if(download_state_load(resume_url, sizeof(resume_url), resume_path, sizeof(resume_path), NULL)) {
        download_resume_show(app, resume_url, resume_path);
    } else {
        file_downloader_check_queue_or_keyboard(app);
    }
}

static App* s_download_found_app = NULL;

void download_found_confirm(App* app) {
    // Catalog installs and GitHub file downloads only ever show File Found
    // after already picking their destination path with no network probe
    // beforehand (unlike a plain URL download) - this is their one and
    // only connection, so it's confirmed the instant the user presses it.
    app->download_confirmed = true;
    app_start_download(app);
}

void app_start_download(App* app) {
    bool tracks_resume_state = app->download_confirmed &&
                                app->download_purpose != DownloadPurposeCatalogPage &&
                                app->download_purpose != DownloadPurposeGithubRepoInfo &&
                                app->download_purpose != DownloadPurposeGithubTree;
    if(tracks_resume_state) {
        download_state_mark_started(app->download_url, app->download_path);
    }

    download_progress_view_reset(app->download_progress_view);
    app->current_view = FoxDownloaderViewDownloadProgress;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewDownloadProgress);
    furi_timer_start(app->download_progress_timer, furi_ms_to_ticks(200));
    download_start_worker(app);
}

void download_found_cancel(App* app) {
    esp_at_send(app->esp_at, "[DOWNLOAD/CANCEL]");
    EspAtMsg msg;
    esp_at_receive(app->esp_at, &msg, 2000);
    if(app->download_return_view == FoxDownloaderViewMenu) {
        app_switch_to_menu(app, app->menu_return_context);
    } else {
        app->current_view = app->download_return_view;
        view_dispatcher_switch_to_view(app->view_dispatcher, app->download_return_view);
    }
}

static void download_found_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_download_found_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "File Found");

    bool suspicious = app->download_found_type_suspicious;
    int32_t name_y = suspicious ? 11 : 14;
    int32_t size_y = suspicious ? 20 : 25;

    char name_str[32];
    size_t len = strlen(app->download_found_name);
    if(len >= sizeof(name_str)) {
        snprintf(
            name_str,
            sizeof(name_str),
            "...%s",
            app->download_found_name + len - (sizeof(name_str) - 4));
    } else {
        snprintf(name_str, sizeof(name_str), "%.31s", app->download_found_name);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, name_y, AlignCenter, AlignTop, name_str);

    char size_str[24];
    if(app->download_found_size > 0) {
        snprintf(size_str, sizeof(size_str), "%lu KB", (unsigned long)(app->download_found_size / 1024));
    } else {
        snprintf(size_str, sizeof(size_str), "size unknown");
    }
    canvas_draw_str_aligned(canvas, 64, size_y, AlignCenter, AlignTop, size_str);

    if(!suspicious && app->download_found_type[0] != '\0') {
        char type_str[40];
        snprintf(type_str, sizeof(type_str), "%.36s", app->download_found_type);
        canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignTop, type_str);
    }

    if(suspicious) {
        canvas_draw_str_aligned(
            canvas, 64, 29, AlignCenter, AlignTop, "Looks like a web page, not a");
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignTop, "file - check the URL?");
    }

    int32_t bar_y = 64 - 16;
    int32_t btn_gap = 4;
    int32_t btn_w = (128 - btn_gap * 3) / 2;
    int32_t left_x = btn_gap;
    int32_t right_x = btn_gap * 2 + btn_w;
    bool focus_left = app->download_found_focus_left;

    canvas_set_color(canvas, ColorBlack);
    if(focus_left) {
        canvas_draw_rbox(canvas, left_x, bar_y, btn_w, 16, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, left_x + btn_w / 2, bar_y + 8, AlignCenter, AlignCenter, "Cancel");
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, right_x, bar_y, btn_w, 16, 3);
        canvas_draw_str_aligned(
            canvas, right_x + btn_w / 2, bar_y + 8, AlignCenter, AlignCenter, "Download");
    } else {
        canvas_draw_rframe(canvas, left_x, bar_y, btn_w, 16, 3);
        canvas_draw_str_aligned(canvas, left_x + btn_w / 2, bar_y + 8, AlignCenter, AlignCenter, "Cancel");
        canvas_draw_rbox(canvas, right_x, bar_y, btn_w, 16, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(
            canvas, right_x + btn_w / 2, bar_y + 8, AlignCenter, AlignCenter, "Download");
        canvas_set_color(canvas, ColorBlack);
    }
}

static bool download_found_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort) return false;

    switch(event->key) {
    case InputKeyLeft:
        if(!app->download_found_focus_left) {
            app->download_found_focus_left = true;
            with_view_model(app->download_found_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyRight:
        if(app->download_found_focus_left) {
            app->download_found_focus_left = false;
            with_view_model(app->download_found_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyOk:
        if(app->download_found_focus_left) {
            download_found_cancel(app);
        } else {
            download_found_confirm(app);
        }
        return true;
    case InputKeyBack:
        download_found_cancel(app);
        return true;
    default:
        return false;
    }
}

View* download_found_view_alloc(App* app) {
    s_download_found_app = app;
    View* view = view_alloc();
    view_set_draw_callback(view, download_found_draw_cb);
    view_set_input_callback(view, download_found_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void download_found_view_free(View* v) {
    s_download_found_app = NULL;
    view_free(v);
}

static App* s_download_resume_app = NULL;

void download_resume_show(App* app, const char* url, const char* path) {
    snprintf(app->download_resume_url, sizeof(app->download_resume_url), "%s", url);
    snprintf(app->download_resume_path, sizeof(app->download_resume_path), "%s", path);
    url_derive_filename(url, app->download_resume_name, sizeof(app->download_resume_name));
    app->download_resume_focus_left = false;

    app->menu_return_context = app->menu_context;
    app->current_view = FoxDownloaderViewDownloadResume;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewDownloadResume);
}

void download_resume_start(App* app) {
    snprintf(app->download_url, sizeof(app->download_url), "%s", app->download_resume_url);
    snprintf(app->download_found_name, sizeof(app->download_found_name), "%s", app->download_resume_name);
    snprintf(app->download_path, sizeof(app->download_path), "%s", app->download_resume_path);
    app->download_purpose = DownloadPurposeFile;
    // Resuming an interrupted download was already confirmed the first
    // time around (that's why there's a resume marker on disk to resume
    // from) - skip straight to the normal Downloading screen, no
    // Connecting/Install step.
    app->download_confirmed = true;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, FOX_DOWNLOAD_DIR);
    furi_record_close(RECORD_STORAGE);

    download_progress_view_reset(app->download_progress_view);
    app->current_view = FoxDownloaderViewDownloadProgress;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewDownloadProgress);
    furi_timer_start(app->download_progress_timer, furi_ms_to_ticks(200));
    download_start_worker(app);
}

void download_resume_delete(App* app) {
    // "Restart" - delete whatever partial data was saved, then start the
    // same download over from scratch instead of dropping back to the
    // menu. The state marker is left alone: it already points at this
    // download's URL/path, and with the .download file gone,
    // url_download.c's resume-offset detection naturally finds nothing to
    // resume from and downloads from byte 0.
    if(app->download_resume_path[0] != '\0') {
        char work_path[FOX_DOWNLOAD_PATH_MAX + 10];
        download_work_path(app->download_resume_path, work_path, sizeof(work_path));
        Storage* storage = furi_record_open(RECORD_STORAGE);
        storage_common_remove(storage, work_path);
        furi_record_close(RECORD_STORAGE);
    }
    download_resume_start(app);
}

static void download_resume_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_download_resume_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Download Interrupted");

    char name_str[32];
    size_t len = strlen(app->download_resume_name);
    if(len >= sizeof(name_str)) {
        snprintf(
            name_str,
            sizeof(name_str),
            "...%s",
            app->download_resume_name + len - (sizeof(name_str) - 4));
    } else {
        snprintf(name_str, sizeof(name_str), "%.31s", app->download_resume_name);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignTop, name_str);
    canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, "didn't finish last time.");

    int32_t bar_y = 64 - 16;
    int32_t btn_gap = 4;
    int32_t btn_w = (128 - btn_gap * 3) / 2;
    int32_t left_x = btn_gap;
    int32_t right_x = btn_gap * 2 + btn_w;
    bool focus_left = app->download_resume_focus_left;

    canvas_set_color(canvas, ColorBlack);
    if(focus_left) {
        canvas_draw_rbox(canvas, left_x, bar_y, btn_w, 16, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, left_x + btn_w / 2, bar_y + 8, AlignCenter, AlignCenter, "Restart");
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, right_x, bar_y, btn_w, 16, 3);
        canvas_draw_str_aligned(
            canvas, right_x + btn_w / 2, bar_y + 8, AlignCenter, AlignCenter, "Resume");
    } else {
        canvas_draw_rframe(canvas, left_x, bar_y, btn_w, 16, 3);
        canvas_draw_str_aligned(canvas, left_x + btn_w / 2, bar_y + 8, AlignCenter, AlignCenter, "Restart");
        canvas_draw_rbox(canvas, right_x, bar_y, btn_w, 16, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(
            canvas, right_x + btn_w / 2, bar_y + 8, AlignCenter, AlignCenter, "Resume");
        canvas_set_color(canvas, ColorBlack);
    }
}

static bool download_resume_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort) return false;

    switch(event->key) {
    case InputKeyLeft:
        if(!app->download_resume_focus_left) {
            app->download_resume_focus_left = true;
            with_view_model(app->download_resume_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyRight:
        if(app->download_resume_focus_left) {
            app->download_resume_focus_left = false;
            with_view_model(app->download_resume_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyOk:
        if(app->download_resume_focus_left) {
            download_resume_delete(app);
        } else {
            download_resume_start(app);
        }
        return true;
    case InputKeyBack:
        app_switch_to_menu(app, MenuContextMain);
        return true;
    default:
        return false;
    }
}

View* download_resume_view_alloc(App* app) {
    s_download_resume_app = app;
    View* view = view_alloc();
    view_set_draw_callback(view, download_resume_draw_cb);
    view_set_input_callback(view, download_resume_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void download_resume_view_free(View* v) {
    s_download_resume_app = NULL;
    view_free(v);
}
