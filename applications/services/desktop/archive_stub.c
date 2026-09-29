#include "archive_stub.h"
#include <furi.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <applications/main/archive/helpers/archive_favorites.h>

#define ARCHIVE_FAV_PATH EXT_PATH("favorites.txt")

const void* FLIPPER_ARCHIVE = NULL;

static int32_t archive_stub_read_line(File* file, char* buffer, size_t max_len) {
    size_t i = 0;
    while(i < max_len - 1) {
        char c;
        uint16_t read = storage_file_read(file, &c, 1);
        if(read == 0) {
            if(i == 0) return 0;
            break;
        }
        if(c == '\r') continue;
        if(c == '\n') break;
        buffer[i++] = c;
    }
    buffer[i] = '\0';
    return i;
}

void archive_favorites_handle_setting_pin_unpin(const char* app_name, const char* setting) {
    UNUSED(app_name);
    UNUSED(setting);

}

bool archive_is_favorite(const char* format, ...) {
    if(!format) {
        return false;
    }

    va_list args;
    va_start(args, format);
    char path_buffer[256];
    vsnprintf(path_buffer, sizeof(path_buffer), format, args);
    va_end(args);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    bool found = false;
    if(storage_file_open(file, ARCHIVE_FAV_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buffer[256];
        while(archive_stub_read_line(file, buffer, sizeof(buffer)) > 0) {
            if(strcmp(buffer, path_buffer) == 0) {
                found = true;
                break;
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return found;
}

void archive_add_to_favorites(const char* file_path) {
    if(!file_path) {
        return;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, ARCHIVE_FAV_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        storage_file_write(file, (uint8_t*)file_path, strlen(file_path));
        storage_file_write(file, (uint8_t*)"\n", 1);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool archive_favorites_delete(const char* format, ...) {
    if(!format) {
        return false;
    }

    va_list args;
    va_start(args, format);
    char path_buffer[256];
    vsnprintf(path_buffer, sizeof(path_buffer), format, args);
    va_end(args);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file_in = storage_file_alloc(storage);
    File* file_out = storage_file_alloc(storage);

    bool result = false;

    if(storage_file_open(file_in, ARCHIVE_FAV_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_open(file_out, ARCHIVE_FAV_PATH ".tmp", FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            char buffer[256];
            while(archive_stub_read_line(file_in, buffer, sizeof(buffer)) > 0) {
                if(strcmp(buffer, path_buffer) != 0) {
                    storage_file_write(file_out, (uint8_t*)buffer, strlen(buffer));
                    storage_file_write(file_out, (uint8_t*)"\n", 1);
                }
            }
            result = true;
            storage_file_close(file_out);
        }
        storage_file_close(file_in);
    }

    if(result) {
        storage_common_remove(storage, ARCHIVE_FAV_PATH);
        storage_common_rename(storage, ARCHIVE_FAV_PATH ".tmp", ARCHIVE_FAV_PATH);
    }

    storage_file_free(file_in);
    storage_file_free(file_out);
    furi_record_close(RECORD_STORAGE);

    return result;
}

bool archive_favorites_rename(const char* src, const char* dst) {
    if(!src || !dst) {
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file_in = storage_file_alloc(storage);
    File* file_out = storage_file_alloc(storage);

    bool result = false;

    if(storage_file_open(file_in, ARCHIVE_FAV_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_open(file_out, ARCHIVE_FAV_PATH ".tmp", FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            char buffer[256];
            while(archive_stub_read_line(file_in, buffer, sizeof(buffer)) > 0) {
                if(strcmp(buffer, src) == 0) {
                    storage_file_write(file_out, (uint8_t*)dst, strlen(dst));
                    storage_file_write(file_out, (uint8_t*)"\n", 1);
                } else {
                    storage_file_write(file_out, (uint8_t*)buffer, strlen(buffer));
                    storage_file_write(file_out, (uint8_t*)"\n", 1);
                }
            }
            result = true;
            storage_file_close(file_out);
        }
        storage_file_close(file_in);
    }

    if(result) {
        storage_common_remove(storage, ARCHIVE_FAV_PATH);
        storage_common_rename(storage, ARCHIVE_FAV_PATH ".tmp", ARCHIVE_FAV_PATH);
    }

    storage_file_free(file_in);
    storage_file_free(file_out);
    furi_record_close(RECORD_STORAGE);

    return result;
}
