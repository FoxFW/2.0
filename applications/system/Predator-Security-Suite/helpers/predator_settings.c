#include "predator_settings.h"
#include "../predator_i.h"
#include <storage/storage.h>
#include <furi.h>
#include <string.h>
#include <stdlib.h>

#define SETTINGS_PATH "/ext/predator_settings.cfg"

static bool read_line(File* file, char* out, size_t out_len) {
    size_t idx = 0;
    char ch = 0;
    uint16_t br = 0;
    while(idx + 1 < out_len && (br = storage_file_read(file, &ch, 1)) == 1) {
        if(ch == '\n' || ch == '\r') break;
        out[idx++] = ch;
    }
    out[idx] = '\0';
    return idx > 0 || br == 1;
}

static bool ensure_file(Storage* storage) {
    File* file = storage_file_alloc(storage);
    bool ok = true;
    if(!storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        // Create empty file
        if(storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_close(file);
            ok = true;
        } else {
            ok = false;
        }
    } else {
        storage_file_close(file);
    }
    storage_file_free(file);
    return ok;
}

bool predator_settings_get_int(struct PredatorApp* app, const char* key, int32_t default_value, int32_t* out_value) {
    if(!out_value || !key) return false;
    *out_value = default_value;
    Storage* storage = app && app->storage ? app->storage : furi_record_open(RECORD_STORAGE);
    bool close_storage = (!app || storage != app->storage);
    if(!ensure_file(storage)) { if(close_storage) furi_record_close(RECORD_STORAGE); return false; }

    File* file = storage_file_alloc(storage);
    bool found = false;
    if(storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[96];
        while(read_line(file, line, sizeof(line))) {
            if(line[0] == '\0') continue;
            char* eq = strchr(line, '=');
            if(!eq) continue;
            *eq = '\0';
            const char* k = line;
            const char* v = eq + 1;
            if(strcmp(k, key) == 0) {
                *out_value = (int32_t)strtol(v, NULL, 10);
                found = true;
                break;
            }
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    if(close_storage) furi_record_close(RECORD_STORAGE);
    return found;
}

bool predator_settings_set_int(struct PredatorApp* app, const char* key, int32_t value) {
    if(!key) return false;
    Storage* storage = app && app->storage ? app->storage : furi_record_open(RECORD_STORAGE);
    bool close_storage = (!app || storage != app->storage);
    if(!ensure_file(storage)) { if(close_storage) furi_record_close(RECORD_STORAGE); return false; }

    // Read all lines to memory and replace key
    File* file = storage_file_alloc(storage);
    char buffer[2048]; size_t used = 0;
    if(storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[96];
        while(read_line(file, line, sizeof(line))) {
            if(line[0] == '\0') continue;
            char* eq = strchr(line, '=');
            if(eq) {
                *eq = '\0';
                const char* k = line;
                const char* rest = eq + 1;
                if(strcmp(k, key) == 0) {
                    // skip existing entry
                    continue;
                } else {
                    // append original line
                    size_t len = strlen(k) + 1 + strlen(rest);
                    if(used + len + 2 < sizeof(buffer)) {
                        used += snprintf(buffer + used, sizeof(buffer) - used, "%s=%s\n", k, rest);
                    }
                }
            }
        }
        storage_file_close(file);
    }
    // Append new key=value
    char valbuf[32]; snprintf(valbuf, sizeof(valbuf), "%ld", (long)value);
    used += snprintf(buffer + used, sizeof(buffer) - used, "%s=%s\n", key, valbuf);

    // Write back
    bool ok = false;
    if(storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, buffer, used);
        storage_file_close(file);
        ok = true;
    }
    storage_file_free(file);
    if(close_storage) furi_record_close(RECORD_STORAGE);
    return ok;
}
