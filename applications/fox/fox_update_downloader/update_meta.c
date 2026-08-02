#include "update_meta.h"
#include "json_mini.h"
#include "strutil.h"

#include <string.h>
#include <stdio.h>

#define UPDATER_DOWNLOAD_DIR UPDATER_DATA_DIR "/downloads"

static const char* flow_name(UpdaterFlow flow) {
    return flow == UpdaterFlowFirmware ? "firmware" : "esp32";
}

static UpdaterFlow flow_from_name(const char* name) {
    return (strcmp(name, "firmware") == 0) ? UpdaterFlowFirmware : UpdaterFlowEsp32;
}

void update_meta_write(
    Storage* storage,
    const char* asset_path,
    UpdaterFlow flow,
    const char* tag,
    const char* commit,
    const char* board_folder,
    uint32_t size) {
    char meta_path[UPDATER_PATH_LEN];
    snprintf(meta_path, sizeof(meta_path), "%s.meta", asset_path);

    char json[512];
    snprintf(
        json,
        sizeof(json),
        "{\"flow\":\"%s\",\"tag\":\"%s\",\"commit\":\"%s\",\"board\":\"%s\",\"size\":%lu,"
        "\"asset\":\"%s\"}",
        flow_name(flow),
        tag ? tag : "",
        commit ? commit : "",
        board_folder ? board_folder : "",
        (unsigned long)size,
        asset_path);

    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, meta_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, json, strlen(json));
        storage_file_close(f);
    }
    storage_file_free(f);
}

static bool try_meta_candidate(
    Storage* storage,
    const char* meta_path,
    UpdaterFlow flow,
    const char* board_folder,
    DownloadedMeta* out) {
    char buf[512] = {0};
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, meta_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t got = storage_file_read(f, buf, sizeof(buf) - 1);
        buf[got] = '\0';
        storage_file_close(f);
    }
    storage_file_free(f);
    if(buf[0] == '\0') return false;

    char flow_str[16] = {0};
    json_mini_get_string(buf, "flow", flow_str, sizeof(flow_str));
    UpdaterFlow found_flow = flow_from_name(flow_str);
    if(flow != UpdaterFlowNone && found_flow != flow) return false;

    char found_board[16] = {0};
    json_mini_get_string(buf, "board", found_board, sizeof(found_board));
    if(board_folder && board_folder[0] != '\0' && strcmp(found_board, board_folder) != 0) return false;

    char asset_path[UPDATER_PATH_LEN] = {0};
    json_mini_get_string(buf, "asset", asset_path, sizeof(asset_path));
    if(asset_path[0] == '\0') return false;

    FileInfo stat_info;
    if(storage_common_stat(storage, asset_path, &stat_info) != FSE_OK) return false;

    uint32_t size = 0;
    json_mini_get_uint(buf, "size", &size);
    if(size == 0) return false;

    if(stat_info.flags & FSF_DIRECTORY) {
        static const char* k_esp32_files[3] = {"bootloader.bin", "partitions.bin", "firmware.bin"};
        uint32_t total = 0;
        for(uint8_t k = 0; k < 3; k++) {
            char sub_path[UPDATER_PATH_LEN + 16];
            snprintf(sub_path, sizeof(sub_path), "%s/%s", asset_path, k_esp32_files[k]);
            FileInfo sub_info;
            if(storage_common_stat(storage, sub_path, &sub_info) != FSE_OK) return false;
            total += (uint32_t)sub_info.size;
        }
        if(total != size) return false;
    } else {
        if((uint32_t)stat_info.size != size) return false;
    }

    out->found = true;
    out->flow = found_flow;
    snprintf(out->asset_path, sizeof(out->asset_path), "%s", asset_path);
    json_mini_get_string(buf, "tag", out->tag, sizeof(out->tag));
    json_mini_get_string(buf, "commit", out->commit, sizeof(out->commit));
    str_copy(out->board_folder, sizeof(out->board_folder), found_board);
    out->size = size;
    return true;
}

void update_meta_find(Storage* storage, UpdaterFlow flow, const char* board_folder, DownloadedMeta* out) {
    memset(out, 0, sizeof(*out));

    File* dir = storage_file_alloc(storage);
    FileInfo info;
    char name[128];

    if(storage_dir_open(dir, UPDATER_DOWNLOAD_DIR)) {
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(info.flags & FSF_DIRECTORY) continue;
            size_t len = strlen(name);
            if(len < 6 || strcmp(name + len - 5, ".meta") != 0) continue;

            char meta_path[UPDATER_PATH_LEN];
            snprintf(meta_path, sizeof(meta_path), "%s/%s", UPDATER_DOWNLOAD_DIR, name);
            if(try_meta_candidate(storage, meta_path, flow, board_folder, out)) break;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
}

void update_meta_delete(Storage* storage, const char* asset_path) {
    char meta_path[UPDATER_PATH_LEN];
    snprintf(meta_path, sizeof(meta_path), "%s.meta", asset_path);

    char self_progress[UPDATER_PATH_LEN + 48];
    snprintf(self_progress, sizeof(self_progress), "%s.progress", asset_path);
    storage_common_remove(storage, self_progress);

    FileInfo stat_info;
    if(storage_common_stat(storage, asset_path, &stat_info) == FSE_OK &&
       (stat_info.flags & FSF_DIRECTORY)) {
        static const char* k_esp32_files[3] = {"bootloader.bin", "partitions.bin", "firmware.bin"};
        for(uint8_t k = 0; k < 3; k++) {
            char sub_path[UPDATER_PATH_LEN + 16];
            snprintf(sub_path, sizeof(sub_path), "%s/%s", asset_path, k_esp32_files[k]);
            char sub_progress[UPDATER_PATH_LEN + 48];
            snprintf(sub_progress, sizeof(sub_progress), "%s.progress", sub_path);
            storage_common_remove(storage, sub_progress);
            storage_common_remove(storage, sub_path);
        }
    }
    storage_common_remove(storage, asset_path);
    storage_common_remove(storage, meta_path);
}
