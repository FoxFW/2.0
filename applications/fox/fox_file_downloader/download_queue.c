#include "download_queue.h"

#include <storage/storage.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define QUEUE_HEADER \
    "#Add download urls to this file, one per line, then open Fox File Downloader > File Download (URL)\n"
#define QUEUE_BUF_MAX 4096

void download_queue_ensure_file(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(!storage_common_exists(storage, FOX_DOWNLOAD_QUEUE_PATH)) {
        File* file = storage_file_alloc(storage);
        if(storage_file_open(file, FOX_DOWNLOAD_QUEUE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_write(file, QUEUE_HEADER, strlen(QUEUE_HEADER));
        }
        storage_file_close(file);
        storage_file_free(file);
    }

    furi_record_close(RECORD_STORAGE);
}

bool download_queue_take_next(char* url_out, size_t url_out_size) {
    static char buf[QUEUE_BUF_MAX];

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    size_t len = 0;
    if(storage_file_open(file, FOX_DOWNLOAD_QUEUE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        len = storage_file_read(file, buf, sizeof(buf) - 1);
    }
    storage_file_close(file);
    buf[len] = '\0';

    bool found = false;
    size_t found_start = 0;
    size_t line_start = 0;

    for(size_t i = 0; i <= len; i++) {
        if(i != len && buf[i] != '\n') continue;

        size_t trimmed_end = i;
        if(trimmed_end > line_start && buf[trimmed_end - 1] == '\r') trimmed_end--;

        if(!found) {
            char saved = buf[trimmed_end];
            buf[trimmed_end] = '\0';
            const char* p = buf + line_start;
            while(*p == ' ' || *p == '\t') p++;
            if(*p != '\0' && *p != '#') {
                snprintf(url_out, url_out_size, "%s", p);
                found = true;
                found_start = line_start;
            }
            buf[trimmed_end] = saved;
        }

        line_start = i + 1;
    }

    if(found) {
        if(storage_file_open(file, FOX_DOWNLOAD_QUEUE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            if(found_start > 0) storage_file_write(file, buf, found_start);
            storage_file_write(file, "#", 1);
            storage_file_write(file, buf + found_start, len - found_start);
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return found;
}

void download_state_mark_started(const char* url, const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, FOX_DOWNLOAD_DATA_DIR);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, FOX_DOWNLOAD_STATE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char line[16];
        storage_file_write(file, url, strlen(url));
        storage_file_write(file, "\n", 1);
        storage_file_write(file, path, strlen(path));
        storage_file_write(file, "\n", 1);
        int n = snprintf(line, sizeof(line), "0\n");
        storage_file_write(file, line, (uint16_t)n);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void download_state_set_total(uint32_t total) {
    char url[FOX_TEXT_INPUT_BUFFER_MAX];
    char path[FOX_DOWNLOAD_PATH_MAX];
    if(!download_state_load(url, sizeof(url), path, sizeof(path), NULL)) return;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, FOX_DOWNLOAD_STATE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char line[16];
        storage_file_write(file, url, strlen(url));
        storage_file_write(file, "\n", 1);
        storage_file_write(file, path, strlen(path));
        storage_file_write(file, "\n", 1);
        int n = snprintf(line, sizeof(line), "%lu\n", (unsigned long)total);
        storage_file_write(file, line, (uint16_t)n);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void download_state_clear(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_remove(storage, FOX_DOWNLOAD_STATE_PATH);
    furi_record_close(RECORD_STORAGE);
}

bool download_state_load(
    char* url_out,
    size_t url_out_size,
    char* path_out,
    size_t path_out_size,
    uint32_t* total_out) {
    static char buf[512];

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    size_t len = 0;
    bool opened = storage_file_open(file, FOX_DOWNLOAD_STATE_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    if(opened) {
        len = storage_file_read(file, buf, sizeof(buf) - 1);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!opened || len == 0) return false;
    buf[len] = '\0';

    char* nl = strchr(buf, '\n');
    if(!nl) return false;
    *nl = '\0';
    const char* path_line = nl + 1;

    char* cr = strchr(buf, '\r');
    if(cr) *cr = '\0';

    if(buf[0] == '\0') return false;

    snprintf(url_out, url_out_size, "%s", buf);

    char path_buf[FOX_DOWNLOAD_PATH_MAX];
    snprintf(path_buf, sizeof(path_buf), "%s", path_line);
    char* path_nl = strchr(path_buf, '\n');
    const char* total_line = path_nl ? path_nl + 1 : NULL;
    if(path_nl) *path_nl = '\0';
    char* path_cr = strchr(path_buf, '\r');
    if(path_cr) *path_cr = '\0';
    snprintf(path_out, path_out_size, "%s", path_buf);

    if(total_out) {
        *total_out = total_line ? (uint32_t)strtoul(total_line, NULL, 10) : 0;
    }

    return true;
}
