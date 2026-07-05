#include "archive_stub.h"
#include <furi.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Direct inclusion of the official favorites header to anchor the single source of truth
#include <applications/main/archive/helpers/archive_favorites.h>

#define ARCHIVE_FAV_PATH EXT_PATH("favorites.txt")

/**
 * Archive Stub Implementation
 * * Provides minimal stub functions for archive functionality when Archive
 * is running as an external app (.fap file) instead of built-in.
 * * These stubs handle:
 * - Favorites list management (simple file-based storage)
 * - Settings pin/unpin dialogs
 * - Archive app record (NULL, since it's external)
 */

// FLIPPER_ARCHIVE record - NULL since archive is now external
const void* FLIPPER_ARCHIVE = NULL;

/**
 * Stub: Helper to safely read a line from a Storage File instance
 * Without relying on non-exported implicit library helper layouts.
 */
static int32_t archive_stub_read_line(File* file, char* buffer, size_t max_len) {
    size_t i = 0;
    while(i < max_len - 1) {
        char c;
        uint16_t read = storage_file_read(file, &c, 1);
        if(read == 0) {
            if(i == 0) return 0; // EOF reached immediately
            break;
        }
        if(c == '\r') continue;
        if(c == '\n') break;
        buffer[i++] = c;
    }
    buffer[i] = '\0';
    return i;
}

/**
 * Stub: Handle setting pin/unpin from long-press in settings menus
 * Shows a simple dialog instead of launching archive settings
 */
void archive_favorites_handle_setting_pin_unpin(const char* app_name, const char* setting) {
    UNUSED(app_name);
    UNUSED(setting);
    // Minimal stub - parameters cleanly silenced to satisfy -Werror constraints
}

/**
 * Stub: Check if a file is in favorites list
 * Reads from the favorites.txt file on SD card
 */
bool archive_is_favorite(const char* format, ...) {
    if(!format) {
        return false;
    }

    // Create path from format string
    va_list args;
    va_start(args, format);
    char path_buffer[256];
    vsnprintf(path_buffer, sizeof(path_buffer), format, args);
    va_end(args);

    // Try to read favorites file
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

/**
 * Stub: Add file to favorites list
 * Appends to favorites.txt on SD card
 */
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

/**
 * Stub: Delete file from favorites list
 * Rebuilds favorites.txt without the target entry
 */
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

    // Read old favorites and write new ones, skipping the target
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

    // Replace original with temp file
    if(result) {
        storage_common_remove(storage, ARCHIVE_FAV_PATH);
        storage_common_rename(storage, ARCHIVE_FAV_PATH ".tmp", ARCHIVE_FAV_PATH);
    }

    storage_file_free(file_in);
    storage_file_free(file_out);
    furi_record_close(RECORD_STORAGE);

    return result;
}

/**
 * Stub: Rename file in favorites list
 * Updates all references from src to dst
 */
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