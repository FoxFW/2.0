#include "subghz_saved_match.h"

#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <flipper_format/flipper_format_i.h>
#include <lib/subghz/types.h>
#include <lib/toolbox/path.h>
#include <string.h>

#define TAG                       "SubGhzSavedMatch"
#define SUBGHZ_SAVED_MATCH_MARGIN 50

bool subghz_saved_match_signal(
    FlipperFormat* received_ff,
    FuriString* out_matched_name,
    FuriString* out_matched_path) {
    furi_check(received_ff);
    furi_check(out_matched_name);
    furi_check(out_matched_path);

    FuriString* rx_protocol = furi_string_alloc();
    uint32_t rx_serial = 0;
    uint32_t rx_cnt = 0;
    uint8_t rx_key[sizeof(uint64_t)] = {0};
    bool rx_has_serial = false;
    bool rx_has_key = false;
    bool rx_has_cnt = false;

    flipper_format_rewind(received_ff);
    if(!flipper_format_read_string(received_ff, "Protocol", rx_protocol)) {
        furi_string_free(rx_protocol);
        return false;
    }

    flipper_format_rewind(received_ff);
    if(flipper_format_read_uint32(received_ff, "Serial", &rx_serial, 1)) {
        rx_has_serial = true;
    }

    if(!rx_has_serial) {
        flipper_format_rewind(received_ff);
        if(flipper_format_read_hex(received_ff, "Key", rx_key, sizeof(rx_key))) {
            rx_has_key = true;
        }
    }

    flipper_format_rewind(received_ff);
    if(flipper_format_read_uint32(received_ff, "Cnt", &rx_cnt, 1)) {
        rx_has_cnt = true;
    }

    if(!rx_has_serial && !rx_has_key) {
        FURI_LOG_D(TAG, "No Serial or Key in received signal, skip match");
        furi_string_free(rx_protocol);
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);

    bool found = false;

    if(!storage_dir_open(dir, SUBGHZ_APP_FOLDER)) {
        FURI_LOG_D(TAG, "Cannot open subghz folder");
        storage_dir_close(dir);
        storage_file_free(dir);
        furi_record_close(RECORD_STORAGE);
        furi_string_free(rx_protocol);
        return false;
    }

    FileInfo file_info;
    char file_name[128];
    size_t ext_len = strlen(SUBGHZ_APP_FILENAME_EXTENSION);

    while(storage_dir_read(dir, &file_info, file_name, sizeof(file_name))) {
        if(file_info.flags & FSF_DIRECTORY) continue;

        size_t name_len = strlen(file_name);
        if(name_len <= ext_len) continue;
        if(strcmp(file_name + name_len - ext_len, SUBGHZ_APP_FILENAME_EXTENSION) != 0) continue;

        FuriString* saved_path = furi_string_alloc_printf("%s/%s", SUBGHZ_APP_FOLDER, file_name);

        FlipperFormat* saved_ff = flipper_format_file_alloc(storage);
        if(!flipper_format_file_open_existing(saved_ff, furi_string_get_cstr(saved_path))) {
            flipper_format_free(saved_ff);
            furi_string_free(saved_path);
            continue;
        }

        FuriString* saved_protocol = furi_string_alloc();
        flipper_format_rewind(saved_ff);
        bool protocol_match = flipper_format_read_string(saved_ff, "Protocol", saved_protocol) &&
                              furi_string_cmp(rx_protocol, saved_protocol) == 0;
        furi_string_free(saved_protocol);

        if(!protocol_match) {
            flipper_format_free(saved_ff);
            furi_string_free(saved_path);
            continue;
        }

        bool identity_match = false;
        if(rx_has_serial) {
            uint32_t saved_serial = 0;
            flipper_format_rewind(saved_ff);
            if(flipper_format_read_uint32(saved_ff, "Serial", &saved_serial, 1)) {
                identity_match = (saved_serial == rx_serial);
            }
        } else {
            uint8_t saved_key[sizeof(uint64_t)] = {0};
            flipper_format_rewind(saved_ff);
            if(flipper_format_read_hex(saved_ff, "Key", saved_key, sizeof(saved_key))) {
                identity_match = (memcmp(rx_key, saved_key, sizeof(rx_key)) == 0);
            }
        }

        if(!identity_match) {
            flipper_format_free(saved_ff);
            furi_string_free(saved_path);
            continue;
        }

        if(rx_has_cnt) {
            uint32_t saved_cnt = 0;
            flipper_format_rewind(saved_ff);
            if(flipper_format_read_uint32(saved_ff, "Cnt", &saved_cnt, 1)) {
                int64_t diff = (int64_t)rx_cnt - (int64_t)saved_cnt;
                if(diff < 0) diff = -diff;
                if(diff > SUBGHZ_SAVED_MATCH_MARGIN) {
                    flipper_format_free(saved_ff);
                    furi_string_free(saved_path);
                    continue;
                }
            }
        }

        path_extract_filename(saved_path, out_matched_name, true);
        furi_string_set(out_matched_path, saved_path);
        found = true;

        flipper_format_free(saved_ff);
        furi_string_free(saved_path);
        break;
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(rx_protocol);

    return found;
}
