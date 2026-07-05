// helpers/gdr_storage.c
#include "gdr_storage.h"
#include "../defines.h"
#include "../protocols/protocols_common.h"

#define TAG "GDRStorage"

bool gdr_storage_init(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool result = storage_simply_mkdir(storage, GDR_APP_FOLDER);
    furi_record_close(RECORD_STORAGE);
    return result;
}

void gdr_storage_wipe_history_cache(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage_dir_exists(storage, GDR_HISTORY_FOLDER)) {
        storage_simply_remove_recursive(storage, GDR_HISTORY_FOLDER);
        FURI_LOG_I(TAG, "Wiped history cache");
    }
    furi_record_close(RECORD_STORAGE);
}

void gdr_storage_purge_temp_history_at_startup(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage_dir_exists(storage, GDR_HISTORY_FOLDER)) {
        storage_simply_remove_recursive(storage, GDR_HISTORY_FOLDER);
    }
    furi_record_close(RECORD_STORAGE);
}

bool gdr_storage_ensure_history_folder(void) {
    if(!gdr_storage_init()) {
        return false;
    }
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, GDR_CACHE_FOLDER);
    bool ok = storage_simply_mkdir(storage, GDR_HISTORY_FOLDER);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

void gdr_storage_build_history_path(uint32_t seq, FuriString* out) {
    furi_check(out);
    furi_string_printf(
        out,
        "%s/hist_%08lu%s",
        GDR_HISTORY_FOLDER,
        (unsigned long)seq,
        GDR_APP_EXTENSION);
}

bool gdr_storage_save_history_capture(
    FlipperFormat* flipper_format,
    uint32_t seq,
    FuriString* out_path) {
    furi_check(flipper_format);
    furi_check(out_path);

    if(!gdr_storage_ensure_history_folder()) {
        FURI_LOG_E(TAG, "History folder missing");
        return false;
    }

    gdr_storage_build_history_path(seq, out_path);

    return gdr_storage_save_capture_to_path(
        flipper_format, furi_string_get_cstr(out_path));
}

static void sanitize_filename(const char* input, char* output, size_t output_size) {
    if(!output || output_size == 0) return;
    if(!input) {
        output[0] = '\0';
        return;
    }
    size_t i = 0;
    size_t j = 0;
    while(input[i] != '\0' && j < output_size - 1) {
        char c = input[i];
        if(c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
           c == '>' || c == '|' || c == ' ') {
            output[j] = '_';
        } else {
            output[j] = c;
        }
        i++;
        j++;
    }
    output[j] = '\0';
}

bool gdr_storage_get_next_filename(const char* protocol_name, FuriString* out_filename) {
    if(!protocol_name || !out_filename) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* temp_path = furi_string_alloc();
    uint32_t index = 0;
    bool found = false;

    char safe_name[64];
    sanitize_filename(protocol_name, safe_name, sizeof(safe_name));

    while(!found && index <= 999) {
        furi_string_printf(
            temp_path,
            "%s/%s_%03lu%s",
            GDR_APP_FOLDER,
            safe_name,
            (unsigned long)index,
            GDR_APP_EXTENSION);

        if(!storage_file_exists(storage, furi_string_get_cstr(temp_path))) {
            furi_string_set(out_filename, temp_path);
            found = true;
        } else {
            index++;
        }
    }

    furi_string_free(temp_path);
    furi_record_close(RECORD_STORAGE);
    return found;
}

static const char* const gdr_storage_base_u32_fields[] = {
    "TE",
    FF_SERIAL,
    FF_BTN,
    "BtnSig",
    FF_CNT,
    "Extra",
    "ExtraBit",
    "Extra_bits",
    "Tail",
    "Checksum",
    "CRC",
    FF_TYPE,
};

static const char* const gdr_storage_tail_u32_fields[] = {
    "DataHi",
    "DataLo",
    "RawCnt",
    "Encrypted",
    "Decrypted",
    "KIAVersion",
    "Checksum",
};

static bool gdr_storage_fail(const char* action, const char* key) {
    UNUSED(action);
    UNUSED(key);
    FURI_LOG_E(TAG, "%s failed: %s", action, key);
    return false;
}

static bool
    gdr_storage_get_count(FlipperFormat* flipper_format, const char* key, uint32_t* count) {
    *count = 0;
    flipper_format_rewind(flipper_format);
    return flipper_format_get_value_count(flipper_format, key, count) && (*count > 0);
}

static bool gdr_storage_copy_string_optional(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    const char* key,
    FuriString* value) {
    flipper_format_rewind(flipper_format);
    if(!flipper_format_read_string(flipper_format, key, value)) {
        return true;
    }
    if(!flipper_format_write_string(save_file, key, value)) {
        return gdr_storage_fail("Write", key);
    }
    return true;
}

static bool gdr_storage_copy_string_if_present(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    const char* key,
    FuriString* value) {
    uint32_t count = 0;
    if(!gdr_storage_get_count(flipper_format, key, &count)) {
        return true;
    }
    if(!flipper_format_read_string(flipper_format, key, value)) {
        return gdr_storage_fail("Read", key);
    }
    if(!flipper_format_write_string(save_file, key, value)) {
        return gdr_storage_fail("Write", key);
    }
    return true;
}

static bool gdr_storage_copy_u32_optional(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    const char* key) {
    uint32_t value = 0;
    flipper_format_rewind(flipper_format);
    if(!flipper_format_read_uint32(flipper_format, key, &value, 1)) {
        return true;
    }
    if(!flipper_format_write_uint32(save_file, key, &value, 1)) {
        return gdr_storage_fail("Write", key);
    }
    return true;
}

static bool gdr_storage_copy_u32_fields(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    const char* const* fields,
    size_t field_count) {
    for(size_t i = 0; i < field_count; i++) {
        if(!gdr_storage_copy_u32_optional(save_file, flipper_format, fields[i])) {
            return false;
        }
    }
    return true;
}

static bool gdr_storage_copy_hex_fixed(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    const char* key,
    size_t len,
    bool* copied) {
    uint8_t data[8];
    furi_check(len <= sizeof(data));
    if(copied) {
        *copied = false;
    }

    flipper_format_rewind(flipper_format);
    if(!flipper_format_read_hex(flipper_format, key, data, len)) {
        return true;
    }
    if(copied) {
        *copied = true;
    }
    if(!flipper_format_write_hex(save_file, key, data, len)) {
        return gdr_storage_fail("Write", key);
    }
    return true;
}

static bool gdr_storage_copy_u32_array(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    const char* key,
    uint32_t count,
    uint32_t max_count) {
    if(count >= max_count) {
        FURI_LOG_E(TAG, "%s too large: %lu", key, (unsigned long)count);
        return false;
    }

    uint32_t* data = malloc(sizeof(uint32_t) * count);
    if(!data) {
        FURI_LOG_E(TAG, "Malloc failed: %s (%lu u32)", key, (unsigned long)count);
        return false;
    }

    bool status = false;
    flipper_format_rewind(flipper_format);
    if(!flipper_format_read_uint32(flipper_format, key, data, count)) {
        gdr_storage_fail("Read", key);
    } else if(!flipper_format_write_uint32(save_file, key, data, count)) {
        gdr_storage_fail("Write", key);
    } else {
        status = true;
    }

    free(data);
    return status;
}

static bool gdr_storage_copy_u32_array_if_present(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    const char* key,
    uint32_t max_count) {
    uint32_t count = 0;
    if(!gdr_storage_get_count(flipper_format, key, &count)) {
        return true;
    }
    return gdr_storage_copy_u32_array(save_file, flipper_format, key, count, max_count);
}

static bool gdr_storage_copy_hex_array_if_present(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    const char* key,
    uint32_t max_count) {
    uint32_t count = 0;
    if(!gdr_storage_get_count(flipper_format, key, &count)) {
        return true;
    }
    if(count >= max_count) {
        FURI_LOG_E(TAG, "%s too large: %lu", key, (unsigned long)count);
        return false;
    }

    uint8_t* data = malloc(count);
    if(!data) {
        FURI_LOG_E(TAG, "Malloc failed: %s (%lu bytes)", key, (unsigned long)count);
        return false;
    }

    bool status = false;
    flipper_format_rewind(flipper_format);
    if(!flipper_format_read_hex(flipper_format, key, data, count)) {
        gdr_storage_fail("Read", key);
    } else if(!flipper_format_write_hex(save_file, key, data, count)) {
        gdr_storage_fail("Write", key);
    } else {
        status = true;
    }

    free(data);
    return status;
}

static bool gdr_storage_copy_key(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    FuriString* value) {
    uint32_t count = 0;

    flipper_format_rewind(flipper_format);
    if(flipper_format_read_string(flipper_format, FF_KEY, value)) {
        if(!flipper_format_write_string(save_file, FF_KEY, value)) {
            return gdr_storage_fail("Write", FF_KEY);
        }
        return true;
    }

    if(gdr_storage_get_count(flipper_format, FF_KEY, &count)) {
        return gdr_storage_copy_u32_array(save_file, flipper_format, FF_KEY, count, 1024);
    }

    return gdr_storage_copy_hex_fixed(save_file, flipper_format, FF_KEY, 8, NULL);
}

static bool gdr_storage_copy_hex_or_u32(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    const char* key,
    size_t hex_len) {
    bool copied = false;
    if(!gdr_storage_copy_hex_fixed(save_file, flipper_format, key, hex_len, &copied)) {
        return false;
    }
    return copied || gdr_storage_copy_u32_optional(save_file, flipper_format, key);
}

static bool gdr_storage_copy_key_2(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format,
    FuriString* value) {
    bool copied = false;
    if(!gdr_storage_copy_hex_fixed(save_file, flipper_format, "Key_2", 8, &copied)) {
        return false;
    }
    if(copied) {
        return true;
    }
    return gdr_storage_copy_string_optional(save_file, flipper_format, "Key_2", value) &&
           gdr_storage_copy_u32_optional(save_file, flipper_format, "Key_2");
}

static bool gdr_storage_write_capture_data(
    FlipperFormat* save_file,
    FlipperFormat* flipper_format) {
    furi_check(save_file);
    furi_check(flipper_format);

    FuriString* string_value = furi_string_alloc();
    if(!string_value) {
        FURI_LOG_E(TAG, "Failed to alloc string_value");
        return false;
    }

    bool status = false;
    do {
        if(!gdr_storage_copy_string_optional(
               save_file, flipper_format, FF_PROTOCOL, string_value))
            break;
        if(!gdr_storage_copy_u32_optional(save_file, flipper_format, FF_BIT)) break;
        if(!gdr_storage_copy_key(save_file, flipper_format, string_value)) break;
        if(!gdr_storage_copy_u32_optional(save_file, flipper_format, FF_FREQUENCY)) break;
        if(!gdr_storage_copy_string_optional(
               save_file, flipper_format, FF_PRESET, string_value))
            break;
        if(!gdr_storage_copy_string_optional(
               save_file, flipper_format, "RadioDevice", string_value))
            break;
        if(!gdr_storage_copy_string_if_present(
               save_file, flipper_format, "Custom_preset_module", string_value))
            break;
        if(!gdr_storage_copy_hex_array_if_present(
               save_file, flipper_format, "Custom_preset_data", 1024))
            break;
        if(!gdr_storage_copy_u32_fields(
               save_file,
               flipper_format,
               gdr_storage_base_u32_fields,
               COUNT_OF(gdr_storage_base_u32_fields)))
            break;
        if(!gdr_storage_copy_hex_fixed(save_file, flipper_format, "Key2", 8, NULL)) break;
        if(!gdr_storage_copy_u32_optional(save_file, flipper_format, "KeyIdx")) break;
        if(!gdr_storage_copy_u32_optional(save_file, flipper_format, "Seed")) break;
        if(!gdr_storage_copy_hex_or_u32(save_file, flipper_format, "ValidationField", 2))
            break;
        if(!gdr_storage_copy_key_2(save_file, flipper_format, string_value)) break;
        if(!gdr_storage_copy_hex_or_u32(save_file, flipper_format, "Key_3", 4)) break;
        if(!gdr_storage_copy_u32_optional(save_file, flipper_format, "Key_4")) break;
        if(!gdr_storage_copy_u32_optional(save_file, flipper_format, "Fx")) break;
        if(!gdr_storage_copy_hex_fixed(save_file, flipper_format, "Key1", 8, NULL)) break;
        if(!gdr_storage_copy_u32_optional(save_file, flipper_format, "Check")) break;
        if(!gdr_storage_copy_u32_array_if_present(
               save_file, flipper_format, "RAW_Data", 4096))
            break;
        if(!gdr_storage_copy_u32_fields(
               save_file,
               flipper_format,
               gdr_storage_tail_u32_fields,
               COUNT_OF(gdr_storage_tail_u32_fields)))
            break;
        if(!gdr_storage_copy_string_optional(
               save_file, flipper_format, FF_MANUFACTURE, string_value))
            break;
        status = true;
    } while(false);

    furi_string_free(string_value);

    return status;
}

bool gdr_storage_save_capture_to_path(FlipperFormat* flipper_format, const char* full_path) {
    furi_check(flipper_format);
    furi_check(full_path);

    if(!gdr_storage_init()) {
        FURI_LOG_E(TAG, "Failed to create app folder");
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* save_file = flipper_format_file_alloc(storage);
    bool result = false;

    do {
        // Remove if it already exists (overwrite)
        if(storage_file_exists(storage, full_path)) {
            storage_simply_remove(storage, full_path);
        }

        if(!flipper_format_file_open_new(save_file, full_path)) {
            FURI_LOG_E(TAG, "Failed to create file: %s", full_path);
            break;
        }

        if(!flipper_format_write_header_cstr(save_file, "Flipper SubGhz Key File", 1)) {
            FURI_LOG_E(TAG, "Failed to write header");
            break;
        }

        if(!gdr_storage_write_capture_data(save_file, flipper_format)) {
            FURI_LOG_E(TAG, "Failed to write capture data");
            break;
        }

        result = true;
        FURI_LOG_I(TAG, "Saved capture to %s", full_path);

    } while(false);

    flipper_format_free(save_file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

void gdr_storage_delete_temp(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage_file_exists(storage, GDR_TEMP_FILE)) {
        storage_simply_remove(storage, GDR_TEMP_FILE);
        FURI_LOG_I(TAG, "Deleted temp file");
    }
    furi_record_close(RECORD_STORAGE);
}

bool gdr_storage_save_capture(
    FlipperFormat* flipper_format,
    const char* protocol_name,
    FuriString* out_path) {
    furi_check(flipper_format);
    furi_check(protocol_name);
    furi_check(out_path);

    if(!gdr_storage_init()) {
        FURI_LOG_E(TAG, "Failed to create app folder");
        return false;
    }

    FuriString* file_path = furi_string_alloc();

    if(!gdr_storage_get_next_filename(protocol_name, file_path)) {
        FURI_LOG_E(TAG, "Failed to get next filename");
        furi_string_free(file_path);
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* save_file = flipper_format_file_alloc(storage);
    bool result = false;

    do {
        if(!flipper_format_file_open_new(save_file, furi_string_get_cstr(file_path))) {
            FURI_LOG_E(TAG, "Failed to create file");
            break;
        }

        if(!flipper_format_write_header_cstr(save_file, "Flipper SubGhz Key File", 1)) {
            FURI_LOG_E(TAG, "Failed to write header");
            break;
        }

        if(!gdr_storage_write_capture_data(save_file, flipper_format)) {
            FURI_LOG_E(TAG, "Failed to write capture data");
            break;
        }

        if(out_path) furi_string_set(out_path, file_path);

        result = true;
        FURI_LOG_I(TAG, "Saved capture to %s", furi_string_get_cstr(file_path));

    } while(false);

    flipper_format_free(save_file);
    furi_string_free(file_path);
    furi_record_close(RECORD_STORAGE);
    return result;
}

bool gdr_storage_delete_file(const char* file_path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool result = storage_simply_remove(storage, file_path);
    furi_record_close(RECORD_STORAGE);

    FURI_LOG_I(TAG, "Delete file %s: %s", file_path, result ? "OK" : "FAILED");
    return result;
}
