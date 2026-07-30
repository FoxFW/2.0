#pragma once

#include <furi.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#define GDR_APP_FOLDER       APP_DATA_PATH("saved")
#define GDR_APP_EXTENSION    ".psf"
#define GDR_APP_FILE_VERSION 1
#define GDR_TEMP_FILE        APP_DATA_PATH("saved/.temp.psf")
#define GDR_CACHE_FOLDER     APP_DATA_PATH("cache")
#define GDR_HISTORY_FOLDER   APP_DATA_PATH("cache/history")

bool gdr_storage_init(void);

bool gdr_storage_save_capture(
    FlipperFormat* flipper_format,
    const char* protocol_name,
    FuriString* out_path);

bool gdr_storage_save_capture_to_path(FlipperFormat* flipper_format, const char* full_path);

bool gdr_storage_save_temp(FlipperFormat* flipper_format);

void gdr_storage_delete_temp(void);

bool gdr_storage_get_next_filename(const char* protocol_name, FuriString* out_filename);

bool gdr_storage_delete_file(const char* file_path);

FlipperFormat* gdr_storage_load_file(const char* file_path);

void gdr_storage_close_file(FlipperFormat* flipper_format);

bool gdr_storage_file_exists(const char* file_path);

bool gdr_storage_ensure_history_folder(void);

void gdr_storage_purge_temp_history_at_startup(void);

void gdr_storage_wipe_history_cache(void);

bool gdr_storage_save_history_capture(
    FlipperFormat* flipper_format,
    uint32_t seq,
    FuriString* out_path);

void gdr_storage_build_history_path(uint32_t seq, FuriString* out);
