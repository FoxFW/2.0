#include "session_log.h"

#include <furi_hal_rtc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define FOX_LOG_CONTENT_MAX_CHARS 6000

static void session_log_ensure_dir(App* app) {
    if(app == NULL || app->storage == NULL) return;
    storage_simply_mkdir(app->storage, "/ext/apps_data");
    storage_simply_mkdir(app->storage, "/ext/apps_data/fox_esp32_terminal");
    storage_simply_mkdir(app->storage, FOX_LOG_DIR);
}

void session_log_open(App* app) {
    if(app == NULL || app->storage == NULL) return;
    if(app->session_log_open) return;

    session_log_ensure_dir(app);

    DateTime dt;
    memset(&dt, 0, sizeof(dt));
    furi_hal_rtc_get_datetime(&dt);

    char stamp[32];
    snprintf(
        stamp,
        sizeof(stamp),
        "%04u-%02u-%02u_%02u-%02u-%02u",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);

    snprintf(
        app->session_log_path,
        sizeof(app->session_log_path),
        "%s/%s_terminal.log",
        FOX_LOG_DIR,
        stamp);

    app->session_log_file = storage_file_alloc(app->storage);
    if(app->session_log_file == NULL) {
        app->session_log_path[0] = '\0';
        return;
    }

    if(!storage_file_open(
           app->session_log_file, app->session_log_path, FSAM_READ_WRITE, FSOM_OPEN_APPEND)) {
        storage_file_free(app->session_log_file);
        app->session_log_file = NULL;
        app->session_log_path[0] = '\0';
        return;
    }

    app->session_log_open = true;
}

void session_log_write_line(App* app, const char* line) {
    if(app == NULL || line == NULL) return;
    if(!app->session_log_open || app->session_log_file == NULL) return;
    storage_file_write(app->session_log_file, line, strlen(line));
    storage_file_write(app->session_log_file, "\n", 1);
}

void session_log_close(App* app) {
    if(app == NULL) return;
    if(!app->session_log_open) return;
    if(app->session_log_file != NULL) {
        storage_file_close(app->session_log_file);
        storage_file_free(app->session_log_file);
    }
    app->session_log_file = NULL;
    app->session_log_open = false;
}

static void log_files_sort_desc(App* app) {
    for(size_t i = 1; i < app->log_file_count; i++) {
        char key[FOX_LOG_FILENAME_MAX];
        memcpy(key, app->log_files[i], FOX_LOG_FILENAME_MAX);

        size_t j = i;
        while(j > 0 && strcmp(app->log_files[j - 1], key) < 0) {
            memcpy(app->log_files[j], app->log_files[j - 1], FOX_LOG_FILENAME_MAX);
            j--;
        }
        memcpy(app->log_files[j], key, FOX_LOG_FILENAME_MAX);
    }
}

void log_list_scan(App* app) {
    if(app == NULL) return;

    app->log_file_count = 0;
    app->log_file_selected = 0;
    app->log_file_scroll = 0;

    if(app->storage == NULL) return;

    File* dir = storage_file_alloc(app->storage);
    if(dir == NULL) return;

    if(!storage_dir_open(dir, FOX_LOG_DIR)) {
        storage_dir_close(dir);
        storage_file_free(dir);
        return;
    }

    FileInfo info;
    char name[FOX_LOG_FILENAME_MAX];
    while(app->log_file_count < FOX_LOG_FILE_LIST_MAX &&
          storage_dir_read(dir, &info, name, sizeof(name))) {
        if(info.flags & FSF_DIRECTORY) continue;
        snprintf(app->log_files[app->log_file_count], FOX_LOG_FILENAME_MAX, "%s", name);
        app->log_file_count++;
    }

    storage_dir_close(dir);
    storage_file_free(dir);

    if(app->log_file_count > 0) {
        log_files_sort_desc(app);
    }
}

static void log_read_from_handle(File* file, FuriString* out) {
    uint64_t size = storage_file_size(file);
    bool truncated = size > FOX_LOG_CONTENT_MAX_CHARS;
    if(truncated) {
        storage_file_seek(file, size - FOX_LOG_CONTENT_MAX_CHARS, true);
        furi_string_cat(out, "...(earlier lines truncated)...\n");
    } else {
        storage_file_seek(file, 0, true);
    }

    uint8_t buf[129];
    uint16_t got;
    size_t total_read = 0;
    while((got = storage_file_read(file, buf, sizeof(buf) - 1)) > 0) {
        buf[got] = '\0';
        furi_string_cat(out, (const char*)buf);
        total_read += got;
        if(total_read > (FOX_LOG_CONTENT_MAX_CHARS * 4)) break;
    }
}

bool log_read_file(App* app, const char* filename, FuriString* out) {
    if(app == NULL || filename == NULL || filename[0] == '\0' || out == NULL) return false;
    if(app->storage == NULL) return false;

    furi_string_reset(out);

    bool is_live_session_file = app->session_log_open && app->session_log_file != NULL &&
                                 app->session_log_path[0] != '\0';
    if(is_live_session_file) {
        const char* base = strrchr(app->session_log_path, '/');
        base = (base != NULL) ? base + 1 : app->session_log_path;
        is_live_session_file = (strcmp(base, filename) == 0);
    }

    if(is_live_session_file) {
        uint64_t end_pos = storage_file_size(app->session_log_file);
        log_read_from_handle(app->session_log_file, out);

        storage_file_seek(app->session_log_file, end_pos, true);
        return true;
    }

    char path[FOX_LOG_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", FOX_LOG_DIR, filename);

    File* file = storage_file_alloc(app->storage);
    if(file == NULL) return false;

    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }

    log_read_from_handle(file, out);

    storage_file_close(file);
    storage_file_free(file);
    return true;
}
